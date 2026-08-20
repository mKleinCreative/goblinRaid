---
id: 151
title: SwordLight writes MaxWalkSpeed directly instead of applying UGSGE_MoveSpeedScalar
agent: claude-movespeed
status: abandoned
claimed: 2026-08-14T02:47Z
build: none
waiting_on:
evaluated: 2026-08-14T02:54:38Z
observed:
scenario:
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.h
---

## Goal

`UGSGA_SwordLight::ApplyMoveSpeedScale` caches `MaxWalkSpeed` and writes it back scaled:

```
GSGA_SwordLight.cpp:145    Move->MaxWalkSpeed = CachedMaxWalkSpeed * FMath::Max(Scale, 0.f);
GSGA_SwordLight.cpp:148    void UGSGA_SwordLight::RestoreMoveSpeed()
```

This is the exact pattern `CLAUDE.md` (§GAS gotchas) forbids: *"Never write `MaxWalkSpeed`
directly. Cache-and-restore cannot stack, and `AGSPlayerCharacter::OnStartCrouch` reassigns
`MaxWalkSpeedCrouched` out from under anyone holding a cached value. Apply a
`UGSGE_MoveSpeedScalar` with a SetByCaller magnitude and remove it by handle."*

The sanctioned mechanism is live and in use — `UGSCarryComponent` applies `UGSGE_MoveSpeedScalar`
against the replicated `MoveSpeedMultiplier` attribute (`GSCarryComponent.h:90-96`), and
`AGSPlayerCharacter::ApplyMoveSpeed` is the single place speed is derived from it. SwordLight
bypasses all of it.

**Two concrete failures this predicts** (neither yet observed — see below):

1. **Stacking.** Swing while carrying cargo. `GSCarryComponent`'s scalar and SwordLight's cached
   write are two authorities on one field; whichever restores last wins, and the carry penalty is
   either lost or made permanent.
2. **Crouch.** Swing while crouched, or crouch mid-swing. `OnStartCrouch` reassigns speed under
   the cached value, so `RestoreMoveSpeed` writes back a number that was never correct.

Not a hypothetical style point: #125 (light swings keep 85% of your speed) and #130 (guards get
their swing commitment back) both tuned `MoveSpeedScale` / `RecoveryMoveSpeedScale` without the
rule being applied, so the tuning currently sits on top of the wrong mechanism.

**How this was found.** Reading ACF's `actions-system` skill pack against our source while
verifying whether ACF's `GetPlayRate` was the equivalent of #125. It is not — #125 is movement
retention, not animation rate — but the comparison surfaced this. Incidental to that task, hence
its own ticket rather than a silent fix.

**Scope.** Replace the cache-and-restore in `ApplyMoveSpeedScale` / `RestoreMoveSpeed` with a
`UGSGE_MoveSpeedScalar` applied by handle and removed by handle, matching `GSCarryComponent`.
Per-stage `MoveSpeedScale` and `RecoveryMoveSpeedScale` values are tuned and must survive
unchanged — this is a mechanism swap, not a retune. `EndAbility` already calls `RestoreMoveSpeed`
on the cancelled path (`:539`); the handle must be released on exactly the same paths.

**Blocked on the build gate.** This is a C++ change and needs a compile, and #150 is still open.
Do not build until the queue clears.

**Observation plan.** `GS.Combat.Duel` alone will not exercise this — it is the wrong scenario.
It needs a swing while carrying cargo and a swing while crouched, watched on a horn-summoned
goblin and on the player, with speed read back after the swing ends.

## Generate

**This migration was already done once.** `GSGA_Block.h:51` reads *"Was a cached MaxWalkSpeed.
Cache-and-restore could not stack with the carry slow, and OnStartCrouch reassigned
MaxWalkSpeedCrouched out from under it - the slow is a [GameplayEffect]"*. Block was converted,
`UGSCarryComponent` was written that way from the start, and SwordLight was left behind. This
ticket is not a new design; it is finishing a sweep that missed one file.

### `GSGA_SwordLight.h`

- `CachedMaxWalkSpeed` (float) → `SwingSlowHandle` (`FActiveGameplayEffectHandle`).
- New `SwingSlowEffectClass` (`TSubclassOf<UGameplayEffect>`, `EditDefaultsOnly`), mirroring
  `UGSCarryComponent::CarrySlowEffectClass` so a weapon class can override the slow.
- Rewrote both method comments to record why caching is gone.

### `GSGA_SwordLight.cpp`

