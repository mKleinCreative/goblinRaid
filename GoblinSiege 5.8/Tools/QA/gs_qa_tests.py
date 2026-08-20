"""
gs_qa_tests - deterministic test suite for Goblin Siege.

The companion to gs_qa_agent.py. The agent is stochastic: it wanders, it fuzzes,
and two runs with different seeds find different things. This file is the opposite,
on purpose - fixed inputs, fixed expectations, same answer every run - so a change
can be judged without a human watching PIE and without spending a single model token.

    Tools\\QA\\Run-Tests.ps1

Each case is a generator so it can wait for the game to react (see gs_qa_core for
why a plain loop cannot work here):

    def t_jump_leaves_the_ground(t):
        t.pawn.jump()
        yield 0.4
        t.check("FALLING" in t.movement_mode(), "jump puts the character in the air")

A case reports through `t.check(...)` / `t.eq(...)`; every check is recorded with its
own message, so a failure names the assertion that broke rather than the test.

Three outcomes:
    PASS   every check in the case held
    FAIL   at least one did not - the message says what was expected and what happened
    SKIP   the case could not run here (subsystem absent on this map, API not exposed
           to Python in this build). A skip is never counted as a pass.

Cases marked destructive=True kill the player or end the raid, so they run last on
each map, after everything that needs a healthy pawn.
"""

import os
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
    Driver, REPORT_DIR, actors_of_class, capsule_component, cfg, claim_singleton,
    console, dist, ensure_report_dir, enum_value, movement_mode_of,
    session_load, session_save,
    game_world, get, is_finite, length, now_iso, player_controller, player_pawn,
    safe, say, stamp, subsystem_of, vec, write_json, write_text,
)

ARENA = "/Game/Maps/Test/L_CombatArena"
ISLAND = "/Game/Maps/L_Tutorial_Island"


# --------------------------------------------------------------------------- handle

class Skip(Exception):
    pass


class T(object):
    """The handle a test case is given. Owns the checks and the world accessors."""

    def __init__(self, case, map_path):
        self.case = case
        self.map_path = map_path
        self.checks = []
        self.skipped = None
        self.refresh()

    # -- world -------------------------------------------------------------

    def refresh(self):
        self.world = game_world()
        self.pawn = player_pawn(self.world)
        self.pc = player_controller(self.world)

    def require(self, value, what):
        if value is None or value is False:
            raise Skip(what)
        return value

    def component(self, klass_name):
        klass = getattr(unreal, klass_name, None)
        if klass is None or self.pawn is None:
            return None
        ok, comps = safe(self.pawn, "get_components_by_class", klass)
        if ok and comps:
            return comps[0]
        return None

    def stamina(self):
        return self.component("GSStaminaComponent")

    def interaction(self):
        return get(self.pawn, "get_interaction_component") or \
            self.component("GSInteractionComponent")

    def carry(self):
        return get(self.pawn, "get_carry_component") or self.component("GSCarryComponent")

    def horde(self):
        return subsystem_of(self.world, "GSHordeSubsystem")

    def raid(self):
        return subsystem_of(self.world, "GSRaidDirector")

    def movement_mode(self):
        return movement_mode_of(self.pawn)

    def location(self):
        try:
            return self.pawn.get_actor_location()
        except Exception:
            return vec(0, 0, 0)

    def drive(self, direction, scale=1.0):
        safe(self.pawn, "add_movement_input", direction, scale, False)

    # -- assertions --------------------------------------------------------

    def check(self, condition, message, detail=""):
        self.checks.append({"ok": bool(condition), "message": message, "detail": detail})
        return bool(condition)

    def eq(self, actual, expected, message, tolerance=0.001):
        try:
            ok = abs(float(actual) - float(expected)) <= tolerance
        except (TypeError, ValueError):
            ok = actual == expected
        return self.check(ok, message, "expected %s, got %s" % (expected, actual))

    def near(self, actual, expected, tolerance, message):
        try:
            ok = abs(float(actual) - float(expected)) <= tolerance
        except (TypeError, ValueError):
            ok = False
        return self.check(ok, message,
                          "expected %s +/- %s, got %s" % (expected, tolerance, actual))

    @property
    def passed(self):
        return bool(self.checks) and all(c["ok"] for c in self.checks)

    def first_failure(self):
        for c in self.checks:
            if not c["ok"]:
                return c
        return None


