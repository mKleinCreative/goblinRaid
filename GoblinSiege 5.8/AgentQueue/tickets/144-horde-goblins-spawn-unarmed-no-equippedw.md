---
id: 144
title: Horde goblins spawn unarmed: no EquippedWeapon on BP_HordeGoblin, so a warband is wiped in seconds
agent: claude-gobarm
status: done
claimed: 2026-08-13T05:10Z
build: none
waiting_on:
evaluated: 2026-08-13T05:13:51Z
observed: 2026-08-13T05:13:12Z | Four freshly horn-summoned goblins each came up holding DA_Weapon_HordeGoblin in the sword slot, read straight off the live pawns - the warband is armed and the unarmed diagnosis was false
scenario: PIE in L_CombatArena, one GS.Horde.SpawnTest blast, reading EquippedWeapon and CurrentSlot off each spawned pawn
files: 
  - Content/Blueprints/BP_HordeGoblin.uasset
---

## Goal

Horde goblins spawn unarmed: no EquippedWeapon on BP_HordeGoblin, so a warband is wiped in seconds

## THE PREMISE WAS WRONG - NOTHING WAS BROKEN

**The title of this ticket is false and no file was modified.** Horde goblins are armed. Michael asked
for this fix on the strength of my diagnosis, and my diagnosis was wrong.

Measured live in PIE, `L_CombatArena`, one horn blast:

```
BP_HordeGoblin_C_0  EquippedWeapon=DA_Weapon_HordeGoblin  slot=GSWeaponSlot.SWORD
BP_HordeGoblin_C_1  EquippedWeapon=DA_Weapon_HordeGoblin  slot=GSWeaponSlot.SWORD
BP_HordeGoblin_C_2  EquippedWeapon=DA_Weapon_HordeGoblin  slot=GSWeaponSlot.SWORD
BP_HordeGoblin_C_3  EquippedWeapon=DA_Weapon_HordeGoblin  slot=GSWeaponSlot.SWORD
```

`BP_HordeGoblin`'s CDO already carries `DefaultWeapon = DA_Weapon_HordeGoblin`. **#098 is in effect
and always was.**

## What actually misled me, three times

`UGSWeaponComponent::BeginPlay` (`GSWeaponComponent.cpp:52`) warns when its `EquippedWeapon` is null:

```
[GoblinSiege] BP_HordeGoblin_C_0 has no EquippedWeapon data asset - no weapon meshes, no granted
abilities, and no InitialAttributesEffect, so this character keeps the attribute set's constructor
defaults (100/100/0). Assign one on the owning Blueprint.
```

It is correct for a character that expects `EquippedWeapon` set in the editor. It is **premature for
`AGSHordeGoblin`**, which deliberately leaves that field empty and equips from its own
`DefaultWeapon` in `AGSHordeGoblin::BeginPlay` - which runs *after* `Super::BeginPlay()` has already
dispatched the component's. So the component warns, and a few lines later the goblin arms itself.
The warning describes a state that is real for about one frame and then stops being true, and it
names a remedy ("Assign one on the owning Blueprint") that is already done.

This is the *lying instrument* failure AGENT_STATE names outright - "a lying instrument is worse than
dead code". I quoted it as a finding twice in one session and proposed a fix for it as the top of the
queue, and the actual cost was Michael authorising work on something that was never broken.

## Why the warband still got wiped

The thing this was meant to explain has a duller answer that AGENT_STATE already records: *"Goblins
die to two knight hits (40 HP vs 25) - a warband melts against knights. Knights are elites, 3-4 max
per map; balance the horde against MILITIA."*

The test that prompted this was 8 goblins against **12** humans including `BP_CastleGuard02`s. Losing
0-of-8 against that is the documented, expected outcome, not a defect. The mistake was reaching for a
bug to explain a result the design notes already predicted.

## Generate

**Nothing.** No asset was modified, no code was written. The ticket is closed as an investigation.

## Evaluate

**Verified, and it is the negative result:** four freshly summoned goblins each report
`EquippedWeapon = DA_Weapon_HordeGoblin` and `CurrentSlot = Sword`, read off the live pawns in PIE
rather than off the Blueprint. The CDO check alone would not have settled it - `EquippedWeapon` is
`None` on the CDO, which is exactly what made the warning look true. Only the runtime read
distinguishes "never armed" from "armed one frame later".

**What this ticket owes the record, bluntly:** I reported "the warband is unarmed despite #098" to
Michael twice as a standing finding, and then offered it as the highest-value next fix. Both
statements were false. The evidence I had was a log warning; the evidence I needed was one runtime
property read, which took ninety seconds once I finally did it.

**Two real items fall out of this, neither of them the one in the title:**

1. **The warning should not fire for `AGSHordeGoblin`.** It is a genuine defect - a diagnostic that
   is wrong for a whole class of pawn, that names an already-completed remedy, and that has now
   demonstrably caused a misdiagnosis. Options, none of them done here: defer the warning by a frame
   or a timer so a pawn that arms itself in its own `BeginPlay` is not slandered; or give
   `UGSWeaponComponent` an `bExpectsExternalEquip` flag that `AGSHordeGoblin` sets. Either is a C++
   change and a build, so it is a ticket, not a footnote.
2. **The horde-vs-knights balance is a design question already answered** in AGENT_STATE and does not
   need rediscovering: balance the horde against militia, cap knights at 3-4. The crowd test that
   started all this should be run against a militia patrol, not the full arena roster.

**Left completely alone:** `BP_HordeGoblin`. Changing a working asset to match a wrong diagnosis
would have been the worst outcome available here, and it was one step away.

## Refine

**What changed on self-review: the entire ticket.** It was claimed to make a fix and instead
disproves its own premise. The right move on discovering that was to stop and say so, not to find
something adjacent to change so the ticket had an edit in it.

**The generalisable lesson, for AGENT_STATE:** a warning is evidence that a *condition was true at
the moment it logged*, not that it is true now, and not that its suggested remedy is outstanding.
Where BeginPlay ordering is involved - component before actor - a component's complaint about
uninitialised state can be answered by the owning actor microseconds later. Read the live object
before repeating a log line as a finding.

**Deliberately left undone:**

- The misleading warning itself (item 1 in Evaluate). It is real and worth fixing, but it is a
  separate C++ ticket with a build, and this ticket has already been wrong once about what needs
  doing.
- Any change whatsoever to `BP_HordeGoblin`.
- The separation A/B that started this chain. It is still unverified (see #143), and the next attempt
  should use a **militia** patrol so the goblins survive long enough to form a crowd - which is the
  one genuinely useful thing this investigation produced.
