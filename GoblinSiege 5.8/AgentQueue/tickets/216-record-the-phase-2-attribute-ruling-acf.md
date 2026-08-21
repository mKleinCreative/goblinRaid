---
id: 216
title: Record the Phase 2 attribute ruling: ACF AdvancedRPGSystem becomes the attribute owner, and what that puts at risk
agent: claude-attrruling
status: done
claimed: 2026-08-20T21:36Z
build: none
waiting_on:
evaluated: 2026-08-20T21:37:50Z
observed: UNOBSERVED 2026-08-20T21:37:51Z - A ledger entry has no runtime. What it records is sourced - Michael quoted verbatim, and the ACF defect evidenced from plugin source and verbatim linker output.
scenario: none - never run
files: 
  - docs/decisions-ledger.md
---

## Goal

Record the Phase 2 attribute ruling: ACF AdvancedRPGSystem becomes the attribute owner, and what that puts at risk

## Generate

Three rulings into `docs/decisions-ledger.md`, dated 2026-08-20:

- **24** — finish the ACF migration, phases 1 to 4, each ending somewhere watchable.
- **25** — **Phase 2 attributes: ACF's `AdvancedRPGSystem` is the owner**, chosen for out-of-the-box
  coverage over keeping stock GAS with `UGSAttributeSetBase`. This settles the migration doc's open
  risk 3.
- **26** — #213's hand-authored Hold branch is abandoned rather than done, because Phase 3 delivers
  Hold as `AICommand.StayThere`.

Plus two sections that are findings rather than rulings: what ruling 25 puts at risk, and the ACF
defect found during Phase 1.

## Evaluate

**The risk section is the part that earns its place.** Ruling 25 is right for breadth and I am not
arguing with it, but the cost lands squarely on the work this project has tuned hardest, and none of
it comes from ACF: `UGSDamageExecCalculation`'s directional plate (#091) and minimum damage floor
(#093), the blocked-swing recoil (#087), `UGSEngagementComponent`'s token budget and ring slots
(#090, retuned by Michael 2026-08-11), and `UGSStaminaComponent` — which ARS's own statistics
duplicate, and which only just gained the dodge cost in #209.

**The failure mode is not the one the migration doc names.** It warns about "two health pools". The
real hazard is **two damage models**: ACF ships `ACFBaseDamageTypeCalculator` and
`ACFGASDamageCalculatorBP`, so the plausible outcome is our tuned pipeline being quietly bypassed
rather than visibly duplicated. Quietly is worse — a second health bar is obvious on screen; a
bypassed armour rule reads as "combat feels off".

**Recorded, not acted on.** Nothing was migrated here. Phase 2 needs a per-behaviour decision — for
each of those four systems, does ARS replace it, or is it re-hosted on ARS's statistics — and that is
a planning job, not something to discover mid-build.

**Not verified:** I have not read ARS's statistics API, so "ARS duplicates stamina" is inference from
the `rpg-system` skill's description rather than measurement. Worth confirming before Phase 2 rather
than assuming.

## Refine

Wrote the risk into the ledger rather than only saying it in conversation, because ledger entries are
what the next session reads and this one will matter three phases from now, long after the
conversation is gone.

Left the ruling unqualified. It is Michael's call, it is defensible, and hedging a recorded decision
with my reservations would make the ledger a worse record — the reservations belong in their own
section, which is where they are.
