---
id: 349
title: Rune stage 2: real melee hit detection - replace the overlap sweep with a swept trace carrying bone name, impact point and physical material
agent: claude-combat
status: done
claimed: 2026-08-29T02:50Z
build: none
waiting_on:
evaluated: 2026-08-29T04:33:57Z
observed: 2026-08-29T04:32:56Z | Drove swings in PIE and read the hit log: 'BP_GSPlayerCharacter_C_0 hit BP_CastleGuard01_C_2 bone=RightLeg' with no SYNTHESISED marker, so melee now recovers a real bone, contact point and physical material where it previously fabricated a hit at the actor origin. Damage was unchanged at 25.0 through the same swing. Also watched the montage picker vary across six defender swings - 3x AM_HU_Atk_Light and 2x AM_HU_Atk_Heavy - and Michael confirmed the reworked defender attacks and the widened dodge i-frames look right in play.
scenario: PIE on L_CombatArena and L_Tutorial_Island: player swing driven from Python against a guard placed 120uu ahead with GS.Combat.Debug 1, defender swings fired across five guards, then Michael playing against the knight.
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.h
---

## Goal

Rune stage 2: real melee hit detection - replace the overlap sweep with a swept trace carrying bone name, impact point and physical material

## Generate

**The problem.** `UGSGA_SwordLight::DoSweep` is an `OverlapMultiByObjectType`, and an overlap carries
no impact point, no normal, no bone and no material. The code fabricated a hit from the target's
actor location and said so in its own comment. Three things were impossible as a direct result:
impact FX could not be placed, nothing could tell a head from an arm, and ACF's hit reactions were
fed a synthetic direction.

**`ResolveImpact()`** (`GSGA_SwordLight.cpp`). The overlap stays as the CANDIDATE FINDER - it
already carries the arc, race, dead and invulnerable gating that decides whether a hit counts. For a
target it has already accepted, `ResolveImpact` traces the victim's skeletal mesh and recovers the
real contact point, normal, bone and physical material.

Three decisions inside it:

1. **`LineTraceComponent` on the mesh, not a channel trace.** The first version used
   `ActorLineTraceSingle(..., ECC_Visibility, ...)` and logged
   `[SYNTHESISED - trace missed the mesh] bone='None'` on a hit that had plainly connected.
   **Measured cause: the character mesh answers `ECC_Visibility` with `ECR_IGNORE`.** Channel
   responses are the wrong instrument anyway - the overlap has already decided *whether*, so the
   only open question is *where*.
2. **The trace overshoots the target by 150uu** rather than ending at its origin: a ray that stops
   inside the capsule can terminate before reaching the mesh and return a hit with no bone.
3. **The synthesised hit is KEPT as a fallback.** A capsule-only target, or a mesh the ray grazes,
   must still take the damage the overlap granted. Cosmetics may degrade; a connected swing may not
   stop connecting. The debug line marks when the fallback is used, so its frequency is observable
   rather than assumed.

`PlayImpactEffect` is now called at the real contact point, and `GS.Combat.Debug 1` logs the struck
bone per hit. `CollisionsManager` needed no `Build.cs` change - it is a PUBLIC dependency of
`AscentCombatFramework`, which `AIFramework` already carries.

