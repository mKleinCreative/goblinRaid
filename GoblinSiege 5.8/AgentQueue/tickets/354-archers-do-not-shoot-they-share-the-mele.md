---
id: 354
title: Archers do not shoot: they share the melee-only combat behaviour, so give them native Actions.Defender.* tags, a ranged behaviour asset and their own controller
agent: claude-combat
status: done
claimed: 2026-08-29T05:44Z
build: none
waiting_on:
evaluated: 2026-08-29T05:49:52Z
observed: 2026-08-29T05:49:36Z | Erika shoots again. With BP_ErikaArcher on the new BP_GSAIController_Archer and DA_GS_CombatBehaviour_Archer, SetTarget took her to AIState.Combat and the blackboard settled on CombatState 2 (ERangedCombat) at 510uu; the damage log then showed three arrows from BP_ErikaArcher_C_2 hitting the player for 20.0 each, roughly 3.3s apart, before any hand-triggered shot. CanExecuteAbility(Actions.Defender.Ranged) and TriggerAction both returned True; the one False was the fire-rate limiter refusing a shot right after her own, which is correct. An earlier 'zero GSArrowProjectile actors' reading was a false negative - arrows self-destroy after 5s, so a count between shots reads zero while she is firing.
scenario: PIE on L_CombatArena; Erika placed 700uu in front of the possessed player with GS.Combat.Debug and GS.Combat.LogDamage on, driven into combat via SetTarget, then the BT left to tick for ~6s before reading state and the log.
files: 
  - Source/GoblinSiege/Combat/GSGameplayTags.h
  - Source/GoblinSiege/Combat/GSGameplayTags.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.cpp
  - Content/Data/AI/DA_GS_CombatBehaviour_Archer.uasset
  - Content/Data/AI/DA_GS_AbilitySet_Defender.uasset
  - Content/AI/BP_GSAIController_Archer.uasset
  - Content/Blueprints/Adversaries/BP_ErikaArcher.uasset
---

## Goal

Archers do not shoot: they share the melee-only combat behaviour, so give them native Actions.Defender.* tags, a ranged behaviour asset and their own controller

## Generate

**Cause - a regression I caused in #343, and flagged there as an accepted risk.** Every defender
shares `BP_GSAIController_Militia`, and `CombatBehaviour` lives on the controller. So when #343 gave
that controller `DA_GS_CombatBehaviour_Defender` (melee only: MeleeCombat / ChaseTarget /
StudyTarget, `E_MELEE`), Erika inherited it. Measured live: she was configured to swing
`GA_HU_SwordLight` - a sword she does not carry - with **no `E_RANGED_COMBAT` state at all**, so
nothing could ever trigger `GSGA_BowShot`. Her real ranged behaviour (standoff band, 0.8s draw
telegraph) lived in `BT_Archer`, orphaned by #330 and now actively refused by #347's guard.

**Native action tags** (`GSGameplayTags.h/.cpp`): `Actions.Defender.Melee` and
`Actions.Defender.Ranged`. #343 reused `State.Attacking` as the melee action tag because it was the
only registered tag the sword already carried - a stopgap. `State.*` describes what a character IS
DOING; these describe what an AI may be TOLD TO DO, and conflating them is precisely why the bow
could never be addressed (there was no second `State.*` to borrow). ACF's sample uses an `Actions.*`
namespace but declares those tags only in the sample project, so `Actions.Attack` resolves to
nothing here.

**Both abilities stamped via `SetAssetTags`** (the non-deprecated path `GSGA_Horn` already uses):
- `GSGA_SwordLight` carries `State.Attacking` + `Actions.Defender.Melee`.
- `GSGA_BowShot` carries `Actions.Defender.Ranged`. It previously had NO asset tags on purpose - its
  own comment explains that borrowing `State.Aiming` would let a `CancelAbilities(State.Aiming)` kill
  a shot in flight. An `Actions.*` tag that nothing cancels by keeps that reasoning intact.

**Data:**
- `DA_GS_AbilitySet_Defender` - now two `FAbilityConfig`s: melee -> `GA_HU_SwordLight_C`, ranged ->
  `GSGA_BowShot`.
- `DA_GS_CombatBehaviour_Defender` - melee action moved from `State.Attacking` onto the real tag.
- **`DA_GS_CombatBehaviour_Archer` (new)** - `E_RANGED`, default state `ERangedCombat`. States
  ordered because `GetBestCombatStateByTargetDistance` returns the FIRST whose condition passes
  (`ACFCombatBehaviourComponent.cpp:206-218`): melee only below 250 (target on top of her), ranged
  below 1100 (the old `BT_RangedAttack` MaxRange), chase above. Ranged fires
  `Actions.Defender.Ranged` with `BTWaitTime 1.5` (~ the old `RangedAttackCooldownSeconds`).
