---
id: 267
title: Summoned horde goblins spawn with no EquippedWeapon again: no abilities granted, no ARS attributes, two ACF errors per goblin
agent: claude-warren
status: abandoned
claimed: 2026-08-23T23:54Z
build: none
waiting_on:
evaluated:
observed:
scenario:
files: 
  - Content/Blueprints/BP_HordeGoblin.uasset
---

## Goal

Summoned horde goblins spawn with no EquippedWeapon again: no abilities granted, no ARS attributes, two ACF errors per goblin

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

> 2026-08-23T23:55Z Premise disproved before any edit. #144 already established by runtime read that summoned goblins DO carry DA_Weapon_HordeGoblin; GSHordeGoblin.cpp:63 equips in BeginPlay, after the warning at GSWeaponComponent.cpp:54 has already fired. BP_HordeGoblin was not touched. The real defect is the warning, taken up in a new ticket.
