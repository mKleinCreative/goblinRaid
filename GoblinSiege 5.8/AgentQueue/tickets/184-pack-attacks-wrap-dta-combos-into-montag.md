---
id: 184
title: Pack attacks: wrap DTA combos into montages and repoint the 5 goblin swing stages
agent: claude-packanim
status: done
claimed: 2026-08-18T02:00Z
build: none
waiting_on: Windup corrected on all four pack stages so the damage window straddles the measured contact frame - contact was landing at -63/-26/-33/-38 percent, i.e. before the window opened. Totals held constant, reclaimed time moved into recovery, so recovery/clip on light stage 0 goes 0.146 to 0.333 (above 121s 0.287). NEEDS MICHAEL TO SWING IT: unobserved. Chain speed deliberately NOT changed - his second complaint about the combo not linking fast enough is a separate variable and changing both at once would make it impossible to tell what did what (123).
evaluated: 2026-08-20T17:03:46Z
observed: 2026-08-20T17:03:45Z | Michael swung the goblin combo and the second swing is a DIFFERENT authored animation from the first, so the repointed pack montages are driving the chain and FinishRecovery is advancing stages. He also saw the goblin start to settle before swing 2 begins - measured in the live editor as light stage 0 holding 0.455s of recovery after its damage window closes at 0.514s, against a 1.367s clip that keeps playing its return-to-neutral throughout
scenario: PIE with the player goblin, attack input pressed twice to chain light stage 0 into stage 1; stage timings and montage lengths then read off the GA_GS_SwordLight and GA_GS_SwordHeavy CDOs in the running editor
files: 
  - Content/Blueprints/Abilities/GA_GS_SwordLight.uasset
  - Content/Blueprints/Abilities/GA_GS_SwordHeavy.uasset
  - Content/Blueprints/Abilities/GA_GS_GuardBreak.uasset
---

## Goal

Pack attacks: wrap DTA combos into montages and repoint the 5 goblin swing stages

> **Written by `claude-gddlock` on 2026-08-20, not by `claude-packanim` who did the work.**
> The ticket was left with placeholder Generate/Evaluate/Refine. This is a **third-party
> reconstruction from the frontmatter, the diff and a live measurement**, not the original agent's
> self-assessment.

## Generate

Two changes to the goblin's melee, across `GA_GS_SwordLight`, `GA_GS_SwordHeavy` and
`GA_GS_GuardBreak`:

1. **The pack animations reach the swings.** #182 retargeted the CombatMasterBundle DTA combat set
   onto `GOB_Scout_v2_Skeleton`; this ticket wraps those combos into montages and repoints all five
   goblin swing stages at them (three light, heavy, guard break).
2. **The damage window was fixed to straddle contact.** Contact was landing at
   **-63 / -26 / -33 / -38 percent** on the four pack stages - negative meaning the blade visually
   connected *before* the window opened, so hits registered ahead of the animation. Windup was
   corrected on all four. **Stage totals were held constant, so the time reclaimed from windup was
   moved into `RecoverySeconds`** - light stage 0's recovery/clip ratio going 0.146 -> 0.333.

Chain speed was deliberately **not** touched, on the grounds that Michael's separate complaint about
the combo not linking fast enough is a different variable, and moving both at once would make it
impossible to attribute the result. That call is vindicated below.

## Evaluate

**Observed by Michael, 2026-08-19/20.** He swung the combo and reported the second swing is a
**different** animation from the first - which is the direct confirmation this ticket never got: the
repointed pack montages are driving the chain, `FinishRecovery` is advancing stages, and the montages
are not being silently refused on the goblin skeleton. The agent's own partial evidence
(`PlayAnimMontage` returning 2.5167 rather than 0, 0 of 17 pawns in reference pose) is consistent
with that and is now superseded by a human watching the real path.

**The contact fix ships. Its side effect does not, and it is this ticket's doing.** Measured live in
the running editor, 2026-08-20:

| stage | montage | windup | damage | recovery | next swing | clip |
|---|---|---|---|---|---|---|
| 0 | `AM_GOB_DA_Atk_Light1` | 0.190 | 0.324 | **0.455** | 0.969s | 1.367s |
| 1 | `AM_GOB_DA_Atk_Light2` | 0.185 | 0.375 | **0.333** | 0.893s | 1.267s |
| 2 | `AM_GOB_DA_Atk_Light3` | 0.330 | 0.567 | **0.770** | 1.667s | 1.867s |

Stage 0's damage window closes at 0.514s and the next swing is not permitted until 0.969s. The clip
runs to 1.367s, so it never blends out - what plays during those 0.455s is the **clip's own
return-to-neutral**, which reads exactly like the goblin resetting. Holding stage totals constant put
the reclaimed windup time into the one place that is visible, and 0.146 -> 0.333 of a 1.367s clip is
**0.20s -> 0.455s**: the gap between swing 1 and swing 2 more than doubled.

**What is still unproven:** the guard-break stage was claimed and repointed but has not been measured
or watched; only the three light stages and the heavy were measured. And nothing here was tested on a
**horn-summoned** goblin - the whole measurement is off the player pawn and the ability CDOs.

## Refine

Nothing changed in this ticket's edits. The contact correction is right and stays; the recovery
consequence is a **feel decision** and is deliberately left to a follow-up rather than reversed here,
because reverting it would take the contact fix with it. Closed on Michael's instruction 2026-08-20,
with the recovery change tracked as its own ticket.