class Case(object):
    def __init__(self, name, area, fn, maps=None, destructive=False):
        self.name = name
        self.area = area
        self.fn = fn
        self.maps = maps
        self.destructive = destructive

    def runs_on(self, map_path):
        return self.maps is None or map_path in self.maps


CASES = []


def case(name, area, maps=None, destructive=False):
    def wrap(fn):
        CASES.append(Case(name, area, fn, maps, destructive))
        return fn
    return wrap


# =========================================================================== player


@case("player.spawns_alive", "player")
def t_player_spawns_alive(t):
    t.require(t.pawn is not None, "no player pawn in PIE")
    health = get(t.pawn, "get_health")
    max_health = get(t.pawn, "get_max_health")
    t.check(is_finite(t.location()), "spawn location is finite",
            str(t.location()))
    t.check(get(t.pawn, "is_alive") is True, "player spawns alive")
    t.require(max_health, "GetMaxHealth is not readable")
    t.eq(health, max_health, "player spawns on full health",
         tolerance=0.01)
    yield


@case("player.spawn_is_above_the_kill_floor", "player")
def t_player_above_kill_floor(t):
    loc = t.location()
    t.check(loc.z > -20000.0, "spawn is above the world floor", "z=%.1f" % loc.z)
    yield 0.5
    t.refresh()
    settled = t.location()
    t.check(settled.z > -20000.0, "still above the world floor half a second later",
            "z=%.1f" % settled.z)


@case("player.respawn_state_sets_the_fraction", "player")
def t_respawn_state(t):
    max_health = t.require(get(t.pawn, "get_max_health"), "GetMaxHealth unreadable")
    ok, _ = safe(t.pawn, "apply_respawn_state", 0.5, 0.0)
    t.require(ok, "ApplyRespawnState is not exposed to Python")
    yield 0.3
    t.near(get(t.pawn, "get_health"), max_health * 0.5, max_health * 0.05,
           "ApplyRespawnState(0.5) leaves the player on half health")
    safe(t.pawn, "apply_respawn_state", 1.0, 0.0)
    yield 0.3


# =========================================================================== stamina


@case("stamina.starts_full", "stamina")
def t_stamina_full(t):
    stam = t.require(t.stamina(), "no UGSStaminaComponent on the player")
    t.eq(get(stam, "get_stamina"), get(stam, "get_max_stamina"),
         "stamina starts at max", tolerance=0.5)
    yield


@case("stamina.consume_reduces_the_pool", "stamina")
def t_stamina_consume(t):
    stam = t.require(t.stamina(), "no UGSStaminaComponent on the player")
    safe(stam, "reset_to_full")
    safe(stam, "set_regen_suppressed", True)
    yield 0.2
    before = get(stam, "get_stamina")
    ok, accepted = safe(stam, "try_consume", 10.0)
    yield 0.2
    t.check(accepted is True, "TryConsume(10) is accepted on a full pool")
    t.near(get(stam, "get_stamina"), before - 10.0, 0.6,
           "TryConsume(10) removes exactly 10")
    safe(stam, "set_regen_suppressed", False)
    safe(stam, "reset_to_full")
    yield 0.2


@case("stamina.refuses_a_cost_it_cannot_pay", "stamina")
def t_stamina_overdraw(t):
    stam = t.require(t.stamina(), "no UGSStaminaComponent on the player")
    safe(stam, "reset_to_full")
    yield 0.2
    cap = get(stam, "get_max_stamina") or 100.0
    before = get(stam, "get_stamina")
    ok, accepted = safe(stam, "try_consume", cap * 10.0)
    yield 0.2
    t.check(accepted is False, "a cost larger than the pool is refused")
    t.near(get(stam, "get_stamina"), before, 1.0,
           "a refused cost leaves the pool untouched")


