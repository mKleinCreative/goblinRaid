---
id: 142
title: ACF Phase 0: make AIFramework reachable - Build.cs dependency and uproject plugin pin
agent: claude-acf
status: done
claimed: 2026-08-13T03:18Z
build: required
waiting_on:
evaluated: 2026-08-13T03:30:44Z
observed: 2026-08-13T03:30:44Z | Blowing the horn in the arena still brought four goblins in and debited the pool 20 to 16, active 4 of 10, exactly as before ACF was linked - read from the LogGSHorde line, not watched on screen
scenario: PIE in L_CombatArena after the ACF-linked build, GS.Horde.SpawnTest 1 driven from the PIE world console
files: 
  - Source/GoblinSiege/GoblinSiege.Build.cs
  - MyProject.uproject
---

## Goal

ACF Phase 0 of the migration in `ACF_HORDE_MIGRATION.md`: make `AIFramework` reachable from the
GoblinSiege module, with **no behaviour change**. Prove the dependency links and costs what we think
before anything structural moves.

Michael, 2026-08-12: *"You've been trying to not use the ACF the entire time we've had it and it's
frustrating me. There's a lot of things in there that would be great for us to use so we don't have
to fall into the trap of trying to custom build literally fucking everything."* Then, on scope:
*"Defenders are eventually going to need to do patrols, I believe it's worth having them follow."*

So the project adopts ACF for **both** the horde and the human defenders. #141's hand-rolled order
board is parked as a re-implementation of `UACFCommandsManagerComponent` +
`UACFGroupAIComponent::SendCommandToCompanions`, and `UGSPatrolDirector` (still on AGENT_STATE's NEXT
list, `[BLOCKED]`, ranked 2.33) is superseded by `UACFAIPatrolComponent`.

## Generate

Two files, both one-liners in substance.

**`Source/GoblinSiege/GoblinSiege.Build.cs`** - added `"AIFramework"` to
`PublicDependencyModuleNames`, in the AI block.

- **Only that one module is named.** Its own `PublicDependencyModuleNames` carry the rest
  transitively: `AscentCombatFramework` (which is where `AACFCharacter` lives),
  `AscentCoreInterfaces`, `AscentTargetingSystem`, `AscentGASRuntime`, `ActionsSystem`,
  `InventorySystem`, `AscentTeams`, `SmartObjectsModule`. Listing them here would be noise that hides
  which one we actually reach for.
- **Public, not private** - the opposite of the `Landscape` / `AnimGraphRuntime` reasoning further
  down that same file, and deliberately so. Phases 1 and 2 put ACF types in our *headers*
  (`AGSAIControllerBase : AACFAIController`, then `AGSCharacterBase : AACFCharacter`), so dependents
  need the include paths.

**`MyProject.uproject`** - added `AscentCombatFramework` to the `Plugins` array.

- It was loading only because project-local plugins default to enabled. That is fine for content we
  happen to have and wrong for a module we now link against: nothing in the project file recorded the
  dependency, so a plugin disable would have surfaced as a link error with no breadcrumb.

## Evaluate

**Verified, with evidence:**

- **It builds.** Editor-closed build 2026-08-12, `Result: Succeeded` in 05:29, zero errors. Only the
  two pre-existing `AbilityTags` C4996 warnings (`GSGA_Block.cpp:24`, `GSGA_Interact.cpp:20`) that
  CLAUDE.md already records.
- **The dependency is nearly free.** Six build actions total (three compiles, two links, metadata).
  ACF's ~45 modules were already compiled, so the transitive pull-in flagged as Risk 4 in the
  migration plan cost **zero** rebuild time. That was the main unknown this phase existed to price,
  and it came back cheap.
- **ACF types are reachable from our process**, queried live after relaunch: `ACFAIController`,
  `ACFCommandsManagerComponent`, `ACFGroupAIComponent`, `ACFCompanionGroupAIComponent`,
  `ACFAIPatrolComponent`, `ACFBaseCommand`, `ACFCharacter` all resolve. Those are precisely the Phase
  1 / Phase 2 / Phase 2b targets.
