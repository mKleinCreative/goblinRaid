---
id: 036
title: Code-review findings, tooling half: gs_buildings.py crash + three gsqueue.ps1 silent-failure holes
agent: claude-fixes
status: done
claimed: 2026-08-06T18:04Z
build: none
waiting_on:
evaluated: 2026-08-06T18:11:58Z
files: 
  - tools/hamlet/gs_buildings.py
  - GoblinSiege 5.8/AgentQueue/gsqueue.ps1
  - decision-queue-kanban.html
---

## Goal

Code-review findings, tooling half: gs_buildings.py crash + three gsqueue.ps1 silent-failure holes

## Generate

Four of the nine `HANDOFF.md` code-review findings. The other five are C++ and are batched for one
build (see Refine).

**Finding 1 (HIGH, "the script crashes on any level") - ALREADY FIXED, not by me.** The report says
`gs_buildings.py:110` does `if not any(k in m for k in PIECE_KEYS)` while `mesh_of()` returns `None`
for any actor with no static mesh, so the first PlayerStart or light raises `TypeError`. That code is
not in the file. `git log -S PIECE_KEYS` shows the symbol introduced in `96104be` and **removed in
`837a6a1`** - ticket #033's "a building is a merged house actor" rewrite deleted the crashing block
as a side effect, after the review was taken. Every surviving `in m` is short-circuit guarded: line
117 `m and "Roof" in m`, line 142 `not m or ...`, line 171 `is_roof(m) and ...` where `is_roof(None)`
returns `None`. Line 162 already continues on a null mesh. `py_compile` clean. **No edit made** -
reporting it rather than inventing a fix for a bug that is gone.

**Finding 2 - `set -Status done` bypassed every close check.** `Invoke-Set` wrote any `-Status`
straight to frontmatter, so `set -Id N -Status done` skipped `Invoke-Done` entirely: the G/E/R
placeholder scan, the review gate, and the late-file comparison. `Invoke-Set` now refuses `done`
unless `$script:DoneChecked` is set, which only `Invoke-Done` does - the checked path still works and
only the bypass is closed. `abandoned` is deliberately still allowed: abandoning means the work was
reverted, and demanding a finished Evaluate for it would push agents to close as `done` instead.

**Finding 3 - the #024 stale-Evaluate gate was a no-op.** The cause is not a wrong constant; it is
that **tickets record two path conventions and always have.** Counted across every ticket: 102 claims
are project-relative (`Source/...`, `Content/...`, living under `GoblinSiege 5.8/`) and 24 are
repo-relative (`tools/...`, `CLAUDE.md`, `GoblinSiege 5.8/...`). One root cannot resolve both. Added
`$ProjRoot` and `Resolve-ClaimedPath`, trying the repo root first and the project root second -
repo-first because `CLAUDE.md` exists at BOTH and ticket #010 claims each separately, so that order
gives each the file its author meant. An unresolvable path is now reported rather than skipped.

**Finding 4 - the `evaluated` stamp truncated seconds.** Stamped `yyyy-MM-ddTHH:mmZ` and compared
against `LastWriteTimeUtc`, so any file saved earlier in the same minute as `set -Status review` -
the normal case, since you save and then immediately stamp - looked up to 59s late and forced
`-Reaffirm` on a clean close. Now `yyyy-MM-ddTHH:mm:ssZ`. `Get-EvaluatedAt` still reads the existing
minute-only stamps, treating them as the END of their minute (+59s).

## Evaluate

