---
id: 220
title: Ring promotion leaks the old claim: an agent can hold an inner AND an outer slot
agent: claude-acf
status: done
claimed: 2026-08-21T00:09Z
build: none
waiting_on:
evaluated: 2026-08-21T00:10:29Z
observed: 2026-08-21T00:35:58Z | inner + outer now equals engaged EXACTLY in 16 of 16 samples, never exceeding it. Before the fix the same readout showed 6 inner + 6 outer against engaged 10 - twelve claims held by ten attackers.
scenario: GS.Horde.Order Attack on BP_CastleGuard02_C_0 with 9-10 goblins, CrowdStats sampled five times during the fight. L_CombatArena, 2026-08-21 00:34-00:35.
files: 
  - Source/GoblinSiege/Combat/GSEngagementComponent.cpp
---

## Goal

Ring promotion leaks the old claim: an agent can hold an inner AND an outer slot

## Generate

`ClaimRingSlot` now calls `ReleaseOuterSlot(Claimant)` on success, and `ClaimOuterSlot` calls
`ReleaseRingSlot(Claimant)` on success. Both only on success - freeing the outer slot after a FAILED
inner claim would evict an agent from the one place it legitimately holds.

## Evaluate

**Found by the instrument, not by reading.** #219's readout, on its first ever run:

```
BP_CastleGuard02_C_1   engaged 10/6 <-- OVER ENGAGED   swinging 2   weight 2/4   slots 6 inner + 6 outer
```

**Twelve claims held by ten attackers.** The preceding sample read `6 inner + 4 outer` = exactly 10,
which is what correct looks like, so the two readings together localise the bug to promotion between
the rings rather than to claiming in general.

This is a defect **#218 introduced tonight**, not a pre-existing one: #218 added the outer ring and
wired `ReleaseOuterSlot` into `ReleaseAll`, but never cross-released between the two rings. An agent
promoted outer -> inner kept its outer claim until `SlotClaimTimeoutSeconds` (6s) swept it. The
consequence is the one #218 was written to prevent: ghost claims fill the outer ring, a genuine
overflow agent is refused a slot, and it falls through to the own-bearing hold and stacks.

**NOT COMPILED, NOT RUN at time of writing.** No behavioural claim.

**What this ticket does NOT fix:** the swing trickle. The same run showed `swinging 1` and
`swinging 2` against a gang of 10. That is the `CanBeAttacked` / `TryAcquireToken` lockout
(`GSEngagementComponent.cpp:225-228`), which is a design question about recoil against crowds and
belongs to its own ticket.

**Measurement gap in the run that found this:** `GS.Combat.LogAI 1` was not set, so the session
carries zero AI decision events and swing cadence could not be measured at all - only the two
CrowdStats snapshots. Any future watch of this area needs LogAI on.

## Refine

Considered and rejected: making `ClaimSlotIn` itself sweep the other ring. It would put knowledge of
both rings inside the shared helper, which exists precisely so it does not know which ring it is
working on - `bOuter` only selects geometry. Keeping the cross-release in the two thin wrappers
leaves the helper ring-agnostic.
