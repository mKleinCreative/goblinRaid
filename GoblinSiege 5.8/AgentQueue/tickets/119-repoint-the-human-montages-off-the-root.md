---
id: 119
title: Repoint the human montages off the root-locked A_MX_Gob copies onto the clean A_HU_ import
agent: claude-anim
status: review
claimed: 2026-08-10T22:05Z
build: none
waiting_on:
evaluated: 2026-08-10T22:14:07Z
files: 
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Light.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Spin.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Flurry.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Heavy.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Chop.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Overhead.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Kick.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Block_Idle.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Block_React.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_HitReact_Front.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_HitReact_Left.uasset
---

## Goal

Repoint the human montages off the root-locked A_MX_Gob copies onto the clean A_HU_ import

## Generate

**Michael called the cause and an earlier session rejected it.** His words: *"it looks like the root
node is getting snapped to the ground - that's something I mentioned a while ago that you rejected."*
He was right, and the evidence was already sitting in one of my own probe outputs before he said it.

The repo carries TWO parallel human combat sets:

| Set | Location | count | `force_root_lock` | `enable_root_motion` |
|---|---|---|---|---|
| `A_HU_*` | `/Characters/Humans/Anims/` | 21 (full combat + locomotion) | False | False |
| `A_MX_*_Gob` | `/Characters/Humans/Anims_Combat/` | 9 - exactly the clips #113 edited | **True** | **True** |

The `A_HU_*` set carries the same root settings as the locomotion clips that always rendered
correctly. All 11 `AM_HU_*` montages referenced the OTHER set. `force_root_lock` pins the root bone
to the reference pose while the hips keep their authored travel - measured at 88uu of hips
translation across `A_MX_Atk_Horizontal_Gob` alone - which is exactly "the root is snapped to the
ground", and it is why the SEQUENCE previewed fine while the MONTAGE previewed as a collapsed mesh
(Michael's screenshot, `Pictures/GoblinSiegeScreenshots/AM_HU_Atk_Light.png`).

So #113 did not fail to fix the upward pop; it traded the pop for this.

**The change.** Every montage's segment 0 repointed from `A_MX_<x>_Gob` to `A_HU_<x>`, deriving the
target by name rule and preserving the exact in/out floats and play rate read from the existing
segment. The API has no swap, so each is remove + add + restore in/out. Michael chose this over
reverting `force_root_lock` on the 9 clips, because reverting would restore the state that produced
the original floating complaint #113 was written for, and because he confirmed `A_HU_*` is the good
import and `A_MX_*_Gob` the old attempt.

| Montage | was | now | in/out |
|---|---|---|---|
| AM_HU_Atk_Light | A_MX_Atk_Horizontal_Gob | A_HU_Atk_Horizontal | 0.550-1.700 |
| AM_HU_Atk_Spin | A_MX_Atk_360High_Gob | A_HU_Atk_360High | 0.700-1.750 |
| AM_HU_Atk_Flurry | A_MX_Atk_Combo3_Gob | A_HU_Atk_Combo3 | 0.000-2.733 |
| AM_HU_Atk_Heavy | A_MX_Atk_Combo3_Gob | A_HU_Atk_Combo3 | 0.000-2.733 |
| AM_HU_Atk_Chop | A_MX_Atk_Downward_Gob | A_HU_Atk_Downward | 0.450-1.450 |
| AM_HU_Atk_Overhead | A_MX_Atk_Downward_Gob | A_HU_Atk_Downward | 0.000-2.267 |
| AM_HU_Atk_Kick | A_MX_Atk_Kick_Gob | A_HU_Atk_Kick | 0.000-1.867 |
| AM_HU_Block_Idle | A_MX_Block_Idle_Gob | A_HU_Block_Idle | 0.000-1.833 |
| AM_HU_Block_React | A_MX_Block_ReactLarge_Gob | A_HU_Block_ReactLarge | 0.000-1.333 |
| AM_HU_HitReact_Front | A_MX_Hit_Gut_Gob | A_HU_Hit_Gut | 0.000-1.600 |
| AM_HU_HitReact_Left | A_MX_Hit_FromLeft_Gob | A_HU_Hit_FromLeft | 0.000-1.033 |

The 9 `A_MX_*_Gob` clips were NOT deleted or modified - only dereferenced, so this is reversible by
repointing back.

## Evaluate

**Verified:**

- **Michael previewed `AM_HU_Atk_Light` after the first swap: "he is attacking normally."** That is
  the acceptance test for this ticket, and it was done on ONE montage before the other ten were
  touched, deliberately.
- Every swap re-read after writing: anim name matches the intended target, in/out within 0.002s of
  the originals, montage duration unchanged, one section, still `DefaultSlot`. `changed=10
  skipped=0 failed=0`, all `saved=True`.
- Idempotent: the script skips any montage already pointing at an `A_HU_` clip.

**NOT verified:**

- **In-game.** Only the asset preview has been seen. Everything today says preview and runtime are
  different questions. Michael is running PIE himself from the already-open editor rather than
  having an agent read a log at it - he judges the feel directly. **This ticket is not done until he
  reports back.**
- **A residual head wiggle**, reported by Michael on the fixed preview, and accepted by him as
  liveable - see Refine.
- The other adversaries (`BP_KnightDPelegrini`, `BP_ErikaArcher`, `BP_PeasantMan`,
  `BP_UrielAPlotexia`, `BP_CastleGuard02`) share these montages, so all six should be right now, but
  only `BP_CastleGuard01`'s montage was previewed.

**Owed to AGENT_STATE.md:** a FAILED line. #113 was closed on an unobserved asset edit that made the
problem worse, and Michael's correct diagnosis was rejected at the time. The pattern across #113,
#116 and this ticket is identical: a change argued from static reads, shipped without watching it.

## Refine

**Changed in response to my own evaluation:** the first attempt was going to swap all 11 in one
pass. Split it so one montage could be judged by eye before the other ten were touched - which is
what turned this from another argued fix into a verified one.

**Deliberately left undone:**

- **The head wiggle - ACCEPTED BY MICHAEL, 2026-08-10, do not "fix" it.** His verdict on the
  repointed preview was *"there's a little bit of a head wiggle, but he is attacking normally"*, and
  asked what to do about the wiggle he chose to leave it. It is a known, deliberate non-issue, not
  an oversight. If a later pass wants it anyway, the untested starting points are: the `A_HU_` clips
  animate `head`, `headtop_end`, `righteye` and `lefteye`, and `ABP_Human` runs a `LayeredBoneBlend`
  with `BlendWeights_0 = 1.0` feeding a second `Slot 'UpperBody'` whose LayerSetup was never
  readable through the Python API.
- **The 9 orphaned `A_MX_*_Gob` clips** are now referenced by nothing. They are candidates for
  deletion but that is a separate call - and per Michael they are the old attempt, so a bloat pass
  should confirm with him before removing.
- **`GS.Combat.Debug` still defaults to 1** (`GSGA_SwordLight.cpp:25`), the second bug visible in
  the CombatBugs video. One-line C++ change, unclaimed, needs a build.
