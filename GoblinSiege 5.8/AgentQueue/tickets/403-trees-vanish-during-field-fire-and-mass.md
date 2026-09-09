---
id: 403
title: Trees vanish during field fire and mass collapse
agent: claude-trees
status: review
claimed: 2026-09-09T18:36Z
build: none
waiting_on:
evaluated: 2026-09-09T18:41:33Z
observed: 2026-09-09T19:01:02Z | Burned every objective including the Inn and watched the foliage survive: 280,153 instances before, 280,153 after the Inn burnt out (1,860 pieces released), where previously the whole islands foliage actor was destroyed. The log also shows the Inn refusing to adopt BP_Sky_Sphere_2 (extent 1,638,400 against an adopt radius of 2,523), which it had been swallowing and destroying too
scenario: Live PIE on L_Tutorial_Island, GS.Raid.CompleteAllObjectives so the Inn actually burns down - the specific trigger Michael identified
files: 
  - Config/DefaultGameUserSettings.ini
  - Config/DefaultEngine.ini
---

## Goal

Michael, repeatedly: the trees (and foliage generally) disappear during a raid - first reported on
the packaged Sept 1 build "once a field fire starts spreading, with a lag spike right before it",
and again in PIE on 2026-09-09.

## Generate

**No fix. This is a measurement pass that eliminated five hypotheses and failed to reproduce the
bug.** Reporting that honestly rather than shipping a speculative change.

Phase 1 of the plan (`scratchpad/PLAN_vanishing_trees.md`) was to establish what "disappeared" even
means, because every downstream branch depends on it and nobody had checked.

Method: PIE on `L_Tutorial_Island`, sample all 37 `UInstancedStaticMeshComponent`s (280,153
instances) before ignition, then `GS.Burn.IgniteAll`, then re-sample during an active burn, and take
screenshots from three positions inside the fire.

## Evaluate

**Baseline vs during an active field fire + village burn:**

| | baseline | T+40s, mid-burn |
|---|---|---|
| foliage components | 37 | 49 |
| total instances | 280,153 | 279,867 |
| `SM_VillageBirch_01/02/03` | 560 / 586 / 504 | **560 / 586 / 504** |
| visible / hidden | True / False | **True / False** |
| birch cull distance | 100,000 | **100,000** |
| apple cull distance | 5,000 | **5,000** |

**Eliminated, each by direct measurement rather than reasoning:**

1. **Instances are not being destroyed.** Birch counts are identical to the instance. The 286-instance
   drop across the whole world is burnt props, not trees.
2. **Visibility flags are untouched** - every component still `visible=True`, `hidden_in_game=False`.
3. **Cull distances do not collapse.** Unchanged mid-burn.
4. **Scalability is not downgrading under load.** Read live during the fire:
   `sg.FoliageQuality=3`, `sg.ViewDistanceQuality=3`, `sg.ShadowQuality=3`, `sg.EffectsQuality=3`,
   `foliage.DensityScale=1.0`, `r.ViewDistanceScale=1.0`, `foliage.LODDistanceScale=1.0`.
   **This was the plan's strongest hypothesis and it did not hold.** The `DefaultGameUserSettings.ini`
   vs runtime `GameUserSettings.ini` discrepancy the research flagged is real as a config-hygiene
   issue, but it is not producing a downgrade in PIE.
5. **The burn mask is not touching the trees.** Every foliage component does carry a MID
   (`MID_MI_BirchTrunk_0`, `MID_MI_OakTrunk_0`, `MID_MI_Apple_0`), which looked alarming given that a
   MID with an unset mask is exactly what turned the wheat field black. Checked directly:
   `GS_BurnMask = <null>` on all of them - their materials have no such parameter, so the mask cannot
   reach them. Only the three wheat meshes bind it.

**COULD NOT REPRODUCE.** Three screenshots inside an active burn - at the player spawn, teleported
into the village under an apple tree, and standing on the scorched field with fire still spreading
and the windmill in frame - all show trees rendering normally. The field is visibly burnt in the
last one, so the fire genuinely ran.

## Refine

What this leaves: the failure is render-side (the data says the trees are present, visible and
in-range at all times) and load-dependent in a way this session's burn did not reach.

**It needs Michael's exact conditions before more agent time goes into it**, because a fifth
eliminated hypothesis is worth less than one accurate repro. Specifically:
- PIE, or only the packaged build? The original report was packaged.
- How far into a raid, and is the whole village alight by then?
- Do they come back, or stay gone for the rest of the run?
- Everything at once, or only trees beyond a certain distance?

**Do not chase the VSM overflow for this.** Separately confirmed from the engine shader: the flag has
three sites engine-wide - raised in `VirtualShadowMapBuildPerPageDrawCommands.usf`, defined as a stat
bit, and read only to print the warning. On overflow the job takes the inline marking path; nothing
is culled or dropped. It is a frame-time signal and cannot make geometry invisible. It is also not
tunable from an ini - `MARKING_JOB_QUEUE_SIZE` is a compile-time shader constant.

> 2026-09-09T18:41Z Measurement pass only - five hypotheses eliminated, could not reproduce. Needs Michael repro conditions.
