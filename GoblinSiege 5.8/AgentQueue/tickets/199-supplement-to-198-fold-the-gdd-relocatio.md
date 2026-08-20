---
id: 199
title: Supplement to 198 - fold the GDD relocation, the new ledger, and two rediscovery-prone findings into AGENT_STATE.md
agent: claude-gddlock
status: done
claimed: 2026-08-19T21:37Z
build: none
waiting_on:
evaluated: 2026-08-19T23:01:21Z
observed: UNOBSERVED 2026-08-19T23:16:56Z - AGENT_STATE.md is agent memory - a markdown file with no runtime, so there is nothing to watch by construction. Its only real test is whether a later session behaves differently because of it, which cannot be seen today. What it records IS observed and cited: the drift gate refusing a build (198), the two-map divergence and the absent x1.5 multiplier (both read off the tree), and the statue-scores-but-is-not-an-objective finding behind 179/189/190. Closed on Michael's instruction 2026-08-19.
scenario: none - never run
files: 
  - AGENT_STATE.md
---

## Goal

Supplement to 198 - fold the GDD relocation, the new ledger, and two rediscovery-prone findings into AGENT_STATE.md

## Generate

One edit to `AGENT_STATE.md`, at the top of DECISIONS (it is the only file agents load at run start,
and QUEUE.md is explicit that nothing reads old tickets):

1. **The GDD moved and is now canonical** — `docs/goblin-siege-gdd.md` v1.0 LOCKED; the repo-root doc
   is frozen; the move needed no code change because `ca/config.py` already probed `project_root/docs/`
   first.
2. **The decisions ledger now exists** — `docs/decisions-ledger.md`, the §13 both documents had been
   pointing at for weeks without it being in either of them.
3. **`Tools/check_gdd.py` is the drift gate**, what it checks, its three exit codes, and that it is
   deliberately not yet wired into the build because it currently exits 1 on real drift.
4. **The false rule I shipped and the test that caught it** — a fourth column in §12.1 does not drop
   the row, it corrupts the status string; the actual drop conditions are listed.
5. **The `re.search` first-match hazard** on the never-cut line, and that the line must stay unwrapped.
6. **Two findings that must not be rediscovered**: the tutorial-map/arena divergence, and the absent
   ×1.5 extraction multiplier.

## Evaluate

**Verified:** the entry is in the file at DECISIONS (lines 170 and 193) and `AGENT_STATE.md` still
opens with its existing banner and BUILT section intact — the insert went in at the section anchor,
not over anything.

**Why items 4 and 5 are in here at all.** They are my own mistakes from #198, not project history. They
earn their place because both are *traps in a parser that other agents will edit*: the next session to
touch §12.1 will reach for a fourth column, and the one after that will mention the never-cut heading
in prose. Recording only the outcome and not the trap is how this project got the eight-day interact
framework and the twice-diagnosed C1083.

**Not verified / never run:** nothing here runs. This is a memory edit; its only test is whether a
later agent behaves differently, which cannot be observed today.

**Deliberately not touched:** the **NEXT** list in the same file still carries stale ranked items
(score system "missing `UGSScoreSubsystem`" — it exists; the horn/horde editor list — largely done).
Re-ranking NEXT is a real job and a different one; doing it inside a doc-supplement ticket would be
exactly the unclaimed scope creep rule 1 exists to stop.

## Refine

Nothing changed on review. The first pass survives scrutiny: the entry is dated, ticket-attributed,
states what is *believed* separately from what is *verified*, and names the two findings in the
imperative form that has historically survived being skimmed. It is deliberately long in two places —
the parser traps — because a one-line version of either would be re-learned by the next agent.

---

# Addendum — what the three UNOBSERVED closes leave unproven

#179, #189 and #190 were closed UNOBSERVED on Michael's instruction, so each owes this file a line
saying what is not proven. One entry covers all three, and it exists to stop a specific wrong
inference: **Michael watched a toppled statue score +100 deeds (#196), and that is not evidence the
statue objective works.**

`AGSObjective_ToppleStatue` is instantiated nowhere — the deeds came from `GSTopplableComponent`
calling `AddDeeds` directly, bypassing the mission-objective class. `Marker.ObjectiveAnchor.Statue`
is defined and read by nothing, because no map places an ObjectiveAnchor marker at all. Both are in
the binary; neither has run. Recorded in DECISIONS above the two earlier findings.
