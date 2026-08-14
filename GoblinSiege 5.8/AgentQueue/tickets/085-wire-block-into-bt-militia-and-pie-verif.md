---
id: 085
title: Wire Block into BT_Militia and PIE-verify NPC-vs-NPC melee
agent: claude-npcfight2
status: done
claimed: 2026-08-08T22:01Z
build: none
waiting_on:
evaluated: 2026-08-08T23:16:22Z
files: 
  - Content/AI/BT_Militia.uasset
  - Content/AI/BB_Human.uasset
---

## Goal

Wire Block into BT_Militia and PIE-verify NPC-vs-NPC melee

## Generate

Wired the new `UBTTask_Block` into `BT_Militia` and PIE-verified the whole NPC-vs-NPC loop.

**`Content/AI/BT_Militia`** (via Python - the asset has no EdGraph, and opening it in the BT editor
risks wiping the tree). Root Selector children are now `[Block, MeleeAttack, MoveTo, Wait]`, with
`BTService_AcquireTarget` still on the root. Block is FIRST because it fails instantly whenever it
declines, so the Selector reaches MeleeAttack on the same tick and trying it first costs nothing.

**A `BTDecorator_Blackboard` on the Block edge** - `TargetIsAttacking`, BasicKeyOperation.SET,
**FlowAbortMode = LowerPriority**. This was not in the plan, and the plan was wrong without it. PIE
showed the Selector only re-reaching Block once per full tree cycle, measured at ~2.3s, and a 0.22s
windup essentially never lands inside that window - so the guard went up only on the idle roll and
never in response to anything. A block is a REACTION, so it has to be able to interrupt; the
decorator aborts MoveTo/Wait the moment the telegraph flips true.

`IdleBlockChance` 0.10 -> **0** as a consequence: the decorator gates the whole node on a telegraph,
so the idle path is now unreachable and a non-zero number there would be a lie. The cost is the
readability nicety of a guard bobbing up and down while closing; the session goal is reading
attacks, and that is what the node now does.

**`Content/AI/BB_Human`** += `TargetIsAttacking` (Bool). See Refine - this took two attempts and the
first one silently did nothing.

## Evaluate

**VERIFIED IN PIE ON L_CombatArena, with evidence.** A 4v4 (`GS.Combat.Duel 4`, pairs teleported
face to face at 200uu so pathing could not confound the sample):

| measure | count |
|---|---|
| telegraph SEEN | 44 |
| block ACCEPTED | 8 |
| block DECLINED | 9 |
| damage lines resolving BLOCKED | 7 |
| damage events | 28 |
| guard breaks fired | **0** |
| combatants dead at the end | **6 of 10** |

8/17 accepted against an intended 0.55 is the coin behaving. The proof pair, one line after the
other:

```
[GS.AI]     BP_KnightDPelegrini_C_2: block ACCEPTED (telegraph, p=0.55) vs BP_CastleGuard01_C_0
[GS.Damage] BP_CastleGuard01_C_0 -> BP_KnightDPelegrini_C_2  Damage.Dagger  raw 25.0  x1.00 race
            BLOCKED - armor 6.0  = 0.0   (HP 75/75)
```

One AI saw another AI wind up, chose to guard, faced it, and 25 damage resolved to exactly 0.0.
Neither party is the player. That is the session goal, and the Knight's armor 6 is what makes the
arithmetic unambiguous rather than merely reduced.

**NOT verified, and I am not claiming it:**
- **The guard break has never fired.** 0 in 28 damage events. `CanGuardBreak()` is true on these
  humans and the branch is reachable in principle, but the pairs sat at almost exactly
  `GuardBreakRange` (200uu) and drift either side of it, and the attacker must run its melee task
  during the target's 0.9s guard. Whether that is bad luck, the marginal range, or a real bug is
  **unknown**. First thing to check next session.
- **Pathing.** Both PIE runs needed the combatants teleported together: spawned 600uu apart on
  `L_CombatArena` they acquired each other correctly and then did not close. `MoveTo` may be failing
  against the arena navmesh at those spawn spots. Everything above tests combat, not approach.
- **`BTTask_Block` has a silent decline.** The `State.Attacking` early-out returns Failed with no log
  line, so a defender declining because it is mid-swing is invisible. That is why one combatant
  showed `telegraph SEEN` with no roll beside it and I briefly suspected a second bug. Worth a log
  line next build.
- Nothing here was tested on `L_Tutorial_Island`.

**Touched outside the goal:** `IdleBlockChance` is now 0, so defenders guard ONLY on a read. That
changes how they fight the player too, and it is a judgement call rather than a fix.

## Refine

**The decorator is the change I did not plan and would not have found without running it.** The plan
had Block as a plain first child; the log said it was evaluated once per ~2.3s cycle and the numbers
made that obviously fatal. Added the abort, re-ran, and the telegraph path started firing.

**A tooling lesson worth more than the ticket.** `EditorAssetSubsystem.save_loaded_asset` defaults to
`only_if_is_dirty=True`, and `set_editor_property` on `UBlackboardData.Keys` does **not** mark the
package dirty. So the first `BB_Human` save returned True and wrote nothing - while reading the key
straight back succeeded, because that read the in-memory object. The file kept its 2026-08-05 mtime
for an hour while I hunted a C++ bug that was not there. **Verify an editor write against the DISK
(mtime, or the bytes), not against a read-back.** Fixed with `save_loaded_asset(bb, False)`;
`BB_Human.uasset` went 2674 -> 3057 bytes.

**Deliberately left undone:** the pathing failure above (its own ticket - it may be arena navmesh
rather than anything in this work), the guard-break question, and every tuning dial. The block
numbers held their intended shape on first contact, so there is nothing to tune until someone
watches a fight and says what is wrong with it.
