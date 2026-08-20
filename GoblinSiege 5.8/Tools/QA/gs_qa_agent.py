"""
gs_qa_agent - the adversarial QA agent for Goblin Siege (Assignment 9).

This is not a bot that plays the game. It is a bot that attacks it.

The loop has three layers and they run at different rates:

  POLICY  (a few times a second)  picks the next adversarial BEHAVIOUR from a
          seeded bank and drives it - sprint into a wall, cancel a channel halfway,
          summon until the pool is dry, teleport under the map, kill yourself and
          keep swinging.

  DRIVE   (every frame)           applies whatever movement intent the current
          behaviour set. Movement input in Unreal has to be fed per frame or the
          character never moves, so intent lives here and behaviours only set it.

  MONITOR (every frame)           checks INVARIANTS. This is what "broken" means to
          this agent, stated up front rather than left to a human eye:

              1. The pawn stays inside the world      (finite, above kill Z, not adrift)
              2. Numbers stay inside their own range  (0 <= health <= max, same for stamina)
              3. Intent produces motion               (asking to move for 3s and going
                                                       nowhere is a stuck state)
              4. Transient states end                 (a channel, an attack, a fall must
                                                       terminate - a state that outlives
                                                       its own duration is a hang)
              5. Bookkeeping balances                 (horde active + reserve conserves;
                                                       claimed slots get released)
              6. The engine is not complaining        (ensures, Blueprint runtime errors
                                                       and AccessNones in the log ARE bugs
                                                       even when nothing looks wrong)
              7. The frame does not stall             (a 250ms+ hitch is a defect)

Anything that trips a monitor becomes a Finding with a location, an error type, the
game context at the moment it fired, and a deterministic repro (seed + step + the
console line that puts you back there).

Run it:
    Tools\\QA\\Run-QA.ps1                       (default: both maps, 180s each)
    Tools\\QA\\Run-QA.ps1 -Seed 1337 -Seconds 300
"""

import math
import os
import random
import sys
import time

import unreal

# The launcher stages these three files in a directory with no space in its path
# and tells us where via GSQA_DIR - see the note in Run-QA.ps1 about why.
_HERE = os.environ.get("GSQA_DIR")
if not _HERE:
    try:
        _HERE = os.path.dirname(os.path.abspath(__file__))
    except NameError:                      # executed as a bare statement, no __file__
        _HERE = os.getcwd()
sys.path.insert(0, _HERE)

from gs_qa_core import (                                     # noqa: E402
    DEFAULT_MAPS, Driver, Finding, FindingLog, LogTail, PIE, REPORT_DIR,
    actors_of_class, capsule_component, cfg, claim_singleton, console, dist,
    ensure_report_dir, find_class, game_world, movement_mode_of,
    enum_value, get, hit_actor, is_finite, length, line_trace, now_iso,
    player_controller, player_pawn,
    safe, say, session_load, session_save, stamp, subsystem_of, vec, warn,
    write_csv, write_json,
)

# --------------------------------------------------------------------------- tunables

KILL_Z = -20000.0          # below this the pawn has left the playable world
WORLD_LIMIT = 1.0e6        # beyond this it has left the world in XY too
STUCK_SECONDS = 3.0        # asking to move this long...
STUCK_DISTANCE = 60.0      # ...and covering less than this is stuck
FALL_SECONDS = 20.0        # a fall that never lands
CHANNEL_OVERRUN = 4.0      # channel running this many x its own advertised duration
ATTACK_STUCK_SECONDS = 8.0
HITCH_SECONDS = 0.25
TELEPORT_SPIKE = 5000.0    # uu moved in one frame with no teleport requested


# --------------------------------------------------------------------------- context

