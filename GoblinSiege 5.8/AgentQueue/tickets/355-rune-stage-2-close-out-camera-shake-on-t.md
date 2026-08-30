---
id: 355
title: Rune stage 2 close-out: camera shake on the player's landed hits, and a deflect cue when a knight's plate turns a blow aside
agent: claude-combat
status: done
claimed: 2026-08-29T06:36Z
build: none
waiting_on:
evaluated: 2026-08-29T06:53:37Z
observed: 2026-08-29T06:53:13Z | Deflect cue proven end to end on a knight held still and facing the player at 0.0 deg (yaw sampled steady at -80.0 through the hit): the swing logged 'blow was DEFLECTED by BP_KnightDPelegrini_C_0's plate - attacker-only stop, light shake', and the per-frame trace showed the ASYMMETRY the cue is built on - attacker at 0.050, knight at 1.000 on the same frame, both back to 1.0 next sample. Three earlier runs read PLATE-GAPS(flank) despite frontal placement; the knight was patrolling and bUseControllerDesiredRotation re-faced him toward his waypoint inside the 0.35s windup, so the plate branch never ran - a test-environment fault, not a feature fault, and the reason a facing-dot readout was added to the damage log.
scenario: PIE on L_CombatArena; knight's BT StopLogic'd and movement disabled, placed 120uu ahead and rotated to face the possessed player, GS.Combat.Debug 1, per-frame sampler on both parties' CustomTimeDilation plus the knight's yaw, then the axe light attack fired.
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.h
  - Source/GoblinSiege/Combat/GSDamageExecCalculation.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.h
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Source/GoblinSiege/Combat/GSGameplayTags.h
  - Source/GoblinSiege/Combat/GSGameplayTags.cpp
---

## Goal

Rune stage 2 close-out: camera shake on the player's landed hits, and a deflect cue when a knight's plate turns a blow aside

## Generate

The last two pieces of the impact layer. With #349 (real hit data), #351 (sound) and #353
(hitstop) landed, a normal hit finally has weight - which is the precondition for a DEFLECT to mean
anything, and why this was deferred from Stage 1 rather than built into a silence.

**Camera shake** - `Combat/GSHitCameraShake.h/.cpp`, two `UCameraShakeBase` subclasses
(`_Light`, `_Heavy`) with a `UPerlinNoiseCameraShakePattern` set in the constructor, so no asset
is needed and the swing references them by class. Two CLASSES rather than one with a magnitude
parameter because the camera manager plays and stacks BY CLASS: a heavy landing during a light's
shake must read as a bigger event, and two instances of one class would merely overlap. Light
stacks (`bSingleInstance=false` - three quick taps should build); heavy restarts itself. Both are
tuned to peak inside the 0.06s hitstop and be gone before recovery, so they read as the impact and
not as something happening to the camera afterwards. Played on the attacker's own
`PlayerCameraManager`, so only a player-controlled attacker ever has a camera to shake. Per-stage
data: `bCameraShakeOnHit`, `bHeavyShake`.

**Deflect cue** - the whole reason Michael's knight read as a sponge. The plate was doing its job
and nothing said so: a frontal hit and a flank hit produced the identical impact, so the player saw
a health bar refusing to move and concluded the hit had *failed* rather than been *refused*.

Mechanism, mirroring the existing block-recoil path exactly:
- `GSDamageExecCalculation.cpp` sets a new loose tag `State.LastHitDeflected` on the ATTACKER in
  the `bStraightOn` plate branch. A loose tag, not a callback: the calc runs INSIDE the swing's
  sweep loop (the same hazard `NotifyAttackWasBlocked` documents), so it may not reach back into
  the ability. Set-not-add - it is a verdict, not a stack.
- `UGSGA_SwordLight::DoSweep` reads the tag after `ApplyGameplayEffectSpecToTarget` returns and
  clears it on the same frame, so it can never leak into the next swing's verdict.
- On a deflect: the attacker gets hitstop but the VICTIM does not (the plate absorbed it - the
  knight barely registers while the attacker's blade sticks on the steel; that asymmetry is most of
  the cue), and the shake is forced LIGHT regardless of stage weight (a heavy that bounces off
  plate should feel like it bounced, not like it landed).

## Evaluate

**Observed:** Deflect proven on a knight held still and facing the player at 0.0 deg (yaw steady at -80.0 through the hit): the swing logged 'blow was DEFLECTED ... attacker-only stop, light shake', and the per-frame trace showed the asymmetry the cue is built on - attacker 0.050, knight 1.000 on the same frame, both restored next sample. Camera shake is wired on the same site but is not observable from Python in any honest way; whether it reads as impact or wobble is Michael's judgement. Three earlier runs resolved PLATE-GAPS(flank) despite frontal placement: the knight was patrolling and bUseControllerDesiredRotation re-faced him toward his waypoint inside the 0.35s windup, so the plate branch never ran. A facing-dot readout was added to the damage log so that is legible in one line next time; it is written but needs a build.

**What a runtime probe can and cannot prove here.** The deflect path is fully observable: the tag
on the attacker, its clearance, the asymmetric hitstop (attacker frozen, victim not), and the debug
line naming the deflect. The camera shake is NOT observable from Python in any honest way - a shake
is a transform applied to the player's camera for 0.12s, and reading it back would only prove the
call was made, which the code already shows. **Whether it reads as impact or as camera wobble is
Michael's judgement, and the amplitudes are constructor constants a Blueprint child can override.**

**A hazard avoided by reading the existing code first, not by luck.** The obvious deflect design
is "have the damage calc tell the swing" via a direct call. `NotifyAttackWasBlocked` already
documents why that is unsafe here - the calc is executing inside the swing's own overlap loop, and
re-entering the ability from there tears down state the loop is still iterating over. The loose-tag
channel exists precisely because that lesson was already paid for.

**Touched outside the goal:** nothing. All seven claimed files are the seam this feature lives on.

**AGENT_STATE.md owes:** DECISION - *a plate deflect is signalled to the attacking swing by a
one-frame loose tag set in the damage calc; a deflected blow stops the attacker, not the victim, and
shakes light. Stage 2 of the Rune plan is complete: real hit data, sound, hitstop, shake, deflect.*

## Refine

**Changed in response to my own evaluation:** the first draft played the victim's hitstop on a
deflect too. Reading it back, that made a refused blow feel identical to a landed one at the exact
moment the feature exists to distinguish them - so the victim's stop was removed on deflect, and
that asymmetry became the centre of the cue rather than an afterthought.

**Deliberately left undone:**

1. **No deflect SOUND or spark yet.** The FX table (#351) has one universal entry; a deflect wants
   its own metal clang and a spark at the contact point, which is a second `FMaterialImpactFX` keyed
   on the knight's physical material. Pure data, and it needs the plate's physical material to be
   distinct from flesh - which it currently is not.
2. **Deflect is plate-only.** A BLOCKED hit already has its own cue (recoil, `AM_HU_Block_React`),
   so it is not routed through this path - deliberately, since block and deflect should feel
   different: one opens the attacker, the other merely refuses him.
3. **Arrows are untouched** - no shake, no deflect on a bow shot, consistent with #353 leaving them
   without hitstop. Bow already ignores plate by design ("finds the gaps"), so there is nothing to
   deflect.
4. **Shake amplitudes are a first guess.** 0.6 deg pitch light / 1.4 deg heavy, chosen to sit
   under the hitstop. If they read as wobble, halve them; if invisible, the heavy roll is the first
   thing to raise.
