---
id: 147
title: "The order wheel can never find a target: the aim sweep is blocked by terrain, so Attack and Loot are always refused"
agent: claude-ordertrace
status: done
claimed: 2026-08-13T06:00Z
build: required
waiting_on:
evaluated: 2026-08-13T06:46:47Z
observed: 2026-08-13T06:46:47Z | An Attack order aimed at a guard resolved onto him by name instead of reporting bare ground, the warband broke off and converged on that specific guard, killed him, and the order retired itself the moment he died - and on a second run the beacon was caught alive above the target carrying a dynamic instance of the rune material rather than the engine default
scenario: PIE in L_CombatArena after the editor-closed build, player stood off 600uu with the camera aimed at a militiaman, driving GS.Horde.Order Attack
files: 
  - Source/GoblinSiege/Horde/GSHordeCommandComponent.cpp
  - Source/GoblinSiege/Horde/GSHordeOrderMarker.h
  - Source/GoblinSiege/Horde/GSHordeOrderMarker.cpp
---

## Goal

Michael, first time using the wheel: *"The R commands didn't work though... The wheels come up into
the top left corner of the PIE screen, but doesn't do anything meaningful."*

The log said what was actually happening, and it was not what he described:

```
Order: Hold at V(X=-2993.41, Y=-2649.84, Z=36.00).
Order cleared - the warband is back on Follow.
Ignored a Attack order: it needs something to point at and the trace found bare ground.
Ignored a Loot order:   it needs something to point at and the trace found bare ground.
```

**The wheel was working.** Input, latch, drag, sector maths and the Server RPC all committed
correctly - Hold and Follow landed. What failed was the aim: every Attack and Loot came back "bare
ground" no matter what he pointed at, so the two verbs that need a subject were refused every time by
this ticket's own guard.

## Generate

### The trace, in `UGSHordeCommandComponent::TraceForOrder`

**The bug.** The subject search swept a 60uu sphere on `ECC_Visibility` and walked the hits looking
for something orderable. That reads as though it should work and cannot:

- `SweepMulti` stops at the **first blocking hit**;
- the landscape **blocks** `ECC_Visibility`;
- a third-person camera sits behind and above the pawn and points slightly **downward**.

So on open ground the sphere clipped terrain within a few metres and the sweep never reached the man
being aimed at. `GS.Horde.Order` shares the same function, which is why the console path lied in
exactly the same way and made this look like an input problem rather than an aiming one.

**The fix: query by OBJECT TYPE, not by visibility channel.**

```cpp
FCollisionObjectQueryParams SubjectTypes;
SubjectTypes.AddObjectTypesToQuery(ECC_Pawn);          // defenders, goblins
SubjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);  // loot sacks, crates
SubjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);
World->SweepMultiByObjectType(SubjectHits, ViewLocation, TraceEnd, FQuat::Identity,
    SubjectTypes, FCollisionShape::MakeSphere(Radius), Params);
```

The landscape is `WorldStatic` and is simply **not in the query**, so it cannot block anything. The
function now runs in three ordered steps: sweep for a subject by object type; if none, line-trace
`ECC_Visibility` for a precise world point (which is what Hold wants, and a line is the right tool
for it); if that misses too, drop straight down from the trace end so aiming at the sky still yields
ground.

The hit walk is kept and matters more than before: the sweep will happily return the player's own
goblins, `ResolveOrderSubject` rejects those on race, and without the walk, aiming *past* your own
warband at a guard would resolve to a goblin and be refused.

### The beacon material, in `AGSHordeOrderMarker`

Michael saw grey beacons. `BeaconMesh` is a **native** component, so the material #141 set on
`BP_HordeOrderMarker`'s CDO lived in a component-override record that the Python write never created -
it read back correctly and then silently reverted to `/Engine/EngineMaterials/DefaultMaterial` on the
next Blueprint compile. That material has no `MarkerColour` parameter, so every verb drew the same
grey - precisely the silent failure `ApplyVisuals` documents.

New `TSoftObjectPtr<UMaterialInterface> BeaconMaterial`, defaulted in the C++ constructor to
`/Game/Blueprints/M_HordeOrderMarker` and applied in `ApplyVisuals` **before** the dynamic instance is
created. A recompile cannot undo a C++ default. Soft so a missing asset degrades to the default
material rather than failing the class load; `EditDefaultsOnly` so a designer can still repoint it.

## Evaluate

**Verified at runtime, end to end.** Editor-closed build succeeded in 00:48, zero errors. Then in PIE
on `L_CombatArena`, with the player aimed at a guard from 600uu:

```
06:04:00  Order: Attack at V(X=546.85, Y=159.92, Z=36.00) on BP_CastleGuard01_C_2.
06:04:08  Horde goblin died. ... active 7/10        <- the warband engaged
06:04:12  Order expired - its subject is gone. Back to Follow.   <- they killed him
```

A **named target** instead of "bare ground", the warband converging on that specific guard, and the
order correctly retiring itself the instant its subject died. On a second run the beacon was caught
mid-life: one `AGSHordeOrderMarker` in the world, 150uu above the target, its material reading
`MID_M_HordeOrderMarker_0` - a dynamic instance of the real material, not the engine default.

Note the order resolved to `CastleGuard01_C_2` while the camera was pointed at `C_1`: correct, not a
miss. The sweep takes the **nearest orderable thing along the ray**, and C_2 was closer to the line.

**NOT verified: the wheel itself, by a human.** Every observation here went through `GS.Horde.Order`,
which deliberately bypasses the wheel, the widget and the RPC. The shared trace is what was broken and
the shared trace is what was fixed, so `R` should now behave - but "should" is doing work in that
sentence and Michael has not pressed it since the build.

**Also unverified: the beacon's colour.** The MID is built from the right material and
`ColourForOrder` is a switch, but the applied vector parameter was never read back - the fights
resolve in about twelve seconds and the marker retires with them.

**Owed to AGENT_STATE:** sweeping `ECC_Visibility` for a subject from a third-person camera is a trap
- terrain blocks it and the sweep stops there; query by object type instead. And: a native component's
material cannot be set from the Blueprint side by script, because the override record is never
created.

## Refine

**Changed on self-review:** the first instinct on Michael's report was that the *input* was broken,
because that is what "the R commands didn't work" describes. The log said otherwise within one query -
Hold and Follow were committing fine. Reading it before touching anything is what turned a vague input
investigation into a specific two-line fix in the trace.

**Deliberately left undone:**

- **A human pressing R.** The next session should do this before anything else in the order-wheel
  area; it is the last unproven link.
- **Reading the beacon's colour back.** Needs a longer-lived order than a twelve-second fight allows -
  a Hold order would do it, since Hold has no subject to die.
- **Hold and Loot still do nothing at the AI end.** They need the five blackboard keys and the three
  BT sub-trees, reverted in #143 and still owed. Attack and Follow work because they need neither.
