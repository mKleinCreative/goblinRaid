---
id: 343
title: Defenders cannot attack: ACF CombatBehaviour unconfigured after #330 (BT_Militia/BT_Archer orphaned)
agent: claude-combat
status: done
claimed: 2026-08-28T23:26Z
build: none
waiting_on:
evaluated: 2026-08-29T00:37:18Z
observed: 2026-08-29T00:34:18Z | Michael watched defenders swing at him in PIE after the fix - the regression is closed. He reports the swing reads wrong: they close to almost touching before attacking and the animation does not look like a full swing. Attacks now fire; their range and montage presentation need a follow-up.
scenario: PIE on L_Tutorial_Island after saving DA_GS_CombatBehaviour_Defender, DA_GS_AbilitySet_Defender and the three DA_Char_* ability-set assignments; Michael walked the player up to a militiaman and watched it attack.
files: 
  - Config/DefaultGameplayTags.ini
  - Content/Blueprints/Abilities/Human/GA_HU_SwordLight.uasset
  - Content/Blueprints/Abilities/Human/GA_HU_SwordHeavy.uasset
  - Content/Blueprints/Abilities/Human/GA_HU_Block.uasset
  - Content/Blueprints/Abilities/Human/GA_HU_GuardBreak.uasset
  - Content/AI/BP_GSAIController_Militia.uasset
  - Content/Data/AI/DA_GS_CombatBehaviour_Defender.uasset
---

## Goal

Defenders cannot attack: ACF CombatBehaviour unconfigured after #330 (BT_Militia/BT_Archer orphaned)

## Generate

**Root cause.** #330 moved militia onto ACF's `BT_Defender`. Verified in the editor that ALL
combatant adversaries now use `BP_GSAIController_Militia` -> `BT_Defender` -> (via
`BTTask_RunBehavior.behavior_asset`) `ACFCombatBT`: `BP_CastleGuard01/02`, `BP_ErikaArcher` AND
`BP_KnightDPelegrini`. Only `BP_PeasantMan` differs. `behavior_tree` is `None` on all four
`DA_Race_Human` rows, so `AGSAIControllerBase::OnPossess`'s `RunBehaviorTree` never fires and
`BT_Militia` / `BT_Archer` are orphaned - and with them every GS `UBTTask_MeleeAttack`,
`UBTTask_Block`, `UBTDecorator_HasAttackToken` and `UBTTask_MenaceOrbit` in the defender path.

`ACFCombatBT` does contain ACF's own `BTTask_MeleeAttack_C`, so the tree was not the blocker. The
blocker was one property: `UACFCombatBehaviourComponent::CombatBehaviour` was **None** on every
defender controller, and `TryExecuteActionByCombatState` opens with
`if (!InternalCombatBehaviour) { return false; }` (`ACFCombatBehaviourComponent.cpp:90-93`) - no
log, no warning.

**Files changed (all data; no C++, no build):**

- `Content/Data/AI/DA_GS_CombatBehaviour_Defender.uasset` **(new)** - `UACFCombatBehaviorDataAsset`.
  `ActionByCombatState[EMeleeCombat]` -> one `FActionChances` with `ActionTag = State.Attacking`,
  `Weight 1.0`, `BTWaitTime 1.2`, `Priority EMedium`, `AllowedFromDirection EveryDirection`,
  `bRequiresTicket false`. `CombatStatesConfig` = EMeleeCombat/EWalk/100, EChaseTarget/EJog/100,
  EStudyTarget/EWalk/50. `bNeedsWeapon = false`. `DefaultCombatState = EMeleeCombat`.
- `Content/Data/AI/DA_GS_AbilitySet_Defender.uasset` **(new)** - `UACFAbilitySet`, one
  `FAbilityConfig`: `TriggeringTag = State.Attacking` -> `GA_HU_SwordLight_C`, level 1.
- `Content/AI/BP_GSAIController_Militia.uasset` - `CombatBehav.CombatBehaviour` assigned.