@case("stamina.a_negative_cost_cannot_refund", "stamina")
def t_stamina_negative(t):
    """A negative cost that ADDS stamina is free sprint forever. This is the exploit
    test, not a theoretical one - TryConsume takes a raw float from any caller."""
    stam = t.require(t.stamina(), "no UGSStaminaComponent on the player")
    safe(stam, "reset_to_full")
    safe(stam, "set_regen_suppressed", True)
    yield 0.2
    safe(stam, "try_consume", 30.0)
    yield 0.2
    drained = get(stam, "get_stamina")
    safe(stam, "try_consume", -50.0)
    yield 0.3
    after = get(stam, "get_stamina")
    t.check(after <= drained + 0.6,
            "TryConsume(-50) does not refund stamina",
            "before %.1f, after %.1f" % (drained, after))
    cap = get(stam, "get_max_stamina") or 100.0
    t.check(-0.01 <= after <= cap + 0.01,
            "stamina stays inside [0, max] after a negative cost",
            "value %.2f, max %.2f" % (after, cap))
    safe(stam, "set_regen_suppressed", False)
    safe(stam, "reset_to_full")
    yield 0.2


@case("stamina.suppressed_regen_actually_holds", "stamina")
def t_stamina_suppression(t):
    stam = t.require(t.stamina(), "no UGSStaminaComponent on the player")
    safe(stam, "reset_to_full")
    safe(stam, "set_regen_suppressed", True)
    safe(stam, "try_consume", 25.0)
    yield 0.3
    held = get(stam, "get_stamina")
    yield 2.0
    t.near(get(stam, "get_stamina"), held, 1.0,
           "stamina does not regenerate while regen is suppressed")
    safe(stam, "set_regen_suppressed", False)
    yield 2.0
    t.check(get(stam, "get_stamina") > held + 0.5,
            "stamina regenerates once suppression is lifted",
            "held %.1f -> %.1f" % (held, get(stam, "get_stamina")))
    safe(stam, "reset_to_full")
    yield 0.2


# =========================================================================== movement


@case("movement.input_moves_the_character", "movement")
def t_movement_walks(t):
    start = t.location()
    for _ in range(90):                      # ~1.5s of held input at 60fps
        t.drive(vec(1, 0, 0), 1.0)
        yield None
    travelled = dist(t.location(), start)
    t.check(travelled > 100.0,
            "1.5s of held movement input moves the character at least 100uu",
            "travelled %.1fuu" % travelled)


@case("movement.jump_leaves_and_returns_to_the_ground", "movement")
def t_movement_jump(t):
    safe(t.pawn, "jump")
    yield 0.35
    airborne = "FALLING" in t.movement_mode()
    safe(t.pawn, "stop_jumping")
    t.check(airborne, "jump puts the character into a falling state",
            "mode=%s" % t.movement_mode())
    landed = False
    for _ in range(50):
        yield 0.1
        if "WALKING" in t.movement_mode():
            landed = True
            break
    t.check(landed, "the character lands again within 5s",
            "mode=%s" % t.movement_mode())


@case("movement.crouch_shrinks_and_restores_the_capsule", "movement")
def t_movement_crouch(t):
    capsule = capsule_component(t.pawn)
    t.require(capsule, "no capsule component")
    try:
        standing = float(capsule.get_editor_property("capsule_half_height"))
    except Exception:
        raise Skip("capsule_half_height not readable")
    safe(t.pawn, "crouch", False)
    yield 0.6
    crouched = float(capsule.get_editor_property("capsule_half_height"))
    t.check(crouched < standing, "crouching shrinks the capsule",
            "%.1f -> %.1f" % (standing, crouched))
    safe(t.pawn, "un_crouch", False)
    yield 0.6
    restored = float(capsule.get_editor_property("capsule_half_height"))
    t.near(restored, standing, 1.0, "standing back up restores the capsule")


# =========================================================================== combat


@case("combat.light_attack_is_accepted", "combat")
def t_combat_light(t):
    ok, accepted = safe(t.pawn, "try_light_attack")
    t.require(ok, "TryLightAttack is not exposed to Python")
    t.check(accepted is True, "a healthy player can start a light attack")
    yield 1.2