**Animation work done under the same ticket (Michael's direction, mid-session):**

- **`MontageVariants` + `PickStageMontage`** - one attack that picks its animation at random.
  Variety, NOT a combo: same stage, same damage, same timing. Michael: "I don't want the full combo.
  I want this attack", then "varying it up to use this attack sometimes."
- **Every human/knight attack now plays its FULL animation at rate 1.0.** Measured truncation before
  the fix: Knight's stab lost **40%** (1.50s animation, 0.90s stage), his slash 34%, both heavies
  22%, the guard-break kick 29% *and* it was sped to 1.4x. Each stage was scaled by a single factor
  so the authored windup/window/recovery proportions survive - only the scale was wrong.
- **`AM_HU_Atk_Light` and `AM_HU_Atk_Heavy` moved from the `UpperBody` slot to `DefaultSlot`.** They
  wrap full-body CombatMasterBundle animations (`A_HU_PS_Combo_A1_RM` / `C1_RM`) and were playing
  from the spine up while the legs ran locomotion. `DefaultSlot` is proven in this AnimBP - the hit
  reactions and block montages already use it.
- **Dodge i-frames 0.615s -> 0.85s** to cover the 0.83s roll commit.

## Evaluate

**Observed.** `[GS.Combat] BP_GSPlayerCharacter_C_0 hit BP_CastleGuard01_C_2 bone='RightLeg'` with no
SYNTHESISED marker, and `[GS.Damage] ... = 25.0` through the same swing - real hit data, damage
unchanged. Montage variation measured across six defender swings: 3x A1, 2x C1, the longer one
rate-corrected. Michael confirmed the reworked attacks and the dodge in play.

**A defect I shipped and then caught myself.** `bFitMontageToStage` was defaulted to **true**, which
silently retimed every attack in the project against stage numbers tuned for different montages. The
log caught it: `BP_KnightDPelegrini_C_0 swings 'AM_KN_Atk_Stab' (1.50s) at rate 1.67 to fill a 0.90s
stage` - a swing sped up by two thirds. Michael had just said the player's attack was fine, and an
opt-out default put it at risk. **Now defaults false**: opt-in changes only what asks for it.

**A second consequence I should have flagged before he found it.** Scaling attacks to full length
nearly doubled the Knight's damage windows (0.24 -> 0.400s, 0.30 -> 0.456s). His i-frames covered
0.615s of a 0.83s roll, leaving a 0.215s vulnerable tail - and against a blade live for twice as
long, that tail started catching. Michael: "I got hit twice by a knight's attack trying to dodge
backwards." **The interaction was mine to predict and I did not.** Direction was a red herring;
both dodge montages are 0.83s and the effect is direction-agnostic.

**Written but NOT observed: impact VFX and sound.** `PlayImpactEffect` fires at the right moment with
the right data and then finds nothing to play through:
`Error: Missing Effects Dispatcher Component in GAME STATE! - UACMCollisionsFunctionLibrary`. There
is no `UACMEffectsDispatcherComponent` on the GameState. **So there is still no impact layer** - the
call is correct and inert.

**Touched outside the claim.** This ticket claimed only `GSGA_SwordLight.cpp/.h`. The animation work
also modified five ability Blueprints (`GA_HU_SwordLight/SwordHeavy/GuardBreak`,
`GA_KN_SwordLight/SwordHeavy`), two montages (`AM_HU_Atk_Light/Heavy`) and `GE_GS_DodgeIFrames`,
none of them claimed. Recorded rather than hidden; they were direct requests taken mid-session.

**AGENT_STATE.md owes:** DECISION - *melee recovers a real per-bone impact by tracing the victim's
mesh after the overlap accepts it; the overlap decides whether, the trace decides where.*

## Refine

**Changed in response to my own evaluation:** the channel trace became a mesh trace once the log
showed the mesh ignores `ECC_Visibility`; `bFitMontageToStage` flipped from opt-out to opt-in once
the Knight's 1.67x rate exposed the blast radius.

**Deliberately left undone:**

1. **Impact FX are blocked on a GameState component.** Add `UACMEffectsDispatcherComponent` to
   `AGSGameState` and give it a `UACMImpactsFXDataAsset`. Sound assets already exist
   (`Content/NaPH_RPG_Fantasy_Sounds_Bunle/Weapons_and_Combat/`) and #249 shipped the impact
   attenuation and concurrency classes. **This is the next thing to do.**
2. **Hitstop, camera shake and the deflect cue** - the rest of Stage 2, all unblocked by the real
   hit data but none started.
3. **The damage tag is still hardcoded** - `GSGA_SwordLight.cpp` writes `Damage_Dagger` regardless
   of the weapon's `DamageTypeTag`, which also forces every impact to request axe FX.
4. **The sweep resolves low.** The measured hit came back `RightLeg`: `SweepHeightOffset 120` minus
   capsule half-height puts the sweep centre near the shins. Torso should probably be the default
   contact, and this is now measurable rather than guessed.
5. **Attacks are slower now.** Knight light 0.90 -> 1.50s, heavy 1.76 -> 2.27s, plus a 1.2s
   `BTWaitTime` between swings. Left for playtest; the gap is data.
6. **Only the militia was trimmed to one attack.** The Knight keeps his two-stage chain.