- **Our own types still register**: `GSHordeAIController`, `GSHordeGoblin`,
  `GSHordeCommandComponent`, `GSHordeOrderMarker`.
- **No behaviour change, tested where it can be tested.** PIE in `L_CombatArena`:
  `LogGSHorde: Pool reset: 20 in reserve, cap 10 active` / `Threat scan armed: every 0.50s, radius
  1200` on begin-play, then `GS.Horde.SpawnTest 1` produced
  **`LogGSHorde: Horn: 4 answered. Reserve 16, active 4/10.`** with four `BP_HordeGoblin_C_*` pawns
  constructed. Summoning, the pool debit and the arrival markers all behave exactly as before.

**Honest about the evidence grade:** the summon above is a **runtime log line in the right scenario**,
which is rung 2 of the queue's ladder, not rung 1. **No human has watched the goblins arrive on
screen since ACF was linked.** They very likely do - nothing in this change touches spawning - but
"the log says four spawned" and "four goblins ran in from the treeline" are different claims and only
the first is made here.

**A false start worth recording:** the first PIE attempt launched on an empty untitled map and
reported `1 blast(s) summoned 0` with `No Marker.HordeArrival markers in this level`. That is the
documented behaviour of a map without markers, not a regression - but it is exactly the shape of a
result that would have been reported as "the horn is broken after the ACF change" if the map had not
been checked. Verification re-run on `L_CombatArena`, the only map with the markers.

**Not touched, and deliberately:** no class reparented, no component added, no ACF header included by
any of our `.cpp` or `.h`. `AIFramework` is linked and unused. That is the whole point of Phase 0 -
if it had broken something, the cause could only be load order or module init.

**Owed to AGENT_STATE.md** on close: the ACF adoption DECISION and its scope (horde *and*
defenders); that `UGSPatrolDirector` is superseded by `UACFAIPatrolComponent` and should come off the
NEXT list rather than be built; and that `AscentCombatFramework` is now a pinned, linked dependency
rather than incidental content.

**Two pre-existing defects re-confirmed in this run, neither caused by this ticket and both still
open:** all four spawned goblins logged `has no EquippedWeapon data asset - no weapon meshes, no
granted abilities`, so the warband is unarmed despite #098; and `AGSHordeAIController`'s
`DoNotCreateDefaultSubobject(AIPerceptionComponent)` is still being overridden by the engine
(`Ignored ... as it's marked as required`), so GDD §3.4's perception-less crowd is not in effect.

## Refine

**Changed on self-review:** nothing in the two edits - they are as small as the goal allows and the
build proved them. What changed was the *verification*, twice: the first PIE run was on the wrong map
and had to be discarded, and the level was then explicitly loaded to `L_CombatArena` before retesting.
Reporting the first run's `summoned 0` as a result would have been a fabricated regression.

**Deliberately left undone:**

- **Phase 1 and beyond.** `AGSAIControllerBase : AACFAIController` is the next step and it must move
  the *base*, not `AGSHordeAIController` alone - defenders possess `AGSAIControllerBase` directly, so
  both populations move together or not at all.
- **The Phase 2 fork is unresolved and is Michael's call**: `AGSCharacterBase : AACFCharacter` (one
  edit, the player comes too, matches how ACF is designed) versus reparenting `AGSEnemyCharacter` and
  `AGSHordeGoblin` only (smaller blast radius, but the combat verbs live on `AGSCharacterBase` and the
  player ends up on a different damage path from everything he fights). Recommended: the former.
- **`TickFacing` / `TickSeparation` have not been hoisted.** They live on `AGSAIControllerBase` and
  are signed-off, watched work (Michael, 2026-08-11). Phase 1 changes that class's base and they must
  survive it - a component both controllers carry, not a duplicate, because two facing authorities is
  the failure #108 is named after. This is the single most likely thing to regress silently.
- **The editor's open level is now `L_CombatArena`**, changed by this ticket's verification. Harmless,
  but it is not where it was found.