**Verified end-to-end, with output, on a scratch ticket (#037 - abandoned and deleted afterwards).**
It claimed two paths: `Saved/probe_resolve.txt` (project-relative, real) and
`Saved/does_not_exist_xyz.txt` (unresolvable). After `set -Status review` the probe was rewritten,
then `done` ran and printed:

- `NOTE - #037 has 1 claimed path(s) that resolve to no file ... Saved/does_not_exist_xyz.txt`
- `REFUSED ... Saved/probe_resolve.txt (modified 2026-08-06T18:09:05Z)` against a stamp of
  `2026-08-06T18:09:03Z`.

One run proves all three fixes: the project-relative path **resolved** (the old code skipped it in
silence), the unresolvable one is **surfaced**, and 03s vs 05s were **distinguished** - a comparison
the minute-truncated stamp could not make. Separately, `set -Id 036 -Status done` printed
`REFUSED - use 'done -Id 036', not 'set -Status done'.` and exited 1.

Also checked: the script parses (`[Parser]::ParseFile`, zero errors) and is **pure ASCII** (byte scan,
nothing > 127) - the trap that takes the queue down for every agent at once. `list` exits 0 in a
clean process; the `exit=1` I first saw was a stale `$LASTEXITCODE` in my own compound command.

**Not verified:**
- **The legacy +59s branch has no test.** Every stamp written from now on carries seconds, so it only
  fires on existing tickets, and I confirmed it by reading rather than running.
- **No test that a non-late file PASSES.** Every case I exercised ends in REFUSED, so the clean-close
  path through the new code is unproven - and it is the more common path in daily use.
- `Resolve-ClaimedPath` returns the first root that has the file. If a path existed under both roots
  and the author meant the project one, it would silently pick the repo one. Only `CLAUDE.md` does
  that today and repo-first is right for it, but the rule is positional, not intelligent.

**Finding 1 is the one worth remembering.** A review found a real crash; the fix arrived from an
unrelated rewrite by a different agent; and the finding then sat in HANDOFF.md as HIGH, so the next
agent would have spent time on a bug that no longer existed. **A finding is a claim about a moment,
and re-checking it is part of acting on it.** The check cost one `git log -S`.

**Owed AGENT_STATE.md** - FAILED: *a guard that resolves paths against one root, when the data
carries two conventions, fails OPEN and looks identical to a guard that passed.* The #024
stale-Evaluate gate ran for a day reporting nothing on 102 of 126 claimed files.

**Touched outside the goal:** none - `decision-queue-kanban.html` was claimed. Its `review-findings`
card now reads 5 of 9 with reasons, and `#035` moved out of In flight, where it was falsely sitting.

## Refine

- **Reported finding 1 instead of fixing it.** My first move was to patch the guard the report named;
  the code was not there. Confirming with `git log -S PIECE_KEYS` turned "I cannot find the bug" into
  a dated fact about when it left.
- **Fixed the convention, not the constant.** The obvious patch was `$RepoRoot` -> `$ProjRoot`, which
  would have repaired 102 claims and broken the 24 that already worked. Counting both conventions
  first is what made a two-root resolver obviously right.
- **Made an unresolvable path loud rather than fatal.** Fail-closed was tempting - the finding is
  literally about failing open - but refusing a close over a deleted file or a
  `DIAGNOSIS-ONLY-no-files-claimed-yet` placeholder invents a new way to be stuck. The original bug
  was the silence, so silence is what got fixed.
- **Legacy stamps read as end-of-minute rather than given a tolerance.** A 60s tolerance would also
  mask genuine edits made within a minute of the stamp; +59s is the exact upper bound of what a
  minute-precision stamp can mean.
- **Caught a backtick bug before it shipped.** The refusal message was first written with backticks
  around `done -Id N`. Backtick is PowerShell's escape character, so it would have printed mangled -
  in the message whose entire job is to tell you the correct command. Single quotes now.
- **Deleted the probe ticket and its file** rather than leaving a passing test as clutter in a queue
  whose whole value is that its contents are real.

**Deliberately left undone:** the five C++ findings - they need an editor-closed build, so they go in
one ticket rather than five builds. A clean-close regression test. And re-verifying the remaining
HANDOFF findings against the current tree before acting on them, which finding 1 says plainly is
worth doing for each.
