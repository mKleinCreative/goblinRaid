---
id: 344
title: Defender ability set: humans need an ACF AbilitySet so CanExecuteAbility resolves (supplement to 343)
agent: claude-combat
status: done
claimed: 2026-08-28T23:43Z
build: none
waiting_on:
evaluated: 2026-08-29T00:37:19Z
observed: 2026-08-29T00:41Z | Measured CanExecuteAbility(State.Attacking) flip from False to True on three living defenders the moment DA_GS_AbilitySet_Defender was granted, and Michael then watched militia swing at him in PIE with the set assigned via DA_Char_Militia/Knight/Archer.
scenario: PIE on L_Tutorial_Island: probed the ability handle on live BP_CastleGuard01/02 pawns before and after GrantAbilitySet, then Michael walked the player up to a militiaman after the assets were saved.
files: 
  - Content/Data/Characters/DA_Char_Militia.uasset
  - Content/Data/Characters/DA_Char_Knight.uasset
  - Content/Data/Characters/DA_Char_Archer.uasset
  - Content/Data/AI/DA_GS_AbilitySet_Defender.uasset
---

## Goal

Defender ability set: humans need an ACF AbilitySet so CanExecuteAbility resolves (supplement to 343)

## Generate

#343 gave the defenders a `UACFCombatBehaviorDataAsset`, but that alone still could not fire an
attack. `UACFFunctionLibrary::ShouldExecuteAction` (`ACFFunctionLibrary.cpp:375-378`) gates every
candidate action on `GetActionsComponent()->CanExecuteAbility(action.ActionTag)`, and unlike
`TriggerAction` that call has **no fallback** to `TryActivateAbilitiesByTag`. It resolves a handle
via `GetAbilityHandle`, which matches on
`AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(actionTag)`
(`ACFAbilitySystemComponent.cpp:466`).

Goblin Siege grants its combat abilities through `AGSCharacterBase`'s own `GrantIfSet`
(`GSCharacterBase.cpp:710-712`), which stamps **no** dynamic source tags. So every defender ability
was granted and activatable by class, yet invisible to every tag-addressed ACF call.

**Files changed:**

- `Content/Data/AI/DA_GS_AbilitySet_Defender.uasset` - `UACFAbilitySet` with one `FAbilityConfig`
  in `Abilities` (not `ActionAbilities`): `TriggeringTag = State.Attacking`,
  `GameplayAbility = GA_HU_SwordLight_C`, `AbilityLevel = 1`.
- `Content/Data/Characters/DA_Char_Militia.uasset`, `DA_Char_Knight.uasset`,
  `DA_Char_Archer.uasset` - `DefaultAbilitySet` repointed from `DA_AbilitySet_GS_Empty` to the new
  set.

**Why a new set rather than filling the existing one.** `DA_AbilitySet_GS_Empty` is empty and
**shared by all five** character data assets - Militia, Knight, Archer, Player and HordeGoblin.
Populating it with `GA_HU_SwordLight` would have handed the human sword ability to the player and to
allied goblins. The three human rows now point at a human-specific set; Player and HordeGoblin still
point at the empty one and are unchanged.

`Abilities` was used rather than `ActionAbilities` because `FAbilityConfig` takes a plain
`TSubclassOf<UGameplayAbility>` (`ACFRPGTypes.h:80-96`), which is exactly what our abilities are.
`FActionAbilityConfig` would have required migrating them to `UACFActionAbility` first.

## Evaluate

**Verified by measurement.** On three living `BP_CastleGuard01/02` pawns in PIE,
`CanExecuteAbility(State.Attacking)` returned `False` before the grant and **`True`** immediately
after `GrantAbilitySet(DA_GS_AbilitySet_Defender)` - the exact gate `ShouldExecuteAction` consults.
Michael then observed militia swinging at him once the assignment was saved onto the character data
assets. Assets confirmed on disk (all six of #343+#344 saved at 16:49, editor reports zero dirty
packages).

**This corrects a widely-held reading of `gs-abilities-outside-acf-asc`.** A peer session
(goblinsiege-5-8-07) predicted that plain `UGameplayAbility` classes could not be driven by ACF
action tags at all, and initially recommended adding GS's `UBTTask_MeleeAttack` into `BT_Defender`
instead. The measurement above refutes that, and the peer has since verified and withdrawn the
recommendation. The accurate rule: **an ability fails when granted without dynamic tags, not because
of its base class.** `CanExecuteAbilityByHandle` only demands `IsFullyInit()` when the ability *is* a
`UACFGameplayAbility` (`ACFAbilitySystemComponent.cpp:412`); a plain one with a valid handle passes.

**Concurrent edit, checked not assumed.** goblinsiege-5-8-07 had already saved capsule fixes to
`DA_Char_Knight` and `DA_Char_Archer` (Knight 66.5/172.2, Archer 58/150.1) before I claimed them.
Both sessions drive the same editor, so we were mutating the same in-memory objects; I re-read all
three assets after my change and confirmed the capsule values were intact alongside the new
`DefaultAbilitySet` before saving. Nothing of theirs was clobbered.

**Written but never run:** nothing.

**Touched outside the goal:** nothing. `DA_Char_Player`, `DA_Char_Civilian` and
`DA_Char_HordeGoblin` also show as modified in git - those are the peer's capsule fixes, not mine.

## Refine

**Changed in response to my own evaluation:** nothing in the asset itself; the first pass survives
scrutiny because the gate it targets was measured open before and closed after.

**Deliberately left undone:**

1. **The set holds one ability.** Heavy attack, block and guard break are absent because
   `GA_HU_SwordHeavy` and `GA_HU_GuardBreak` have empty `ability_tags` and `GA_HU_Block` carries
   `State.Blocking`. Giving each a distinct action tag needs new tags in
   `Config/DefaultGameplayTags.ini` and an editor restart, deferred with #343's item 2. Until then
   defenders have exactly one verb.
2. **Double grant accepted.** Each defender ability is now granted twice - once by GS's `GrantIfSet`
   and once by the ACF ability set. Harmless in practice because the GS-granted spec is only reached
   by `TryLightAttack`, whose only caller is the orphaned `BT_Militia`. It should be collapsed if
   those trees are ever restored; noted rather than fixed because removing GS's grant would break the
   horde path, which still uses it.
3. **The archer keeps a melee-only set.** `DA_Char_Archer` points at the same human set, so
   `BP_ErikaArcher` is configured to swing a sword it does not carry. Its ranged behaviour lived in
   the orphaned `BT_Archer`. Left as-is because giving the archer a real ranged action is #343's
   item 5, not this ticket.