class Ctx(object):
    """Everything a behaviour or a monitor needs, refreshed once per frame."""

    def __init__(self, map_path, seed, findings):
        self.map_path = map_path
        self.seed = seed
        self.rng = random.Random(seed)
        self.findings = findings
        self.world = None
        self.pawn = None
        self.pc = None
        self.behaviour = "boot"
        self.step = 0
        self.sim_time = 0.0
        # movement intent, applied every frame by the driver
        self.move_dir = None
        self.move_scale = 0.0
        self.spawned = []          # actors this agent created, cleaned up at map end

    def refresh(self):
        self.world = game_world()
        self.pawn = player_pawn(self.world)
        self.pc = player_controller(self.world)
        return self.live()

    def live(self):
        if not self.world or not self.pawn:
            return False
        try:
            return bool(self.pawn.is_actor_being_destroyed() is False)
        except Exception:
            return True

    # -- reads -------------------------------------------------------------

    def location(self):
        try:
            return self.pawn.get_actor_location()
        except Exception:
            return vec(0, 0, 0)

    def velocity(self):
        try:
            return self.pawn.get_velocity()
        except Exception:
            return vec(0, 0, 0)

    def movement_mode(self):
        return movement_mode_of(self.pawn)

    def component(self, getter):
        return get(self.pawn, getter)

    def stamina(self):
        comp = None
        for name in ("get_components_by_class",):
            klass = getattr(unreal, "GSStaminaComponent", None)
            if klass is None:
                break
            ok, found = safe(self.pawn, name, klass)
            if ok and found:
                comp = found[0]
                break
        return comp

    def horde(self):
        return subsystem_of(self.world, "GSHordeSubsystem")

    def raid(self):
        return subsystem_of(self.world, "GSRaidDirector")

    # -- the structured record the report is built from --------------------

    def snapshot(self):
        loc = self.location()
        vel = self.velocity()
        interaction = self.component("get_interaction_component")
        carry = self.component("get_carry_component")
        stam = self.stamina()
        horde = self.horde()
        raid = self.raid()
        snap = {
            "behaviour": self.behaviour,
            "step": self.step,
            "seed": self.seed,
            "sim_time": round(self.sim_time, 2),
            "health": _num(get(self.pawn, "get_health")),
            "max_health": _num(get(self.pawn, "get_max_health")),
            "alive": get(self.pawn, "is_alive"),
            "movement_mode": self.movement_mode(),
            "speed": round(length(vel), 1),
            "blocking": get(self.pawn, "is_blocking"),
        }
        if stam is not None:
            snap["stamina"] = _num(get(stam, "get_stamina"))
            snap["max_stamina"] = _num(get(stam, "get_max_stamina"))
            snap["exhausted"] = get(stam, "is_exhausted")
        if interaction is not None:
            snap["is_channelling"] = get(interaction, "is_channelling")
            snap["channel_progress"] = _num(get(interaction, "get_channel_progress"))
        if carry is not None:
            snap["is_carrying"] = get(carry, "is_carrying")
        if horde is not None:
            snap["horde_active"] = get(horde, "get_active_count")
            snap["horde_reserve"] = get(horde, "get_reserve_remaining")
            snap["horde_cap"] = get(horde, "get_active_cap")
        if raid is not None:
            snap["raid_ended"] = get(raid, "has_raid_ended")
            snap["objectives_complete"] = get(raid, "are_objectives_complete")
        return snap

    def place(self, extra=None):
        loc = self.location()
        out = {
            "map": self.map_path.rsplit("/", 1)[-1],
            "x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1),
            "actor": _name(self.pawn),
        }
        # What am I standing on / next to - the single most useful thing a developer
        # wants when a report says "stuck at these coordinates".
        hit, res = line_trace(self.world, loc, vec(loc.x, loc.y, loc.z - 500.0), [self.pawn])
        if hit:
            ground = hit_actor(res)
            if ground:
                out["standing_on"] = _name(ground)
        if extra:
            out.update(extra)
        return out

    def report(self, error_type, severity, summary, detector, evidence="", repro=None, extra_loc=None):
        loc = self.location()
        base_repro = [
            "open %s, PIE" % self.map_path,
            "GS.Raid.Goto %.0f %.0f %.0f" % (loc.x, loc.y, loc.z),
            "agent seed %d, behaviour '%s', step %d" % (self.seed, self.behaviour, self.step),
        ]
        return self.findings.add(Finding(
            error_type=error_type, severity=severity, summary=summary, detector=detector,
            location=self.place(extra_loc), context=self.snapshot(),
            evidence=evidence, repro=(repro or []) + base_repro))


def _num(value):
    try:
        return round(float(value), 2)
    except (TypeError, ValueError):
        return None


def _name(obj):
    if obj is None:
        return ""
    try:
        return str(obj.get_name())
    except Exception:
        return str(obj)


# --------------------------------------------------------------------------- monitors

