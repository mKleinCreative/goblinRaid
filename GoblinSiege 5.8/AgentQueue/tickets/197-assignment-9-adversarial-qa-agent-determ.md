---
id: 197
title: Assignment 9: adversarial QA agent + deterministic unit-test suite (editor Python, no C++, no build)
agent: claude-qaagent
status: done
claimed: 2026-08-19T20:38Z
build: none
waiting_on: Michael paused the work 2026-08-19T21:41Z - other issues take priority. Nothing here is half-written into the game; the tooling is self-contained under Tools/QA and touches no C++, no assets and no config. Resume by running `.\Tools\QA\Run-Tests.ps1`.
evaluated: 2026-08-19T21:43:03Z
observed: 2026-08-19T21:42:40Z | 26-case deterministic suite ran in PIE and was watched: 15 pass / 4 fail / 7 skip. Player ran off L_CombatArena and fell forever (MOVE_FALLING, z=-13333 at terminal -4000/s, no kill volume); BP_CastleGuard01_C_0 fell out of the level at BeginPlay and was still accelerating downward at z=-168085; StartCarry(self) returned True so the player carried themselves; TryLightAttack() returned false on a fresh full-health pawn.
scenario: Unattended editor launched by Run-Tests.ps1, PIE on /Game/Maps/Test/L_CombatArena, player pawn driven by the suite. NOT observed on L_Tutorial_Island - the island launch produced no results and the adversarial agent has never been run.
files: 
  - Tools/QA/gs_qa_agent.py
  - Tools/QA/gs_qa_tests.py
  - Tools/QA/gs_qa_core.py
  - Tools/QA/Run-QA.ps1
  - Tools/QA/Run-Tests.ps1
  - Tools/QA/README.md
---

## Goal

Assignment 9: an adversarial QA agent that runs inside the game trying to break it and
emits a structured findings report, plus - Michael's addition mid-task - a deterministic
test suite with readable output that he can run himself without spending model tokens.

Constraint that shaped the whole design: the build gate is shut (7 open tickets, all
STALE) and Michael's ruling was "leave them, work Python-only". So this is editor Python
end to end. No C++, no build, no asset edits.

## Generate

**`Tools/QA/gs_qa_core.py`** - shared harness. PIE lifecycle (open map, StartPIE, poll
for a *usable* world rather than trusting the async return), a generator-based scheduler
(a `while` loop in editor Python blocks the game thread, so PIE never ticks and nothing
under test ever runs - every routine yields how long it wants to wait), a log tailer over
`MyProject.log`, the `Finding` record, and the JSON/CSV/text writers.

**`Tools/QA/gs_qa_agent.py`** - the adversarial agent. Three layers at different rates:
a seeded POLICY that picks the next hostile behaviour, a per-frame DRIVE that applies
movement intent, and a per-frame MONITOR that checks seven invariants. "Broken" is stated
up front rather than left to a human eye: the pawn stays in the world; health and stamina
stay inside their declared ranges; held movement input produces movement; transient
states (channel, attack, fall) terminate; horde pool accounting conserves; the engine log
stays free of ensures and AccessNones; no frame exceeds 250ms. 14 behaviours, from
`sprint_into_geometry` and `ledge_dive` to `carry_abuse` (including carrying your own
pawn), `horn_flood`, `teleport_probe`, `stamina_abuse` and `raid_state_abuse`.

**`Tools/QA/gs_qa_tests.py`** - 26 deterministic cases across player, stamina, movement,
combat, horde, interaction, raid and world. Fixed inputs, fixed expectations, PASS/FAIL/
SKIP where a skip is never counted as a pass. Destructive cases (DebugKill, EndRaid) are
ordered last so they cannot poison what runs after them.

**`Run-Tests.ps1` / `Run-QA.ps1`** - one command each, unattended, editor launched and
closed for you. Run-Tests exits 1 when anything failed, so it can gate a commit.

Four engine-level obstacles had to be solved before any of it ran, all now commented at
the site of the fix so the next session does not rediscover them:

1. **`-ExecutePythonScript` exits the editor as soon as the script returns**, so a tick
   callback never sees a frame. Switched to `-ExecCmds="py <path>"`.
2. **`-ExecCmds` re-fires on every PIE start.** This tooling starts PIE itself, so each
   start re-ran the script and armed a second driver - six full passes in nine minutes,
   none reaching its own report. Fixed with a process-level singleton guard.
3. **UWorldSubsystems are unreachable via `unreal.get_*_subsystem`.** The only route is
   `unreal.SubsystemBlueprintLibrary.get_world_subsystem`. Without it the horde and raid
   cases silently skip.
4. **CharacterMovement and the capsule are reflected as PROPERTIES, not `get_*()`.** The
   first run reported `movement_mode=unknown` and "no capsule component" on a pawn that
   plainly had both.

## Evaluate

**Verified by watching it run**, not by read-back: 26 cases executed in PIE on
L_CombatArena, 15 passed, 4 failed, 7 skipped, 19.7s. Report on disk at
`Saved/QAReports/tests_latest.txt` and `tests_20260819_143712.json`.

