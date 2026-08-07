# Goblin Siege — Goal-Oriented Coding Agent

**Assignment #5 · Michael Klein**

An agent that reads the *Goblin Siege* GDD, scans the project's 135-file Unreal C++ codebase,
works out which designed features do not exist yet, argues for one of them, and writes it.

```bash
python gsagent.py run          # all six stages
# or one at a time:
python gsagent.py scan-gdd     # 1. what does the design document require?
python gsagent.py scan-code    # 2. what exists?          (static — no API calls, free)
python gsagent.py gaps         # 3. implemented / partial / missing, per feature
python gsagent.py plan         # 4. rank the missing work and pick one
python gsagent.py generate     # 5. write the code
python gsagent.py verify       # 6. cross-check it against the real codebase + engine
```

Requires `ANTHROPIC_API_KEY` in a gitignored `.env` (in `coding-agent/`, the repo root, or
`GoblinSiege 5.8/`). Model: Claude Opus 5, adaptive thinking, structured outputs. `numpy` is not
needed; the only dependency is `anthropic`.

Each stage writes its output to `out/` as JSON, so any stage can be re-run without repeating the
expensive ones, and every decision is inspectable after the fact.

---

## What the agent built

**The crouch-and-confirm stealth detection model (GDD §2.4)** — 981 lines across five files in a
new `Source/GoblinSiege/Stealth/` module folder:

| File | Purpose |
|---|---|
| `GSStealthTypes.h` | Stance enum (Standing/Crouched/Hidden), `FGSDetectionTuning` (base range, crouch and cover multipliers, the 1.5s confirm hold, forget window, cone half-angle), doze tuning, per-target sighting record |
| `GSSightPerceptionComponent.h/.cpp` | The per-observer spine: cone + range + line-of-sight, five-trace cover exposure, the confirm timer that resets on LOS loss, watchman doze with a drooping vision cone, debug draw |
| `GSStealthSubsystem.h/.cpp` | The confirm funnel — dedupes, emits **exactly one** soft signal into `AGSGameState::ReportConfirmedSighting()`, and keeps the "First Spark Unseen" ledger for the §2.9 +40 bonus |

The agent picked this itself. I did not choose the feature — the whole point of the exercise is
the reasoning layer, so pre-selecting the target and back-filling a rationale would have defeated it.

---

## Why the agent selected it

The prioritiser ranked 16 missing-or-partial features and chose the stealth detection model. Its
reasoning, verbatim from `out/plan.json`:

> It is the only candidate that wins on every criterion at once: explicitly Never cut, scheduled
> for week 1 with that week already past, and completely absent rather than partially scaffolded.
> It is also the widest unblocker left — the alarm system has phase and source enums but literally
> no code that can emit a signal into them; takedown needs an unaware state; the Scout's crouch
> stance has no detection consumer to modify; noise investigation reuses the same perception spine;
> and First Spark Unseen needs a confirm counter to track. Crucially it is one-pass buildable in
> pure C++ […] No art, no animation, no level content, no dependency on unbuilt systems.

The criteria it weighed are read out of the GDD rather than hardcoded — §4.5 names a "Never cut"
quintet and a pre-committed cut order, and the extractor records both per feature, so the ranker
knows that *coin toss* is third in line to be sacrificed and *the stealth core* can never be.

The top of the ranking:

| # | Feature | Never cut | Week | Risk |
|---|---|---|---|---|
| 1 | **Crouch-and-confirm detection model** | **yes** | W1 | low |
| 2 | Four-phase alarm escalation with soft signals | no | W2 | medium |
| 3 | Noise events and guard investigation | no | W1 | low |
| 4 | Raid objective tracking, clock and portal collapse | no | W2 | medium |
| 5 | War-horn summoning and horde goblin pool | yes | W3 | medium |

Runner-up rejected: the alarm escalation, because it is downstream of exactly the thing that does
not exist — it already has `EGSAlarmPhase` and `EGSAlarmSource` enums but nothing that can produce
a signal to feed them.

