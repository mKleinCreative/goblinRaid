---
id: 114
title: A blocked swing must actually be cancelled: SwordLight needs State.Attacking as an ASSET tag
agent: claude-gobkit
status: done
claimed: 2026-08-10T05:57Z
build: none
waiting_on:
evaluated: 2026-08-10T06:01:19Z
files: 
  - Content/Blueprints/Abilities/GA_GS_SwordLight.uasset
  - Content/Blueprints/Abilities/Human/GA_HU_SwordLight.uasset
---

## Goal

A blocked swing must actually be cancelled: SwordLight needs State.Attacking as an ASSET tag

## Generate

Top asset-only finding from the combat bug-hunt workflow, picked by Michael.

**`AGSCharacterBase::NotifyAttackWasBlocked` cancels the attacker's swing by tag** -
`GSCharacterBase.cpp:192-194` builds a container holding `State.Attacking` and calls
`ASC->CancelAbilities(&CancelTags)`. **`CancelAbilities` filters on an ability's ASSET tags**, and
`UGSGA_SwordLight` declared `State.Attacking` only in `ActivationOwnedTags` - its asset-tag container
was empty. So the cancel matched nothing and the blocked swing carried on to its damage frames.

`GA_GS_Block` and `GA_GS_Interact` both duplicate their tag into `AbilityTags`, with a comment saying
that is what makes it work. SwordLight is the one that never got it.

`AbilityTags` is `EditDefaultsOnly`, so this is data: set on **`GA_GS_SwordLight`** (player and horde)
and **`GA_HU_SwordLight`** (the human variant from #112). No build.

## Evaluate

**The data change is verified from freshly loaded assets, not the objects I wrote:**

```
GA_GS_SwordLight   ability_tags = (GameplayTags=((TagName="State.Attacking")))
GA_HU_SwordLight   ability_tags = (GameplayTags=((TagName="State.Attacking")))
both compile BS_UP_TO_DATE
```

Matches `GA_GS_Block`'s existing `(GameplayTags=((TagName="State.Blocking")))` exactly, which is the
shape known to work.

**THE RUNTIME EFFECT IS NOT DIRECTLY OBSERVED, and I am not going to claim it is.** A fight ran with
**8 `block LANDED - X is open, dropping guard`** lines, so the block -> recoil path fires - but that
line was already appearing before this change, so it proves the notify ran, not that the swing was
cancelled. `GS.Combat.LogDamage 1` produced **zero** `[GS.Damage]` lines in that PIE session despite 8
blocks landing, so I could not check the thing that would settle it: whether a blocked attacker still
lands its hit immediately afterwards. The cvar not taking is its own small mystery, unexamined.

What the claim rests on instead: the asset tag is present and correct, `CancelAbilities` is documented
and was read in engine source to filter on asset tags, and two other abilities in this project already
depend on that exact mechanism.

## Refine

**Set the tag on the HUMAN variant too.** `GA_HU_SwordLight` is a duplicate made in #112 and inherits
nothing from the goblin original, so fixing only `GA_GS_SwordLight` would have left every human
defender still swinging through blocks - which is most of the fights being watched.

**Did not switch the C++ to `SetAssetTags()`.** It is the non-deprecated route and the ability header
already carries a C4996 warning, but that is a build for a change the data path makes today.

**Left undone, and it is the pair to this one:** bug #4 from the hunt - `BTTask_MeleeAttack:46` grants
the punish on `IsRecoiling()` and `:78` then refuses it because `CanBeAttacked()` is false on exactly
that state. Measured 7 blocks, 0 counters. Until that is fixed the blocked attacker is interrupted but
still cannot be punished, so half the exchange is visible.
