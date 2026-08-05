"""
Wire L_Tutorial_Island into a winnable raid.

    python gs_ue.py tools\\hamlet\\gs_wire_tutorial.py --timeout 300

Idempotent - safe to re-run. Every step checks before it creates.

Requires UGSRaidLibrary (2026-08-05): editor Python cannot construct an FGameplayTag in this build,
so a script can place a burn objective but can never make it count toward the win condition without
that bridge. See AGENT_STATE.md FAILED.

SAVES the level at the end, on purpose. An earlier version deliberately did not, so that
overwriting a 184 MB map stayed a deliberate act - and then the editor died during the PIE test that
followed and took every placement with it, because they existed only in memory. Re-running this is
cheap and reverting it is `git checkout`; losing an editor session is neither.
"""
import math

import unreal

OUT = []


def say(s):
    # Accumulate and print once at the end: execute_python_code DISCARDS all buffered stdout when
    # the script raises, so anything printed before an exception is lost.
    OUT.append(str(s))


def flush():
    print("\n".join(OUT))


def step(label, fn):
    """Run one step in isolation so a failure costs that step, not the whole pass."""
    try:
        fn()
    except Exception as exc:
        say("  !! %s FAILED: %r" % (label, exc))


eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lib = unreal.GSRaidLibrary


def actors():
    return eas.get_all_level_actors()


def mesh_of(a):
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    return c.static_mesh.get_name() if (c and c.static_mesh) else None


def by_label(name):
    return [a for a in actors() if a.get_actor_label() == name]


def ground_z(x, y, fallback):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    try:
        hit = unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(x, y, fallback + 50000.0), unreal.Vector(x, y, fallback - 50000.0),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
        if hit:
            return hit.to_tuple()[4].z
    except Exception as exc:
        say("  (ground trace failed: %s)" % exc)
    return fallback


# ------------------------------------------------------------------ guard: PIE
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    say("REFUSING: PIE is running. Editor-world edits are invisible during PIE and would be lost.")
    flush()
    raise SystemExit

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
say("WORLD: %s" % (world.get_name() if world else None))
if not world or "Tutorial_Island" not in world.get_name():
    say("REFUSING: expected L_Tutorial_Island to be the open level.")
    flush()
    raise SystemExit


# ------------------------------------------------------------------ 1. tag the existing field
def tag_existing():
    say("\n[1] Tagging existing burn carriers")
    found = False
    for a in actors():
        if isinstance(a, unreal.GSFieldFireObjective) and a.get_actor_label() != "GS_FarmField":
            ok = lib.set_objective_identity(a, "Objective.Burn.Field", "The Wheat Field")
            say("  MODIFIED %s -> Objective.Burn.Field %s" % (a.get_actor_label(), "OK" if ok else "FAILED"))
            found = True
    if not found:
        say("  (no untagged field found)")


# ------------------------------------------------------------------ 2. a sibling field
def second_field():
    say("\n[2] Second field (so the Q-37 demotion pass is actually exercised)")
    if by_label("GS_FarmField"):
        say("  SKIP: GS_FarmField exists")
        return
    fx, fy = 30000.0, 60000.0
    fz = ground_z(fx, fy, 1224.0)
    f = eas.spawn_actor_from_class(unreal.GSFieldFireObjective,
                                   unreal.Vector(fx, fy, fz), unreal.Rotator(0, 0, 0))
    f.set_actor_label("GS_FarmField")
    for prop, val in (("rows", 12), ("columns", 12), ("cell_size", 640.0)):
        try:
            f.set_editor_property(prop, val)
        except Exception as exc:
            say("  (set %s failed: %s)" % (prop, exc))
    lib.set_objective_identity(f, "Objective.Burn.Field", "The Farm Field")
    say("  CREATED GS_FarmField at (%.0f, %.0f, %.0f) 12x12" % (fx, fy, fz))


# ------------------------------------------------------------------ 3. the windmill
def windmill():
    say("\n[3] Windmill objective")
    if any(isinstance(a, unreal.GSMillObjective) for a in actors()):
        say("  SKIP: a mill objective already exists")
        return
    mills = [a for a in actors() if mesh_of(a) == "SM_WIndmill_Base"]
    if not mills:
        say("  SKIP: no SM_WIndmill_Base in the level")
        return
    mills.sort(key=lambda m: m.get_actor_location().x)
    target = mills[0]
    loc = target.get_actor_location()
    m = eas.spawn_actor_from_class(unreal.GSMillObjective, loc, target.get_actor_rotation())
    m.set_actor_label("GS_Windmill")
    lib.set_objective_identity(m, "Objective.Burn.Mill", "The Windmill")
    say("  CREATED GS_Windmill at (%.0f, %.0f, %.0f) on %s" % (loc.x, loc.y, loc.z, target.get_actor_label()))
    say("  NOTE: no Window-tagged primitives yet, so a PLAYER cannot light it - the mill refuses")
    say("        exterior fire by design. GS.Burn.IgniteAll calls IgniteInterior() directly.")


