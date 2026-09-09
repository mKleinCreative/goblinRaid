# Assignment #10 — Complete AI Dev Pipeline

## Student & Game Overview

**Student Name:** Michael Klein

**Capstone Game Title:** Goblin Siege

**Game Concept Brief:** A third-person raid game where you play the monster, not the hero — a
goblin sent by an unseen Overlord through a runic portal to burn, rob, and escape a small human
hamlet before the county organizes a response. Core loop: approach, work quietly or go loud on
purpose, summon and direct a horde of up to ten goblins from the Warren, destroy three required
objectives (Market, Statue, Windmill) through fire and physical destruction, then extract through
the portal before a 30-minute clock runs out. Every system in the game — combat, fire spread,
Chaos-fracture destruction, horde AI, world corruption, the front-end menu — was built by AI
agents operating the Unreal Editor directly, coordinated through a claim-based ticket queue.

## Deliverable 1: Playable Link

**Playable Game Link:** https://drive.google.com/file/d/1fllurR6kmJVv8fUKQ8jR6Yy2rsOb1SYc/view?usp=drive_link

Verified live: "Anyone with the link — Viewer" access, no sign-in required. Windows Shipping
build, ~2.6GB zipped. Unzip and launch `MyProject.exe`; the Microsoft Visual C++ 2015-2022 x64
Redistributable is bundled inside the zip (`Engine/Extras/Redist/en-us/vc_redist.x64.exe`) for
anyone missing it.

*Honest note against the gate criterion's exact wording:* the download itself will likely exceed
2 minutes on an average connection given the file size, even though the game needs zero setup or
configuration once launched.

## Deliverable 2: Pipeline Source Code & Engine Integration

**Pipeline Repository Link:** https://github.com/mKleinCreative/goblinRaid/tree/fire-mill-debris-and-vfx-work

Public, no login wall. 144 commits, 395 completed agent tickets under `AgentQueue/tickets/`.

**Pipeline Run Video Link:** https://drive.google.com/file/d/1XpZOACXwVOj5xqG6pa1w5SOrNLx9rUMh/view?usp=sharing

Verified live: "Anyone with the link — Viewer" access, no sign-in required.

### Integration Breakdown

**Target Game Engine:** Unreal Engine 5.8

**Automated Flow Description:** The engine integration is VibeUE, an MCP server plugin that
extends Unreal's native AI toolset system. It lets an agent run real Python inside the running
Editor process, calling the same underlying engine functions the Editor's own UI calls —
`unreal.BlueprintService`, `unreal.EditorActorSubsystem`, `unreal.BlueprintEditorLibrary`, and
Epic's own engine toolsets. There is no intermediate file format, no reformatting, and no manual
reimport step between an agent's call and the result: a component property change is visible in
the Editor viewport the instant the call runs, a Blueprint compile happens in-process, and a
saved asset is the same `.uasset` the Editor itself would have written.

**One documented remaining manual step:** editing a `UBehaviorTree` asset's live graph (not just
its underlying data) cannot be done safely from Python in this engine version — a Python-injected
behavior tree node works until the asset is next opened in the Behaviour Tree editor, at which
point Unreal regenerates the graph from its own serialized state and silently discards the
injection with no error (documented in `AgentQueue/tickets/213-hold-is-unbuilt-bt-hordegoblin-has-no-br.md`).
That specific class of edit is deliberately done by hand in the interactive editor rather than
automated, and is the one gap the pipeline knowingly leaves for a human.

## Deliverable 3: Pipeline Audit & Cost Analysis

See `REFLECTION_ESSAY.md` in this folder (condensed to fit the 1-page requirement) for the full
Pipeline Production & Functionality breakdown, Architectural Reflection, Cost Analysis, and
Mid-Project Cost-Reduction Change with real, measured figures.
