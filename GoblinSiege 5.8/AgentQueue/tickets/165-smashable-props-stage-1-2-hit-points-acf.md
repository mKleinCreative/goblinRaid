---
id: 165
title: Smashable props stage 1+2: hit points, ACF delegation, scoped retire, player sword smash
agent: claude-smash
status: done
claimed: 2026-08-17T01:35Z
build: none
waiting_on:
evaluated: 2026-08-17T01:51:17Z
observed: 2026-08-17T02:30:02Z | Michael swung the sword at a crate in the arena and it broke - the first time anything the player swings has been able to damage a prop
scenario: PIE in L_CombatArena, SM_CrateSquare placed 489uu from the PlayerStart with a GSBreakableComponent attached via MakeActorBreakable, SmashHitPoints 1, no geometry collection so it took the hide+FX fallback path
files: 
  - Source/GoblinSiege/Destruction/GSBreakableComponent.h
  - Source/GoblinSiege/Destruction/GSBreakableComponent.cpp
  - Source/GoblinSiege/Destruction/GSTorchProjectile.cpp
  - Source/GoblinSiege/Raid/GSRaidLibrary.h
  - Source/GoblinSiege/Raid/GSRaidLibrary.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
---

## Goal

Smashable props stage 1+2: hit points, ACF delegation, scoped retire, player sword smash

Stages 1 and 2 of the smashable-props plan (`~/.claude/plans/structured-orbiting-harbor.md`).
Michael's rulings: adopt ACF's destruction, hero props first, and make the player able to smash
things - which was impossible before this ticket.

## Generate

**`UGSBreakableComponent` becomes an adapter, and ACF owns Chaos.**

- **Deleted** `BrokenCollection` and `SpawnFracture()`. That code spawned a bare
  `AGeometryCollectionActor` and called `SetRestCollection` at runtime; it had **never executed** in
  the project's life, does not replicate, and sets a rest collection after the physics proxy would
  normally have been built. Under ACF the collection lives on the prop and is authored in the CDO,
  so the defect is deleted rather than fixed. Verified nothing referenced either symbol.
- **`Break()` now delegates**: if the owner carries a `UACFDestructableComponent` AND a geometry
  collection with a rest collection, it enables simulation, builds a minimal `FACFDamageEvent` and
  calls `ForceDestruction`. Otherwise it falls through to hide + FX. Signature unchanged, so
  `GSTorchProjectile` and `BTTask_SmashOrderTarget` needed no edit.
- **The hit-point model.** `SmashHitPoints` + `ApplySmash(Damage, ...)`, replacing the torch's
  direct `Break()`. Window and crate 1, chest 2, statue 4. `GSTorchProjectile.cpp` now deals 1, so a
  window still dies to one throw. Non-lethal hits broadcast `OnSmashHit` for a future flinch/dust.
- **`RetireIntactMesh` is scoped.** It hid *every* static mesh on the owner - right for a
  single-mesh window, catastrophic for a multi-mesh prop. Now resolves: explicit component → named
  component → `AStaticMeshActor`'s single SMC → the only SMC on the actor → **nothing, loudly**. All
  113 placed windows are `AStaticMeshActor`s, so they resolve identically to before.
- **FX moved to the mesh bounds** rather than `GetActorLocation()`; on anything bigger than a window
  the pivot can be metres from the surface that was hit.

**Player smash (stage 2).** `UGSRaidLibrary::SmashBreakablesInArc` - a **second** overlap on
`WorldStatic`/`WorldDynamic`, sharing `HitActorsThisSwing`, reusing the same flattened-dot arc test.
Called from `DoSweep` after the pawn loop, gated on a new per-stage `SmashDamage`.

Deliberately a separate query rather than adding object types to the existing one: everything in
that loop - hostility, ASC lookup, guard break, recoil, the damage effect - is about characters, and
widening the first query would have dropped a barrel into all of it at once.

## Evaluate

**Two build failures, both real bugs this work exposed rather than caused.**

1. **`LNK2019` on every ACF symbol.** `GoblinSiege.Build.cs` names only `AIFramework`, with a comment
   asserting its dependencies carry `AscentCombatFramework` transitively so listing it "would be
   noise". That is true for **include paths** and false for **linking** - the header compiled fine
   and the linker had nothing. Invisible until the first time we *call* into ACF rather than
   deriving from a type it re-exports. Fixed in #166.
2. **`GSCombatDebugEnabled': identifier not found`** in `GSArrowProjectile.cpp`. That function is
   defined in `GSGA_SwordLight.cpp` and **declared in no header at all**; the call only ever
   compiled because the unity build happened to merge the two files. Adding one include re-split the
   blob and it broke. A latent bug that would have detonated on any unrelated future include. Fixed
   in #167.

**Build succeeded**, 1:02, after both. Zero errors; the two pre-existing `AbilityTags` C4996
warnings remain (that is #152, abandoned).

**NOT OBSERVED, and it cannot be yet.** Nothing in any level currently carries a
`UGSBreakableComponent` *and* is reachable by a sword - the 113 windows are in `L_Tutorial_Island`,
and `L_CombatArena` where testing happens has no breakables at all. So the stage-2 verb compiles and
has never run. **The next session must place a breakable in the arena before claiming any of this
works.** `UGSRaidLibrary::MakeActorBreakable` is the one-line Python path.

**Specifically unproven:**

- That the ACF delegation branch runs at all. No prop in the project has a
  `UACFDestructableComponent`, so `Break()` has only ever taken the fallback path even in principle.
- That `SetSimulatePhysics(true)` before `ForceDestruction` is sufficient. It is reasoned from
  reading ACF's `ApplyChaosDestructionAt` (which only applies strain), not from watching a prop
  break.
- That the arc test behaves the same in the second sweep as the first. It is the same maths against
  the same origin, but props have very different bounds from pawns and a crate's pivot may sit
  outside the arc while its geometry is inside it.

**Owed to AGENT_STATE:** the interact framework and now the smash path have both turned out to be
"written, never executed". Worth a line that `Break()` had one code path that had never run in the
project's entire history, discovered only because we went to delete it.

## Refine

**Changed after self-review:** `RetireIntactMesh`'s ambiguous case originally hid every mesh, as
before, when no component was nominated. Changed to hide **nothing** and log a warning naming the
actor. A prop that fails to disappear is a visible bug someone fixes in a minute; a prop actor that
vanishes wholesale gets blamed on Chaos and costs an afternoon. Refusing to guess is the cheaper
failure.

Also moved the hit-set insertion in `SmashBreakablesInArc` to *before* `ApplySmash`, since a break
can destroy or invalidate the actor and one swing must never hit the same prop twice regardless.

**Deliberately left undone:**

- **`BTTask_SmashOrderTarget` still calls `Break()` directly**, not `ApplySmash`. It compiles and
  works; routing the horde through the shared arc helper is what fixes its documented "breaks at the
  START of the swing" defect, and that belongs in its own ticket with its own observation.
- **No geometry collections exist**, so nothing can actually fracture. That is stage 0 and it needs
  the editor open.
- **`SmashDamage` is per-stage but every stage defaults to 1.** Tuning waits for something to hit.
