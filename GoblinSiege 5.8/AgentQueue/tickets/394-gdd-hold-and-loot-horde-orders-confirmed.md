---
id: 394
title: GDD: Hold and Loot horde orders confirmed working, update roster and inert-order status
agent: claude-gdd
status: done
claimed: 2026-09-01T00:30Z
build: none
waiting_on:
evaluated: 2026-09-01T00:31:57Z
observed: 2026-09-01T00:31:55Z | Michael reported in session that all four horde order-wheel verbs (Attack, Hold, Loot, Follow) work in play
scenario: Live playtest, reported directly by Michael, not run by this agent
files: 
  - docs/goblin-siege-gdd.md
  - docs/decisions-ledger.md
---

## Goal

GDD: Hold and Loot horde orders confirmed working, update roster and inert-order status

## Generate

Michael reported live, in session, that all four horde order-wheel verbs (Attack, Hold, Loot,
Follow) now work — GDD v1.4 (§5, §12.1 rows 6/17) still documented Hold and Loot as inert, per
ticket #213's unfinished spec (a Behaviour Tree edit that could not safely go through Python
because `BT_HordeGoblin` carries an EdGraph that regenerates over injected nodes on next open).
The working tree already shows `Content/AI/BB_HordeGoblin.uasset` and `BT_HordeGoblin.uasset` as
modified, uncommitted — consistent with #213's spec having been carried out by hand.

Updated `docs/goblin-siege-gdd.md`:
- Revision history: new v1.5 row, ticket #394.
- §5 "Order-wheel status": rewritten from "Attack and Follow work; Hold, Loot and Smash are
  inert" to all four wheel verbs confirmed, Smash called out explicitly as never having been a
  wheel command in the first place (so it isn't part of this claim either way).
- §12.1 row 6 (Horn & horde): WIRED → BUILT.
- §12.1 row 17 (Loot couriers): "WIRED, no cargo" → WIRED, with the still-open livestock/
  CarrySocket gap kept and #379-381's UNOBSERVED status on livestock left intact rather than
  folded into this claim.

Added a dated entry to `docs/decisions-ledger.md` (ruling 73, 2026-08-31) sourcing this to
Michael directly rather than to a ticket's own observed-evidence report, and noting the
`BT_HordeGoblin`/`BB_HordeGoblin` edit itself has no ticket of its own by design (#213's own
conclusion).

## Evaluate

**What is verified:** the design-document text now matches what Michael reported watching in
play. That report is the evidence — this ticket did not itself run PIE or read a log, and says so
in both documents rather than implying a queue-ticket-grade observation.

**What is not re-verified here:** I did not open `BT_HordeGoblin` or diff its graph against
#213's spec to confirm the implementation matches ordering/abort-mode details (Hold above
Menace-Orbit-etc., `Observer Aborts: Both`). The GDD claims the *outcome* (all four verbs land),
not the *shape* of the fix — if the wiring differs from #213's spec but still produces working
orders, this entry is still accurate; only a claim about *how* it works would need that check.

**Touched outside the goal:** nothing beyond the two claimed files.

**AGENT_STATE.md line owed:** a DECISIONS entry pointing at ruling 73, added below.

## Refine

Nothing left undone within this ticket's scope. Two things flagged for whoever picks up next
rather than guessed at here: (1) the `BT_HordeGoblin`/`BB_HordeGoblin` changes are uncommitted —
worth a commit of their own before they sit alongside unrelated working-tree changes much longer;
(2) Smash (`BTTask_SmashOrderTarget`) still has no consumer and was never in scope here — a
real gap, just not this one.