class Monitors(object):
    """The invariants. One tick each, cheap, stateful where it has to be."""

    def __init__(self):
        self.reset()
        self.tail = LogTail()
        self.log_seen = set()

    def reset(self):
        self.last_loc = None
        self.moving_since = None
        self.moving_from = None
        self.falling_since = None
        self.channel_since = None
        self.channel_expect = 0.0
        self.attacking_since = None
        self.horde_peak = 0
        self.last_tick = time.perf_counter()
        self.grace_until = time.perf_counter() + 2.0
        self.teleport_expected_until = 0.0

    def expect_teleport(self, seconds=0.5):
        """Behaviours that deliberately teleport announce it, so the position-spike
        monitor does not report the agent's own tools as a bug."""
        self.teleport_expected_until = time.perf_counter() + seconds

    def tick(self, ctx, dt):
        now = time.perf_counter()
        frame = now - self.last_tick
        self.last_tick = now

        self._hitch(ctx, frame)
        self._log_lines(ctx)

        if now < self.grace_until or not ctx.live():
            self.last_loc = None
            return

        loc = ctx.location()
        self._world_bounds(ctx, loc)
        self._position_spike(ctx, loc, now)
        self._attributes(ctx)
        self._stuck(ctx, loc, now)
        self._falling(ctx, now)
        self._channel(ctx, now)
        self._attack_state(ctx, now)
        self._horde_books(ctx)
        self.last_loc = loc

    # -- 1. the pawn stays inside the world --------------------------------

    def _world_bounds(self, ctx, loc):
        if not is_finite(loc):
            ctx.report("nan_transform", "critical",
                       "player transform became non-finite",
                       "WorldBounds", evidence="location=%s" % loc)
            return
        if loc.z < KILL_Z:
            ctx.report("out_of_world", "critical",
                       "player fell below the world floor (z=%.0f, limit %.0f)" % (loc.z, KILL_Z),
                       "WorldBounds",
                       evidence="no kill volume or respawn caught the fall")
        if abs(loc.x) > WORLD_LIMIT or abs(loc.y) > WORLD_LIMIT:
            ctx.report("out_of_world", "high",
                       "player left the world in XY (%.0f, %.0f)" % (loc.x, loc.y),
                       "WorldBounds")

    def _position_spike(self, ctx, loc, now):
        if self.last_loc is None or now < self.teleport_expected_until:
            return
        moved = dist(loc, self.last_loc)
        if moved > TELEPORT_SPIKE:
            ctx.report("position_spike", "high",
                       "player moved %.0fuu in one frame without a teleport" % moved,
                       "PositionSpike",
                       evidence="from (%.0f, %.0f, %.0f) to (%.0f, %.0f, %.0f)"
                                % (self.last_loc.x, self.last_loc.y, self.last_loc.z,
                                   loc.x, loc.y, loc.z))

    # -- 2. numbers stay inside their range --------------------------------

    def _attributes(self, ctx):
        health = get(ctx.pawn, "get_health")
        max_health = get(ctx.pawn, "get_max_health")
        if health is not None and max_health:
            if health < -0.01:
                ctx.report("attribute_out_of_range", "high",
                           "health went negative (%.2f)" % health, "Attributes")
            elif health > max_health + 0.01:
                ctx.report("attribute_out_of_range", "medium",
                           "health %.2f exceeds max %.2f" % (health, max_health), "Attributes")
        stam = ctx.stamina()
        if stam is not None:
            value = get(stam, "get_stamina")
            cap = get(stam, "get_max_stamina")
            if value is not None and cap:
                if value < -0.01 or value > cap + 0.01:
                    ctx.report("attribute_out_of_range", "high",
                               "stamina %.2f outside [0, %.2f]" % (value, cap), "Attributes")

    # -- 3. intent produces motion -----------------------------------------

    def _stuck(self, ctx, loc, now):
        wants_move = ctx.move_scale > 0.1 and ctx.move_dir is not None
        alive = get(ctx.pawn, "is_alive", True)
        channelling = False
        interaction = ctx.component("get_interaction_component")
        if interaction is not None:
            channelling = bool(get(interaction, "is_channelling"))
        # Being held still by design is not a stuck state.
        if not wants_move or not alive or channelling:
            self.moving_since = None
            self.moving_from = None
            return
        if self.moving_since is None:
            self.moving_since = now
            self.moving_from = loc
            return
        held = now - self.moving_since
        if held < STUCK_SECONDS:
            return
        travelled = dist(loc, self.moving_from)
        if travelled < STUCK_DISTANCE:
            ctx.report("stuck_state", "high",
                       "held movement input %.1fs and travelled %.0fuu" % (held, travelled),
                       "StuckWhileMoving",
                       evidence="mode=%s speed=%.0f - geometry trap or a movement veto"
                                " that never clears" % (ctx.movement_mode(), length(ctx.velocity())))
        self.moving_since = None
        self.moving_from = None

    # -- 4. transient states end -------------------------------------------

    def _falling(self, ctx, now):
        mode = ctx.movement_mode()
        if "FALLING" in mode.upper():
            if self.falling_since is None:
                self.falling_since = now
            elif now - self.falling_since > FALL_SECONDS:
                ctx.report("endless_fall", "high",
                           "falling for %.0fs with no landing and no kill volume"
                           % (now - self.falling_since),
                           "TransientStates",
                           evidence="z=%.0f velocity_z=%.0f"
                                    % (ctx.location().z, ctx.velocity().z))
                self.falling_since = now      # re-arm rather than spam
        else:
            self.falling_since = None

    def _channel(self, ctx, now):
        interaction = ctx.component("get_interaction_component")
        if interaction is None:
            return
        channelling = bool(get(interaction, "is_channelling"))
        if not channelling:
            self.channel_since = None
            return
        if self.channel_since is None:
            self.channel_since = now
            self.channel_expect = float(get(interaction, "get_active_channel_duration", 1.0) or 1.0)
            return
        held = now - self.channel_since
        budget = max(self.channel_expect, 0.5) * CHANNEL_OVERRUN
        if held > budget:
            focus = get(interaction, "get_focused_interactable")
            ctx.report("stuck_state", "high",
                       "interaction channel has run %.1fs against an advertised %.1fs"
                       % (held, self.channel_expect),
                       "TransientStates",
                       evidence="progress=%s focus=%s - the channel never completed"
                                " and never aborted"
                                % (get(interaction, "get_channel_progress"), _name(focus)))
            self.channel_since = now

    def _attack_state(self, ctx, now):
        # An attack that owns the character forever is the single most player-visible
        # hang this project can have, and it has shipped before (#128).
        attacking = bool(get(ctx.pawn, "is_attacking", False)) or \
            bool(get(ctx.pawn, "is_recoiling", False))
        if not attacking:
            self.attacking_since = None
            return
        if self.attacking_since is None:
            self.attacking_since = now
        elif now - self.attacking_since > ATTACK_STUCK_SECONDS:
            ctx.report("stuck_state", "high",
                       "attack/recoil state held for %.0fs" % (now - self.attacking_since),
                       "TransientStates",
                       evidence="the character cannot act while this is set")
            self.attacking_since = now

    # -- 5. bookkeeping balances -------------------------------------------

    def _horde_books(self, ctx):
        horde = ctx.horde()
        if horde is None:
            return
        active = get(horde, "get_active_count")
        reserve = get(horde, "get_reserve_remaining")
        cap = get(horde, "get_active_cap")
        pool = get(horde, "get_raid_pool_size")
        if active is None or reserve is None:
            return
        if reserve < 0:
            ctx.report("accounting_error", "high",
                       "horde reserve went negative (%d)" % reserve, "HordeBookkeeping",
                       evidence="the pool issued goblins it did not have")
        if cap and active > cap:
            ctx.report("accounting_error", "high",
                       "horde active %d exceeds the cap of %d" % (active, cap),
                       "HordeBookkeeping")
        if pool and (active + reserve) > pool:
            ctx.report("accounting_error", "medium",
                       "horde active+reserve (%d) exceeds the raid pool (%d)"
                       % (active + reserve, pool), "HordeBookkeeping",
                       evidence="goblins were created without being drawn from the reserve")
        self.horde_peak = max(self.horde_peak, active)

    # -- 6. the engine is not complaining ----------------------------------

    def _log_lines(self, ctx):
        for line in self.tail.read_new():
            fingerprint = line[-160:]
            if fingerprint in self.log_seen:
                continue
            self.log_seen.add(fingerprint)
            severity = "high" if ("Error" in line or "Ensure" in line or "Assertion" in line) \
                else "medium"
            kind = "engine_error"
            if "Blueprint Runtime Error" in line or "AccessNone" in line:
                kind = "blueprint_runtime_error"
            elif "Ensure condition failed" in line or "Assertion" in line:
                kind = "engine_ensure"
            ctx.report(kind, severity, line[:220], "EngineLog",
                       evidence="raised while behaviour '%s' was running" % ctx.behaviour)

    # -- 7. the frame does not stall ---------------------------------------

    def _hitch(self, ctx, frame):
        if frame > HITCH_SECONDS and ctx.live():
            ctx.report("frame_hitch", "low",
                       "frame took %.0fms" % (frame * 1000.0), "FrameTime",
                       evidence="threshold %.0fms" % (HITCH_SECONDS * 1000.0))


