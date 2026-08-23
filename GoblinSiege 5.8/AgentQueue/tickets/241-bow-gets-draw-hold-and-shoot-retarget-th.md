---
id: 241
title: Bow gets draw, hold and shoot: retarget the archery set to human and goblin
agent: claude-acf
status: done
claimed: 2026-08-21T19:25Z
build: none
waiting_on: Bow ships and plays without montages. Only remaining item is Michael eyeballing the three _Gob archery clips before any montage is authored on them.
evaluated: 2026-08-23T21:30:50Z
observed: 2026-08-23T21:30:09Z | Michael watched the three retargeted goblin bow clips and confirmed they all animate correctly after the re-retarget through RTG_MixamoToGoblin_InPlace
scenario: the three _Gob clips opened in the animation editor and played back
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.cpp
  - Content/Characters/ScoutV2/RTG_MixamoToGoblin.uasset
---

## Goal

Bow gets draw, hold and shoot: retarget the archery set to human and goblin

## Generate

Retargeted three Mixamo archery clips onto both skeletons so the bow could have draw / hold / release
animation at all: `draw_arrow` (1.03s), `aim_overdraw` (3.77s), `aim_recoil` (0.70s), as `_Gob` under
`Content/Characters/ScoutV2/Anims_Bow/` and `_Hum` under `Content/Characters/Humans/Anims_Bow/`.
Also added `UGSGA_BowShot::GetFireCooldownRemaining`, so the draw could be told whether a shot would
actually be allowed.

The goblin set was first produced through a NEW `RTG_MixamoToGoblin` built with default chain
mapping. That was wrong twice over - see the correction below - and the clips were re-retargeted
through the project's existing `RTG_MixamoToGoblin_InPlace`, overwriting in place.

## Evaluate

Observed by Michael: he opened the three `_Gob` clips and confirmed they all animate. Measured
alongside that, per animated bone at t=0 / mid / end: 87.95, 36.48 and 87.37 degrees of rotation,
with `aim_overdraw` matching its Mixamo source exactly.

The first pass measured **0.00 degrees on all three** while passing read-back, skeleton match and
compile - the failure mode `verification-means-runtime-not-readback` exists to catch.

`GetFireCooldownRemaining` shipped and was then removed from its only call site under #258: gating
the draw on it meant a quick second press showed no bar at all. The accessor is still correct and
still used by nothing; it should either find a use or be deleted.

## Refine

Changed after Michael's report: the whole goblin retarget, redone through the existing retargeter.

Left for Michael: `RTG_MixamoToGoblin` is now referenced by nothing and wants deleting, but it is an
asset rather than scratch so it is not mine to bin.


---

## Correction (2026-08-23) — the retarget was dead, and the asset should never have existed

Michael: *"the goblin is completely still for the standing_aim animations."* Measured with
`AnimSequenceService.get_bone_transform_at_time`, sampling every animated bone at t=0, mid and end:

| clip | animated bones | max rotation across the clip |
|---|---|---|
| SRC draw_arrow (Mixamo) | 65 | 96.60 deg |
| SRC aim_overdraw | 65 | 36.48 deg |
| aim_overdraw **_Hum** | 43 | 36.48 deg |
| draw_arrow **_Gob** | 41 | **0.00 deg** |
| aim_overdraw **_Gob** | 41 | **0.00 deg** |
| aim_recoil **_Gob** | 41 | **0.00 deg** |

All THREE goblin clips were dead, not just the aim pair - they carried 41 bone tracks in which every
key was the same pose. The human retarget off the same sources was perfect, which is what isolates
the fault to `RTG_MixamoToGoblin`.

**The real mistake was creating that asset at all.** Three working Mixamo-to-Goblin retargeters were
already in the project - `RTG_MixamoToGoblin_InPlace`, `_RootMotion`, `_Traversal` - and the clips
they produced move (`A_MX_Throw_Gob` 36.7 deg, the climbing set 24-119 deg). I built a fourth with
default chain mapping and shipped its output. Both retargeters point at the SAME rigs and the SAME
meshes (`IK_MixamoXBot` -> `IK_GoblinScout_v2`, `SK_MixamoXBot` -> `GOB_Scout_v2`); the only
difference is the chain mapping mine never had.

Re-retargeted the three clips through `RTG_MixamoToGoblin_InPlace`, overwriting in place so the
montages from #259 needed no change. Verified after saving: 87.95, 36.48 and 87.37 degrees, with
`aim_overdraw` matching its source exactly.

`RTG_MixamoToGoblin` is now referenced by NOTHING. It is broken and redundant and wants deleting, but
it is an asset rather than scratch, so that is Michael's call.

**What this cost, and the rule it breaks:** a dead animation passes read-back, skeleton match and
compile - which is exactly what `verification-means-runtime-not-readback` records, and exactly why
#241 was gated on Michael eyeballing the clips. The gate was right; I authored six montages on top of
the unreviewed output before it was ever checked.