Full gap table — **6 missing (all at high confidence), 10 partial, 1 implemented** — is in
`out/REPORT.md`. The one it judged fully implemented is the torch/fire/burn-objective system, which
is correct: that is what the last several weeks of tickets actually built.

---

## How each requirement is met

**Read the GDD.** Heading-scoped sectioning of `goblin-siege-design-document.md`, then a
structured-output call extracting 17 features. Each carries the section it came from, the
player-facing behaviour, **acceptance signals** (what would exist in a UE C++ codebase if it were
built — roles and responsibilities, deliberately not guessed filenames), search keywords, and the
scheduling metadata §4.5 states about it.

**Scan the codebase.** Pure static pass over 135 `.h`/`.cpp` files — no LLM, no cost. Extracts
`UCLASS`/`USTRUCT`/`UENUM` names, `UFUNCTION`s, gameplay tags and subsystem base classes: 86
symbols, 27 tags, 3 subsystems.

**Detect gaps.** Per feature, files are ranked by keyword hits weighted **symbol and path far above
body text**, and the judge is handed the candidates *plus the project's entire symbol list*, and
required to cite a real symbol for any claim of presence.

> This weighting is the whole game. A naive text grep for the `bind` mechanic returns 32 files —
> every one of them a delegate `BindUObject` call. `Horn` hits mesh names. Symbol-level matching
> plus "cite a symbol whose *role* matches" is what makes the difference between a gap detector
> and a word counter. The prompt also states the converse explicitly: absence of evidence in a
> *complete* symbol index **is** evidence of absence.

**Prioritise.** Weighs, in order: what the GDD protects (never-cut > cut-order position), what
unblocks other work, whether the scheduled week has arrived, whether one pass can deliver something
real, and whether finished assets are already waiting on it. It must name the runner-up and say why
it lost — a ranking with no rejected alternative is not a decision.

**Generate code.** Real files, given the relevant GDD sections, the full symbol index (with
"do not reference anything absent"), and **two real project files as style exemplars**.

**Verify.** A sixth stage I added after the fact — see below.

---

## Were you able to run this in your game?

**The code is placed in the project and statically verified; it has not been compiled yet, and
that is a project constraint rather than an oversight.**

`GoblinSiege 5.8/CLAUDE.md` constraint 1 — the defining constraint of this project — is that agent
sessions can read and write files on the dev machine but cannot *execute* on it: no compiles, no
editor commands. Builds are an explicit human step. On top of that, the repo runs an agent work
queue with a build gate (`Nobody compiles until the queue is empty`), and other tickets were open
while this ran. So the agent files a ticket and hands over, which is what every other agent on this
project does.

The five files are in `Source/GoblinSiege/Stealth/` under queue ticket **#059**. To compile:

```powershell
cd D:\goblinRaid
.\Build-GoblinSiege.ps1            # gated: refuses if the editor is open or the queue is busy
```

No `GoblinSiege.Build.cs` change is required — the code uses only modules already declared
(`Engine`, `Core`, `CoreUObject`, `GameplayTags`).

### What *was* verified, without a compiler

The `verify` stage cross-checks the generated code against reality:

- **Type references** — every `AGS*`/`UGS*`/`FGS*`/`EGS*` token must exist in the project's symbol
  index or be defined by the generated files. **0 unknown.**
- **Method calls** — checked against the project source *and a cached index of 138,393 identifiers
  harvested from the engine's `Runtime` headers*. **0 unresolved.**
- **Header hygiene** — `#pragma once`, `.generated.h` present and **last**, each `.cpp` including
  its own header. **0 problems.**

It also confirmed the one thing most likely to break a build: `GSStealthSubsystem.cpp` calls
`AGSGameState::ReportConfirmedSighting()`, and that method genuinely exists at
`Core/GSGameState.h:69`. The agent wired into a real seam rather than inventing one.

This is not a compiler. It cannot check types, overload resolution, or include order beyond the
`.generated.h` rule. It catches the failure mode this kind of agent is actually prone to —
confidently calling something that does not exist — and nothing more.