# --------------------------------------------------------------------------- behaviours
#
# Each behaviour is a generator. It sets ctx.move_dir / ctx.move_scale (the driver
# applies them every frame) and calls whatever it wants to abuse, yielding to let the
# game react. The point of every one of them is to do something a player COULD do but
# a designer did not picture.

def _rand_dir(ctx):
    angle = ctx.rng.uniform(0.0, math.tau)
    return vec(math.cos(angle), math.sin(angle), 0.0)


def _forward(ctx):
    try:
        return ctx.pawn.get_actor_forward_vector()
    except Exception:
        return vec(1, 0, 0)


def b_wander(ctx, mon):
    """Baseline: cover ground so the other behaviours fire in varied places."""
    for _ in range(4):
        ctx.move_dir = _rand_dir(ctx)
        ctx.move_scale = 1.0
        yield ctx.rng.uniform(1.0, 2.5)
        if ctx.rng.random() < 0.4:
            safe(ctx.pawn, "jump")
            yield 0.4
            safe(ctx.pawn, "stop_jumping")
    ctx.move_scale = 0.0


def b_sprint_into_geometry(ctx, mon):
    """Find a wall and run at it until something gives. Looks for capsule penetration,
    a stuck state, or the pawn squeezing through."""
    loc = ctx.location()
    best = None
    for i in range(12):
        angle = ctx.rng.uniform(0.0, math.tau)
        probe = vec(loc.x + math.cos(angle) * 900.0, loc.y + math.sin(angle) * 900.0, loc.z + 40.0)
        hit, _res = line_trace(ctx.world, vec(loc.x, loc.y, loc.z + 40.0), probe, [ctx.pawn])
        if hit:
            best = vec(math.cos(angle), math.sin(angle), 0.0)
            break
    ctx.move_dir = best or _rand_dir(ctx)
    ctx.move_scale = 1.0
    yield 4.0                       # long enough for StuckWhileMoving to arm
    # Now grind along the wall - corners and door frames are where capsules get eaten.
    for _ in range(3):
        d = ctx.move_dir
        ctx.move_dir = vec(-d.y, d.x, 0.0)
        yield 1.5
    ctx.move_scale = 0.0


def b_ledge_dive(ctx, mon):
    """Walk off whatever is highest nearby. Tests fall handling, kill volumes and
    landing - the classic 'player leaves the level' family."""
    ctx.move_dir = _rand_dir(ctx)
    ctx.move_scale = 1.0
    safe(ctx.pawn, "jump")
    yield 0.3
    safe(ctx.pawn, "stop_jumping")
    yield 6.0                       # let the fall resolve (or not)
    ctx.move_scale = 0.0


