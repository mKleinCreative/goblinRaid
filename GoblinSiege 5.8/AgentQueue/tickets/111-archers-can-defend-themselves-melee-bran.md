---
id: 111
title: Archers can defend themselves: melee branch and the kick
agent: claude-gobkit
status: done
claimed: 2026-08-10T00:53Z
build: none
waiting_on:
evaluated: 2026-08-10T00:59:56Z
files: 
  - Content/AI/BT_Archer.uasset
  - Content/Blueprints/Adversaries/BP_ErikaArcher.uasset
---

## Goal

Archers can defend themselves: melee branch and the kick

## Generate

Michael: "archers need a generic melee attack and the ability to kick."

**Both already existed and neither could run.** `BP_ErikaArcher` already carries
`LightAttackAbilityClass = GA_GS_SwordLight`, `HeavyAttackAbilityClass`, and
**`GuardBreakAbilityClass = GA_GS_GuardBreak` - which IS the kick** (its montage is `AM_GS_Atk_Kick`).
`BT_Archer` already had a `Melee (cornered)` node. Nothing needed adding.

**The melee node was starved by node order** - the same trap as #089 and #090:

```
before:  Loose an arrow | Hold bow range | Melee (cornered) | Watch
after :  Loose an arrow | Melee (cornered) | Hold bow range | Watch
```

`Hold bow range` is a MoveTo with `StandoffRadiusOverride` 700, so a cornered archer runs it forever
trying to back away, and a Selector never reaches the child below a running one. She kited instead of
defending herself, permanently.

Ranged stays FIRST on purpose: `BTTask_RangedAttack` has `MinRange` 350 and fails on its own when a
target is closer than that, which is exactly the gap melee should fill. So the hand-off is
automatic - inside 250 she swings, 250-350 she backs off, beyond 350 she shoots.

**Separately, on Michael's report that "the sword is sticking out of the back of the hand":** the
`hand_r_weapon` socket on `SK_Human_Skeleton` was at pitch 0 / yaw 180 / roll -180. Set to
**pitch 0 / yaw -19.7 / roll -103.9**, derived rather than eyeballed: take the forearm direction
(`RightForeArm` -> `RightHand`) in world space, express it in the RightHand bone's own space (which is
the socket's space), and build the rotation that puts the mesh's +Z blade axis along it.

## Evaluate

**The sword is measured, before and after:**

```
blade-vs-forearm angle   76.1 deg  ->  8.3 deg     (0 = continues the arm, 180 = out the back)
grip to RightHand bone                 22.4uu      (the socket's own 20uu offset into the palm)
```

76 degrees is essentially perpendicular, which is what "out the back of the hand" measures as.

**The socket had been changed since I last read it** - I recorded identity earlier, it was
yaw 180/roll -180 today, so Michael has been tuning it himself. **My value overwrites his.** It is one
field in the Skeleton asset and the derivation above is the reasoning, so if 8.3 degrees reads too
straight, the number to nudge is that roll.

**NOT VERIFIED: whether the archer actually swings or kicks in play.** The node order is fixed and the
abilities are present, but I have not watched a cornered archer fight. The kick specifically fires
through `BTTask_MeleeAttack`'s guard-break branch, which needs the target to have blocked twice - so
it is conditional by design and may look absent in a short test.

**And it will not ANIMATE.** Every montage in the project is on the goblin skeleton (#101), so a human
melee or kick resolves damage with no animation at all. Michael asked about this in the same breath;
it is a content gap, not this ticket.

## Refine

**Reordered rather than adding a distance decorator.** A "target within 250" decorator would have
duplicated the range check `BTTask_MeleeAttack` already does internally, and two places deciding one
thing is how #108 happened.

**Left the melee node without a `HasAttackToken` decorator**, unlike the militia and goblin trees.
Consistency argues for adding it; being cornered argues against - an archer who cannot defend herself
because a token is taken is exactly the complaint being fixed. Flagged rather than silently chosen.

**Deliberately left undone:** human combat animations, which is the real answer to why none of this
will look like anything yet.
