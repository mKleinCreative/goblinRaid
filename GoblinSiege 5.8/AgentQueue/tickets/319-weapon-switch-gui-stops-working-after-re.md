---
id: 319
title: Weapon-switch GUI stops working after respawning from a fire death
agent: unassigned
status: abandoned
claimed: 2026-08-26T18:44Z
build: none
waiting_on:
evaluated:
observed:
scenario:
files: []
---

## Goal

Weapon-switch GUI stops working after respawning from a fire death

## Report (not started - this is a bug report, nobody has picked it up)

**Michael, 2026-08-26, playing L_Tutorial_Island:** *"the GUI for switching weapons didn't work after
respawn from a fire related death."*

**What he was doing.** Burning houses on L_Tutorial_Island in PIE, torch in hand, during the #317
crumble testing. He died to fire - his own or a spreading one is not established - respawned, and the
weapon-switch GUI no longer responded. He kept playing rather than stopping to characterise it, so
what follows is the limit of what is actually known.

**Known:**
- Death was fire-related.
- It is the weapon-SWITCH GUI specifically that stopped working, after the respawn.
- The session had a working weapon switch before the death: the log has
  `BP_GSPlayerCharacter_C_0 is now holding Grapple` / `Bow` / `Torch` lines from that session.

**NOT known, and worth establishing before changing any code:**
- Whether the widget is gone, unresponsive, or present-but-not-updating.
- Whether the underlying switch still works by direct input while only the GUI is dead - that
  separates a UI-layer bug from an input/equipment-state bug, and they live in different modules.
- Whether it reproduces on a non-fire death, which is the single most useful next data point: if it
  reproduces on any death, "fire-related" is a red herring and this is a respawn bug.
- Whether it survives a further death/respawn, or is permanent for the session.

**Where to look.** The Common UI layer stack and the tag-based widget registry (ACF's ANS module -
see the `ui-navigation` skill), plus whatever rebuilds the HUD on respawn. `Source/GoblinSiege/UI/`
holds `GSPlayerHUDWidget`; note #314 touched that file and was ABANDONED with its Refine unwritten,
so the HUD is not in a reviewed state and that is a plausible neighbour to this bug rather than a
coincidence.

**Deliberately not diagnosed here.** I was mid-way through #317's crumble work when Michael reported
this and did not investigate it at all - guessing at a cause from one sentence is how three inferred
diagnoses failed live in one session earlier in this project. First step is a repro, not a fix.

## Generate

<!-- not started -->

## Evaluate

<!-- not started -->

## Refine

<!-- not started -->

> 2026-08-26T18:44Z Bug report from Michael, unowned. Needs a repro before any fix.

---

## ABANDONED 2026-08-27 - dead, no session, nothing to revert

Michael: *"close 319 and 321, they're dead."*

Never claimed by an agent (`agent: unassigned`), never `active`, claimed **no files**, and had no
uncommitted edits behind it. Nothing to revert; this is a clean close.

It was, however, **holding the build gate shut for 5+ hours with no work in progress** - it is named
in #325's `waiting_on` as one of the tickets blocking that compile. That is the whole cost of
leaving an unowned ticket open.

The defect itself is NOT fixed and is still real: the weapon-switch GUI stops working after
respawning from a fire death. Re-open it when the ACF migration is done, since the migration is
moving the equipment and character layer underneath it and may change or resolve the cause.