def b_ability_spam(ctx, mon):
    """Mash every combat verb with no regard for state: attack during attack, block
    during guard-break, ranged during melee. Attacks are supposed to be interruptible
    or queued - what must never happen is a state that latches."""
    verbs = ["try_light_attack", "try_heavy_attack", "try_guard_break",
             "start_blocking", "try_ranged_attack", "stop_blocking"]
    for i in range(24):
        verb = verbs[ctx.rng.randrange(len(verbs))]
        safe(ctx.pawn, verb)
        if ctx.rng.random() < 0.3:
            ctx.move_dir = _rand_dir(ctx)
            ctx.move_scale = 1.0
        yield ctx.rng.uniform(0.05, 0.25)
    safe(ctx.pawn, "stop_blocking")
    ctx.move_scale = 0.0


def b_interact_interrupt(ctx, mon):
    """Start a channelled interaction and then break every rule it depends on: walk
    away mid-channel, abort, re-begin on the same frame, take damage, die.

    Channels are the state most likely to leak, because the abort path has three
    callers and the completion path has one."""
    interaction = ctx.component("get_interaction_component")
    if interaction is None:
        return
    for i in range(8):
        ctx.move_dir = _rand_dir(ctx)
        ctx.move_scale = 1.0
        yield 0.8
        ctx.move_scale = 0.0
        safe(interaction, "begin_channel")
        yield ctx.rng.uniform(0.05, 0.4)
        choice = ctx.rng.randrange(4)
        if choice == 0:
            safe(interaction, "release_interact_input")
        elif choice == 1:
            safe(interaction, "abort_channel", enum_value("EGSInteractEndReason", "CANCELLED", 6))
        elif choice == 2:
            ctx.move_dir = _rand_dir(ctx)          # just walk out of it
            ctx.move_scale = 1.0
        else:
            safe(interaction, "begin_channel")     # re-enter without ending
        yield 0.6
        ctx.move_scale = 0.0
    yield 1.0


def b_carry_abuse(ctx, mon):
    """Carry things that should not be carryable, and drop them into geometry.

    Deliberately includes the self-carry case (carrying your own pawn) - a recursive
    attachment is a hard hang if the component does not refuse it."""
    carry = ctx.component("get_carry_component")
    if carry is None:
        return
    candidates = [ctx.pawn]
    world_actors = actors_of_class(unreal.StaticMeshActor, ctx.world)
    ctx.rng.shuffle(world_actors)
    candidates.extend(world_actors[:6])
    for actor in candidates:
        safe(carry, "start_carry", actor)
        yield 0.5
        if get(carry, "is_carrying"):
            ctx.move_dir = _rand_dir(ctx)
            ctx.move_scale = 1.0
            yield 1.0
            ctx.move_scale = 0.0
            safe(carry, "put_down")
            yield 0.5
            if get(carry, "is_carrying"):
                ctx.report("stuck_state", "medium",
                           "PutDown left the carry component still carrying %s"
                           % _name(get(carry, "get_carried_actor")),
                           "CarryAbuse",
                           evidence="carry state cannot be cleared - the player is stuck"
                                    " holding it")
                safe(carry, "destroy_carried")
                yield 0.3
    ctx.move_scale = 0.0


def b_horn_flood(ctx, mon):
    """Summon until the pool is dry, then keep summoning. The invariant under test is
    conservation: active + reserve must never exceed the raid pool, and neither may
    go negative no matter how hard the horn is mashed."""
    horde = ctx.horde()
    if horde is None or ctx.pc is None:
        return
    for i in range(14):
        safe(horde, "summon_wave", ctx.pc)
        yield 0.25
    yield 3.0
    # Now order them into contradictory states as fast as the wheel allows.
    for verb in ("Attack", "Hold", "Loot", "Follow", "Attack", "Hold"):
        console("GS.Horde.Order %s" % verb, ctx.world)
        yield 0.3
    yield 2.0


def b_teleport_probe(ctx, mon):
    """Boundary probing. Put the pawn where a player should never be - deep under the
    map, far outside the bounds, straight up - and see whether the game notices."""
    origin = ctx.location()
    probes = [
        ("under the map", vec(origin.x, origin.y, origin.z - 5000.0)),
        ("far outside bounds", vec(origin.x + 400000.0, origin.y + 400000.0, origin.z)),
        ("high above", vec(origin.x, origin.y, origin.z + 30000.0)),
        ("inside the floor", vec(origin.x, origin.y, origin.z - 120.0)),
    ]
    for label, target in probes:
        mon.expect_teleport(1.0)
        ok, moved = safe(ctx.pawn, "set_actor_location", target, False, True)
        yield 3.0
        here = ctx.location()
        if ok and moved is True and dist(here, target) < 50.0 and label != "inside the floor":
            ctx.report("boundary_break", "medium",
                       "pawn parked '%s' at (%.0f, %.0f, %.0f) and the game did not "
                       "recover it" % (label, here.x, here.y, here.z),
                       "BoundaryProbe",
                       evidence="no kill volume, respawn or bounds clamp reacted within 3s")
        mon.expect_teleport(1.0)
        safe(ctx.pawn, "set_actor_location", origin, False, True)
        yield 1.5
    ctx.move_scale = 0.0