(`DA_Char_{Militia,Knight,Archer}` are the other half of the fix and are recorded under #344.)

**Three design choices, each with a reason:**

1. **`State.Attacking`, not `Actions.Attack`.** ACF's shipped `DA_ACFMeleeCB` uses
   `Actions.Attack` / `Actions.HeavyAttack`, but those tags are declared **nowhere** in the plugin -
   neither natively nor in its `Config` (confirmed independently by goblinsiege-5-8-07 against the
   uncooked copy at `D:\ACF_4.4_replaced_20260820`); they live in the sample project we do not have.
   The same defect is visible in ACF's own `ACFGASDamageCalculatorBP`, which the log reports as
   *"Invalid GameplayTag Actions.HeavyHit"*. `GA_HU_SwordLight` already carries
   `ability_tags = State.Attacking`, and no other defender ability shares it, so it uniquely selects
   the light attack. **This avoided declaring new tags entirely - no ini edit and no editor restart.**
2. **`bNeedsWeapon = false`.** `CheckEquipment()` returns true (and aborts the attack) whenever
   `bNeedsWeapon` is set and the pawn's `CombatType != EMelee`. Every GS weapon resolves to
   `EUnarmed` because all six `BP_ACFWeapon_*` derive base `AACFWeaponActor` rather than
   `AACFMeleeWeaponActor`, which `AACFCharacter::HandleEquipmentChanged` type-tests
   (`ACFCharacter.cpp:275-281`). With `bNeedsWeapon` true the defenders would have aborted forever.
   That base-class bug is real and left unfixed - see Refine.
3. **`bRequiresTicket = false`,** dropping the one field ACF's reference asset sets. Tickets need
   `UACFAIManagerComponent`, which `gs-attacker-rationing` measures as strictly worse than our
   `UGSEngagementComponent` (`MaxAttackersPerTarget = 1` by default, ticket equality on controller
   only, `ReleaseTicket` called from nowhere).

## Evaluate

**Verified by runtime measurement, not read-back.** Each gate was probed live in PIE:

| Gate | Evidence |
|---|---|
| Teams hostile | `AreEnemyTeams(guard, player) == True`; guard `Teams.Human`, player `Teams.Goblin` |
| ACF targeting | `AACFAIController::GetTarget()` returned the player / a horde goblin on 8 controllers |
| `CombatBehaviour` | **was `None` on all 9 controllers** - the fault |
| `CanExecuteAbility(State.Attacking)` | `False` before; **`True`** on all three living defenders after `GrantAbilitySet` |
| `TriggerAction(State.Attacking)` | returned **`True`** on living defenders (`False` on dead ones, correctly blocked by `State.Dead`) |

**Observed before:** Michael confirmed by eye that defenders never attacked; measured alongside it,
10 horde goblins sat at full 40 HP and the player at 100 while two guards were killed.
**Observed after:** Michael watched defenders swing at him.

**Why a plain `UGameplayAbility` works here** (this refines `gs-abilities-outside-acf-asc` and is the
load-bearing finding): `UACFAbilitySystemComponent::GetAbilityHandle` matches on
`AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(actionTag)` (`:466`), and `GrantAbilitySet`
stamps those tags **regardless of the ability's base class**. `CanExecuteAbilityByHandle` only
demands `IsFullyInit()` when the ability *is* a `UACFGameplayAbility` (`:412`). So the rule is not
"plain abilities cannot work with ACF" but **"abilities cannot work when granted without tags"** -
our own `GrantIfSet` stamps nothing, an ACF ability set does.

**Written but never run:** nothing. Every changed field was exercised in PIE.

**Two things I got wrong mid-investigation, corrected here so the record is clean:**

- I read a `GameplayTag`'s Python repr `{}` as "empty" and briefly concluded the AI state was unset.
  It is not - `get_tag_name()` shows `AIState.Patrol`, and `AIState.Wait` only on dead pawns
  (`HandlePawnDeath`). A `GameplayTag` prints `{}` whether or not it holds a value.
- I first described the impact/FX work as "a ten-line call". It is not: the melee sweep is an
  overlap that fabricates its `FHitResult` from the target's actor location
  (`GSGA_SwordLight.cpp:430-442`), so per-material impact FX cannot be placed correctly until that
  becomes a real trace.

**Touched outside the goal:** nothing. Four files I claimed were never edited - three `GA_HU_*`
ability Blueprints and `Config/DefaultGameplayTags.ini` - because choosing an already-registered tag
made them unnecessary. They are listed in `files:` as claimed, not changed.

**AGENT_STATE.md owes:** DECISION - *defender AI attacks are driven by ACF's
`UACFCombatBehaviourComponent` reading a GS-authored `UACFCombatBehaviorDataAsset`, with GS's plain
gameplay abilities reached through a `UACFAbilitySet` keyed on `State.Attacking`.*

## Refine

**Changed in response to my own evaluation:** the first `CombatStatesConfig` held only
`EMeleeCombat`. `EvaluateCombatState` returns false for any state absent from that array
(`ACFCombatBehaviourComponent.cpp:294-301`), which would have blocked the chase and study
transitions, so `EChaseTarget` and `EStudyTarget` were added to mirror ACF's shipped
`DA_ACFMeleeCB`.

**Deliberately left undone, and why:**

1. **The swing reads wrong.** Michael's own words: defenders *"close to almost touching before
   attacking and it doesn't look like a full swing."* Both halves are explained and neither is this
   ticket's fault. The range is ACF's - `TargetActorDistance` is measured **negative** (-9.7 to
   -31.7 uu observed), i.e. capsule-to-capsule, so ACF's melee decorator lets them close until they
   overlap; GS's own `UBTTask_MeleeAttack` used `AttackRange = 250` between origins instead. The
   clipped animation is the known montage problem: `AM_HU_Atk_Light` plays on the **UpperBody** slot
   at `MontagePlayRate 1.5`, and #184 corrected contact-vs-damage-window timing for the goblin
   stages **but never the human ones**. This needs its own ticket; it is a presentation and range
   defect, not the regression.
2. **Only the light attack is wired.** `GA_HU_SwordHeavy`, `GA_HU_Block` and `GA_HU_GuardBreak`
   carry no distinct asset tag (Heavy and GuardBreak have empty `ability_tags`), so they cannot be
   addressed until tags are declared. Defenders therefore do not block or guard-break today. That
   costs a `Config/DefaultGameplayTags.ini` edit plus an editor restart, which is why it is not in
   this pass.
3. **The `AACFWeaponActor` base-class bug is unfixed.** Every GS weapon resolving to `EUnarmed` is
   worked around here with `bNeedsWeapon = false` rather than corrected, because reparenting the six
   weapon item/actor Blueprints is a separate change with its own blast radius.
4. **`BT_Militia` and `BT_Archer` remain orphaned,** and with them the engagement token/ring-slot
   system on the defender side - `UGSEngagementComponent` is currently exercised only by allied
   goblins attacking defenders. Whether to port those GS nodes into the ACF path or retire them is a
   design decision for Michael, not a bug fix.
5. **Not verified: the archer.** `BP_ErikaArcher` now runs the melee combat behaviour like everyone
   else; its ranged standoff band and 0.8s draw telegraph lived in the orphaned `BT_Archer` and are
   not running. No archer was observed shooting.
