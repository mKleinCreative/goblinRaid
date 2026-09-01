---
id: 377
title: Town garrison: 8 -> 24 defenders, first real archer placement, patrol splines reused
agent: claude-town
status: done
claimed: 2026-08-30T09:07Z
build: none
waiting_on:
evaluated: 2026-08-30T09:21:30Z
observed: 2026-08-30T09:21:28Z | Michael watched the new patrol routes run in PIE and confirmed they look good, including the ground-snap fix (navmesh projection) for the placements that had been floating/in-ground/on a roof.
scenario: Live PIE watch of the town garrison patrolling L_Tutorial_Island
files: 
  - Content/Maps/L_Tutorial_Island.umap
  - Content/Blueprints/Adversaries/BP_ErikaArcher.uasset
---

## Goal

Town garrison: 8 -> 24 defenders, first real archer placement, patrol splines reused

## Goal

Michael: "get combat in the town feeling good... enough troops patrolling around" - the town
(`L_Tutorial_Island`) had 8 total defenders (4x `BP_CastleGuard01`, 3x `BP_CastleGuard02`, 1x
`BP_KnightDPelegrini`), all melee, all on `BP_GSAIController_Militia`. Separately asked "do archers
work at all" - answer researched and reported: `#354` fixed and verified the archer AI path
(`BP_GSAIController_Archer` + `DA_GS_CombatBehaviour_Archer`), but only on `BP_ErikaArcher` in an
isolated test arena - she was never placed in any real map. Both complaints share one cause: the
fixed archer path was never deployed.

Michael's placement direction, given live: ~24 total ("heavier presence"), majority in and around
town, 1-2 patrols near the windmills, 1 patrol specifically in the market, 1 specifically in lower
town near the bridge.

## Generate

**Found 31 `ACF_SplinePath_BP_C` patrol routes already authored in the level, only 7 in use** - 24
pre-built patrol routes were sitting unused, strongly suggesting a larger garrison was always the
intent. Used 16 of them (kept the existing 8 guards' routes untouched):

- 2 near the windmill cluster (mill objectives at ~(-20350,67632) and (7494,75496))
- 1 at the only unused spline in the lower-town/bridge zone (near `BP_GS_RunicSite_C`, the
  extraction point, at (-1551,19009))
- 1 at the spline closest to `GSMarketObjective_0` (-11965,54964)
- 12 more spread across the remaining unused town-zone splines

Composition: 12 new melee (duplicated from the existing `BP_CastleGuard01`/`02` instances, so they
carry the same hand-tuned CDO overrides rather than fresh defaults) + 4 new archers (`BP_ErikaArcher`,
spawned fresh). Final: 20 melee-type + 4 archer = 24 total, ~5:1 rather than the ~3:1 discussed -
weighted further toward melee because the pre-authored splines skewed that way and reusing existing
infrastructure took priority over hitting an exact ratio.

**`BP_ErikaArcher` had no `ACFAIPatrolComponent` at all** - confirmed by listing her components after
the first 4 placements silently got no patrol (`patrol` list came back empty, no error). She was only
ever authored as a stationary test target for `#354`. Added `ACFAIPatrolComponent` to the Blueprint
itself via `BlueprintService.add_component` + recompile - matches how the guard Blueprints each carry
their own instance of the component (confirmed: `BP_CastleGuard01` and `BP_ErikaArcher` share the same
parent, `AGSEnemyCharacter`, which does NOT have the component - each defender BP adds its own). The 4
already-placed archer instances picked up the new component automatically on recompile; set
`patrol_type=E_FOLLOW_SPLINE`, `path_to_follow`, `wait_time_at_point=2.0` on each afterward.
`ai_controller_class` confirmed still `BP_GSAIController_Archer_C` on all 4 (inherited from the BP
CDO, unaffected by adding an unrelated component).

Saved. No C++ touched, no rebuild needed - pure level/Blueprint content.

## Evaluate

**NOT verified in PIE - this is placement, not observed behavior.** What IS verified directly:
final actor counts (`BP_CastleGuard01_C`: 10, `BP_CastleGuard02_C`: 9, `BP_ErikaArcher_C`: 4,
`BP_KnightDPelegrini_C`: 1 = 24), each new actor's `path_to_follow` resolves to its intended spline
(no `None`s), all 4 archers carry the patrol component post-recompile and the correct AI controller
class. None of that proves they actually patrol, engage, or that an archer standing at her assigned
spline point can actually see/shoot the player from there (unlike Erika's `#354` test spot, these
were chosen from pre-existing spline geometry, not for archer sightlines specifically - flagged as a
real risk, not asserted as fine).

**Not addressed:** `AGSSpawnerActor` (Barracks/Watch-Station/Bunker, the alarm-tier reinforcement
system) - zero instances are placed on this map at all. Static patrol density and reinforcement
escalation are two different levers; this ticket only touched the first. Worth a separate
conversation with Michael on whether spawner buildings should exist on this map too.

## Refine

Handing back at `review` - needs Michael to actually watch a patrol route run, and specifically to
watch at least one of the 4 new archers fight (the thing that has never been observed outside an
isolated arena test). If an archer's spline point turns out to have no sightline to anywhere useful,
the fix is picking a better point on the same spline or nudging the archer off it - not re-doing the
whole placement.
