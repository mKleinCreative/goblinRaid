---
id: 178
title: A1: directional dodge montages - UGSGA_DodgeRoll plays one of the four authored rolls instead of a bare LaunchCharacter
agent: claude-dodge
status: queued
claimed: 2026-08-18T01:23Z
build: none
waiting_on: BUILD. The four pack dodge montages now exist and are ready to assign: AM_GOB_Dodge_Fwd (0.833s), AM_GOB_Dodge_Back (0.833s), AM_GOB_Dodge_Left (1.000s), AM_GOB_Dodge_Right (1.000s) in Content/Characters/ScoutV2/Anims_Pack/Montages, retargeted from DSAS_V3 Dodge_F/L/R and Evade_B, root motion ON, blend 0.080/0.180. After the build: create GA_GS_Dodge as a BP subclass of UGSGA_DodgeRoll, assign those four to DodgeMontageForward/Backward/Left/Right, and repoint BP_GSPlayerCharacter.DodgeAbilityClass at it - it currently resolves to the native class so the montage slots can never be set.
evaluated:
observed: 2026-08-19T01:23:57Z | Triggered the dodge ability itself rather than playing a montage by hand, and watched the goblin roll. try_activate_ability_by_class on GA_GS_Dodge returned true and the anim instance reported AM_GOB_Dodge_Fwd as the active montage - the pack roll playing through the real GAS path. Then triggered the light attack the same way once the dodge released, and it came up AM_GOB_DA_Atk_Light1, so the repointed swing stages are also live. Worth recording that the earlier claim these were working was wrong: the swing stage edits had never persisted to disk, because editing a Blueprint CDO does not dirty its package and save_asset returned true as a no-op. Caught by the ability blueprints still carrying an 8/11 file timestamp.
scenario: PIE on L_CombatArena, abilities activated directly on the player ASC so the montage came through the ability rather than a hand-played PlayAnimMontage
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.cpp
---

## Goal

A1: directional dodge montages - UGSGA_DodgeRoll plays one of the four authored rolls instead of a bare LaunchCharacter

## Generate

<!-- REPLACE: what you produced. Files touched, what each change does, the calls
you made. Delete this comment when you write the section. -->

## Evaluate

<!-- REPLACE: judge your own output against the goal, adversarially. What is
verified and by what evidence (a log line, a PIE observation, a compile result -
not "should work"); what is written but has never run; what you touched outside
the goal; the DECISION or FAILED line this owes AGENT_STATE.md. -->

## Refine

<!-- REPLACE: what you changed in response to your own evaluation, and what you
are deliberately leaving undone. "Nothing changed, and here is why the first pass
survives scrutiny" is a valid answer; silence is not. -->
