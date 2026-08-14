---
id: 115
title: Apply the verified bloat-audit findings: delete leftover scripts and staging dupes, untrack build logs and pycache, strip dead C++ and Python
agent: claude-bloat
status: done
claimed: 2026-08-10T19:45Z
build: required
waiting_on:
evaluated: 2026-08-10T19:59:48Z
files: 
  - gs_ue.ps1
  - gs_run2.ps1
  - gs_step.py
  - gs_step2.py
  - gs_build.bat
  - tools/gs_editor.py
  - Baked/_probe.fbx
  - dev/null
  - .gitignore
  - CLAUDE.md
  - Build-GoblinSiege.ps1
  - level-gen/gslevelgen/evaluate.py
  - level-gen/gslevelgen/generate.py
  - level-gen/README.md
  - GoblinSiege 5.8/HANDOFF.md
  - GoblinSiege 5.8/Tools/CodeArchitect/out/PROMOTED.md
  - GoblinSiege 5.8/Tools/CodeArchitect/out/runs/live-003/staging/Source
  - GoblinSiege 5.8/Source/GoblinSiege/Horde/GSHordeSubsystem.h
  - GoblinSiege 5.8/Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_RangedAttack.h
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_RangedAttack.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/GSAIControllerBase.h
  - GoblinSiege 5.8/Source/GoblinSiege/AI/GSAIControllerBase.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/Weapons/Abilities/GSGA_Horn.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/Characters/GSEnemyCharacter.h
  - GoblinSiege 5.8/Source/GoblinSiege/Characters/GSEnemyCharacter.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/Combat/GSDamageExecCalculation.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/UI/GSPlayerHUDWidget.h
---

## Goal

Apply the verified bloat-audit findings: delete leftover scripts and staging dupes, untrack build logs and pycache, strip dead C++ and Python

## Generate

Source of the findings: three multi-agent audit passes over this branch (committed diff, the
uncommitted working tree, and the modified C++). Every item below survived an adversarial verifier
whose default was "not bloat". 25 candidates were REFUTED and are listed under Refine — they are
part of the deliverable, not omissions.

**Deleted outright (20 files):**

- `gs_ue.ps1` — superseded by `gs_ue.py`; documented unusable in three places (the 15.7 GB SSE leak).
- `gs_run2.ps1` + `gs_step2.py` — line-for-line clone of `gs_run.ps1` with one filename changed,
  plus its 3-line stub. `git grep -I -F gs_run2` exited 1.
- `gs_build.bat` — third near-identical raw-UBT wrapper; its only mention was `CLAUDE.md:75`
  listing it as a bypass *not to use*.
- `tools/gs_editor.py` — no callers. **Its knowledge was not discarded**: see Build-GoblinSiege.ps1
  below.
- `Baked/_probe.fbx` — bake probe, header still carrying placeholder `DocumentUrl /foobar.fbx`.
- `dev/null/{post-checkout,post-commit,post-merge,pre-push}` — Windows artefact of a `> /dev/null`
  redirect. Byte-identical to `.git/hooks/*`, and `core.hooksPath` is unset so git never ran them.
- `Tools/CodeArchitect/out/runs/live-003/staging/Source/` (10 files) — 8 byte-identical to the
  promoted code in `Source/GoblinSiege/`, 2 stale. **`staging/NOTES.md` deliberately kept** — it is
  the only copy of the post-review patch list.

**Untracked, left on disk (34 files):** 20 root `build*.log`, 13 `__pycache__/*.pyc`, and
`gs_step.py`. New root-anchored `/*.log`, `/gs_step.py` and `dev/null/` rules in `.gitignore`.

**Dead C++ stripped:**