- Constructor: `SwingSlowEffectClass = UGSGE_MoveSpeedScalar::StaticClass();` beside the existing
  `DamageEffectClass` line. Added the `GSGE_MoveSpeedScalar.h` include.
- `ApplyMoveSpeedScale` builds a spec, sets `GSTags::Data_MoveSpeedScalar` to the stage's scale,
  and applies via `ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo,
  CurrentActivationInfo, Spec)`.
- `RestoreMoveSpeed` removes **by handle**, then clears it.

**Two behaviours preserved deliberately, both of which the old cache provided by accident:**

1. **No compounding across stages.** The old code cached once per activation so stage 2 would not
   scale stage 1's result (`0.55^3 = 17%`). The new code removes its own handle before each apply,
   so there is only ever one, and every stage scales the character's base speed. Same guarantee,
   reached without reading `MaxWalkSpeed`.
2. **`Scale >= 1.0` applies nothing.** `MoveSpeedScale`'s doc comment says *"1.0 disables the slow
   entirely"*. An effect multiplying by one would burn a handle and an attribute recalculation to
   change nothing, and would sit in the active-effects list during a playtest looking like a bug.

Tuned values are untouched: `MoveSpeedScale = 0.55`, `RecoveryMoveSpeedScale = 0.75`. This is a
mechanism swap, and #125/#130's tuning carries over unchanged.

## Evaluate

**Verified by grep, which is read-back, not observation.** After the change the only remaining
writes to `MaxWalkSpeed` in the whole module are the three sanctioned ones:
`GSCharacterBase.cpp:98` (BaseWalkSpeed capture), `GSCharacterBase.cpp:128` (the single derivation
from `MoveSpeedMultiplier`), and `GSPlayerCharacter.cpp:293` (`MaxWalkSpeedCrouched`). No line in
`GSGA_SwordLight.cpp` writes it any more; the only match left in that file is a comment.

**BUILD ATTEMPT 1 — FAILED at UnrealHeaderTool, 54s** (Michael ran it; the flag pair
`-Force -IgnoreQueue` is refused by the agent permission layer, so builds here are his to run).
One error, and it was mine:

```
GSGA_SwordLight.h(202): Error: BlueprintReadOnly should not be used on private members
```

`SwingSlowEffectClass` was given `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ...)` — the
specifier pattern copied from the public `FGSSwingStage` properties — while sitting in the
ability's private section. Fixed by dropping `BlueprintReadOnly`: nothing reads it from Blueprint,
it is a CDO knob, and the CDO stays editable through `EditDefaultsOnly` alone. Note UHT ran with
`-WarningsAsErrors`.

Worth recording that the failure was *cheap and loud* — UHT rejected it in 54 seconds before a
single translation unit compiled. The two risks flagged below survived that pass untested, because
UHT never got far enough to exercise them.

**BUILD ATTEMPT 2 — SUCCEEDED.** 6/6 actions, link clean, `UnrealEditor-GoblinSiege.dll` written
20:18:48 against sources last touched 20:10. Zero errors. Both risks flagged below are settled by
the compiler: the `ApplyGameplayEffectSpecToOwner` signature was correct as written, and the unused
`CharacterMovementComponent.h` include is harmless.

Two pre-existing C4996 warnings surfaced (`GSGA_Block.cpp:24`, `GSGA_Interact.cpp:20` — deprecated
`UGameplayAbility::AbilityTags`). Neither is from this change and neither is in this claim; they
are the debt CLAUDE.md already records as "compiles in 5.8, will not after the next upgrade".
Worth its own ticket, not a silent fix inside this one.

**STILL NOT OBSERVED — the ticket cannot close on this.** A successful compile is precisely the
evidence this project has learned not to trust. Nobody has yet swung while carrying a sack or
swung while crouched. See the observation plan in the Goal.

**Risks the build settled** (kept for the record):

- `ApplyGameplayEffectSpecToOwner` is a `const` member of `UGameplayAbility` returning
  `FActiveGameplayEffectHandle`; the signature is used from memory of the engine API, not verified
  against 5.8 headers.
- `GameFramework/CharacterMovementComponent.h` is now an unused include in the .cpp — `ApplyLunge`
  uses `ACharacter::LaunchCharacter`, and nothing else reaches for the movement component. Left in
  place rather than removed, because an IWYU trim I cannot compile is a gratuitous risk.