def b_stamina_abuse(ctx, mon):
    """Hand the stamina pool values it was never meant to see. A negative cost that
    refunds stamina is an exploit; a cost that drives it below zero is a bug."""
    stam = ctx.stamina()
    if stam is None:
        return
    before = get(stam, "get_stamina")
    for cost in (-50.0, 0.0, 1.0e9, -1.0e9):
        safe(stam, "try_consume", cost)
        yield 0.2
        value = get(stam, "get_stamina")
        cap = get(stam, "get_max_stamina") or 0.0
        if value is not None and (value < -0.01 or value > cap + 0.01):
            ctx.report("exploit", "high",
                       "TryConsume(%.0f) left stamina at %.2f (valid range 0..%.2f)"
                       % (cost, value, cap),
                       "StaminaAbuse",
                       evidence="the cost argument is not validated")
    safe(stam, "reset_to_full")
    yield 0.5


def b_death_actions(ctx, mon):
    """Die, then try to keep playing. A dead pawn that can still swing, interact or
    summon is a logic violation, and it is exactly the state a respawn race produces."""
    if get(ctx.pawn, "is_alive") is False:
        return
    interaction = ctx.component("get_interaction_component")
    safe(ctx.pawn, "debug_kill")
    yield 0.6
    if get(ctx.pawn, "is_alive") is not False:
        yield 1.0
    if get(ctx.pawn, "is_alive") is False:
        acted = []
        for verb in ("try_light_attack", "try_heavy_attack", "try_guard_break",
                     "start_blocking", "try_ranged_attack"):
            ok, result = safe(ctx.pawn, verb)
            if ok and result is True:
                acted.append(verb)
            yield 0.1
        if interaction is not None:
            ok, result = safe(interaction, "begin_channel")
            if ok and result is True:
                acted.append("begin_channel")
        if acted:
            ctx.report("logic_violation", "high",
                       "a dead player still accepted: %s" % ", ".join(acted),
                       "DeathActions",
                       evidence="IsAlive() is False and these calls still returned True")
        ctx.move_dir = _rand_dir(ctx)
        ctx.move_scale = 1.0
        start = ctx.location()
        yield 2.0
        travelled = dist(ctx.location(), start)
        ctx.move_scale = 0.0
        if travelled > 200.0:
            ctx.report("logic_violation", "medium",
                       "a dead player walked %.0fuu under movement input" % travelled,
                       "DeathActions")
    yield 3.0
    console("GS.Raid.SetLives 3", ctx.world)
    yield 1.0


def b_smash_abuse(ctx, mon):
    """Feed the breakable component values a weapon never would: negative damage,
    absurd damage, and repeated smashes after it has already broken."""
    klass = getattr(unreal, "GSBreakableComponent", None)
    if klass is None:
        return
    targets = []
    for actor in actors_of_class(unreal.Actor, ctx.world)[:400]:
        ok, comps = safe(actor, "get_components_by_class", klass)
        if ok and comps:
            targets.append((actor, comps[0]))
        if len(targets) >= 3:
            break
    if not targets:
        return
    for actor, comp in targets:
        loc = actor.get_actor_location()
        for damage in (-100, 0, 1, 1000000):
            safe(comp, "apply_smash", int(damage), loc, vec(0, 0, 0), ctx.pawn)
            yield 0.15
        # Keep hitting it after it is gone - a second Break() must be a no-op.
        for _ in range(3):
            safe(comp, "break_", loc, vec(0, 0, 0))
            safe(comp, "break", loc, vec(0, 0, 0))
            yield 0.15
    yield 1.0


def b_raid_state_abuse(ctx, mon):
    """Drive the raid state machine out of order: end it twice, end it and then keep
    completing objectives, expire the clock after a win."""
    raid = ctx.raid()
    if raid is None:
        return
    if get(raid, "has_raid_ended"):
        return
    console("GS.Raid.Status", ctx.world)
    yield 0.5
    safe(raid, "end_raid", enum_value("EGSRaidResult", "EXTRACTED", 1))
    yield 1.0
    ended_once = get(raid, "has_raid_ended")
    safe(raid, "end_raid", enum_value("EGSRaidResult", "LEFT_BEHIND", 2))
    yield 1.0
    result = get(raid, "get_raid_result")
    if ended_once and result is not None:
        console("GS.Raid.ExpireClock", ctx.world)
        yield 1.0
        after = get(raid, "get_raid_result")
        if str(after) != str(result):
            ctx.report("logic_violation", "high",
                       "the raid result changed after the raid had already ended "
                       "(%s -> %s)" % (result, after),
                       "RaidStateAbuse",
                       evidence="EndRaid is not idempotent - a lose path can overwrite a win")
    yield 1.0