| File | Change |
|---|---|
| `GSHordeSubsystem.{h,cpp}` | `FGSHordeThreat::Provoker` was written on every registration and never read; dropped the field, the `RegisterThreat` parameter and all 4 call-site arguments (3 in `GSHordeGoblin.cpp`, 1 internal). Removed `UnregisterThreat` (zero callers; threats are only ever removed by `PruneStaleThreats`). |
| `BTTask_RangedAttack.{h,cpp}` | `bDrawing` set 3x, read never — `ShotAtTime` alone times the draw. Removed the field and all writes; the `if (Memory)` block in `OnTaskFinished` became empty and went with it. |
| `GSAIControllerBase.{h,cpp}` | `HandlePerceptionUpdated` had an **empty body**; removed it with its `AddDynamic`/`RemoveDynamic` (binding a no-op is itself a no-op) and the unused `HasPerception()` accessor. **Perception component kept** — see Refine. |
| `GSEnemyCharacter.{h,cpp}` | 3 includes and 2 forward decls orphaned when the combat verbs hoisted to `AGSCharacterBase` in #069. |
| `BTTask_MenaceOrbit.cpp`, `GSGA_Horn.cpp` | 3 unused includes. |
| `GSDamageExecCalculation.cpp` | Deleted the trailing `else` in the plate chain — unreachable for `Armor > 0`, and a no-op at `Armor == 0` since `FinalDamage` is initialised to `DamageAfterRace`. |
| `GSPlayerCharacter.cpp` | `Input_BlockStart`/`Input_BlockStop` re-implemented `AGSCharacterBase::StartBlocking`/`StopBlocking` verbatim; now delegate to the base. Handlers kept — they are what the input bindings point at. |
| `GSPlayerHUDWidget.h` | Two stacked, mutually contradictory doc blocks merged into one. |

**Dead Python stripped:** `origin_for()` (24 lines, zero call sites — proven by AST walk, not just
grep) and the unreachable `__ROOF_TOO_NARROW__` branch in `evaluate.py` (commit `1ba0a7b` deleted
the only emitter and left the consumer).

**Knowledge preserved rather than deleted (the part that is not cleanup):**

- `Build-GoblinSiege.ps1:83` was using **exactly** the naive `Get-Process UnrealEditor` check that
  `tools/gs_editor.py` existed to warn against — a cached .NET `Process` object outlives the process
  it describes, which aborted a build as "STILL RUNNING" on 2026-08-05 with the log already reading
  `LogExit: Exiting`. Deleting the tool and leaving the bug would have been a net loss, so the check
  was ported to `tasklist` (live OS process table) and the full rationale moved into the script.
- `HANDOFF.md:170` claimed "arrows do not respect RaceTag" — **false since #038**. Rewritten to
  record the surviving fact instead: an arrow STICKS in an ally for zero damage, which is Michael's
  settled 2026-08-06 ruling, taken with the horde case explicitly on the table.
- `CLAUDE.md:75` and `PROMOTED.md:8` repointed so neither strands a deleted path.
- Four in-source comments naming the deleted perception handler retensed.

## Evaluate

**Verified by evidence:**

- `python level-gen/test_pipeline.py` — **all 21 fixtures pass** after both Python deletions,
  including the roof-coverage checks that live in the edited function.
- `Build-GoblinSiege.ps1` parses clean (`[Parser]::ParseFile` → PARSE OK).
- Repo-wide grep for each deleted C++ symbol: `HasPerception` 0, `UnregisterThreat` 0, `bDrawing` 0,
  `Provoker` 0. `HandlePerceptionUpdated` returns 4 hits, **all prose comments**, all retensed.
- Every removed `#include` re-checked against its file: `GSEnemyCharacter.cpp` (no
  `AbilitySystemComponent`/`GSTags::`/`FGameplayTag`/`GameplayAbility`), `BTTask_MenaceOrbit.cpp`
  (no `Engagement`/`CharacterMovement`), `GSGA_Horn.cpp` (no `GameplayStatics`) — all clean.
- `git check-ignore -v`: `build_climb.log` and `gs_step.py` now ignored;
  `content-pipeline/out/_run1_unverified/run.log` and `coding-agent/run.log` deliberately NOT
  ignored, confirming the root anchor did not bury the kept pipeline records.
- `AGSCharacterBase::StartBlocking`/`StopBlocking` confirmed to exist at `GSCharacterBase.cpp:474`
  and `:479` before the player handlers were pointed at them.