- **`BP_GSAIController_Archer` (new)** - parent `GSAIControllerBase`, mirrors the militia
  controller exactly (same `BT_Defender`, same `LocomotionStateByAIState`, same `DefaultState`);
  only the combat behaviour differs. `BP_ErikaArcher.AIControllerClass` -> this.

**Two things verified BEFORE the build so they could not bite after:** `DA_Weapon_Erika` has no
`ArrowItemClass`, and the code's own rule is *"null means infinite, not broken"* - so the ammo gate
in `CanActivateAbility` cannot silently block an AI archer. And an AI aims via `GetControlRotation()`
(`FireArrow`), which `UGSAISteeringComponent` drives toward the target.

## Evaluate

**Observed:** Erika shoots. On the new controller and ranged behaviour she reached CombatState 2 (ERangedCombat) at 510uu and the damage log recorded three arrows from BP_ErikaArcher_C_2 hitting the player for 20.0 each, ~3.3s apart, before any hand-triggered shot. CanExecuteAbility and TriggerAction on Actions.Defender.Ranged both True. An earlier zero-arrow-actor count was a false negative: arrows self-destroy after 5s, so counting between shots reads zero while she fires - the damage log was the right instrument.

**A trap that cost one failed run, recorded so it is not rediscovered:** `GA_HU_SwordLight` is a
Blueprint child, and it carries its OWN serialized `ability_tags = [State.Attacking]`, which masks
the C++ `SetAssetTags` default. The C++ CDO had both tags; the BP CDO still had one. So the tag had
to be stamped onto the BP CDO too. **A `SetAssetTags` in C++ does not reach a Blueprint child that
has ever saved its own tag container.** `GSGA_BowShot` has no BP child, which is why it worked
first time.

**The MCP server does not reconnect on its own** after the editor it was attached to dies.
Verified the project note: VibeUE serves HTTP MCP on `127.0.0.1:8000/mcp`; POST `initialize`, carry
`Mcp-Session-Id` on every call. Sessions also expire - one went silent between calls and had to be
re-initialised. Every asset in this ticket was authored over that raw HTTP path.

**Two editor-shutdown facts, both cost time:** `CloseMainWindow()` stalled for minutes with the
window still up (a swallowed message, memory still rising); `QUIT_EDITOR` from inside worked
immediately. And quitting an editor launched from a `.uproject` **auto-relaunched a fresh one**
(PID changed, started after the quit), which then blocked the build - it had to be quit a second
time and confirmed to stay closed.

**Touched outside the claim:** `GSGA_SwordLight.cpp` (the melee asset tag) and
`DA_GS_CombatBehaviour_Defender` (retagged). Both are the same change from the other side and were
claimed under #353 / authored under #343 by the same agent.

**AGENT_STATE.md owes:** DECISION - *AI attacks are addressed by native `Actions.Defender.*` tags,
resolved to abilities through the ACF ability set; `State.Attacking` is no longer an action tag.
Archers run their own controller because ACF's combat behaviour lives on the controller.*

## Refine

**Changed in response to my own evaluation:** the authoring script originally sourced the melee tag
FROM the sword Blueprint - circular, and it failed the moment the BP masked the C++ tag. Re-sourced
from the C++ CDO, which is authoritative, and the BP became a patch target rather than a source.

**Deliberately left undone:**

1. **The player's bow is untouched.** `AGSPlayerCharacter` activates `GSGA_BowShot` by class, not by
   tag, so the new asset tag is inert for the player - but it is now on the ability, and any future
   `CancelAbilities` that matches `Actions.*` would reach a player shot. Nothing does today.
2. **Erika's stand-off is data, not observed feel.** 250 / 1100 are the old BT numbers; whether she
   kites well under ACF's `StudyTarget`-less config is a playtest question.
3. **Heavy, block and guard-break still have no action tags**, so defenders still have exactly the
   verbs the ability set names. The same pattern (native tag + `SetAssetTags` + BP-CDO stamp +
   ability-set row) extends to each; noted in #343 as the follow-up and unchanged here.
4. **The `Invalid Action Ability Tag` warning persists** (#352 item 2) - ACF's own unset
   `EngagingAction`, benign, left alone on purpose.