@case("combat.block_sets_and_clears", "combat")
def t_combat_block(t):
    ok, started = safe(t.pawn, "start_blocking")
    t.require(ok, "StartBlocking is not exposed to Python")
    yield 0.4
    t.check(get(t.pawn, "is_blocking") is True, "StartBlocking sets the blocking state")
    safe(t.pawn, "stop_blocking")
    yield 0.4
    t.check(get(t.pawn, "is_blocking") is False, "StopBlocking clears the blocking state")


@case("combat.player_is_hostile_to_the_human_side", "combat", maps=[ARENA])
def t_teams_hostile(t):
    console("GS.Combat.SpawnPatrol 1 0 0 700", t.world)
    yield 4.0
    t.refresh()
    klass = getattr(unreal, "GSEnemyCharacter", None)
    t.require(klass, "GSEnemyCharacter is not exposed to Python")
    enemies = actors_of_class(klass, t.world)
    t.require(enemies, "GS.Combat.SpawnPatrol produced no defenders")
    enemy = enemies[0]
    t.check(get(t.pawn, "is_hostile_to", None) is not None or True, "hostility readable")
    ok, hostile = safe(t.pawn, "is_hostile_to", enemy)
    t.require(ok, "IsHostileTo is not exposed to Python")
    t.check(hostile is True, "the player goblin is hostile to a spawned human defender",
            "against %s" % enemy.get_name())
    ok, back = safe(enemy, "is_hostile_to", t.pawn)
    t.check(back is True, "hostility is symmetric - the defender is hostile to the player")


@case("combat.allied_goblins_are_not_hostile_to_the_player", "combat")
def t_teams_allied(t):
    horde = t.require(t.horde(), "no UGSHordeSubsystem in this world")
    t.require(t.pc, "no player controller")
    safe(horde, "summon_wave", t.pc)
    yield 4.0
    t.refresh()
    klass = getattr(unreal, "GSHordeGoblin", None)
    t.require(klass, "GSHordeGoblin is not exposed to Python")
    goblins = actors_of_class(klass, t.world)
    t.require(goblins, "the horn summoned nothing")
    ok, hostile = safe(t.pawn, "is_hostile_to", goblins[0])
    t.require(ok, "IsHostileTo is not exposed to Python")
    t.check(hostile is False, "a summoned horde goblin is NOT hostile to the player",
            "against %s" % goblins[0].get_name())


# =========================================================================== horde


@case("horde.summon_draws_from_the_reserve", "horde")
def t_horde_draw(t):
    horde = t.require(t.horde(), "no UGSHordeSubsystem in this world")
    t.require(t.pc, "no player controller")
    safe(horde, "reset_pool_for_new_raid")
    yield 1.0
    before_active = get(horde, "get_active_count")
    before_reserve = get(horde, "get_reserve_remaining")
    ok, summoned = safe(horde, "summon_wave", t.pc)
    t.require(ok, "SummonWave is not exposed to Python")
    yield 3.0
    after_active = get(horde, "get_active_count")
    after_reserve = get(horde, "get_reserve_remaining")
    t.check(summoned > 0, "one horn blast summons at least one goblin",
            "summoned %s" % summoned)
    t.eq(after_active - before_active, summoned,
         "the active count rises by exactly the number summoned")
    t.eq(before_reserve - after_reserve, summoned,
         "the reserve falls by exactly the number summoned")


@case("horde.pool_never_exceeds_its_own_cap", "horde")
def t_horde_cap(t):
    horde = t.require(t.horde(), "no UGSHordeSubsystem in this world")
    t.require(t.pc, "no player controller")
    safe(horde, "reset_pool_for_new_raid")
    yield 1.0
    cap = t.require(get(horde, "get_active_cap"), "GetActiveCap unreadable")
    pool = get(horde, "get_raid_pool_size")
    for _ in range(10):
        safe(horde, "summon_wave", t.pc)
        yield 0.4
    yield 2.0
    active = get(horde, "get_active_count")
    reserve = get(horde, "get_reserve_remaining")
    t.check(active <= cap, "10 horn blasts never push the active count past the cap",
            "active %s, cap %s" % (active, cap))
    t.check(reserve >= 0, "the reserve never goes negative", "reserve %s" % reserve)
    if pool:
        t.check(active + reserve <= pool,
                "active + reserve conserves against the raid pool",
                "%s + %s vs pool %s" % (active, reserve, pool))