**NOT verified — this is the honest gap:**

- **Nothing here has been compiled.** The build gate refuses while this ticket is open, so every
  C++ change is static-analysis-verified only. This is the one thing that could still bite: removing
  an `#include` is the classic way to break a build through a *transitive* dependency that no grep
  of the file itself can see. `build: required` is set. **The build is the acceptance test for this
  ticket, and it has not run.**
- No PIE. Behaviour is argued to be unchanged, not observed to be.

**Touched outside the claimed file list:** `Horde/GSHordeGoblin.cpp` (3 call sites forced by the
`RegisterThreat` signature change), and comment-only retensing in `AI/Tasks/BTService_AcquireTarget.h`
and `Horde/GSHordeSubsystem.h`. None was claimed by another open ticket — the queue was empty.

**Owes AGENT_STATE.md a DECISION line:** Michael chose during this ticket to keep the `Provoker`
removal rather than preserve the attribution data for future courier/Warren work, on the grounds
that re-adding a field and a parameter is trivial if a feature ever needs it.

## Refine

**Changed in response to my own evaluation:**

- `gs_step.py` was on the delete list. Reading `gs_run.ps1:32` showed it is a **scratch buffer** the
  runner executes by absolute path — deleting it from disk would have broken Michael's local
  editor-run loop. Downgraded to `git rm --cached` + an ignore rule, which restores exactly what
  `main` has (present on disk, absent from git).
- `tools/gs_editor.py` was on the delete list with "lift the docstring into a comment". A comment
  next to a known-broken check is worse than useless, so the check itself was fixed.
- The audit also proposed collapsing the three plate branches at `GSDamageExecCalculation.cpp:308-326`
  into one. **Deliberately not done**: they are explicit no-ops that document *why* a bow and a flank
  get no mitigation, and #091's PIE figures were taken against this shape.

**Deliberately left undone:**

- **The `UAIPerceptionComponent` itself stays.** Its only C++ consumer was the empty handler now
  deleted, so on the evidence it is dead — but `BT_Militia.uasset` is binary, modified on this
  branch, and could hold a stock BT node or EQS query reading it. Removing it needs an in-editor
  check first. This is the single largest remaining item and is a real follow-up, not a hedge.
- **The `GSDamageExecCalculation` arc-helper de-duplication** (~20 verbatim-duplicated lines at
  :287 vs :226). Four details must survive exactly or the mitigation numbers move with no compile
  error: false on null instigator, false on a near-zero flattened vector, flatten Z *before*
  normalising, and `>=` against `Cos(Arc * 0.5f)`. Wants its own ticket with #091's PIE figures
  re-checked after (PLATE-FRONT 25.0 → 1.5, PLATE-GAPS flank 25.0 → 25.0).
- **`compose_house_synthesised()`** (151 dead lines) — ticket 066 records a *deliberate* decision to
  keep it. Not an auditor's call to reverse; Michael's decision item.
- **The five `*_Backup_2026080*/` directories (~95 MB)** and `Baked/*.fbx` — `git rm --cached`
  candidates, but untracking removes them for anyone else on the branch. Michael's call.
- **Shipping guards on the three debug-command files** (`GSDebugCommands` 584 lines,
  `GSRaidDebugCommands` 596, `GSBurnDebugCommands` 543, none behind `#if !UE_BUILD_SHIPPING`).
  Refuted as bloat — `AGENT_STATE.md:436` has an open `[ELIGIBLE]` item depending on those exact
  commands. Pre-ship hardening ticket, not cleanup.
- **`NotifyCourierDelivered` / `NotifyGoblinSpentOnWarren`** (33 lines, zero callers) — refuted:
  untracked, never-committed scaffolding for a GDD §2.6 feature that does not exist yet. Unbuilt is
  not dead.
- **`GS.Combat.Debug` defaults to 1**, the only debug cvar in the project that is on by default, so
  melee trace spheres and on-screen combo text draw in every session. Refuted as bloat (live,
  integrated tooling) but it is a defaults question worth Michael's answer.
