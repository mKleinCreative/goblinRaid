---
id: 264
title: An attack order becomes a place, not a person: arrive, sweep locally, engage whatever is nearest
agent: claude-acf
status: done
claimed: 2026-08-23T22:37Z
build: none
waiting_on: BUILT. Order an attack on a guard standing with others - the band should spread across whoever is there instead of queueing on the one marked.
evaluated: 2026-08-23T23:06:03Z
observed: 2026-08-23T23:06:17Z | Michael watched the ordered horde in play and accepted the behaviour - goblins take an attack order as a place, move to it and engage what is nearest, rather than chasing one named target. He reported it as good enough to close. The fine detail of what he saw was not relayed to claude-warren, who is recording this on his instruction rather than from having watched it.
scenario: Michael in PIE during the claude-acf session of 2026-08-23, before that session was closed. Exact map and band size not relayed.
files: 
  - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
  - Source/GoblinSiege/Horde/GSHordeSubsystem.h
---

## Goal

An attack order becomes a place, not a person: arrive, sweep locally, engage whatever is nearest

## Generate

**RECONSTRUCTED FROM THE DIFF BY claude-warren, 2026-08-23.** The authoring session (claude-acf) was
closed by Michael before it wrote its own Generate/Evaluate/Refine, and the three placeholders were
still in place. This is a factual description of what changed, not a report in the author's voice -
their reasoning is lost and is not being invented here.

`UGSHordeSubsystem.{h,cpp}`, roughly 299 insertions across the two files, changing an attack order
from naming a TARGET to naming a PLACE:

- The order carries `Order->Location` as an anchor; goblins move to it and then engage whatever is
  nearest rather than pursuing one specific actor. An in-code comment states the intent directly -
  go to the place, then attack what is around them rather than a specific item.
- `SummonerLocation` is now derived from the summoner's pawn where one exists.

**These files also carry #263's work** (`GetFollowPostFor`, formation posts behind the summoner).
The two tickets' changes are intermingled in the same diff and cannot be separated after the fact.

## Evaluate

**Michael observed this and accepted it.** Beyond that, this section cannot honestly be filled in:
the author's own assessment - what it verified, what it left unrun, what it touched outside the goal
- went with the closed session.

Known from inspection rather than from the author: the change is confined to
`UGSHordeSubsystem.{h,cpp}`, and it shares those files with #263.

## Refine

Not recoverable. The session was closed before writing it.

**Closed on Michael's explicit instruction**, 2026-08-23: *"close 264 I observed it, ya'll didn't
close it."* Recording the provenance because a ticket closed by a different agent than the one that
wrote the code is unusual and should not look like ordinary practice.

> 2026-08-23T23:06Z G/E/R reconstructed from the diff by claude-warren; authoring session closed before writing them.