def b_crouch_spam(ctx, mon):
    """Crouch/uncrouch every other frame while moving and jumping. Capsule resizing
    under a low ceiling is a reliable way to end up inside geometry."""
    ctx.move_dir = _rand_dir(ctx)
    ctx.move_scale = 1.0
    for i in range(20):
        if i % 2:
            safe(ctx.pawn, "crouch", False)
        else:
            safe(ctx.pawn, "un_crouch", False)
        if i % 5 == 0:
            safe(ctx.pawn, "jump")
        yield 0.12
    safe(ctx.pawn, "un_crouch", False)
    safe(ctx.pawn, "stop_jumping")
    ctx.move_scale = 0.0
    yield 1.0


def b_combat_pressure(ctx, mon):
    """Stand up a real fight and then behave badly inside it: spawn a patrol on top of
    the player, spam verbs, and walk away mid-swing. Crowd code is where the engagement
    slot and token bookkeeping leaks live (#106)."""
    console("GS.Combat.LogDamage 1", ctx.world)
    console("GS.Combat.SpawnPatrol 4 2 1 600", ctx.world)
    yield 4.0
    for i in range(18):
        safe(ctx.pawn, ("try_light_attack", "try_heavy_attack", "start_blocking",
                        "stop_blocking")[i % 4])
        ctx.move_dir = _rand_dir(ctx)
        ctx.move_scale = 1.0 if i % 3 else 0.0
        yield 0.35
    ctx.move_scale = 0.0
    console("GS.Combat.CrowdStats", ctx.world)
    yield 2.0


BEHAVIOURS = [
    ("wander", b_wander, 3),
    ("sprint_into_geometry", b_sprint_into_geometry, 3),
    ("ledge_dive", b_ledge_dive, 2),
    ("ability_spam", b_ability_spam, 3),
    ("interact_interrupt", b_interact_interrupt, 3),
    ("carry_abuse", b_carry_abuse, 2),
    ("horn_flood", b_horn_flood, 2),
    ("teleport_probe", b_teleport_probe, 2),
    ("stamina_abuse", b_stamina_abuse, 2),
    ("crouch_spam", b_crouch_spam, 2),
    ("smash_abuse", b_smash_abuse, 2),
    ("combat_pressure", b_combat_pressure, 2),
    ("death_actions", b_death_actions, 1),
    ("raid_state_abuse", b_raid_state_abuse, 1),
]


def _weighted_bag():
    bag = []
    for name, fn, weight in BEHAVIOURS:
        bag.extend([(name, fn)] * weight)
    return bag


# --------------------------------------------------------------------------- driver

