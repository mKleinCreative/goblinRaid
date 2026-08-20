---
id: 152
title: Retire deprecated AbilityTags in GSGA_Block and GSGA_Interact (C4996)
agent: claude-assettags
status: abandoned
claimed: 2026-08-14T03:19Z
build: none
waiting_on:
evaluated: 2026-08-14T03:21:03Z
observed:
scenario:
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_Block.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_Interact.cpp
---

## Goal

The #151 build surfaced the last two `UGameplayAbility::AbilityTags` writes in the project:

```
GSGA_Block.cpp(24,2):    warning C4996: 'UGameplayAbility::AbilityTags': Use GetAssetTags()...
GSGA_Interact.cpp(20,2): warning C4996: 'UGameplayAbility::AbilityTags': Use GetAssetTags()...
```

Warnings today, hard errors after the next engine upgrade — the deprecation text says so outright
("otherwise your project will no longer compile"), and CLAUDE.md already records it.

**Another migration that missed two files.** `UGSGA_Horn` uses `SetAssetTags` and says why;
`GSGA_BowShot.cpp:25` explicitly notes *"GSGA_Block and GSGA_Interact still use the old"*. So the
correct form, the reasoning and even a written record of the two stragglers were all already in
the repo. This is the same shape as #151 (`MaxWalkSpeed`), found the same way, one build apart.

**What must not break.** In both files the asset tag is load-bearing, not decorative:
`UAbilitySystemComponent::CancelAbilities` filters on asset tags, and both comments say so. It is
what lets a guard break cancel a channel, and what lets releasing block drop the guard, without
`CancelAbilities(nullptr)` taking every ability the victim has. `UGSGA_SwordLight`'s blocked-swing
cancel (#114/#151) calls `CancelAbilities(&CancelTags)` with exactly `State.Blocking` and
`State.Interacting`, so if these tags stop being asset tags, that cancel silently stops working.

## Generate

Both constructors, identical change, copied from `UGSGA_Horn.cpp:20-24`:

```cpp
FGameplayTagContainer AssetTags;
AssetTags.AddTag(GSTags::State_Blocking);      // State_Interacting in GSGA_Interact
SetAssetTags(AssetTags);
```

`Set` rather than add is safe in both cases because each ability carries exactly one asset tag —
checked, not assumed. If either ever gains a second, this becomes a read-modify-write.

Existing comments kept and amended: "ability tag" → "ASSET tag" (the accurate name, and the thing
`CancelAbilities` filters on), plus the C4996 reasoning and a pointer to `UGSGA_Horn` as the form
to copy. No include changes needed — all three files already pull `Combat/GSGameplayTags.h`, and
`GSGA_Horn.cpp` compiles this exact pattern with a strictly smaller include set.

## Evaluate

**BUILD — SUCCEEDED, clean.** `Result: Succeeded` in 38.5s; `UnrealEditor-GoblinSiege.dll` written
20:24:58. **Zero C4996 in the log** — both warnings gone — and zero warnings or errors of any other
kind. The old form now has no call sites left in the module, which is the actual definition of this
migration being done.

**Not observed, and the observation is not optional.** The asset tags are load-bearing for
cancellation, and the failure mode if `SetAssetTags` does not populate the same container
`CancelAbilities` reads is *silent*: no warning, no error, just a guard that never breaks and a
block that never drops on release. The compiler cannot see that. Two things must be watched:

1. **Release block while a swing is in flight** — the guard drops, the swing survives.
2. **Kick a blocking defender** — the guard break rips the block open (the #114/#117 path).

A duel is the right scenario for these two, unusually — this is ability-tag plumbing, not AI.

**Touched outside the claim:** nothing.

**Owed to AGENT_STATE:** second time in one session that a completed migration turned out to have
missed exactly two files, with the stragglers already named in a comment nobody acted on
(`GSGA_BowShot.cpp:25` for this one). A migration is not done when the new form exists; it is done
when the old form has no call sites left. Worth a grep-for-the-old-form step at the end of any
mechanism swap.

## REVERTED 2026-08-14 — see #151

Reverted alongside #151 after Michael reported NPCs not attacking and broken animations in both
scenarios, working before this build. Full record and bisect plan in #151. Diff preserved at
`<scratchpad>/151-152-combat-changes.patch`.

**This ticket is the LESS likely culprit but must be cleared first anyway**, because it is two
constructor lines against #151's mechanism swap. One caveat that makes it non-zero: `SetAssetTags`
*replaces* the asset tag container, where `AbilityTags.AddTag` added to it. That is equivalent only
if nothing else populates asset tags on these abilities - including a Blueprint child CDO. Not
checked before the change went in. If a BP subclass of either ability carries its own asset tags,
this wiped them, and cancellation would fail silently in exactly the way the Evaluate section
predicted.

## Refine

**Reconsidered and rejected — leaving these alone as "just warnings".** They compile today, and
touching working combat plumbing has a real cost. Rejected because the deprecation is explicit
that the next upgrade breaks it, the correct form was already in the repo, and a comment naming
these two files had been sitting unactioned. The cost of doing it now is one build; the cost of
doing it during an engine upgrade is debugging cancellation while nothing compiles.

**Deliberately left undone:** no audit of the other deprecation warnings in the build. This ticket
is scoped to C4996 on `AbilityTags`, which is the one CLAUDE.md calls out and the one with a
stated deadline.