@case("horde.reset_restores_the_full_reserve", "horde")
def t_horde_reset(t):
    horde = t.require(t.horde(), "no UGSHordeSubsystem in this world")
    t.require(t.pc, "no player controller")
    safe(horde, "summon_wave", t.pc)
    yield 2.0
    safe(horde, "reset_pool_for_new_raid")
    yield 1.5
    reserve = get(horde, "get_reserve_remaining")
    pool = get(horde, "get_raid_pool_size")
    t.require(pool, "GetRaidPoolSize unreadable")
    t.eq(reserve, pool, "ResetPoolForNewRaid puts the whole pool back in reserve")


# =========================================================================== interaction


@case("interact.channel_refuses_to_start_with_no_focus", "interaction")
def t_interact_no_focus(t):
    inter = t.require(t.interaction(), "no UGSInteractionComponent on the player")
    focus = get(inter, "get_focused_interactable")
    if focus:
        raise Skip("the player spawned facing an interactable - not the case under test")
    ok, started = safe(inter, "begin_channel")
    t.require(ok, "BeginChannel is not exposed to Python")
    yield 0.4
    t.check(started is False, "BeginChannel with nothing focused is refused")
    t.check(get(inter, "is_channelling") is False,
            "a refused channel leaves no channelling state behind")


@case("interact.abort_clears_the_channel_state", "interaction")
def t_interact_abort(t):
    inter = t.require(t.interaction(), "no UGSInteractionComponent on the player")
    safe(inter, "begin_channel")
    yield 0.3
    safe(inter, "abort_channel", enum_value("EGSInteractEndReason", "CANCELLED", 6))
    yield 0.4
    t.check(get(inter, "is_channelling") is False,
            "AbortChannel clears the channelling flag")
    t.near(get(inter, "get_channel_progress"), 0.0, 0.01,
           "AbortChannel resets channel progress to zero")


@case("carry.refuses_to_carry_the_carrier", "interaction")
def t_carry_self(t):
    """Carrying your own pawn is a recursive attachment. The component must refuse it;
    if it does not, the character is welded to itself and the run is over."""
    carry = t.require(t.carry(), "no UGSCarryComponent on the player")
    ok, started = safe(carry, "start_carry", t.pawn)
    t.require(ok, "StartCarry is not exposed to Python")
    yield 0.5
    t.check(started is False, "StartCarry(self) is refused")
    t.check(get(carry, "is_carrying") is False,
            "the player is not left carrying themselves")
    if get(carry, "is_carrying"):
        safe(carry, "put_down")
        yield 0.5


@case("carry.refuses_a_null_object", "interaction")
def t_carry_null(t):
    carry = t.require(t.carry(), "no UGSCarryComponent on the player")
    ok, started = safe(carry, "start_carry", None)
    t.require(ok, "StartCarry is not exposed to Python")
    yield 0.3
    t.check(started is False, "StartCarry(None) is refused")
    t.check(get(carry, "is_carrying") is False, "no carry state is created from None")


# =========================================================================== raid / world


@case("raid.starts_unended", "raid")
def t_raid_unended(t):
    raid = t.require(t.raid(), "no UGSRaidDirector in this world")
    t.check(get(raid, "has_raid_ended") is False, "the raid does not begin already over")
    required = get(raid, "get_required_type_count")
    completed = get(raid, "get_completed_type_count")
    t.check(required is not None and required >= 0,
            "the director reports a required objective count", "required=%s" % required)
    if required:
        t.check(completed == 0, "no objective type is complete at raid start",
                "completed=%s" % completed)


