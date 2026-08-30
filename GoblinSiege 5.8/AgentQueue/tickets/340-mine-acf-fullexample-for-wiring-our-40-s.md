---
id: 340
title: Mine ACF FullExample for wiring our 40 skill packs do not cover, emit GS-specific skills
agent: claude-skills
status: done
claimed: 2026-08-28T00:30Z
build: none
waiting_on:
evaluated: 2026-08-28T04:05:25Z
observed: 2026-08-28T04:05:24Z | The packs were used on live tickets and changed outcomes both ways: gs-locomotion-bands predicted the BaseWalkSpeed jog-snapshot defect, which then measured 606.2999 live; gs-ai-patrol-advancers correctly identified the MaxDistanceFromHome 8500 gate, watched tripping at home=8510. Its double-advance prediction was refuted - guards walked WP1->WP2->WP0 and later alternated WP0<->WP1 for nine samples with no skip. Both corrections are now written into the packs.
scenario: PIE on L_Tutorial_Island across tickets 331/334/339/341 - pack claims checked against ACF source and against live guard behaviour
files: 
  - .claude/skills
---

## Goal

Mine ACF FullExample for wiring our 40 skill packs do not cover, emit GS-specific skills

## Generate

**Recorded by claude-acf on 2026-08-28, not by the agent that ran it.** The workflow was launched
detached by session B and that session ended before writing anything here; the ticket sat `queued`
with placeholder G/E/R while its output was already on disk and in use. Michael asked for it to be
closed out.

What landed in `.claude/skills/` (40 -> **49** entries):

- **9 new `gs-*` packs**: `gs-abilities-outside-acf-asc`, `gs-ai-patrol-advancers`,
  `gs-anim-adoption-gaps`, `gs-anim-rig-compatibility`, `gs-attacker-rationing`,
  `gs-behaviour-tree-wiring`, `gs-character-data-asset`, `gs-locomotion-bands`,
  `gs-teams-damage-spawning` (written 18:11-18:14).
- **Dated "Goblin Siege addendum" sections on 12 packs**: acf-core, actions-system, ai-framework,
  anim-blueprints, character-controller, collisions-manager, gas-runtime, gs-character-data-asset,
  inventory-system, music-manager, targeting-system, teams.

Session B's handoff records the run as 16 agents, 45 findings, 41 kept after triage, with the triage
phase dropping anything the existing 40 packs already covered.


## Evaluate

Judged by USING the packs on live tickets this session, which is the only evidence that counts here.

**Validated:**
- `gs-locomotion-bands` §4 predicted that `BaseWalkSpeed` is snapshotted after ACF applies
  `DefaultState` EJog and therefore captures the Jog value. Confirmed in our source
  (`GSCharacterBase.cpp:235-237`, `:305`) and then **measured live: 606.2999**, exactly the Jog band.
  That is a real defect the pack found and nobody else had.
- `gs-ai-patrol-advancers` §1-§2 mechanism claims all check out against ACF source: `patrolIndex++`
  per call (`ACFAIPatrolComponent.cpp:45`), `HandleMoveCompleted` -> `TryGoToNextWaypoint`, the
  Patrol/Routine state gate (`:111-116`), unbind-before-bind, `bStartImmediately` consuming a
  waypoint. `UACFAIStateFragment` exists and behaves as described.
- Its §4 home-leash section, and the matching `ai-framework` addendum entry, correctly identified
  `MaxDistanceFromHome` 8500 as a patrol-breaking gate. Measured tripping at home=8510; fixed in #341.

**Refuted by observation:**
- The **double-advance / "guards visit every OTHER spline point"** prediction is WRONG. Watched in
  PIE with both advancers active: `BP_CastleGuard01_C_2` walked WP1->WP2->WP0->WP1->WP2->WP0, and
  after #341's junction fix `GS_Guard_A1` alternated WP0<->WP1 nine consecutive samples with no
  skip. The pack labelled this "partial - asset name-table evidence, not observed in PIE", which was
  the honest caveat; the `ai-framework` addendum then restated it as fact, which is not.

**Missed:** none of the 49 packs contains the actual cause of the defenders never patrolling -
`GS_Junction_*` orbs are QUERY_AND_PHYSICS meshes at scale 4 sitting on the waypoints, making
arrival physically impossible. Found by Michael, confirmed in #341.

**Not reviewed:** the other 7 `gs-*` packs and 10 addenda have not been checked by anyone. They
carry citations, but citations are not verification.


## Refine

Corrected the `ai-framework` addendum: the double-advance claim is marked refuted-by-observation,
and the junction-orb cause added. See #341 for the evidence behind both.

Deliberately left undone: a claim-by-claim audit of the remaining 7 packs and 10 addenda. That is a
larger job than this ticket, and the packs are useful enough as leads that gating them behind a full
review would waste them. The standing rule stays what session B wrote: treat every `gs-*` pack as a
lead with citations, spot-check before acting.