class AdversaryDriver(Driver):

    def __init__(self, maps, seed, seconds_per_map):
        Driver.__init__(self, maps)
        self.seed = seed
        self.seconds_per_map = seconds_per_map
        self.findings = FindingLog()
        self.monitors = Monitors()
        self.ctx = None
        self.per_map = []
        self.behaviour_counts = {}
        self.run_started = now_iso()
        self.tag = stamp()          # fixed once, so repeated flushes overwrite one file

    # every frame: apply movement intent, then run the invariants
    def _on_tick(self, delta_seconds):
        if self.ctx is not None and self.pie_active and not self.finished:
            try:
                if self.ctx.refresh():
                    self._apply_drive()
                    self.monitors.tick(self.ctx, delta_seconds)
                    self.ctx.sim_time += delta_seconds
            except Exception as exc:
                warn("monitor tick failed: %s" % exc)
        Driver._on_tick(self, delta_seconds)

    def _apply_drive(self):
        ctx = self.ctx
        if ctx.move_dir is None or ctx.move_scale <= 0.0:
            return
        safe(ctx.pawn, "add_movement_input", ctx.move_dir, ctx.move_scale, False)

    def run_map(self, map_path):
        ctx = Ctx(map_path, self.seed, self.findings)
        ctx.refresh()
        self.ctx = ctx
        self.monitors.reset()
        # Quiet the debug overlays so a hitch is the game's fault, not the HUD's.
        console("GS.PlayerView 1", ctx.world)
        yield 0.5

        bag = _weighted_bag()
        started = time.perf_counter()
        step = 0
        say("driving '%s' for %ds (seed %d, %d behaviours in the bag)"
            % (map_path, self.seconds_per_map, self.seed, len(BEHAVIOURS)))

        while time.perf_counter() - started < self.seconds_per_map:
            if not ctx.refresh():
                warn("lost the pawn - waiting for a respawn")
                yield 1.0
                continue
            name, fn = bag[ctx.rng.randrange(len(bag))]
            step += 1
            ctx.step = step
            ctx.behaviour = name
            ctx.move_dir = None
            ctx.move_scale = 0.0
            self.behaviour_counts[name] = self.behaviour_counts.get(name, 0) + 1
            try:
                for wait in fn(ctx, self.monitors):
                    yield wait
            except Exception as exc:
                # A behaviour that throws is itself a finding: it means a call the
                # game exposes to Blueprint blew up when handed a legal value.
                ctx.report("agent_exception", "medium",
                           "behaviour '%s' raised %s" % (name, exc),
                           "BehaviourHarness",
                           evidence=str(exc))
            ctx.move_scale = 0.0
            yield 0.3

        elapsed = time.perf_counter() - started
        self.per_map.append({
            "map": map_path,
            "seconds": round(elapsed, 1),
            "steps": step,
            "findings_so_far": len(self.findings),
        })
        say("map done: %d steps in %.0fs, %d distinct findings so far"
            % (step, elapsed, len(self.findings)))
        self.ctx = None

    def release_world(self):
        ctx, self.ctx = self.ctx, None
        if ctx is not None:
            ctx.world = None
            ctx.pawn = None
            ctx.pc = None
            ctx.spawned = []
        Driver.release_world(self)

    # -- report ------------------------------------------------------------

    def finish(self):
        ensure_report_dir()
        findings = self.findings.sorted()
        tag = cfg("TAG", self.tag)

        # Fold this launch into the session. The launcher runs one editor per map
        # (see Run-QA.ps1 for why), so the report has to accumulate across launches
        # or it would only ever describe the last map played.
        carried = session_load("agent")
        merged = dict(carried.get("findings", {}))
        for f in findings:
            item = f.to_dict()
            item_key = "|".join([
                item["error_type"], str(item["location"].get("map", "")),
                str(int(float(item["location"].get("x", 0)) // 500)),
                str(int(float(item["location"].get("y", 0)) // 500)),
                str(item["game_context"].get("behaviour", "")),
            ])
            if item_key in merged:
                merged[item_key]["occurrences"] += item["occurrences"]
            else:
                merged[item_key] = item
        runs = list(carried.get("per_map", [])) + self.per_map
        behaviours = dict(carried.get("behaviours", {}))
        for name, count in self.behaviour_counts.items():
            behaviours[name] = behaviours.get(name, 0) + count
        session_save("agent", {"findings": merged, "per_map": runs,
                               "behaviours": behaviours})

        rank = dict((n, i) for i, n in enumerate(("critical", "high", "medium", "low")))
        all_findings = sorted(merged.values(),
                              key=lambda d: (rank.get(d["severity"], 9), -d["occurrences"]))
        self.per_map = runs
        self.behaviour_counts = behaviours
        by_severity = {}
        by_type = {}
        for d in all_findings:
            by_severity[d["severity"]] = by_severity.get(d["severity"], 0) + 1
            by_type[d["error_type"]] = by_type.get(d["error_type"], 0) + 1

        payload = {
            "tool": "gs_qa_agent",
            "assignment": "09 - adversarial QA agent",
            "project": "Goblin Siege (UE 5.8)",
            "run": {
                "started": self.run_started,
                "finished": now_iso(),
                "seed": self.seed,
                "seconds_per_map": self.seconds_per_map,
                "maps": self.maps,
                "wall_clock_seconds": round(time.perf_counter() - self.started, 1),
                "behaviours_run": self.behaviour_counts,
                "per_map": self.per_map,
                "aborted": self.aborted,
            },
            "invariants": [
                "pawn transform stays finite and inside the world",
                "health and stamina stay inside their own declared ranges",
                "held movement input produces movement",
                "transient states (channel, attack, fall) terminate",
                "horde pool accounting conserves (active + reserve <= pool)",
                "the engine log stays free of ensures, errors and AccessNones",
                "no frame exceeds 250ms",
            ],
            "summary": {
                "total_findings": len(all_findings),
                "total_occurrences": sum(d["occurrences"] for d in all_findings),
                "by_severity": by_severity,
                "by_error_type": by_type,
            },
            "findings": all_findings,
        }
        json_path = os.path.join(REPORT_DIR, "qa_run_%s.json" % tag)
        csv_path = os.path.join(REPORT_DIR, "qa_run_%s.csv" % tag)
        write_json(json_path, payload)
        write_csv(csv_path, all_findings)
        write_json(os.path.join(REPORT_DIR, "qa_run_latest.json"), payload)

        say("-" * 70)
        say("ADVERSARIAL RUN COMPLETE - %d distinct findings, %d occurrences"
            % (len(all_findings), sum(d["occurrences"] for d in all_findings)))
        for sev in ("critical", "high", "medium", "low"):
            if by_severity.get(sev):
                say("  %-8s %d" % (sev, by_severity[sev]))
        for d in all_findings[:12]:
            say("  [%s] %s (x%d) @ %s" % (d["severity"], d["error_type"], d["occurrences"],
                                          d["location"].get("map", "?")))
            say("      %s" % d["summary"])
        say("-" * 70)


# --------------------------------------------------------------------------- entry

def main():
    maps = cfg("MAPS", list(DEFAULT_MAPS))
    seed = cfg("SEED", 20260819)
    seconds = cfg("SECONDS", 180)
    quiet_quit = cfg("QUIT", False)   # manual in-editor runs must not close the editor
    say("adversarial agent starting - seed=%d seconds/map=%d maps=%s"
        % (seed, seconds, ", ".join(maps)))
    driver = AdversaryDriver(maps, seed, seconds)
    driver.quit_when_done = quiet_quit
    driver.start()
    return driver


if claim_singleton("agent"):
    DRIVER = main()
else:
    say("an agent is already running in this editor - ignoring the re-entrant launch")