# ------------------------------------------------------------------ 4. market + flammable stalls
def market():
    say("\n[4] Market objective + flammable stalls")
    stalls = [a for a in actors() if mesh_of(a) in ("SM_MarketStallStructure", "SM_MarketTable")]
    if not stalls:
        say("  SKIP: no stall meshes found")
        return

    # AdoptCluster name-filters on {"Stall","Market"} AND requires a flammable component. The mesh
    # names already satisfy the filter; the components are what is missing, and without them the
    # market adopts ZERO stalls and reports it only in a log the player never sees.
    comps = []
    for a in stalls:
        c = lib.make_actor_flammable(a)
        if c:
            comps.append(c)
    say("  flammable components present on %d / %d stall actors" % (lib.count_flammable(stalls), len(stalls)))

    # UGSFlammableComponent::SpreadRadius defaults to 450 uu, which is smaller than the gap between
    # any two stalls on this map - so a lit stall burns out alone, the market sits at 1/64 = 1.6%
    # forever, and the raid cannot be won. Nothing reports this: spread simply never happens.
    #
    # Size it from the real geometry: the p75 nearest-neighbour distance, with margin. Using p75
    # rather than max keeps one outlying market table from turning the whole square into a single
    # blast radius, and using nearest-neighbour rather than centre-distance is what actually governs
    # whether fire can step from one stall to the next.
    pts = [(a.get_actor_location().x, a.get_actor_location().y) for a in stalls]
    nn = []
    for i, (x1, y1) in enumerate(pts):
        best = None
        for j, (x2, y2) in enumerate(pts):
            if i == j:
                continue
            d = math.hypot(x1 - x2, y1 - y2)
            if best is None or d < best:
                best = d
        if best is not None:
            nn.append(best)
    nn.sort()
    if nn:
        p75 = nn[int(len(nn) * 0.75)]
        spread = max(600.0, min(3000.0, p75 * 1.4))
        for c in comps:
            try:
                c.set_editor_property("spread_radius", spread)
                c.set_editor_property("can_spread", True)
            except Exception as exc:
                say("  (spread set failed: %s)" % exc)
                break
        say("  nearest-neighbour gap: median=%.0f p75=%.0f max=%.0f" % (nn[len(nn)//2], p75, nn[-1]))
        say("  spread_radius -> %.0f on %d components (default 450 could not bridge any gap)"
            % (spread, len(comps)))

    # THE MARKET IS ONE SQUARE, NOT EVERY MARKET-ISH PROP ON THE MAP.
    #
    # Getting this wrong makes the objective mathematically impossible, silently. The market
    # completes at 75% of the stalls it ADOPTED, and fire only travels between stalls within
    # SpreadRadius of each other - so adopting scattered props inflates the denominator with stalls
    # the fire can never reach. On this map the 67 stall actors form SEVENTEEN disconnected clusters:
    # the largest is 24 stalls, and even a 4000 uu spread links at most 30 of them. Adopting all 64
    # meant needing 48 to burn when at most ~30 were reachable. Nothing reports that; the market
    # simply sits below threshold forever.
    #
    # So: find the connected clusters at the spread distance, take the largest, and make THAT the
    # market. 75% of one connected cluster is reachable by definition.
    link = spread if nn else 600.0
    pts = [(a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z)
           for a in stalls]

    seen, comps = set(), []
    for i in range(len(pts)):
        if i in seen:
            continue
        stack, comp = [i], []
        seen.add(i)
        while stack:
            k = stack.pop()
            comp.append(k)
            for j in range(len(pts)):
                if j not in seen and math.dist(pts[k], pts[j]) <= link:
                    seen.add(j)
                    stack.append(j)
        comps.append(comp)
    comps.sort(key=len, reverse=True)

    big = comps[0]
    cx = sum(pts[i][0] for i in big) / len(big)
    cy = sum(pts[i][1] for i in big) / len(big)
    cz = sum(pts[i][2] for i in big) / len(big)
    spanr = max(math.hypot(pts[i][0] - cx, pts[i][1] - cy) for i in big)
    say("  clusters at link=%.0f: %d total, sizes %s" % (link, len(comps), [len(c) for c in comps[:5]]))

    existing = [a for a in actors() if isinstance(a, unreal.GSMarketObjective)]
    if existing:
        mk = existing[0]
        say("  SKIP create: a market objective already exists")
    else:
        mk = eas.spawn_actor_from_class(unreal.GSMarketObjective,
                                        unreal.Vector(cx, cy, cz), unreal.Rotator(0, 0, 0))
        mk.set_actor_label("GS_Market")
        lib.set_objective_identity(mk, "Objective.Burn.Market", "The Market")
        say("  CREATED GS_Market")

    # Centre it ON the cluster every run, not on the centroid of all stalls. The centroid of
    # seventeen scattered clusters lands in empty ground between them, which is why the first stall
    # GS.Burn.IgniteAll lit was an isolated straggler 1307 uu from anything: it burned out alone and
    # the market never moved off 1/64.
    mk.set_actor_location(unreal.Vector(cx, cy, cz), False, True)

    radius = spanr * 1.25
    mk.set_editor_property("auto_adopt_radius", radius)
    reach = sum(1 for p in pts if math.hypot(p[0] - cx, p[1] - cy) <= radius)
    say("  centred on the largest cluster (%d stalls, span %.0f) at (%.0f, %.0f, %.0f)"
        % (len(big), spanr, cx, cy, cz))
    say("  auto_adopt_radius -> %.0f, adopting ~%d stalls; needs 75%% = ~%d to burn"
        % (radius, reach, int(reach * 0.75 + 0.5)))


# ------------------------------------------------------------------ 5. the runic site
def runic_site():
    say("\n[5] Runic site")
    if any(isinstance(a, unreal.GSRunicSite) for a in actors()):
        say("  SKIP: GS_RunicSite exists")
        return
    starts = [a for a in actors() if isinstance(a, unreal.PlayerStart)]
    if starts:
        loc, rot = starts[0].get_actor_location(), starts[0].get_actor_rotation()
    else:
        loc, rot = unreal.Vector(-14200, 59000, 1326), unreal.Rotator(0, 0, 0)
    s = eas.spawn_actor_from_class(unreal.GSRunicSite, loc, rot)
    s.set_actor_label("GS_RunicSite")
    say("  CREATED GS_RunicSite at (%.0f, %.0f, %.0f)" % (loc.x, loc.y, loc.z))
    say("  NOTE: reparent to BP_GS_RunicSite for the Portal 4 visuals (SM_Portal4 / M_Portal4 /")
    say("        N_Portal4_V2). Bare C++ actor has no mesh assigned, so the portal is invisible.")


for label, fn in (("tag_existing", tag_existing), ("second_field", second_field),
                  ("windmill", windmill), ("market", market), ("runic_site", runic_site)):
    step(label, fn)

# ------------------------------------------------------------------ report
say("\n=== FINAL ROSTER ===")
types = {}
for a in actors():
    if isinstance(a, unreal.GSBurnObjectiveBase):
        t = a.get_editor_property("objective_type_tag")
        tn = unreal.GameplayTagLibrary.get_tag_name(t)
        tn = str(tn) if tn else "<EMPTY>"
        types.setdefault(tn, []).append(a.get_actor_label())
        say("  %-16s %-24s tag=%s" % (a.get_actor_label(), a.get_class().get_name(), tn))
    elif isinstance(a, unreal.GSRunicSite):
        say("  %-16s %s" % (a.get_actor_label(), a.get_class().get_name()))

say("\nTypes required to win: %d" % len([k for k in types if k != "<EMPTY>"]))
for k, v in types.items():
    say("  %-28s %d carrier(s): %s" % (k, len(v), ", ".join(v)))
if "<EMPTY>" in types:
    say("  WARNING: untagged carriers can never satisfy the win condition.")


# SAVE, immediately, before anything else touches the editor.
#
# This script originally refused to save on the reasoning that overwriting a 184 MB map should be a
# deliberate act. That reasoning cost the whole pass on 2026-08-05: the editor died during the PIE
# test that came next, and every placement above went with it because it lived only in memory.
# Wiring is cheap to re-run and cheap to revert (git); an unsaved editor is neither. Save first,
# review after - and PIE only ever runs against something already on disk.
try:
    les.save_current_level()
    say("\nSAVED L_Tutorial_Island.")
except Exception as exc:
    say("\nSAVE FAILED: %r - the placements above exist only in memory, save manually NOW." % (exc,))

flush()