### FINDING - falling out of the world never ends, and that is Michael's call to fix

Michael's ruling, 2026-08-19: *the arena is not a real map and the agent keeps running
off it - assume players will do that too; if they fall off the map, kill them after a
certain distance so they are not falling forever.*

Two observations behind it, both from PIE:

- **The player runs off `L_CombatArena` and falls forever.** `MOVE_FALLING` for the whole
  5s window, then indefinitely: the log shows `pos Z=-13333` and dropping at a terminal
  `vel Z=-4000`, still printing the per-frame climb trace. No kill volume, no respawn, no
  bounds clamp. `movement.jump_leaves_and_returns_to_the_ground` is the case that catches
  it, and `movement.input_moves_the_character` "travelled 56055uu in 1.5s" is the same
  fall measured sideways.
- **A placed defender falls out of the arena at BeginPlay and never stops.**
  `BP_CastleGuard01_C_0` was at z=-168085 with its `GSAIControllerBase_6` trailing at
  z=-135799, and the number is *different every run* because it is still accelerating
  downward when the check reads it. Nothing in the level notices.

The fix Michael wants is a distance/depth kill rather than per-map kill volumes: below
some Z relative to the level, the character dies (player -> the existing lives/respawn
path, AI -> destroy and release its engagement slot, which #106 showed leaks otherwise).
That belongs in `AGSCharacterBase` or the game mode, needs a build, and so is NOT done
here - this ticket touches no C++ by ruling. Recording it so it is not lost.

### The other two failures

- **`carry.refuses_to_carry_the_carrier` FAILS**: `UGSCarryComponent::StartCarry(self)`
  returns True. The player can pick themselves up - a recursive attachment. The null case
  (`StartCarry(None)`) is correctly refused, so the guard exists and simply does not cover
  the self case.
- **`combat.light_attack_is_accepted` FAILS**: `TryLightAttack()` returns not-True on a
  fresh, full-health pawn on the arena. NEEDS MICHAEL'S EYE - this could be correct
  behaviour (no weapon in the active wheel slot at spawn) or a real regression, and I am
  not going to assert which from inside the harness. Blocking, hostility and the dead-pawn
  refusal all behave, so the ability system itself is alive.

### What is NOT verified

- **`L_Tutorial_Island` has never produced results.** Every run so far reports only
  L_CombatArena, which is why 7 cases skip with "no UGSHordeSubsystem / no
  UGSRaidDirector in this world" - those subsystems do not exist on the arena. The
  per-map launcher rework landed but the island launch still yielded nothing, and I
  paused before diagnosing it. Until that is fixed, the horde, raid and allied-goblin
  cases are untested, not passing.
- **`gs_qa_agent.py` has never been run.** It compiles and shares the harness the suite
  proved out, but not a single adversarial behaviour has executed. Nothing in the report
  above comes from it.
- **The editor access-violates during PIE teardown** (`python311.dll`, inside the
  reference collector's pass over Python-held UObjects) and `CrashReportClientEditor`
  then relaunches the editor, re-arming the script. This is my tooling's interaction with
  the engine, NOT a Goblin Siege bug, and it is deliberately not written into any report
  as one. Worked around two ways - the report is flushed BEFORE any teardown, and the
  launcher runs one editor per map so a mid-run teardown never happens - but the
  underlying crash is not fixed.

### Owed to AGENT_STATE.md

DECISION: QA tooling is editor Python under `Tools/QA`, driven by a Slate post-tick
callback with generator routines, one editor launch per map. The three engine gotchas in
Generate (ExecutePythonScript exits / ExecCmds re-fires / world subsystems need
SubsystemBlueprintLibrary) belong in the CLAUDE.md Editor Python section.

## Refine

Changed in response to my own evaluation, before Michael paused the work:

- Reports are now written **before** PIE teardown, not after, because the first two runs
  did all 26 cases and then lost every result to the teardown crash.
- The launcher runs **one editor per map** and kills the crash reporter between launches,
  because the reporter's relaunch turned one crash into an endless crash-restart loop.
- `write_csv` takes plain dicts so the report can be rebuilt from the merged session file
  rather than from live objects that do not survive a launch boundary.
- Default `GSQA_QUIT` flipped to False, so running these by hand from the Output Log in
  Michael's own editor cannot close it out from under him.

Deliberately left undone, and why:

- **The kill-Z fix is not implemented.** It is C++, it needs a build, the gate is shut and
  the ruling was Python-only. Written up above instead.
- **`combat.light_attack_is_accepted` is left FAILING rather than relaxed.** Making the
  assertion softer to get a green run would delete the only signal there is. It stays red
  until Michael says which behaviour is correct.
- **The island run is not diagnosed** and the adversarial agent is not exercised. Both are
  the first two things to do on resume, in that order - the agent is worth far more on the
  island than on a test arena, as Michael pointed out.
- **No `README.md` yet** (it is claimed in `files` but not written). It is an assignment
  deliverable and needs the agent's real findings in it, which do not exist yet.
