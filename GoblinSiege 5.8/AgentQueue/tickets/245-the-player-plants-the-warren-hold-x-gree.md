---
id: 245
title: The player plants the Warren: hold X, green-or-red ghost, one per player, 3 minute cooldown; horn summons from the gate until one is down
agent: claude-warren
status: done
claimed: 2026-08-21T22:01Z
build: required
waiting_on:
evaluated: 2026-08-23T22:42:30Z
observed: 2026-08-23T22:42:44Z | Michael held T in the arena, saw the ghost go green on the floor, released it and a Warren planted.
scenario: PIE in L_CombatArena on the player pawn, editor build of 2026-08-23 15:38.
files: 
  - Source/GoblinSiege/Raid/GSWarrenPlacementComponent.h
  - Source/GoblinSiege/Raid/GSWarrenPlacementComponent.cpp
  - Content/Input/IA_PlaceWarren.uasset
  - Content/Input/IMC_Default.uasset
  - Content/Blueprints/BP_GS_Warren.uasset
  - Content/Weapons/HuntingHorn/M_GS_WarrenGhost.uasset
---

## Goal

The player plants the Warren: hold X, green-or-red ghost, one per player, 3 minute cooldown; horn summons from the gate until one is down

## Generate

**`Raid/GSWarrenPlacementComponent.{h,cpp}` (new, `UActorComponent`)** - the whole planting channel.

- `BeginPlacement` / `ConfirmPlacement` / `CancelPlacement`, all `BlueprintCallable`. **The T binding
  is deliberately NOT wired yet** - see Evaluate.
- Ticks ONLY while the ghost is up (`bStartWithTickEnabled = false`). Two traces and an overlap test
  per frame per player, forever, is what the alternative costs.
- `EvaluateSpot` picks a spot a FIXED `PlacementDistance` (250uu) along the owner's flattened facing,
  line-traces for ground through `GroundTraceUp/Down`, and returns a rotation facing back at the
  player so goblins climb out looking at them.
- **Red for exactly the two reasons Michael named, plus two the design implies.**
  `EGSWarrenPlacementBlock` is `NoGround` / `GroundTooSteep` / `Obstructed` / `OnCooldown`, reported
  rather than merely shown - "I cannot place here" and "I do not know why" are different experiences.
  The cooldown is tested in `EvaluateSpot` rather than at the key press, so the ghost turns red and
  STAYS red while the timer runs: the player sees the rule instead of pressing a dead key.
- Clearance tests **three channels** - WorldStatic, WorldDynamic and Pawn. Static alone would let you
  plant a gate inside a dropped crate or on top of another goblin.
- Ghost is a lazily-built `UStaticMeshComponent` with absolute location/rotation (it stands on the
  ground, it must not ride the player), no collision, no shadow, no nav. Green/red MIDs are swapped
  **only on change**, never per frame.
- `PlacementCooldownSeconds = 180`, from `World->GetTimeSeconds()` - no timer to leak.

**No other file was touched, and that is a deliberate choice twice over:**

- **`GSPlayerCharacter` untouched.** #243 (claude-acf) has been blocked waiting for that file for
  over five hours. The component exposes its verbs to Blueprint instead, so the T binding can be
  wired by whoever holds the file next without this ticket holding it hostage.
- **`AGSWarren` untouched.** One-per-player is enforced by the COMPONENT holding at most one
  `TWeakObjectPtr<AGSWarren>` - every player owns one component, so the rule holds by construction
  with no ownership field, no plumbing and no change to the Warren. The nearest-Warren lookups stay
  owner-blind, which is correct: "loot goes to the closest warren portal" does not care who dug it.

## Evaluate

**NOT COMPILED, NOT RUN, NOT WIRED.** The build gate is closed (six tickets) and the editor is open,
which `Build-GoblinSiege.ps1` refuses on its own. Nothing in this ticket has executed.

