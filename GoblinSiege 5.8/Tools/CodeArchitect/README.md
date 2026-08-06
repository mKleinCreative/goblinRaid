# Code Architect — Goblin Siege goal-oriented coding agent

**ELVTR Assignment #5.** Reads the GDD, scans the codebase, detects gaps, prioritizes with an
explicit utility rubric, and generates code for the top eligible feature. Raw orchestration
(direct API calls, no framework) — per the S07 slides and this project's standing pattern
(Bark Foundry made the same pivot; its S04 audit is the argument).

## The loop

```
receive -> scan -> diff -> score -> build -> remember
```

| Stage | Module | What it does |
|---|---|---|
| receive | `ca/gdd.py` | Parses GDD §12.1 systems inventory + §12.2 blocks. Status column treated as a CLAIM — never trusted |
| scan | `ca/perception.py` | Live-tree perception: every UCLASS wired/stubbed/missing, symbol index, Content data-layer sweep (DA_/BT_/GE_/...). **Shared module** — Bark Foundry imports this to close its "read scene files" gap |
| diff | `ca/scorer.py` | Evidence check per feature from `features.json` (classes, symbols, content assets) |
| score | `ca/scorer.py` | `utility = (2·impact + urgency + 3·unblocked_missing) / effort`, hard gates: `requires_editor` (never headless-generatable — routed to supervised sessions), `depends_on`, block order A→H on ties |
| build | `ca/generator.py` | LLM writes the missing classes with real headers as context. Output **staged** under `out/runs/<id>/staging/` — never written into `Source/` (human gate = post-run review) |
| remember | `ca/state.py` | `AGENT_STATE.md` at repo root: BUILT / DECISIONS / NEXT / FAILED / RUNS. NEXT rewritten from the ranking each run |

Every run writes a **blackboard** (`out/runs/<id>/blackboard.md` + `events.jsonl`): what it
perceived, what it scored (full table, before generation), the exact prompts it issued, and every
generated file with hashes — logged in that order, enforced by test. Agent-side by default;
`promote()` items additionally land in `out/PROMOTED.md`, the one file Michael reads between runs.

## Usage (on doomsday)

```powershell
cd "D:\goblinRaid\GoblinSiege 5.8\Tools\CodeArchitect"
python architect.py --project-root "D:\goblinRaid\GoblinSiege 5.8"                  # full run (provider auto-resolves)
python architect.py --project-root "D:\goblinRaid\GoblinSiege 5.8" --scan-only      # perception + ranking only
python architect.py --project-root "D:\goblinRaid\GoblinSiege 5.8" --ensure-editor  # reopen the editor if closed
python architect.py --project-root "D:\goblinRaid\GoblinSiege 5.8" --build-and-relaunch  # editor-closed build, then relaunch
```

`--ensure-editor` also self-heals the stranded-UBT state (kills the orphaned dotnet) before
declaring the editor healthy. `--build-and-relaunch` prefers `Plugins/VibeUE/BuildAndLaunchGame.ps1`
and falls back to Build.bat + a fresh launch. All command knowledge is ported from CLAUDE.md.

**Providers.** `--provider auto` (default): uses the API (`ANTHROPIC_API_KEY`, separate
Claude-Platform pay-as-you-go billing) if a key is set; otherwise **headless Claude Code**
(`claude -p`), which bills Michael's **Max subscription** — no API credits needed, and the
right default on doomsday; otherwise fixture replay (`fixtures/`, Bark Foundry's pattern).
`CA_CLI_MODEL` optionally pins the CLI model. Tests: `python -m pytest tests/ -q` — offline.

## Files

- `features.json` — the feature knowledge: GDD system ↔ verifiable evidence ↔ scoring inputs.
  A GDD⇄features drift check runs every run and promotes mismatches to Michael.
- `docs/goblin-siege-gdd.md` — the GDD export the parser reads (re-export from the Claude
  project when the canonical doc revs).
- `AGENT_STATE.md` — seeded copy; the live one lives at the repo root.
