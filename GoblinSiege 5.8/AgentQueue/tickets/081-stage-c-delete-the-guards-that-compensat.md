---
id: 081
title: "Stage C: delete the guards that compensated for the wrong climb model; first pass at Moria-feel cadence"
agent: claude-climbrebuild
status: done
claimed: 2026-08-08T20:44Z
build: none
waiting_on:
evaluated: 2026-08-08T20:49:47Z
files: 
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
  - Content/Characters/ScoutV2/Animations/ThirdPerson_AnimBP_Gob.uasset
---

## Goal

Michael cleared the decision list: **(1) Stage C - go with the recommendation**, (2) teleports to the
unclimbable roofs, (3) observe plane transitions before building anything, (4) hold-E to climb stays.
Then: *"5, let's do this, I'd love for it to look smooth and a little hectic. I want the goblins to
climb like they would in that one scene of lord of the rings in the mines of moria."*

## Generate

**Stage C - 13 nodes deleted from `BP_GSPlayerCharacter` (772 -> 759).**

| removed | why |
|---|---|
| `8B71747F` SET ClimbBlockedSeconds, `BF8090CC` SET ClimbLastZ | the accumulator, replaced by stateless `IsClimbBlockedUpward` |
| `ABBE87E0` `blocked > 0.10` | dead - 0 consumers since #080 |
| `7599E879` lean-out `blocked > 0.60` | the guard that sabotaged the top-out for two sessions |
| `5E1A0842` old ledge sphere trace, `88CDAEB5` sky line trace | superseded by the C++ search - **2 traces/tick saved** |
| `55D17294` 0.4695 gate, `6C5683D0` / `AF995599` old ANDs, `82F641B4` / `470F0FBC` / `F5DD5E82` / `E854959B` sky-check maths | all superseded |

Exec was **rewired before deleting**, not after: `AAB325D5 -> A7B5471D` (past the old probes) and
`8A1B96BA -> 98201626` (past the accumulator). The lean-out `Select` now has no condition, so it
defaults to `bPickA=false` -> `ClimbWallOffset` (70), which is exactly the no-lean-out behaviour -
**verified before deleting, not assumed**.

**Stage 6 first pass - cadence from measured root motion.**

`AnimationLibrary.get_bone_pose_for_time` on the root bone gives each clip's actual travel:

| state | clip | travel | rate was | rate now |
|---|---|---|---|---|
| ClimbUp | `A_MX_Braced_Hop_Up_Gob` | **77.1uu vertical / 1.667s = 46.3 uu/s** | 7.0 | **6.33** |
| ShimmyL | `A_MX_Braced_Hop_Left_Gob` | 147.7uu lateral | 7.0 | **2.98** |
| ShimmyR | `A_MX_Braced_Hop_Right_Gob` | 131.4uu lateral | 7.0 | **3.72** |

Rates are `achieved climb speed / clip speed`, using the **measured 293 uu/s** (log median,
15.26uu per tick at 19fps) rather than the commanded 430. `validate_state_machine` valid, compiled,
saved.

## Evaluate

**The 7.0 I shipped yesterday was a guess and it was wrong by ~10%.** 7.0 implies 324 uu/s against an
actual 293, so the feet climbed faster than the goblin - the residual foot-slide Michael reported two
tests ago and which I "fixed" by guessing a bigger number. 6.33 comes from the clip's own root track.

**The two shimmies now have different rates (2.98 vs 3.72) because the clips travel different
distances** - 147.7uu vs 131.4uu. Matching each to its own travel is what removes the slide, and it
incidentally makes left and right slightly asymmetric, which serves "a little hectic" for free.

**I did NOT swap the clips.** #075 records a Michael decision - braced hops, *"do not improve this to
a smooth cycle again"* - and a continuous cycle is exactly what "smooth" could be misread as. At 6.33
a hop lasts 0.26s, so ~3.8 hops/sec: scrambling, which is the Moria read. Smoothness has to come from
cadence matching, not from replacing the clips.

**NOT PLAYED.** Stage C is a deletion verified by graph readback (all six accept-path checks pass,
`UP_TO_DATE`, 13 trace nodes down from 15); the anim rates are arithmetic from the root track. Neither
has run in PIE.

**Protocol slip, mine:** I edited `ThirdPerson_AnimBP_Gob.uasset` before adding it to this ticket's
claim. No other agent was running so nothing was lost, but the claim should have come first. The file
is listed above now.

**Owed AGENT_STATE.md** - DECISION (2026-08-08, Michael): climb animation targets the Moria scene -
smooth but hectic, braced hops kept. Play rates are DERIVED from clip root travel over achieved climb
speed, not tuned by eye.

## Refine

- **Rewired exec before deleting.** Deleting a node in an exec chain severs it silently; the
  accumulator sat mid-chain in `ClimbTick` and the probes mid-chain in `ClimbStaminaExits`.
- **Checked what the lean-out `Select` falls back to before removing its condition.** If A/B had been
  the other way round, deleting the trigger would have pinned the offset at `ClimbOffsetBlocked` 150
  and pushed the climber a metre off every wall.
- **Deleted the accumulator rather than leaving it computing.** Its cost was never cycles - it read
  0.000 for 1285 ticks while the character was visibly stuck and sent me down three wrong diagnoses.
  Leaving a lying instrument in place is worse than leaving dead code.
- **Took the rate from the root track instead of tuning by eye**, which is what produced the 7.0.

**Deliberately left undone:** per-goblin cadence jitter so a horde does not climb in lockstep (the
"hectic" half, and it matters most with many goblins - the sequence player's PlayRate is a node
property here, not an exposed pin, so driving it live needs the pin exposed first); braced<->free-hang
blending; the two-window motion warp; montage-by-rise. Stage 4 is deliberately untouched pending
Michael's observation. Roof continuation still owed for `Small_8`/`Small_10`; `Medium_02` E/W miss the
walkable gate by 0.02 (nz 0.45 vs 0.4695) and may be a gate question rather than a feature.
