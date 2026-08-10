---
id: 078
title: "CLAUDE.md: require agents to clean up temporary and helper files at the end of a task"
agent: claude-horde
status: done
claimed: 2026-08-07T20:31Z
build: none
waiting_on:
evaluated: 2026-08-07T23:30:44Z
files: 
  - CLAUDE.md
---

## Goal

CLAUDE.md: require agents to clean up temporary and helper files at the end of a task

## Generate

One new section in the **repo-root** `D:\goblinRaid\CLAUDE.md`, "Clean up after yourself", placed
between the agent-queue section and Building. Michael's ask, 2026-08-07.

It says: temporary files, scripts and helper files created to iterate with get deleted before the
task is handed back. It also says to prefer writing scratch work outside the repo in the first
place, since the session scratchpad never appears in `git status` and therefore never needs
remembering.

Two guard rails written in deliberately, because a bare "delete your temp files" is dangerous in a
repo where several agents work at once:

- **Do not delete files you did not create.** Another agent's work-in-progress and your own leftover
  are indistinguishable from inside a session — the same reason `gsqueue.ps1` refuses to let you
  abandon someone else's ticket.
- **Do not silently bin something worth keeping**, and do not silently promote it into the repo
  either. Say so and let Michael decide. A discarded diff with real thinking in it can be kept as a
  patch in the scratchpad and referenced from the ticket.

Closes with `git status` as the check before hand-back.

## Evaluate

**Repo-root `CLAUDE.md`, not `GoblinSiege 5.8/CLAUDE.md`, and that choice is the main judgement in
this ticket.** The project-local file's entire top half is VibeUE auto-generated ("AI agent guide",
§1-§10) and that plugin is a separate git repo — a rule added there is lost on a plugin update, the
same reasoning the repo-root CLAUDE.md already gives for why the build gate cannot live in
`BuildAndLaunchGame.ps1`. The repo-root file is also the one both agents and Michael read first.

**Considered and rejected: writing it as a skill.** A skill has to be invoked. This has to apply to
every agent on every task without anyone remembering it, which is what `CLAUDE.md` is for.

**Verified by applying it to myself before closing.** `git status` at hand-back: the only untracked
files I introduced this session are four intentional new source files (`GSHordeSubsystem.h/.cpp`,
`GSGA_Horn.h/.cpp`) and my own ticket records. My scratch — the abandoned #068 pass-4 patch and the
build log — went to the session scratchpad, outside the repo. Nothing to clean.

**Found while checking, NOT mine, NOT touched:** `build_climb.log` at the repo root and
`GoblinSiege 5.8/Tools/ClimbCorpus/` are untracked leftovers belonging to `claude-climbrebuild`
(#070/#074-#077). They are exactly what this rule targets, and deleting them is exactly what the
rule forbids me from doing. Flagged for their author. That the very first application of this rule
turned up a case where the right move was to leave the files alone is a reasonable sign the guard
rail earns its place.

**Not verified:** whether agents actually comply. This is an instruction, and nothing enforces it —
same class of limitation as #073's doc fixes.

## Refine

Changed after my own read: the first draft was three lines and said only "delete your temp files".
Adding the two guard rails roughly tripled it, and that is the right trade in a repo where the queue
exists precisely because agents cannot tell each other's work apart. A terse version of this rule
would have licensed exactly the deletion I just declined to make.

Deliberately not done: no enforcement. A `gsqueue.ps1 done` check comparing untracked files at claim
time against hand-back time would be the real version, and it needs care — the honest case (new
source files, new tickets) looks identical to the leftover case. Raised, not built. It is the same
shape as the check #073 wanted for stale doc references, and if both get built they probably want to
be one pass over `gsqueue.ps1`.