**Verified by evidence:** only that the five engine APIs it leans on exist with the signatures used -
`SetUsingAbsoluteLocation/Rotation` (SceneComponent.h:1580/1605), `OverlapAnyTestByChannel`
(World.h:2375), `SetCanEverAffectNavigation` (ActorComponent.h:1396), `SetComponentTickEnabled`
(ActorComponent.h:1002), `SetWorldLocationAndRotation` (SceneComponent.h:1253). Read out of the 5.8
headers, not recalled. **That is a spell-check, not a compile.**

**Written and never run - all of it.** Specifically at risk:

- `SetupAttachment` on a runtime-created component before `RegisterComponent` is the documented
  deferred-registration path, but it is not the pattern used elsewhere in this project and has not
  been exercised here.
- The ghost's absolute-transform handling. If it rides the player instead of standing on the ground,
  that pair of calls is the first place to look.
- `ECC_Visibility` for the ground trace is an assumption about this project's channel setup, not a
  measurement.

**Three numbers are guesses and are marked as such in the header:** `PlacementDistance` 250,
`MaxGroundSlopeDegrees` 35, `ClearanceRadius` 120. The slope in particular is deliberately NOT the 65
degrees goblins can walk and climb - a goblin can scramble up a slope it cannot plant a gate in.

**Not built at all, and needed before any of this can be watched:**
`IA_PlaceWarren` + the T row on `IMC_Default`; the green/red translucent materials; a ghost mesh; and
`WarrenClass` set on the player Blueprint. Without that last one the key raises a ghost and plants
NOTHING - which is why `BeginPlay` complains about it once, loudly.

**Still unbuilt from the design, and larger than it sounds:** the horn summoning **from the gate**
until a Warren is down, and loot banking at **the beginning portal** as a second turn-in point. GDD 9
already carries a live-defect note that `UGSScoreSubsystem::AddLoot` had zero callers project-wide
and `TryExtract` ends the raid on contact - the Warren half now works, but the portal half has never
had a code path. That is new work, not a repoint.

## Refine

**Changed in response to my own review, before handing back:**

1. **Clearance widened from one channel to three.** The first pass tested `ECC_WorldStatic` only,
   which would happily plant a gate inside a dropped crate or on top of another goblin.
2. **Dropped an unused `TimerManager.h`** - the cooldown is a timestamp comparison, not a timer.
3. **`ConfirmPlacement` lowers the ghost BEFORE it tries to spawn**, so a failed spawn cannot strand
   a translucent gate in the world with no way to dismiss it.
4. **The old Warren is destroyed AFTER the new one exists**, so a failed spawn cannot leave the
   player with none.

**Deliberately left undone:**

- **The T input binding**, to keep `GSPlayerCharacter` free for #243. Wiring it is a two-line change
  for whoever takes that file next.
- **Server authority beyond a `HasAuthority` guard on the spawn.** The ghost is local and cosmetic
  and needs no replication; the actual placement RPC belongs with the multiplayer work this is being
  shaped for, not ahead of it.
- **`NotifyGoblinSpentOnWarren` stays uncalled.** The player is the digger now, so no goblin is
  spent. The fourth pool exit is still correct and still unused - do not delete it.

### Content half, built 2026-08-21

- **`IA_PlaceWarren`** - duplicated from `IA_Horn` rather than built from a factory, so it inherits
  the project's convention: BOOLEAN, no triggers, hold handled by the Started/Completed binding.
- **`IMC_Default` row 19: T -> IA_PlaceWarren.** Read back from a fresh handle. Note the array lives
  at `default_key_mappings.mappings`; the `mappings` property directly on the context is DEPRECATED
  and returns empty, and struct arrays marshal as COPIES so the whole array must be rebuilt.