---

## What went wrong, and what that cost

**The generator silently truncated.** The first generation run died with
`JSONDecodeError: Unterminated string`. The cause was mine: I checked `stop_reason` for `refusal`
but not `max_tokens`, so a response cut off at the 32k output cap was handed straight to
`json.loads`. Five C++ files needed **37,147 output tokens**. Fixed by raising the cap for
generation, and — more importantly — by detecting `max_tokens` and saying so, since the parse error
pointed at the wrong thing entirely.

**My verifier was wrong before the generated code was.** Its first run reported 7 problems. All
seven were false positives:

| Reported | Reality |
|---|---|
| 3 unknown `FGSOn*` types | `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*` — declared by macro, invisible to a `struct`/`class` regex |
| `UGSSightPerceptionComponent` unresolved call | A constructor definition, parsed as a method call |
| `LineTraceTestByChannel` unresolved | Real `UWorld` engine API — the checker only knew project symbols |
| 2 × "does not include its own header" | They do, as `#include "Stealth/GSFoo.h"` — this module sets `PublicIncludePaths.Add(ModuleDirectory)`, so module-root-relative includes are the house convention, which I confirmed against `Core/GSGameState.cpp` before changing anything |

Each got a real fix rather than a suppression: delegate macros feed the local type set, constructor
names are excluded, the engine header index was built, and the include check matches by path
suffix. A verifier that cries wolf is worse than no verifier, because it trains you to ignore it.

**Risks the agent flagged about its own output** (`out/generated.json`) — these are its words, and
the sharpest one is genuinely self-critical:

- The confirm de-duplication is a time window per observer/target pair, and *"is not the same as
  the §2.6 corroboration rule proper"*.
- Detection ticks server-only, so a dedicated-server client would not see the doze droop.
- Cover exposure traces `ECC_Visibility` only; foliage on another channel reads as fully exposed.
- No watchman actor class, no automation tests — the 1.5s hold was verified by construction and
  debug draw, not by a functional test.

---

## An unplanned result worth reporting

Hours before this ran, a *different* pipeline (Assignment #4) found that GDD §2.4 and §2.6
contradicted each other about what an unconfirmed sighting costs, and Michael ruled:
*half-confirmed means confirmed but uncorroborated*. The GDD was updated (commit `369ab26`).

This agent then read the corrected document and emitted, in `GSStealthSubsystem.cpp`:

```cpp
// Exactly one uncorroborated soft signal: Quiet -> Suspicious, and it takes a second signal
// to escalate past that (design doc §2.4, §2.6).
```

That is the ruling, implemented. Nobody told the agent about it — it was simply in the document it
was asked to read. It is a small thing, but it is the clearest evidence I have that the pipeline
is genuinely GDD-driven rather than pattern-matching a generic stealth system.

---

## Cost

Measured from the run logs, not estimated:

| Stage | Calls | In | Out |
|---|---|---|---|
| scan-gdd | 1 | 15,340 | 6,501 |
| gaps | 17 | 45,576 | 16,915 |
| plan | 1 | 11,304 | 7,078 |
| generate | 1 | 7,144 | 37,147 |
| **total** | **20** | **79,364** | **67,641** |

At Opus 5 rates that is **≈ $2.09**, plus **≈ $0.84 wasted** on the generation call that truncated
at the 32k cap before I fixed it — **≈ $2.93 all-in**. `scan-code` and `verify` cost nothing (pure
static analysis), and the engine symbol index is cached after its first build.

## Files

```
gsagent.py                  the agent (six stages)
out/features.json           17 features extracted from the GDD
out/codebase.json           static index: 135 files, 86 symbols, 27 tags
out/gaps.json               per-feature verdict + evidence + missing pieces
out/plan.json               full ranking, the choice, and the rejected runner-up
out/generated.json          generated files, integration steps, self-flagged risks
out/verify.json             the static cross-check result
out/REPORT.md               human-readable run report
out/generated/Stealth/      the five generated C++ files (also placed in the project)
```
