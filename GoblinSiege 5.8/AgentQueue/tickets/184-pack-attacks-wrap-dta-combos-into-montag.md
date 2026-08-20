---
id: 184
title: Pack attacks: wrap DTA combos into montages and repoint the 5 goblin swing stages
agent: claude-packanim
status: queued
claimed: 2026-08-18T02:00Z
build: none
waiting_on: Windup corrected on all four pack stages so the damage window straddles the measured contact frame - contact was landing at -63/-26/-33/-38 percent, i.e. before the window opened. Totals held constant, reclaimed time moved into recovery, so recovery/clip on light stage 0 goes 0.146 to 0.333 (above 121s 0.287). NEEDS MICHAEL TO SWING IT: unobserved. Chain speed deliberately NOT changed - his second complaint about the combo not linking fast enough is a separate variable and changing both at once would make it impossible to tell what did what (123).
evaluated:
observed: 2026-08-18T02:04:46Z | In PIE I played the retargeted pack combo on the player goblin and PlayAnimMontage returned 2.5167 rather than 0, so the montage is accepted on GOB_Scout_v2_Skeleton and is not being silently refused. I could not get a clean look at the swing itself: GS.Horde.SpawnTest puts ten goblins around the player and they crowd the third-person camera into their faces from every angle I tried, and I cannot send attack input, so the full input-to-stage-to-montage path was NOT exercised. What is confirmed is that all five goblin swing stages now point at pack montages, every stage total sits inside its clip length at play rate 1.0, and the goblins were animating normally with 0 of 17 in reference pose.
scenario: PIE on L_CombatArena with ten horn-summoned goblins; montage played directly on the player pawn at reduced rate to try to catch it on camera
files: 
  - Content/Blueprints/Abilities/GA_GS_SwordLight.uasset
  - Content/Blueprints/Abilities/GA_GS_SwordHeavy.uasset
  - Content/Blueprints/Abilities/GA_GS_GuardBreak.uasset
---

## Goal

Pack attacks: wrap DTA combos into montages and repoint the 5 goblin swing stages

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
