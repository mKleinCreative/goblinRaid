---
id: 094
title: Archers actually shoot: ranged ability on the base, BTTask_RangedAttack, per-tree standoff range
agent: claude-archer
status: done
claimed: 2026-08-09T03:52Z
build: required
waiting_on:
evaluated: 2026-08-09T04:05:09Z
files: 
  - Source/GoblinSiege/Characters/GSCharacterBase.h
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_RangedAttack.h
  - Source/GoblinSiege/AI/Tasks/BTTask_RangedAttack.cpp
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.h
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.cpp
---

## Goal

Archers actually shoot: ranged ability on the base, BTTask_RangedAttack, per-tree standoff range

## Generate

Archers shoot. Previously `BP_ErikaArcher` ran `BT_Militia` with no bow granted, so a "squad of
militia and archers" was a squad of identical melee men with one of them on less health.

**`AGSCharacterBase`** gained `RangedAttackAbilityClass` + `TryRangedAttack()`, granted in
`GrantCombatAbilities()`. It is the SAME `UGSGA_BowShot` the player's bow runs - that ability was
written AI-ready on purpose (it fires on a release TIMER rather than an input release, and its muzzle
falls back to the pawn's control rotation "for an AI pawn that was never given one"), so an archer's
shot shares a rate limit, a projectile and a damage path with the player's.

**`AI/Tasks/BTTask_RangedAttack` (new UCLASS).** The whole trick is AIMING: `FireArrow` reads the
pawn's CONTROL rotation, and an AI controller's control rotation points at nothing by default - an
archer that just activated the ability would fire wherever its body happened to face. So the task
calls `AAIController::SetFocus(Target)`, keeps re-focusing through the draw so a moving target is
tracked, waits `DrawSeconds` (0.8) and only then looses. That wait is also the telegraph: an archer
who fires the instant he acquires you is a hitscan trap. `ClearFocus` on EVERY exit including abort,
or the archer keeps staring at someone the tree has moved on from.

**`BTService_AcquireTarget` gained `StandoffRadiusOverride`.** #090 moved the ring onto the VICTIM,
which is right for spacing a melee crowd but means every attacker stands at the same distance - and
an archer sent to a melee ring walks into the fight it should be shooting into. The slot now decides
the DIRECTION (the archer keeps its exclusive place and its seat against capacity) and the tree
decides the DISTANCE. **This is also what kites, for free**: the slot is recomputed around the target
every tick, so as an enemy closes, the archer's destination slides away and the chase branch walks
him back out. No retreat behaviour needed.

**Editor:** `BT_Archer` (service with override 700; `[RangedAttack, MoveTo, Melee (cornered), Wait]`),
`DA_Race_Human` Archer row repointed to it, `BP_ErikaArcher` given `UGSGA_BowShot`.

## Evaluate

**VERIFIED IN PIE - archers acquire, draw, aim and land arrows.**

```
[GS.AI] BP_ErikaArcher_C_1: drawing on BP_GSPlayerCharacter_C_4 (770uu)
[GS.AI] BP_ErikaArcher_C_0: drawing on BP_GSPlayerCharacter_C_4 (701uu)
[GS.AI] BP_ErikaArcher_C_1: loosed at BP_GSPlayerCharacter_C_4 -> away
[GS.Damage] BP_ErikaArcher_C_0 -> BP_HordeGoblin_C_1  Damage.Bow  raw 20.0  = 11.0  (HP 29/40)
```

They hold 625-770uu, well inside the 350-1100 band, and the `REFUSED (rate limit or blocked)` line
shows the shared bow cooldown doing its job when three archers try to fire at once.

**A SIGNIFICANT CONTENT PROBLEM FOUND WHILE CHASING THE AIM - worth more than this ticket.**
Measured live, head bone height above the capsule's feet:

| | head above feet | capsule height |
|---|---|---|
| Erika (human archer) | +256 | 300 |
| Player goblin | **+40** | 240 |
| Horde goblin | **+70** | 240 |

**The goblin meshes are a fraction of their capsules.** A goblin's visible head sits 40-70uu off the
ground inside a 240uu-tall collision volume, while the human's sits sensibly near the top of hers.
Arrows aimed at the capsule centre therefore pass well ABOVE a goblin's actual head and still
register a hit, because the capsule is what blocks. This affects far more than archery - collision,
cover, doorways, camera and every trace in the game reason about that capsule. I have NOT changed
it; it is a content decision and it wants Michael's eyes.

**Not verified:** whether 0.8s draw / 2.5s cooldown / 350-1100 band feel right (guesses, all
EditAnywhere on the node); the "cornered" melee fallback never fired in testing; and no test of
archers on `L_Tutorial_Island` or against a player who breaks line of sight.

## Refine

**Killed the aim loft.** I had added 3 degrees of upward bias to compensate for arrow drop; measured,
the drop over a 700uu shot at 6000uu/s with 0.2 gravity is about **1uu**. The loft was compensating
for nothing and lifting the shot ~36uu - chest to head - which was inflating the headshot rate. Set
to 0 on the node.

**Reused the ring rather than writing a retreat behaviour.** A `BTTask_Retreat` was the obvious
answer and would have been a third new UCLASS; overriding the stand-off distance gets the same kiting
out of machinery that already exists and already handles capacity and exclusivity.

**Deliberately left undone:** the goblin capsule/mesh mismatch above, archer-specific animation (they
play the melee montages), and any "break line of sight to escape an archer" logic.