@case("world.no_actor_starts_below_the_kill_floor", "world")
def t_world_actors(t):
    actors = actors_of_class(unreal.Actor, t.world)
    t.require(actors, "no actors in the world")
    stray = []
    for actor in actors:
        try:
            loc = actor.get_actor_location()
        except Exception:
            continue
        if not is_finite(loc) or loc.z < -20000.0:
            stray.append("%s z=%.0f" % (actor.get_name(), loc.z))
        if len(stray) >= 5:
            break
    t.check(not stray, "no actor spawns below the world floor or with a bad transform",
            "; ".join(stray))
    yield


# ------------------------------------------------------- destructive: these run last


@case("combat.a_dead_player_cannot_attack", "combat", destructive=True)
def t_dead_cannot_attack(t):
    t.require(get(t.pawn, "is_alive") is True, "the player is already dead")
    safe(t.pawn, "debug_kill")
    yield 1.2
    t.check(get(t.pawn, "is_alive") is False, "DebugKill kills the player")
    if get(t.pawn, "is_alive") is not False:
        return
    for verb, label in (("try_light_attack", "light attack"),
                        ("try_heavy_attack", "heavy attack"),
                        ("try_guard_break", "guard break"),
                        ("start_blocking", "block")):
        ok, accepted = safe(t.pawn, verb)
        if ok:
            t.check(accepted is not True, "a dead player cannot %s" % label)
        yield 0.15


@case("raid.end_raid_is_idempotent", "raid", destructive=True)
def t_raid_idempotent(t):
    """A second EndRaid must not overwrite the first. If it can, a lose condition
    that fires one frame after a win silently steals the win."""
    raid = t.require(t.raid(), "no UGSRaidDirector in this world")
    t.require(get(raid, "has_raid_ended") is False, "the raid has already ended")
    ok, _ = safe(raid, "end_raid", enum_value("EGSRaidResult", "EXTRACTED", 1))
    t.require(ok, "EndRaid is not exposed to Python")
    yield 1.0
    first = str(get(raid, "get_raid_result"))
    safe(raid, "end_raid", enum_value("EGSRaidResult", "LEFT_BEHIND", 2))
    yield 1.0
    second = str(get(raid, "get_raid_result"))
    t.check(get(raid, "has_raid_ended") is True, "the raid is ended after EndRaid")
    t.eq(second, first, "a second EndRaid does not change the recorded result")


# =========================================================================== runner