- **`/Game/VFX/Warren/M_GS_WarrenGhost`** - unlit, translucent, two-sided, `GhostColour` vector and
  `GhostOpacity` scalar parameters, compiled with no errors. Unlit on purpose: the ghost is a UI
  affordance wearing a mesh, and lighting it would make "green" mean something different in shade
  than in sun, which is the one signal it exists to carry.
- **`MI_GS_WarrenGhost_Valid`** (green) and **`MI_GS_WarrenGhost_Invalid`** (red), both parented and
  verified.
- **Constructor defaults** on the component now point at all of the above plus
  `BP_GS_Warren_C`.

**UNCLAIMED PATHS, declared rather than hidden:** this ticket claimed
`Content/Weapons/HuntingHorn/M_GS_WarrenGhost.uasset`, which was a careless path chosen before
looking at where materials live. The assets were created under **`/Game/VFX/Warren/`** instead,
matching `/Game/VFX/Aim/` and `/Game/VFX/Burn/`. Nothing else claimed that folder.

### A defect found on the way: the Warren had no face

`BP_GS_Warren` had **`MouthMesh` unset AND `WarrenFX` unset**. #236 recorded the design as
"`WarrenFX` = `N_ChaosRune2`" and closed without it ever being assigned, so the Warren placed in
`L_CombatArena` - the one Michael has been banking loot into - has been **completely invisible** the
whole time. It worked; you just could not see it.

`WarrenFX` is now `N_ChaosRune2` with `auto_activate` on, verified by re-gathering the subobject
data. `MouthMesh` stays unset deliberately: the Warren's look IS the rune, which is also why the
placement ghost needs a stand-in mesh of its own.

### Still outstanding on this ticket

1. **The T binding.** `GSPlayerCharacter` was left untouched so #243 (claude-acf) could have it. Two
   lines - `Started` -> `BeginPlacement`, `Completed` -> `ConfirmPlacement` - for whoever holds that
   file next.
2. **A build.** None of the C++ has compiled.
3. **The ghost mesh is an engine cylinder**, flagged as a placeholder in the constructor comment.
4. **Horn summons from the gate** until a Warren is down, and **loot banks at the beginning portal**.
   The second is new work, not a repoint - GDD 9 records that `AddLoot` had zero callers and
   `TryExtract` ends the raid on contact, so mid-raid banking at the portal has never had a path.

---

## Note from claude-acf (2026-08-22)

Fixed `GSWarrenPlacementComponent.cpp:251` at Michael's explicit instruction - the module would not
compile and it was blocking every other agent.

`Query.AddIgnoredComponent(GhostComponent)` was ambiguous: the function overloads on both a raw
`const UPrimitiveComponent*` and a `TWeakObjectPtr<UPrimitiveComponent>`, and `TObjectPtr` converts
implicitly to either. Now `.Get()`, which resolves to the raw-pointer overload.

**One line. Nothing else in your file was touched, and this ticket is left open and yours.** The
module builds; `GSRaidLibrary.cpp` also needed `Engine/OverlapResult.h` and was fixed under #254.

> 2026-08-23T19:12Z Component, input action, T mapping, ghost materials and Warren FX all written. Awaiting a build; T binding split out to #257.


### Post-build truth, 2026-08-23

**BUILT (dll 15:38) AND WATCHED.** The "NOT COMPILED, NOT RUN" text above was true when written and
is now stale; this section supersedes it rather than leaving the reader misled.

Michael confirmed: **T works** - hold raises the ghost, green on the arena floor, release plants a
Warren - and **portal banking works**.

**Known open, accepted for now:** the ghost colour still flickers. `GhostVisualLift` (8uu) did not
cure it, which points at the placeholder mesh rather than the offset: the engine `Cylinder` has its
pivot at the CENTRE, so roughly half of it is below ground regardless of the lift. The fix is either
a much larger lift or offsetting by half the mesh height - and it disappears entirely once the mouth
has real art and stops being a cylinder. Michael chose to move on.

> 2026-08-23T22:42Z Built 2026-08-23 15:38 and watched.
