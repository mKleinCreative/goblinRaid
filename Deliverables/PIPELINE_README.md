# Goblin Siege — Pipeline & Engine Integration

This is not a code dump. It's an honest account of the AI agent pipeline that built Goblin
Siege: an Unreal Engine 5.8 project developed almost entirely through Claude Code sessions
driving the editor directly, over roughly seven weeks (2026-07-08 → present), 143 commits and
395 completed work tickets.

## The engine integration: VibeUE

The bridge between Claude and the Unreal Editor is **VibeUE**, an MCP server plugin
(`Plugins/VibeUE/`) that extends Unreal 5.8's native AI toolset system. It doesn't reimplement
editor functionality — it exposes the engine's own services (Blueprint, Material, Niagara,
Landscape, Widget, AnimGraph, StateTree, and more) as Python calls (`unreal.BlueprintService.
build_graph(...)`) that an agent can run in a single round-trip inside the running editor, plus
Epic's own native engine toolsets reachable the same way. On top of that it serves ~88 lazy-
loaded "AgentSkill" packs — domain write-ups (how to build a StateTree, how blueprint graph
building actually works here) that an agent loads on demand instead of guessing at API shape.

Practically, this means an agent isn't editing `.uasset` files as opaque binary blobs from
outside the engine — it opens the real Unreal Editor, calls the same underlying C++ services the
editor UI calls, and can capture a viewport screenshot to visually check its own work before
reporting success.

## The agent swarm and the queue

Multiple Claude Code sessions run against this repo concurrently, each a narrowly-scoped
specialist for one system (fire/VFX, AI/combat, packaging, and so on — visible in commit history
and ticket `agent:` fields as slugs like `claude-fire`, `claude-package`, `claude-acf`). Nothing
else keeps them off each other's files, so before any agent edits anything it takes a position in
`AgentQueue/QUEUE.md`'s claim system:

```
gsqueue.ps1 claim -Agent <slug> -Title "<t>" -Files "a,b"   # take a position
gsqueue.ps1 check -Id <n>                                    # is anyone ahead of me?
gsqueue.ps1 set -Id <n> -Status active                        # start editing
gsqueue.ps1 set -Id <n> -Status review                        # hand back for done
```

The rule that actually prevents damage is the **observed-evidence gate** (rule 3a, introduced by
ticket #136 after three fixes shipped without anyone watching them run — all three turned out
wrong, #113/#116/#118). Closing a ticket requires `observed -What "<what was SEEN>" -Scenario
"<where>"`, and `-What` is phrase-scanned to reject non-evidence like "compiles" or "should work."
An honest escape hatch exists — `done -Unobserved "<reason>"` — but it permanently marks the
ticket **UNOBSERVED** on the board rather than letting it look verified. This is the single
mechanism most responsible for the pipeline being trustworthy rather than merely fast.

Each ticket is a markdown file reporting **Generate → Evaluate → Refine**: what was built, an
adversarial self-judgment of what's actually verified vs. assumed, and what was deliberately left
undone. Nobody compiles the project while the queue has open work — a separate `buildgate` check
enforces that.

## Shared memory

Individual tickets are detailed but numerous (395 of them); nothing re-reads old tickets
automatically. Durable findings — gotchas, dead ends, settled decisions — get folded into a
single file, `AGENT_STATE.md`, that every agent reads at session start and appends to at session
end. It replaced an earlier sprawl of roughly 69 separate memory documents that had drifted out
of sync with each other. Its sections are blunt on purpose: **FAILED** (dead ends, reverted
work), **BUILT** (shipped features with real evidence), **DECISIONS** (settled rulings and
gotchas worth not re-learning).

## A worked example: tickets #387 → #388 → #389

The night of 2026-08-31, an agent set out to fix one bug — a main-menu button that did nothing
outside the editor (`OpenLevel` called with a short map name, which only the editor can resolve).
Following that thread honestly, staying inside the claim/observe discipline, it grew into the
first successful full cook-and-package in the project's history:

- **#387**: found and removed a dead `DoNotCreateDefaultSubobject` call that logged an
  `Error:`-level line on every AI controller construction — harmless at runtime, but Unreal's
  packaging tool (UAT) hard-fails a cook on any `Error:` log line regardless of whether the cook
  itself completes.
- **#388**: the real work — six more distinct, previously-unknown cook blockers found by actually
  trying to package (a map never staged because `-allmaps` isn't a real flag; a corrupted
  Blueprint construction script that only broke on a cook-time load, never in-editor; soft-
  referenced fire/loot/fracture content the cooker doesn't auto-discover without a hard
  reference). Each is logged with an exact root cause and fix, not just "packaging works now."
  It also flags what's explicitly *not* fixed — `BP_GrappleHook`'s rope-placement logic was
  reverted rather than guessed at, left for a human with real design context.
- The result: a real `.exe` that launched standalone and stayed responsive, `Saved/StagedBuilds/
  Windows/` (~3.2GB), ready to hand to a hosting step — see the Playable Link deliverable.

That shape — Goal, then an honest Generate/Evaluate that admits what's unverified, then a Refine
that names what's left — repeats across all 395 tickets. It's the actual unit of work in this
pipeline, not the commit.

## The other skill system: ACF

Goblin Siege is built on the Ascent Combat Framework (ACF), a third-party Unreal plugin with its
own ~40 author-written Claude Code skill packs (`acf-core`, `ai-framework`, `inventory-system`,
etc.) — a second, unrelated skill mechanism from VibeUE's AgentSkills, pulled in locally via
`Register-ACFSkills.ps1` since Claude only discovers skills under a project's own `.claude/
skills/`. Reading the relevant ACF pack before reading engine headers directly has repeatedly
saved a session from re-deriving answers the framework's own author already documented.

## In short

The pipeline is not "an AI writes code, a human reviews a diff." It's several concurrent AI
sessions operating the actual editor, coordinated by a file-claim queue instead of a lock, held
honest by a rule that a claimed fix isn't done until something — usually a human, sometimes a
runtime log in the right scenario — has actually watched it behave correctly, and accountable
through a shared memory file that survives no single session having the whole picture.
