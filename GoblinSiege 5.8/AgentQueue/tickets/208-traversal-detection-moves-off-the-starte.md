---
id: 208
title: Traversal detection moves off the Started pin onto Triggered behind a Sequence, so a tap of E is the dodge alone
agent: claude-traversepin
status: done
claimed: 2026-08-20T20:27Z
build: none
waiting_on:
evaluated: 2026-08-20T20:36:20Z
observed: 2026-08-20T20:36:21Z | Michael tapped E in all four directions and reported it works - the goblin now plays one correct directional roll per tap with no dive, and the log backs it: 63 dodge picks across Back, Fwd, Left and Right, every one resolving with clean plus or minus 1.000 dot products
scenario: PIE on L_CombatArena with GS.Combat.LogDodge 1, tapping E while holding each direction, after the traversal chain was moved off the Started pin and the Blueprint's four DodgeMontage variables were cleared
files: 
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Traversal detection moves off the Started pin onto Triggered behind a Sequence, so a tap of E is the dodge alone

## Generate

Two edits to `BP_GSPlayerCharacter`, both aimed at one thing: stop the Blueprint's rival dodge from
competing with the C++ one.

**1. The traversal chain moved off the `Started` pin.** `IA_Traverse` fires `Started` on the initial
press *regardless of the Hold trigger* added in #207, so a tap ran the C++ dodge **and** the
Blueprint traversal chain, both playing into `DefaultSlot`.

```
BEFORE                                   AFTER
  Started   -> Print -> [traversal]        Started   -> (nothing)
  Triggered -> Print -> Try Enter Climb    Triggered -> Sequence
                                                         then_0 -> Print -> [traversal]
                                                         then_1 -> Print -> Try Enter Climb
```

A `K2Node_ExecutionSequence` preserves the original ordering - traversal detection ran before climb
when `Started` preceded `Triggered`. `Completed` is untouched. **Moved rather than cut, because vault
and mantle live on that chain and nothing else calls them.**

**2. The Blueprint's four dodge montages cleared.** This was the actual cause of "a forward roll in
the wrong direction":

```
DodgeMontage_Fwd = DodgeMontage_Back = DodgeMontage_Left = DodgeMontage_Right = AM_GS_Dive_RM
```

**All four pointed at the same clip.** The Blueprint's dodge could only ever play one dive, whichever
way you went. Set to None so its `Play Anim Montage` calls become no-ops. `VaultMontage`,
`MantleMontage` and the five climb montages are untouched and confirmed intact.

**Nothing was deleted.** No graph nodes removed, no variables removed - the motion-warping and
traversal logic around those calls is exactly as it was, and this is one property away from being
undone. Backup at `_PristineBackups/BP_GSPlayerCharacter_pre208_20260820.uasset`.

## Evaluate

**Watched and confirmed by Michael: "it works".** Backed by the log - **63 dodge picks** across
Back / Fwd / Left / Right, every one resolving with clean plus-or-minus 1.000 dot products, and no
dive.

**Both edits verified structurally against the reloaded asset**, not against the in-memory object:
`Started` drives nothing, `Triggered` drives the Sequence, `then_0` and `then_1` reach the correct
target node ids (checked by GUID, not by node title - both branches begin with a `Print String` and
titles alone could not tell them apart), and the four montage variables read `<None>` while the seven
traversal/climb ones still read their assets.

**The real lesson is the diagnosis, not the fix.** The C++ dodge was correct the entire time and I
spent three investigation passes hunting a bug in it - selection maths, montage contents, root motion
axes, play rate - because the reported symptom was believed to be about the dodge. It was about a
**second, undiscovered dodge system in Blueprint**, reached by a different key. The instrument added
in #204 is what eventually made that visible: correct picks in the log alongside a wrong animation on
screen is a contradiction that can only mean two systems.

**Not verified:** whether vault and mantle still work. They were preserved deliberately and the
Sequence routing is confirmed, but nobody has vaulted or mantled since the change. That is the one
thing this ticket could plausibly have broken.

## Refine

Cleared variables rather than deleting nodes, after Michael asked to "delete the wrong animations".
Deleting from a 759-node event graph to change which montage plays is irreversible, risks the
motion-warping wired around those calls, and cannot be undone with one property - clearing achieves
the same visible result and can.

Took a backup first, because `Content/` is LFS-tracked but a mid-session checkout is a worse recovery
path than a local copy.