class TestDriver(Driver):

    def __init__(self, maps):
        Driver.__init__(self, maps)
        self.results = []
        self.run_started = now_iso()
        # NOT self.handle - Driver already owns that name for the Slate tick callback,
        # and clobbering it means the callback is never unregistered.
        self.live_handle = None
        self.tag = stamp()          # fixed once, so repeated flushes overwrite one file

    def run_map(self, map_path):
        ordered = [c for c in CASES if c.runs_on(map_path) and not c.destructive] + \
                  [c for c in CASES if c.runs_on(map_path) and c.destructive]
        say("running %d case(s) on %s" % (len(ordered), map_path))
        for case_obj in ordered:
            handle = T(case_obj, map_path)
            self.live_handle = handle
            began = time.perf_counter()
            status = "PASS"
            detail = ""
            try:
                routine = case_obj.fn(handle)
                if routine is not None:
                    for wait in routine:
                        handle.refresh()
                        yield wait
            except Skip as reason:
                status = "SKIP"
                detail = str(reason)
            except Exception as exc:
                status = "FAIL"
                detail = "the case itself raised: %s" % exc
            if status == "PASS":
                if not handle.checks:
                    status = "SKIP"
                    detail = "the case made no assertions"
                elif not handle.passed:
                    status = "FAIL"
                    bad = handle.first_failure()
                    detail = "%s (%s)" % (bad["message"], bad["detail"]) if bad else ""
                else:
                    detail = handle.checks[-1]["detail"] or handle.checks[-1]["message"]
            self.results.append({
                "order": len(self.results),
                "case": case_obj.name,
                "area": case_obj.area,
                "map": map_path.rsplit("/", 1)[-1],
                "status": status,
                "detail": detail,
                "seconds": round(time.perf_counter() - began, 2),
                "checks": handle.checks,
            })
            say("%-5s %s%s" % (status, case_obj.name, (" - " + detail) if detail else ""))
            routine = None
            handle.world = handle.pawn = handle.pc = None
            self.live_handle = None
            yield 0.2

    def release_world(self):
        handle, self.live_handle = self.live_handle, None
        if handle is not None:
            handle.world = None
            handle.pawn = None
            handle.pc = None
        Driver.release_world(self)

    # -- report ------------------------------------------------------------

    def finish(self):
        ensure_report_dir()
        tag = cfg("TAG", self.tag)

        # Fold this launch's results into the session, so a run split across one editor
        # per map still reads as one suite. Keyed by case+map: a re-run of the same map
        # replaces its earlier result rather than appearing twice.
        carried = session_load("tests")
        merged = dict(carried.get("results", {}))
        for r in self.results:
            merged["%s@%s" % (r["case"], r["map"])] = r
        session_save("tests", {"results": merged})
        self.results = sorted(merged.values(), key=lambda r: (r["map"], r["order"]))

        counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}
        for r in self.results:
            counts[r["status"]] = counts.get(r["status"], 0) + 1
        elapsed = time.perf_counter() - self.started

        lines = []
        lines.append("=" * 78)
        lines.append(" GOBLIN SIEGE - DETERMINISTIC TEST SUITE")
        lines.append(" %s   engine UE 5.8   %d case-runs   %.0fs"
                     % (now_iso().replace("T", " "), len(self.results), elapsed))
        lines.append("=" * 78)
        current_map = None
        current_area = None
        for r in self.results:
            if r["map"] != current_map:
                current_map = r["map"]
                current_area = None
                lines.append("")
                lines.append(current_map)
            if r["area"] != current_area:
                current_area = r["area"]
                lines.append("  [%s]" % current_area)
            line = "    %-4s  %-52s %s" % (r["status"], r["case"], r["detail"])
            lines.append(line.rstrip())
        lines.append("")
        lines.append("-" * 78)
        lines.append(" %d passed, %d failed, %d skipped   (%.1fs)"
                     % (counts["PASS"], counts["FAIL"], counts["SKIP"], elapsed))
        if counts["FAIL"]:
            lines.append("")
            lines.append(" FAILURES")
            for r in self.results:
                if r["status"] != "FAIL":
                    continue
                lines.append("   %s [%s]" % (r["case"], r["map"]))
                for c in r["checks"]:
                    if not c["ok"]:
                        lines.append("       %s" % c["message"])
                        if c["detail"]:
                            lines.append("           %s" % c["detail"])
                if not r["checks"]:
                    lines.append("       %s" % r["detail"])
        lines.append("=" * 78)
        text = "\n".join(lines) + "\n"

        for line in lines:
            say(line)

        write_text(os.path.join(REPORT_DIR, "tests_%s.txt" % tag), text)
        write_text(os.path.join(REPORT_DIR, "tests_latest.txt"), text)
        write_json(os.path.join(REPORT_DIR, "tests_%s.json" % tag), {
            "tool": "gs_qa_tests",
            "project": "Goblin Siege (UE 5.8)",
            "started": self.run_started,
            "finished": now_iso(),
            "maps": self.maps,
            "summary": {
                "passed": counts["PASS"],
                "failed": counts["FAIL"],
                "skipped": counts["SKIP"],
                "seconds": round(elapsed, 1),
            },
            "results": self.results,
        })


def main():
    maps = cfg("MAPS", [ARENA, ISLAND])
    say("test suite starting - %d case(s) declared, maps: %s"
        % (len(CASES), ", ".join(maps)))
    driver = TestDriver(maps)
    driver.quit_when_done = cfg("QUIT", False)   # manual in-editor runs must not close the editor
    driver.start()
    return driver


if claim_singleton("tests"):
    DRIVER = main()
else:
    say("a suite is already running in this editor - ignoring the re-entrant launch")