**NOT observed, and this is the gate.** Both predicted failures in the Goal are still predictions.
Nobody has swung while carrying a sack, and nobody has swung while crouched. `GS.Combat.Duel` does
not exercise either. Until someone watches those two, the claim "this fixes a stacking bug" is
unproven - the fix is *correct by construction*, which is exactly the kind of confidence this
project has been burned by.

**Owed to AGENT_STATE:** the MoveSpeedMultiplier migration was done for Block and Carry and missed
SwordLight; when a rule gets a mechanism, the sweep needs to cover every existing violator or the
rule quietly becomes advisory.

## REVERTED 2026-08-14 — regression reported

Michael played the build and reported **NPCs not attacking each other, and "a lot of the animations
broken"**, in **both** scenarios he tried, and confirmed **it was working before this build**.

Both this ticket's and #152's changes were reverted to HEAD (`git checkout --` on all four files)
before any further diagnosis. Working state first, diagnosis second. The diff is preserved at
`<scratchpad>/151-152-combat-changes.patch` (198 lines) and reapplies cleanly.

**What this ticket could plausibly have caused:** the swing slow leaking. `UGSGE_MoveSpeedScalar`
is Infinite and removed by handle, so any exit path that skips `RestoreMoveSpeed` leaves a pawn
permanently at 0.55 speed - which reads exactly as "never closes, never attacks". The old
cache-and-restore wrote `MaxWalkSpeed` directly and would have been re-derived by
`AGSCharacterBase::ApplyMoveSpeed` anyway, so it was *self-healing* in a way the attribute route is
not. That asymmetry was not considered when the swap was written and is the leading suspect.

**What it could NOT have caused:** the animation half. Nothing in these four files touches montages,
skeletons, AnimBPs, retargeting or notifies.

**CONFIRMED 2026-08-14, after revert + rebuild: this ticket caused it.** Michael reports NPCs are
attacking each other again. The discriminator was clean - one variable removed, symptom gone - so
the "not attacking" half is attributable to this change and not to #152, and not to animation.

**The defect, stated precisely.** The old code wrote `MaxWalkSpeed` directly, and
`AGSCharacterBase::ApplyMoveSpeed` re-derives that same field from the `MoveSpeedMultiplier`
attribute. A leaked slow under the old mechanism was therefore **self-healing**: the next
derivation overwrote it. The replacement applies an **Infinite** `UGSGE_MoveSpeedScalar` removed
only by handle, so any exit path that skips `RestoreMoveSpeed` pins that pawn at 0.55 permanently
with nothing to correct it. The swap traded a self-healing mechanism for one that is not, and that
asymmetry was never considered - the Generate section argued "correct by construction" on the
strength of no-compounding alone.

This is the substantive lesson, and it belongs in AGENT_STATE: **a mechanism that is more correct
in principle can be less forgiving in practice.** Cache-and-restore was wrong the way CLAUDE.md
says, but its wrongness was transient. The attribute route is right, and its wrongness is
permanent. Any reapplication has to prove removal on EVERY exit path - not just the ones EndAbility
covers - or use a finite duration as a backstop so a leak self-clears.

**Still unexplained and NOT attributable to this ticket:** the broken animations, and (as of the
same rebuild) the goblin horde still not attacking while everyone else does. Neither reverted with
this change. Both are separate threads.

**Bisect order when reapplying:** #152 first (two constructor lines, small blast radius), rebuild,
watch. Then #151. Reapplying both at once is what made this ambiguous in the first place.

## Refine

**Changed after self-review:** added the `Scale >= 1.0` early-out. The first pass applied an
effect unconditionally, which would have put a no-op 1.0 multiplier on every character whose stage
disables the slow - harmless numerically, misleading in a playtest.

**Reconsidered and kept:** removing and re-applying the handle on every stage boundary, rather than
holding one effect and mutating its SetByCaller magnitude. Mutating an active effect's magnitude is
possible but fiddly and easy to get wrong under replication; remove-then-apply is dumber and its
failure mode (a frame at base speed) is invisible at these durations.

**Deliberately left undone:**

- **The unused include.** See above - it needs a compile to remove safely.
- **No sweep of the other abilities.** `GSGA_DodgeRoll`, `GSGA_BowShot` and `GSGA_TorchToss` were
  not audited for the same violation. The grep above says none of them writes `MaxWalkSpeed`
  today, so there is nothing outstanding, but I did not read them.
- **`ApplyMoveSpeedScale` is still called from the same four sites.** The call graph is unchanged
  on purpose, so if behaviour differs after the build, the mechanism is the only variable.
