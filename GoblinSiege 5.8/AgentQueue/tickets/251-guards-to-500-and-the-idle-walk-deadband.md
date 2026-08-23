---
id: 251
title: Guards to 500 and the Idle-Walk deadband the guard data finally justifies
agent: claude-acf
status: done
claimed: 2026-08-22T02:13Z
build: none
waiting_on: Asset-only, no build. Needs a fight + GS.AI.LogLocomotion 8; guards are the row to read.
evaluated: 2026-08-22T02:15:17Z
observed: 2026-08-22T03:55:54Z | All four guards went smooth at 450 with the retuned bands: flick/s 2.4/2.0/1.5 down to 0.6/0.4/0.5, rest time roughly doubled
scenario: 8s capture, four castle guards chasing and fighting
files: 
  - Content/Blueprints/Adversaries/BP_CastleGuard01.uasset
  - Content/Blueprints/Adversaries/BP_CastleGuard02.uasset
  - Content/Characters/Humans/ABP_Human.uasset
## Goal

Archers are fixed (#250: Erika_C_1 flick/s 2.0 -> 0.2, `smooth`). The guards became the visible
offender the moment a capture caught them chasing: flick/s 1.5-2.4 at ~900 uu/s.

## Generate

**The evidence that reversed an earlier call.** In the archer capture, `prop/s` equalled `flick/s`,
so I told Michael the hysteresis was worthless and dropped it. In the guard capture it is roughly
HALF `flick/s` on every guard (2.4->1.1, 2.0->1.0, 1.5->1.0). Same simulation, opposite verdict,
because a signal swinging 0..1023 clears any threshold while one swinging 0..900 does not.

`ABP_Human` Locomotion transitions:

| transition | was | now |
|---|---|---|
| Idle -> Walk | `> 10` | **`> 60`** |
| Walk -> Idle | `< 10` | **`< 25`** |
| Walk -> Run | `> 500` | **`> 290`** |
| Run -> Walk | `< 450` | **`< 250`** |

`BP_CastleGuard01` 1210.40 and `BP_CastleGuard02` 1238.62 -> **450**.

**Why Walk->Run moved, which Michael did not ask for.** He approved "guards to ~500". At a 500 cap
`> 500` can never fire, so they would have been stranded in Walk playing `A_HU_Std_WalkF` (authored
167.5) at 500 - a 3x slide, worse than the problem. This is the identical trap already hit once with
Erika at 400, so setting it and saying nothing was not an option. 290 is the midpoint of the two
authored clip speeds (167.5 and 406.9), which is where the boundary belongs.

Resulting bands: Idle <25 | Walk 60-290 against a 167.5 clip (<=1.7x) | Run 290-450 against a 406.9
clip (0.7-1.1x). Erika at 200 now sits inside Walk permanently at 1.2x.

## Evaluate

All six values read back from the asset after save: four rules intact with `has_rule=True` and blend
durations still 0.200, both guards at 450, Erika still 200. That matters because
`set_transition_rule_comparison` documents itself as clearing prior rule logic first - a silent loss
of a rule would read as "transition never fires", not as an error.

Not watched. No capture since. Slide is now bounded but not eliminated; the Walk band is still 1.7x
at its top.

Untouched: `BS_GS_Locomotion_Hu` remains an orphan node, so `HU_Direction` is still computed and
consumed by nothing and every direction of travel still plays forward clips. That is the real
animation debt and it is not this ticket.

## Refine

Nothing further changed. Recorded for AGENT_STATE: *a fix simulated as worthless against one
population can be worth half the defect against another - `prop/s` vs `flick/s` must be read per row,
never pooled.*
