# AGENT QUEUE — Goblin Siege

**Every agent doing work on this project takes a ticket before touching a file.** The queue exists
because on 2026-08-04 two agents edited `GSPlayerCharacter.cpp` inside the same build window and the
only reason it was caught was comparing source mtimes against the DLL (`AGENT_STATE.md`, Ranged
combat entry). One of those two builds described a source tree that no longer existed.

Do not hand-edit ticket frontmatter. `gsqueue.ps1` is the only writer.

```powershell
cd "D:\goblinRaid\GoblinSiege 5.8"
& ".\AgentQueue\gsqueue.ps1" list
```

---

## The six rules

**1. Claim before you edit.** Name every file you intend to write, up front. A file you did not
claim is a file another agent may be holding.

```powershell
& ".\AgentQueue\gsqueue.ps1" claim -Agent aim-arc -Title "Arc + landing materials" -Files "Content/Materials/M_GS_AimArc.uasset,Source/GoblinSiege/Combat/GSAimComponent.cpp" -Build
```

`claim` prints your ticket number and tells you immediately whether anyone is ahead of you.
Use `-Build` if your work will need a compile; that is a request, not permission (see rule 4).

**2. The lower ticket number has right of way.** If a ticket ahead of you is open and claims a file
you claimed, you do not touch that file — you wait. Not "wait a bit and retry anyway": wait until
that ticket is `done` or `abandoned`.

```powershell
& ".\AgentQueue\gsqueue.ps1" check -Id 007      # exit 0 = clear, exit 1 = blocked
```

Run `check` when you claim, and again before your first write if any time has passed. `set -Status
active` re-runs the check itself and **refuses** if someone ahead of you holds your files. While
you are blocked: work the files you *are* clear on, or go `blocked` and tell the orchestrator.
Never edit around a block.

**Say what is stalling you.** `-WaitingOn` takes a ticket number or plain prose, and shows on the
board so the orchestrator can see the hold-up without opening your ticket. Use it for anything you
cannot proceed without — another ticket, a human decision, a capture you need taken:

```powershell
& ".\AgentQueue\gsqueue.ps1" set -Id 007 -Status blocked -WaitingOn "5"
& ".\AgentQueue\gsqueue.ps1" set -Id 007 -WaitingOn "a frame capture with the editor FOCUSED"
& ".\AgentQueue\gsqueue.ps1" set -Id 007 -WaitingOn ""    # cleared, moving again
```

It is optional and settable on its own without changing status. It does **not** enforce anything —
the file locks come from claims and queue position, not from this field.

**3. Present your work Generate → Evaluate → Refine.** Your ticket file has those three headings.
Fill them in *before* you hand the work back — this is the review the orchestrator reads, and
`done` refuses to close a ticket whose sections are still placeholders.

- **Generate** — what you produced. Files touched, what each change does, the calls you made.
- **Evaluate** — judge your own output against the goal, *adversarially*. What is verified and by
  what evidence (a log line, a PIE observation, a compile result — not "should work"). What is
  written but has never run. What you touched outside the goal. What this owes `AGENT_STATE.md`
  as a DECISION or FAILED line.
- **Refine** — what you changed in response to your own evaluation, and what you are deliberately
  leaving undone. "Nothing changed, and here is why the first pass survives scrutiny" is a valid
  answer; silence is not.

**The circuit breaker: three passes at a failing check, then stop and say so.** Refine is bounded.
If a check keeps failing — a build error, a broken test, an evaluator finding, a placement that
will not satisfy its rule — you get three attempts:

- **Pass 1** — the smallest fix that addresses the *documented* failure.
- **Pass 2** — tighten the same fix using the same error context.
- **Pass 3** — final attempt.

Then you **stop**, go `blocked`, and write a **problem statement**: what failed, what you tried on
each pass, what you now believe the real cause is, and what you need in order to proceed. That is a
result, not a defeat — an agent that reports a clean dead end after three passes has done its job.

Do not start over from scratch, and do not change unrelated things hoping something sticks. Both
read as progress and are how a session burns an hour and a build slot moving sideways. You are a
controlled tool that reports back when it hits a limit, not an autonomous guesser; Michael decides
the next move. This is rule 6's STALE principle applied to *work* instead of to *time*.

Then `set -Id <n> -Status review` and report. The orchestrator closes the ticket, not you — except
that you run `done` yourself once the orchestrator says so.

**3a. Somebody has to have WATCHED it. `done` refuses otherwise** (#136).

```powershell
& ".\AgentQueue\gsqueue.ps1" observed -Id 007 -What "<what you SAW>" -Scenario "<what you ran it in>"
```

Two fields, because two different things go wrong:

- **`-What`** — what the software *did*, not what the tooling *said*. A phrase list rejects
  "compiles", "reads back", "no errors", "looks correct" and friends. Every one of those has been
  offered here as proof that something worked while the thing did nothing at all.
- **`-Scenario`** — *where* you saw it. This is the one that keeps being skipped and it is the most
  expensive: #132 and #133 were both tested with `GS.Combat.Duel`, which spawns **defenders**, so
  neither ever ran on a horn-summoned goblin — and #133's blendspace was signed off from the player
  pawn, which exercises **one of its five direction columns**. "In the editor" is not a scenario.
  "PIE `L_CombatArena`, 6 horn-summoned goblins vs a militia patrol" is.

**The escape hatch is real, and it is loud.** A broken editor must never deadlock the queue:

```powershell
& ".\AgentQueue\gsqueue.ps1" done -Id 007 -Unobserved "<why nobody could watch it>"
```

That closes the ticket and marks it `**UNOBSERVED**` on the board permanently. Use it honestly —
it is far better than a confident Evaluate about something that never ran.

**Why this gate exists, in one paragraph.** Every other check in `gsqueue.ps1` inspects the ticket's
*text and timestamps*; none can tell working code from dead code. Ticket **#120** exists solely to
record *"three fixes in a row shipped without anyone watching them run, and all three were wrong"* —
and #120 was itself closed unwatched, its own Evaluate reading *"No PIE. Nobody has watched a
fight."* Every gate passed it. The pattern then recurred twice inside 24 hours (#133, #135). The
lesson had been written down five times and prevented nothing, because **prose is not a gate.**

**4. Nobody builds until the queue is empty.** Compiling while another agent is mid-edit produces
a binary that matches no source tree anyone can name. Before any `Build.bat`, `BuildAndLaunchGame.ps1`,
Live Coding, or in-editor Compile:

```powershell
& ".\AgentQueue\gsqueue.ps1" buildgate           # exit 0 = go, exit 1 = stop
```

Exit 1 means stop, regardless of how ready *your* work is. Only the orchestrator triggers the build,
and only after the gate opens.

**5. Close your ticket.** An open ticket holds its files hostage and keeps the build gate shut.
If you abandon work, say so — `set -Status abandoned` — and revert your edits first.

**6. A STALE ticket is a question for Michael, never a decision you make.** `list`, `claim` and
`buildgate` flag any open ticket older than **2 hours** (`-StaleHours <n>` to change it) and print
the exact question to put to him. From inside the repo a long-running job and a session that
crashed look identical — you cannot tell them apart, and abandoning live work is far worse than
waiting. So ask, then do only what he says:

| he says | you do |
|---------|--------|
| still live | nothing — carry on with unblocked work |
| finished | `set -Id <n> -Status done` (needs G/E/R written) |
| dead session | `set -Id <n> -Status abandoned` — revert its edits first |

**Never abandon a ticket that is not yours without being told to.**

---

## Status meanings

## The status quo ante, for anyone tempted to loosen this

The evidence ladder `gsqueue.ps1` prints when it refuses is not invented; it is this project's own
history, sorted:

| strength | kind of evidence | why it sits there |
|---|---|---|
| strongest | **a human watched it** | Michael previewing ONE montage settled #119 before the other ten were touched |
| | **a runtime log line, right cvar, right scenario** | `GS.Combat.LogAI 1` + a duel settled the recoil punish in one run |
| | **a screenshot you actually opened** | GoblinSiege CLAUDE.md §6 |
| weakest | **a static re-read, a compile, a tool return value** | *not evidence* — see below |

That bottom row is where every expensive failure in this repo has come from: *"a successful tool call
is not evidence"* (2026-08-06), *"verify an editor write against the DISK — never against a
read-back"* (2026-08-08), *"static reads confirm what is configured; they never show what the engine
does with it"* (2026-08-10), and *"every check this project's tooling can perform passes on a dead
asset"* (2026-08-11, four passes at one blendspace).

---

| status | holds file claims | blocks the build | means |
|--------|-------------------|------------------|-------|
| `queued` | yes | yes | claimed, not started |
| `active` | yes | yes | editing right now |
| `review` | yes | yes | G/E/R written, waiting on the orchestrator |
| `blocked` | yes | yes | stalled on another ticket; files may be half-edited |
| `done` | no | no | closed, reviewed |
| `abandoned` | no | no | closed, edits reverted |

`blocked` still blocks the build on purpose: a stalled agent's files may be half-written. The
escape hatch is `abandoned`, not a build that ignores it.

---

## Orchestrator's side

- **The orchestrator is not exempt from rule 1.** Closing a ticket, rendering the board and
  running `buildgate` are queue operations and need no ticket of their own — **writing any repo
  file does.** This is written down because it was breached: the agent that built this queue
  edited seven files across a whole session without a ticket, including `AGENT_STATE.md` while
  ticket #005 held a claim on it. The collision was caught by an editor's staleness check, not
  by the queue. Ticket #010 is the record.
- Read the board before assigning anything; hand overlapping work to one agent instead of two
  where you can.
- When a ticket hits `review`, read Evaluate first — an agent that cannot find a weakness in its
  own work usually has not looked. Push back and let it Refine again rather than closing it.
- Close with `done -Id <n>`, then fold anything durable into `AGENT_STATE.md` (BUILT / DECISIONS /
  FAILED). Tickets are committed, so a closed one stays as the review record — but **nothing reads
  old tickets at run start.** `AGENT_STATE.md` is the memory agents actually load; a finding left
  only in a ticket is a finding the next agent will rediscover.
- Run `buildgate` yourself before the compile. `AGENT_STATE.md` and `CLAUDE.md` both note new
  `UCLASS` types need an **editor-closed full build** — Live Coding cannot register them.

---

## Board

<!-- BOARD:BEGIN -->
### Open - in queue order (lowest id has right of way)

| # | status | agent | title | claimed files | build |
|---|--------|-------|-------|---------------|-------|
| 406 | review | claude-acfsample | Install ACFU FullSample as reference and reconcile the GS-vs-ACF gap notes | .gitignore<br>CLAUDE.md<br>.claude/skills/gs-character-data-asset/SKILL.md<br>.claude/skills/gs-behaviour-tree-wiring/SKILL.md<br>Content/FullSample | none |

**BUILD GATE: CLOSED - 1 ticket(s) still open. Do not build game files.**

### Closed

| # | status | agent | title |
|---|--------|-------|-------|
| 001 | done | claude-perf | Kill the 50s Niagara recompile on L_Tutorial_Island load |
| 002 | done | claude-perf | Diagnose game-thread bound frame (16.9ms, GPU idle) |
| 003 | done | claude-perf | Re-save the remaining 18 PortalVFX Niagara systems (001 fixed only 2 of 20) |
| 004 | done | claude-ranged | Ranged combat: aim framework, bow, torch collision fixes, swap diagnostics |
| 005 | done | claude-ranged | Supplement to #004 - files missed in that claim (same body of work) |
| 006 | done | claude-perf | Defender crowding: stand-off slots so they stop converging on one point |
| 007 | done | claude-ranged | Commit and push the session's work to origin (repo-wide git operation) |
| 008 | done | claude-perf | PIE-verify defender crowding, tune stand-off dials if they do not hold |
| 009 | done | claude-raid | Raid loop: director, runic site, extraction; fix player spawning inside geometry |
| 010 | done | claude-queue | Agent work queue: gsqueue.ps1, protocol, build gate, decision-queue board |
| 011 | done | claude-raid | Portal 4 visuals on BP_GS_RunicSite, and the four missing HUD widgets |
| 012 | done | claude-raid | Kill the per-frame GSDBG|CLIMB print spam on BP_GSPlayerCharacter |
| 013 | done | claude-raid | Burnable buildings: torch through a window or onto the roof sets the house alight |
| 014 | done | claude-raid | Invisible meshes still present in L_Tutorial_Island |
| 015 | done | claude-raid | Objective.Burn.House tag, then build and place burnable buildings |
| 016 | done | claude-raid | Fix building clustering scale and breakable-component persistence |
| 017 | done | claude-raid | Collapse the HUD objective list by type so 11 houses do not fill the screen |
| 018 | done | claude-raid | GS.Raid.* debug commands so the two lose paths can be driven and verified |
| 019 | done | claude-raid | GS.Raid.Goto / GotoActor / SpawnAt teleport debug commands |
| 020 | done | claude-raid | Burning houses need to LOOK like it: char, smoulder and flames |
| 021 | done | claude-raid | GotoBuilding / BurnHere / BuildingStatus - and find out why char is not showing |
| 022 | done | claude-perf | Delete empty gigantic Dreamscape blocking volumes from L_Tutorial_Island |
| 023 | done | claude-ranged | Torch throw feel: fix the giant arc square, show the reticle, more range, visible in flight |
| 024 | done | claude-queue | Stop tickets closing with a stale Evaluate |
| 025 | done | claude-raid | Buildings measured by pivot not geometry - 76 of 113 windows own nothing |
| 026 | done | claude-ranged | Editor-side torch/bow wiring: swapped aim materials, missing bow+quiver meshes, unset bow ability |
| 027 | done | claude-raid | Debug commands must resolve the game world; buildings must not double-own pieces |
| 028 | done | claude-raid | Teleport reported success while TeleportTo silently failed |
| 029 | done | claude-raid | Buildings: cluster on shell not interiors, merge floors, score the shell |
| 030 | done | claude-ranged | Code-review fixes: arc pool truncates the ribbon, decal depth reverted, flame warn-latch is per-instance |
| 031 | done | claude-raid | One building per ROOF: cover every roofed structure on the map |
| 032 | done | claude-ranged | Handoff doc: outstanding code-review findings + ranged/torch state for the single-agent takeover |
| 033 | done | claude-raid | A building is a merged house actor or an attached kit hierarchy - not a cluster radius |
| 034 | done | claude-ranged2 | Bow: no fire-rate limit (RangedAttackCooldownSeconds is dead), and the arrow is invisible in flight |
| 035 | done | claude-orchestrator | Refresh AGENT_STATE.md NEXT against what is actually built, then regenerate the decision-queue board |
| 036 | done | claude-fixes | Code-review findings, tooling half: gs_buildings.py crash + three gsqueue.ps1 silent-failure holes |
| 037 | done | claude-fixes | Code-review findings, C++ half: adopt radius double-count, silent completion freeze, two lying diagnostics |
| 038 | done | claude-fixes | Arrows respect RaceTag (they currently damage allied goblins); document why melee AttackCooldownSeconds must stay unwired |
| 039 | done | claude-wheel | Radial weapon wheel, C++ half: EGSWeaponSlot replaces the bRangedMode bool, selection maths, BP-facing state and events |
| 040 | done | claude-wheel | Weapon wheel input + ATTACK routes by slot; torch throw moves onto the montage notify |
| 041 | done | claude-wheel | Editor half of the weapon wheel: IA_WeaponWheel + Q mapping, retire IA_ThrowTorch, WBP_WeaponWheel widget |
| 042 | done | claude-cam | Aim camera digs into the ground when you pitch up to lob a torch |
| 043 | done | claude-cam | Aim camera: lengthen the arm at high pitch instead of shortening it - the goblin was crowding the frame |
| 044 | done | claude-cam | Aim video: arc material missing SplineMeshes usage flag, and the aim camera sits inside the grass canopy |
| 045 | done | claude-cam | Clamp view pitch so the aim camera cannot swing below the wheat; build WBP_WeaponWheel |
| 046 | done | claude-wheel | Wire the wheel widget: UGSWeaponWheelWidget binds the slot delegates, character owns its lifetime |
| 047 | done | claude-wheel | Fix C4458: local 'Slot' shadows UWidget::Slot in GSWeaponWheelWidget |
| 048 | done | claude-wheel | RETROACTIVE: reparent WBP_WeaponWheel to GSWeaponWheelWidget, assign WeaponWheelWidgetClass, PIE-verify |
| 049 | done | claude-raid2 | Exercise the two never-run lose paths: LeftBehind and OutOfLives |
| 050 | done | claude-hud | End-of-raid panel in C++, and stop the clock when the raid ends |
| 051 | done | claude-hud | End panel: title and detail run together on one line instead of stacking |
| 052 | done | claude-content | Dynamic content pipeline: RAG over the GDD + critic loop, generating bark/whisper/tutorial-prompt data tables |
| 053 | done | claude-score | Score system: UGSScoreSubsystem two-kind tally (deeds/loot), fed by real raid events, shown on the end panel |
| 054 | done | claude-stamina | Stamina to C++, swimming and drowning, climb refinements (build 1: foundation) |
| 055 | done | claude-stamina | BP safe edits: ClimbDrainIdle to zero, kill the red climb trace draw |
| 056 | done | claude-content | GDD 2.6: half-confirmed means confirmed-but-uncorroborated (Michael's ruling on the 2.4/2.6 ambiguity) |
| 057 | done | claude-content | Regenerate tutorial prompts against the corrected GDD 2.4/2.6, rebuild the Assignment 04 bundle |
| 058 | done | claude-input | Movement remap: Space=jump, E=vault/mantle/climb, F=interact, G=block, LeftAlt=dodge; jump-into-climb |
| 059 | done | claude-agent5 | Stealth detection spine: crouch-and-confirm perception, generated by the goal-oriented agent (Assignment 5) |
| 060 | done | claude-input | Jump binding: JumpAction UPROPERTY so SpaceBar actually jumps |
| 061 | done | claude-input | Finish the remap: E to IA_Traverse, assign JumpAction, set jump distance tuning |
| 062 | done | claude-move | Jump feels floaty; climb cannot pass a rooftop lip |
| 063 | done | claude-levelgen | GER level generator: modular building composer + settlement layout with GDD-derived evaluator (Assignment 6) |
| 064 | done | claude-hudfix | HUD stamina frozen: BindWidgetOptional properties are not BlueprintReadOnly, so WBP_GSPlayerHUD fails to compile |
| 065 | done | claude-roofgen | Roof does not close: tile by measured piece width, and give the evaluator a geometric coverage check that can see it |
| 066 | done | claude-styleguide | Learn building composition from hand-authored examples (Showcase + tutorial island kitbashes), write a style guide, rebuild the composer to follow it |
| 067 | done | claude-climbpolish | Climb polish: stop the lip jam, wire the 40 unused traversal anims, jump-to-hang |
| 068 | abandoned | claude-stamp | Stamped buildings are close but broken: full transform fidelity (pitch/roll/scale), dropped meshes, chimney stacking, buried rock facade |
| 069 | done | claude-horde | Allied goblin horde: pool subsystem, stimulus bus, horn on MMB, BT runner, combat verbs hoisted to GSCharacterBase |
| 070 | done | claude-climbrebuild | "Climbing rebuild stage 0-1: GDD traversal spec, regression corpus, capsule-fit top-out, stamina migration" |
| 071 | done | claude-horde | Supplement to #069 - GSArrowProjectile Frenzy hook (files missed in that claim, same body of work) |
| 072 | done | claude-climbrebuild | "Stamina: SetRegenSuppressed + GetDrainRate so the pool can freeze on the wall" |
| 073 | done | claude-horde | Stale docs: the nine HANDOFF findings are already fixed, the run-start banner says otherwise; record the horde decision lines |
| 074 | done | claude-climbrebuild | "Climb: never top out while there is still wall above your head" |
| 075 | done | claude-climbrebuild | "Climb anims: back to the braced hop set (Michael's call - better feel for goblins)" |
| 076 | done | claude-climbrebuild | "Stamina migration: BP SprintStamina retires to UGSStaminaComponent; freeze on the wall; HUD reads the component" |
| 077 | done | claude-climbrebuild | Rebuild house collision for climbing: simple clean shell on SM_MERGED_House_Medium_11 first |
| 078 | done | claude-horde | "CLAUDE.md: require agents to clean up temporary and helper files at the end of a task" |
| 079 | done | claude-climbrebuild | "Climb: the top-out was aiming at a 79-degree fascia, and the 0.15 deck gate waved it through" |
| 080 | done | claude-climbrebuild | "Climb ledge search + stall detection in C++ (UGSClimbLibrary), with a real log line" |
| 081 | done | claude-climbrebuild | "Stage C: delete the guards that compensated for the wrong climb model; first pass at Moria-feel cadence" |
| 082 | done | claude-climbrebuild | "Climb cadence: drive the hop play rate from actual speed instead of a fixed constant" |
| 083 | done | claude-npcfight | NPC-vs-NPC melee: hostile acquisition, block AI, attack telegraph |
| 084 | done | claude-climbrebuild | "Goblins have claws: walkable floor angle 62 -> 65, which is also the climb/walk boundary" |
| 085 | done | claude-npcfight2 | Wire Block into BT_Militia and PIE-verify NPC-vs-NPC melee |
| 086 | done | claude-npcfight2 | Resolve BT key selectors: IsSet() was a permanent false, so the telegraph never reached the blackboard |
| 087 | done | claude-recoil | A blocked swing is turned aside: cancel the attack and open the attacker up |
| 088 | done | claude-horn | Stage 2: the horn actually summons - BP_HordeGoblin, BT/BB, class path, arrival markers, MMB |
| 089 | done | claude-hordefix | Summoned goblins never engage: the proximity threat scan registers nothing |
| 090 | done | claude-crowd | Massed combat phase 1: attack tokens, ring-slot reservation, engagement capacity, menace orbit |
| 091 | done | claude-plate | Knight workover: directional plate - front shrugs off a dagger, gaps do not (GDD 217) |
| 092 | done | claude-plate | Supplement to #091 - headshots: an arrow to the head does extra damage |
| 093 | done | claude-plate | Mitigation must never zero a hit: a minimum damage floor |
| 094 | done | claude-archer | Archers actually shoot: ranged ability on the base, BTTask_RangedAttack, per-tree standoff range |
| 095 | done | claude-archer | Supplement to #094 - every arrow was a headshot: resolve by distance to the head bone, not nearest bone |
| 096 | done | claude-arrowfix | Arrows render perpendicular to flight: GS_Arrow's long axis is +Z, pivot at the tail |
| 097 | done | claude-kit | RETROACTIVE claim - bow/arrow +25%, and guards finally hold a sword |
| 098 | done | claude-gobkit | Horde goblins hold the sword too |
| 099 | done | claude-gobkit | Sword a little bigger, and the guard sword has no material |
| 100 | done | claude-gobkit | RETROACTIVE - guard upside down, sword held by the tip, player sword to 1.6 |
| 101 | done | claude-gobkit | Humans T-pose in combat: goblin-skeleton montages play on the human rig |
| 102 | done | claude-gobkit | Point the human weapons at the new hand_r_weapon socket |
| 103 | done | claude-gobkit | Bake GS_Sword_Guard pivot to the grip so socket preview is WYSIWYG |
| 104 | done | claude-gobkit | Erika finally holds a bow |
| 105 | done | claude-gobkit | Patrol combat run: baseline crowding measurement |
| 106 | done | claude-gobkit | The dead never let go: engagement slots and ring claims leak on death |
| 107 | done | claude-gobkit | Crowd fix: orbit on your own slot bearing, 6 slots, honest arrival tolerance, player avoidance |
| 108 | done | claude-gobkit | Jitter: MenaceOrbit and MoveTo fight over two different radii |
| 109 | done | claude-gobkit | Snapping: MenaceOrbit sets facing instantly, up to 167 degrees in one frame |
| 110 | done | claude-gobkit | Bigger warband, and start the player across the arena from the patrol |
| 111 | done | claude-gobkit | Archers can defend themselves: melee branch and the kick |
| 112 | done | claude-gobkit | Humans get combat animations: retarget the goblin montage set onto SK_Human_Skeleton |
| 113 | done | claude-gobkit | Retargeted human attacks pop upward: lock the root; archers never shoot while kiting |
| 114 | done | claude-gobkit | A blocked swing must actually be cancelled: SwordLight needs State.Attacking as an ASSET tag |
| 115 | done | claude-bloat | Apply the verified bloat-audit findings: delete leftover scripts and staging dupes, untrack build logs and pycache, strip dead C++ and Python |
| 116 | done | claude-correctness | Code-review correctness fixes: the recoil punish is unreachable, LoseRadius never takes effect, JumpAction is bound unguarded |
| 117 | done | claude-correctness | PIE says the punish still never fires: the recoil flinch sets State.HitReact, which vetoes it in turn |
| 118 | done | claude-anim | CombatBugs video: humans T-pose in melee, and debug spheres draw in normal play |
| 119 | done | claude-anim | Repoint the human montages off the root-locked A_MX_Gob copies onto the clean A_HU_ import |
| 120 | done | claude-wrapup | GS.Combat.Debug must not default to on, and record the three-in-a-row unwatched-fix failure in AGENT_STATE |
| 121 | done | claude-animsmooth | Animation smoothness pass 1: swings play at real speed, with press-to-contact timings rescaled to match |
| 122 | done | claude-input2 | Right mouse blocks when the sword is out; retire G |
| 123 | done | claude-animsmooth | Animation pass 2a: lengthen the outlier montage blends so flinches stop cutting in |
| 124 | done | claude-animsmooth | Animation pass 2b: the melee facing snap turns 120 degrees in one frame |
| 125 | done | claude-animsmooth | Light swings keep 85 percent of your speed so an archer can actually be chased down |
| 126 | done | claude-arena | Arena spawn puts the camera over the void: move the PlayerStart in off the rim |
| 127 | done | claude-arena | Hill arena: a calibrated slope range for testing melee up and down gradients |
| 128 | done | claude-animsmooth | Attacking freezes the player: the goblin attack clips hand movement to root motion that contributes nothing |
| 129 | done | claude-animsplit | Upper-body split for humans: move the attack montages onto the UpperBody slot ABP_Human already has |
| 130 | done | claude-crowd2 | RETROACTIVE - guards get their swing commitment back after 85 percent made them crowd |
| 131 | done | claude-space | Personal space: a capsule-derived minimum-distance floor in the orbit, plus the spatial instrumentation nobody had |
| 132 | done | claude-space2 | Four attackers at once, and the spacing floor still lets them press in |
| 133 | done | claude-facing | Combat agents never face their target: one facing authority, plus the directional locomotion to make it read |
| 134 | done | claude-eyes | Eyes sit outside the head on Erika and the Knight: per-character bind pose is discarded by Animation translation retargeting |
| 135 | done | claude-hordetick | Summoned goblins never face or separate: AGSHordeAIController disables the tick both #132 and #133 rely on |
| 136 | done | claude-gate | The queue refuses to close an unobserved ticket: observed/scenario fields, evidence ladder, loud escape hatch |
| 137 | done | claude-animsnap | GS.Anim.Snapshot: the runtime readout animation never had |
| 138 | done | claude-weapons | Import the ArtSource weapons: six meshes into /Game/Weapons, with material instances where bakes exist |
| 139 | done | claude-armingsword | Put the arming sword in the militia's hands: DA_Weapon_ArmingSword on the four Militia-row defenders |
| 140 | done | claude-record | Record Michael's sign-off on the combat animation: #133 closed claiming it had never been watched |
| 141 | done | claude-orders | Order wheel and world markers: the horde takes Attack/Hold/Loot/Follow |
| 142 | done | claude-acf | ACF Phase 0: make AIFramework reachable - Build.cs dependency and uproject plugin pin |
| 143 | done | claude-acf | ACF Phase 1a: hoist the facing authority and separation steer off AGSAIControllerBase into UGSAISteeringComponent |
| 144 | done | claude-gobarm | Horde goblins spawn unarmed: no EquippedWeapon on BP_HordeGoblin, so a warband is wiped in seconds |
| 145 | done | claude-cam2 | Camera lurches in and out during a crowd fight: every character blocks the spring arm's camera probe |
| 146 | done **UNOBSERVED** | claude-wheelui | Order wheel labels sit in the corner instead of around the wheel; beacon material reverted to DefaultMaterial |
| 147 | done | claude-ordertrace | "The order wheel can never find a target: the aim sweep is blocked by terrain, so Attack and Loot are always refused" |
| 148 | done **UNOBSERVED** | claude-crosshair | "No crosshair: the player aims orders, the bow and the torch with nothing on screen to aim with" |
| 149 | done | claude-reticle2 | Reticle turns gold when the crosshair is on a valid order target |
| 150 | done | claude-acfskills | Register ACF's 40 author-written Claude skills so sessions can see them |
| 151 | abandoned | claude-movespeed | SwordLight writes MaxWalkSpeed directly instead of applying UGSGE_MoveSpeedScalar |
| 152 | abandoned | claude-assettags | Retire deprecated AbilityTags in GSGA_Block and GSGA_Interact (C4996) |
| 153 | done | claude-hordebt | "BT_HordeGoblin is gutted: 0 tasks, 5 null decorators, blackboard repointed to ACFAIBB" |
| 154 | done | claude-hordebt | Supplement to #153 - BB_HordeGoblin is missing the four order keys #141 writes |
| 155 | done **UNOBSERVED** | claude-gitignore | Resolve the unresolved merge conflict in the repo-root .gitignore |
| 156 | done **UNOBSERVED** | claude-gdd | GDD 2.8/2.9: objective roster becomes Windmill/Market/Statue; granary removed, field demoted to optional |
| 157 | done **UNOBSERVED** | claude-gdd2 | GDD 2.8: reverse the hand-authored-only ruling - generator gets a timeboxed attempt with the blockout as fallback |
| 158 | done **UNOBSERVED** | claude-gddexport | Re-export the CodeArchitect GDD: roster, controls, map ruling and real build status |
| 159 | done **UNOBSERVED** | claude-interactplan | Record the interaction-framework plan and the ACF comparison in AGENT_STATE |
| 160 | done **UNOBSERVED** | claude-autosave | Disable editor autosave programmatically (Editor Preferences UI will not open) |
| 161 | done **UNOBSERVED** | claude-interact | Unblock the interact framework: loud unset-input log, CarrySocket guard, carry-does-not-consume |
| 162 | done **UNOBSERVED** | claude-interact | Supplement to #161 - the consume fix belongs in GSInteractableComponent, not the caller |
| 163 | done | claude-interactables | The project's first two interactables: a carryable pig and a lootable chest |
| 164 | done **UNOBSERVED** | claude-collapse | Looted containers collapse: bCollapseOnComplete on the interactable |
| 165 | done | claude-smash | Smashable props stage 1+2: hit points, ACF delegation, scoped retire, player sword smash |
| 166 | done **UNOBSERVED** | claude-smash | Supplement to #165 - AscentCombatFramework must be named explicitly to LINK, not just include |
| 167 | done **UNOBSERVED** | claude-smash | Supplement to #165 - GSCombatDebugEnabled had no declaration and only linked via unity build |
| 168 | done | claude-lidcrate | Smashing a container opens it: bUnlockInteractableOnBreak |
| 169 | done | claude-channelring | Channel progress ring around the reticle: the interact feedback nothing was bound to |
| 170 | done | claude-rosterdrift | Assignment 6: level-gen objective roster follows GDD 156 (granary out, Market/Statue/Windmill in); submission docs |
| 171 | done | claude-axe | Goblin sword to axe: SM_Axe_DA on DA_Weapon_Scout and DA_Weapon_HordeGoblin, grip offset re-derived |
| 172 | done | claude-channelring | A completed interaction leaves no trace: log the channel lifecycle |
| 173 | done | claude-grapple | Grappling hook prototype: throw, stick, rope appears (Blueprint only, no build) |
| 174 | done | claude-axe | BS_GS_Locomotion_Gob measured dead in PIE and reverted - #133's blend-surface fix was never applied to it |
| 175 | done | claude-grapple | Supplement to 173: M_GS_Rope master material (flat colour, spline-mesh flag) so the shared Dreamscape parent is not touched |
| 176 | done | claude-grapple | Grapple as a weapon-wheel slot: EGSWeaponSlot::Grapple, 4 sectors, UGSGA_GrappleThrow (WRITTEN, gate closed - not compiled) |
| 177 | abandoned | claude-grapple | Grapple as a weapon-wheel slot: EGSWeaponSlot::Grapple, 4 sectors, UGSGA_GrappleThrow (WRITTEN, gate closed - not compiled) |
| 178 | done | claude-dodge | A1: directional dodge montages - UGSGA_DodgeRoll plays one of the four authored rolls instead of a bare LaunchCharacter |
| 179 | done **UNOBSERVED** | claude-anchortags | Marker.ObjectiveAnchor.Statue added, .Granary retired (GDD 156 roster) - WRITTEN, staged for the next build window |
| 180 | done | claude-grapple | Supplement to 176: WBP_WeaponWheel has no Label_Grapple, and the 90-degree sectors left Bow and Sword labels in the old 120-degree positions |
| 181 | done | claude-channelring | Smashable lootable barrel: BP_LootBarrel on the SM_Barrel_01 to SM_BarrelBroken swap pair |
| 182 | done | claude-packanim | Replace goblin combat+locomotion animation with CombatMasterBundle: retarget DK2 in-place loco (16) and DTA combat (29) onto GOB_Scout_v2_Skeleton |
| 183 | done | claude-grapple | Grapple hook renders red and weird: kill the 15s debug trace draw, and stop the placeholder cone standing upright on the wall |
| 184 | done | claude-packanim | Pack attacks: wrap DTA combos into montages and repoint the 5 goblin swing stages |
| 185 | done | claude-packanim | Play the pack Buff montage when a horde order commits |
| 186 | done | claude-packanim | Camera-relative movement: body follows the camera by default so the goblin can strafe and backpedal |
| 187 | done | claude-prompt | Interact prompt: OnFocusChanged finally bound, and a locked container tells you to smash it |
| 188 | done | claude-roadpush | level-gen: _move_building_clear rotates a house back onto the road it was escaping, burning all 3 passes and opening a sightline |
| 189 | done **UNOBSERVED** | claude-statuerename | Rename the granary objective: AGSObjective_BurnGranaries -> AGSObjective_ToppleStatue, Objective.Granary -> Objective.Statue, with CoreRedirects - WRITTEN, needs build |
| 190 | done **UNOBSERVED** | claude-statuerename | Supplement to 189 - comment-only cross-references to the renamed class in three files missed by that claim |
| 191 | done | claude-styleagent | Assignment 7: Style Guide Agent - GDD-anchored rules, scoring Evaluator (SCORE+REASON), Refiner loop, three before/after demos |
| 192 | done | claude-fracture | Stage 0: the statue actually shatters - Dataflow fracture pipeline and the first real GeometryCollection |
| 193 | done | claude-idol | Tear down the false idol: grapple the statue and haul it over by walking away |
| 194 | done | claude-styledeterm | Style agent determinism: code-computed score, response cache with replay, so a graded run is byte-reproducible |
| 195 | done | claude-idol | Supplement to 193 - the same kinematic trap in GSBreakableComponent::Break(): SetSimulatePhysics does not make a collection dynamic |
| 196 | done | claude-idol | A toppled idol scores: OnToppled feeds UGSScoreSubsystem deeds |
| 197 | done | claude-qaagent | Assignment 9: adversarial QA agent + deterministic unit-test suite (editor Python, no C++, no build) |
| 198 | done | claude-gddlock | GDD v1.0: export becomes canonical, moved to docs/, reconciled to the live tree, and locked (freeze rule + drift check + ledger + scope table) |
| 199 | done **UNOBSERVED** | claude-gddlock | Supplement to 198 - fold the GDD relocation, the new ledger, and two rediscovery-prone findings into AGENT_STATE.md |
| 200 | done | claude-gddlock | Regenerate the banked bark/prompt/whisper rows off the Market-Statue-Windmill roster, then wire check_gdd.py into Build-GoblinSiege.ps1 |
| 201 | abandoned | claude-gddlock | Sweep the banked text of cut and deferred systems (prisoners/bind, well/bucket brigade) and add the missing Statue coverage |
| 202 | done | claude-gddlock | Supplement to 200 - repoint the bark machine's RAG corpus at the canonical GDD (it still reads the frozen assignment doc) |
| 203 | done | claude-combofeel | Combo link speed: the contact fix parked its reclaimed time in RecoverySeconds, which is the gap between swings |
| 204 | done **UNOBSERVED** | claude-dodgedir | Dodge plays the forward roll in every direction: add the instrument the dodge path has never had, then diagnose |
| 205 | done **UNOBSERVED** | claude-acfswap | Two ACF installs collide and no build can run: retire the project 4.4 copy, adopt the engine 4.4.2, repoint the skills registration |
| 206 | done | claude-ubacheck | The UBA cache check passes while UBA cannot write: it probes a NEW file at the root, UBA fails on admin-owned pre-existing ones |
| 207 | done **UNOBSERVED** | claude-dodgekey | Dodge moves to E (tap), traversal becomes hold-E, LeftAlt retired - and the traversal roll stops stealing the dodge's animation slot |
| 208 | done | claude-traversepin | Traversal detection moves off the Started pin onto Triggered behind a Sequence, so a tap of E is the dodge alone |
| 209 | done **UNOBSERVED** | claude-dodgestam | Dodging costs stamina so it cannot be spammed - UGSGA_DodgeRoll gains a TryConsume gate |
| 210 | done **UNOBSERVED** | claude-hordeorder | An ordered goblin still publishes a FollowTarget, so Follow Summoner and Chase Target both pass and the tree oscillates |
| 211 | done **UNOBSERVED** | claude-statelog | Fold the traversal/dodge session into AGENT_STATE: two dodge systems, the Started pin, the horde follow competition, and what the UNOBSERVED closes leave unproven |
| 212 | done **UNOBSERVED** | claude-hordetarget | BT_HordeGoblin's AcquireTarget service has bSelectTarget ON, so it overwrites the ordered target every rescan and the goblin flickers between attacking and following |
| 213 | abandoned | claude-hordehold | Hold is unbuilt: BT_HordeGoblin has no branch reading OrderVerb, so a Hold order lands nowhere |
| 214 | done | claude-acfphase1 | ACF Phase 1: reparent AGSAIControllerBase onto AACFAIController - moves the horde AND the defenders in one edit |
| 215 | done | claude-acfentity | ACF 4.4.2 ships AACFBaseAIController implementing only 2 of IACFEntityInterface's 4 methods - supply the other two or nothing deriving from it can link |
| 216 | done **UNOBSERVED** | claude-attrruling | Record the Phase 2 attribute ruling: ACF AdvancedRPGSystem becomes the attribute owner, and what that puts at risk |
| 217 | done | claude-acf | Silence per-frame GSDBG climb/LIP print spam in BP_GSPlayerCharacter |
| 218 | done | claude-acf | Overflow attackers get a distinct outer-ring bearing instead of stacking |
| 219 | done | claude-acf | CrowdStats: report outer-ring claims and mark a breached engagement cap |
| 220 | done | claude-acf | Ring promotion leaks the old claim: an agent can hold an inner AND an outer slot |
| 221 | done | claude-acf | Crowd feel: TokenBudget 6, stagger blocks only its causer, ordered overflow spills to another enemy |
| 222 | abandoned | claude-warren | The Warren: N_ChaosRune2 as arrival mouth, respawn point and loot bank |
| 223 | done | claude-acf | ACF Phase 2a: AGSCharacterBase reparents to AACFCharacter, one ASC |
| 224 | abandoned | claude-warren | Horn: tap summons one, hold streams the squad out of the Warren (supplement to 222) |
| 225 | done **UNOBSERVED** | claude-acf | ACF Phase 2c: audit the class-identity checks against the AACFCharacter reparent |
| 226 | done | claude-acf | ACF Phase 2b-1: author the ARS attribute DataTable for all six archetypes |
| 227 | done | claude-acf | GS.Stats.Dump: read ARS and GS attributes side by side |
| 228 | done | claude-acf | ACF Phase 2b-1b: health consumers move to ARS, death routed through ACF |
| 229 | done | claude-acf | ACF config debts: team manager + teams config, collisions master, ability set |
| 230 | done **UNOBSERVED** | claude-acf | The Scout axe wears the old mesh offset: SM_WoodcutterAxe needs the sword convention |
| 231 | done | claude-acf | ACF Phase 2b-3: stamina storage moves to ARS, GSStaminaComponent becomes policy |
| 232 | abandoned **UNOBSERVED** | claude-acf | ACF Phase 2b-4: sprint and slows move to ACF locomotion states |
| 233 | done | claude-acf | GE_GSStatModifier: the missing effect that made every ACF statistic write a no-op |
| 234 | done **UNOBSERVED** | claude-acf | ACF Phase 2b-2a: port our damage rules into a UACFDamageCalculation subclass |
| 235 | done **UNOBSERVED** | claude-acf | ACF Phase 2b-2b: GS damage types carrying our damage tags |
| 236 | done | claude-warren | The Warren on N_ChaosRune2, and the horn that fills it: arrival mouth, respawn, loot bank, tap-or-hold summon to a squad of 10 |
| 237 | done | claude-acf | ACF Phase 2b-2c: the axe swing delivers damage through ACF |
| 238 | done **UNOBSERVED** | claude-acf | AI reaction delay: a beat between noticing and swinging |
| 239 | done | claude-acf | Follow slots renumber when a goblin dies, so the whole horde jostles |
| 240 | done | claude-acf | Archers hold a latched bearing instead of re-racing for a melee ring slot |
| 241 | done | claude-acf | Bow gets draw, hold and shoot: retarget the archery set to human and goblin |
| 242 | done | claude-warren | The horn becomes a visible prop: SM_HuntingHorn attaches to the hand, and the goblin blows it instead of shouting |
| 243 | done | claude-acf | Bow timing minigame: sweep, bands, damage multiplier and aim sway |
| 244 | done | claude-acf | Bow timing bar on the HUD: gradient material, widget binds, show and hide |
| 245 | done | claude-warren | The player plants the Warren: hold X, green-or-red ghost, one per player, 3 minute cooldown; horn summons from the gate until one is down |
| 246 | done | claude-acf | Instrument: GS.AI.LogLocomotion, per-frame AI speed, to prove or refute the one-foot-step diagnosis |
| 247 | done | claude-acf | Archer stutter: hold a range band instead of chasing a sliding point, and restore combat focus the shot task clears |
| 248 | done | claude-acf | Erika reposition speed 1023 to 520 so she reaches the run clip instead of sliding 2.5x |
| 249 | done | claude-audio | Audio phase A: mixer spine - sound classes, submixes, attenuation, concurrency, surface types |
| 250 | done | claude-acf | Archer step 1: reposition speed to 200 and widen the hold band to 300-1400 |
| 251 | done | claude-acf | Guards to 500 and the Idle-Walk deadband the guard data finally justifies |
| 252 | abandoned | claude-corruption | Ruling: world corruption joins the slice (scope freeze amendment) |
| 253 | done | claude-acf | Bow timing hookup: the component on the pawn, draw on press, quality on release, cancel everywhere else |
| 254 | done **UNOBSERVED** | claude-acf | GSRaidLibrary misses Engine/OverlapResult.h so FOverlapResult is undefined |
| 255 | done | claude-warren | Loot banks at the beginning portal too: one banking component, given to the runic site |
| 256 | done | claude-acf | Draw strength flattens the shot: a weak release lobs, a perfect one flies straight, and the arc shows it live |
| 257 | done | claude-warren | Wire T to the Warren placement component: component on the pawn, Started and Completed bindings |
| 258 | done | claude-acf | The fire-interval gate suppresses the whole draw, so a quick second shot shows no bar at all |
| 259 | done | claude-acf | Bow montages: draw, hold loop and release on both skeletons, fired from the timing component |
| 260 | done | claude-acf | The holstered axe sits in the aim sightline: hide it while aiming |
| 261 | done **UNOBSERVED** | claude-acf | Delete GetFireCooldownRemaining - its only caller was the draw gate removed in 258 |
| 262 | done | claude-acf | GS.Horde.Slots and GS.Horde.KillSlot: make the follow-slot fix testable |
| 263 | done | claude-acf | The horde crowds you because FollowSlot drives nothing: give each goblin a formation post behind the summoner |
| 264 | done | claude-acf | An attack order becomes a place, not a person: arrive, sweep locally, engage whatever is nearest |
| 265 | done | claude-warren | Horn summons from the gate until a Warren is down: arrival falls back to the runic site, not the treeline markers |
| 266 | done | claude-warren | AGSWarren moves onto the shared loot bank component, so there is one banking implementation not two |
| 267 | abandoned | claude-warren | Summoned horde goblins spawn with no EquippedWeapon again: no abilities granted, no ARS attributes, two ACF errors per goblin |
| 268 | done | claude-warren | The unarmed-goblin warning is a false diagnostic: it fires before AGSHordeGoblin equips itself, and has now caused two misdiagnoses |
| 269 | done **UNOBSERVED** | claude-warren | ACF Phase 3 scoping: we have carried an unused UACFEquipmentComponent since Phase 2a while UGSWeaponComponent does the same job |
| 270 | done | claude-warren | ACF Phase 3 stage 2: equip the goblin axe through ACF on one character, alongside the existing path |
| 271 | done | claude-warren | ItemSlot gameplay tags so ACF equipment has slots to equip into |
| 272 | done | claude-warren | The no-EquippedWeapon warning fires on six defender Blueprints that are correctly armed and correctly statted - it is the #268 false positive one class over |
| 273 | done | claude-warren | 48 ACF errors and warnings a run, of which only one kind was ours - the character data assets had no DefaultAbilitySet |
| 274 | done | claude-warren | "Stage 3 - EGSWeaponSlot retired: the weapon wheel is four gameplay tags and its contents are data" |
| 275 | done | claude-warren | Peasants become civilians - no sword, 10 HP, and an ACF data asset they never had |
| 276 | done | claude-warren | Five null input triggers, and why filling them would have broken blocking and the heavy charge |
| 277 | done **UNOBSERVED** | claude-warren | "RULING: finite arrows join the slice - a player-only quiver, against the 12.4 scope freeze" |
| 278 | done | claude-warren | Finite arrows stage 1 - the arrow item and the player quiver |
| 279 | done | claude-warren | Finite arrows stage 2 - walk-over resupply, AGSAmmoPickup |
| 280 | done | claude-warren | Finite arrows stage 3 - the gate and the decrement |
| 281 | done | claude-warren | Finite arrows stage 4 - the arrow count on the HUD |
| 282 | done | claude-warren | Empty quiver must not draw the bow, and the starting quiver drops to 15 |
| 283 | done **UNOBSERVED** | claude-warren | "RULING: the weapon wheel migrates onto ACF equipment; Uriel is demo scope, not prototype" |
| 284 | done | claude-warren | The wheel slot is Primary, not Sword - the goblin holds an axe |
| 285 | done **UNOBSERVED** | claude-warren | "RULING: sneaking is cut from the demo, and the bucket brigade stays deferred" |
| 286 | done | claude-warren | "ACF wheel migration stage 1: the WeaponSlot to ItemSlot mapping and the choke point, flag off" |
| 287 | done | claude-warren | "ACF wheel migration stage 2: the player's primary weapon moves onto ACF" |
| 288 | done **UNOBSERVED** | claude-warren | "RULING: the primary player experience is the bar" |
| 289 | done | claude-warren | "ACF wheel migration stage 3: the bow moves, the quiver does not" |
| 290 | done | claude-warren | The torch goes in the main hand - the throw animation uses the right |
| 291 | done | claude-warren | "ACF wheel migration stage 4: the horde goblins move onto ACF" |
| 292 | done | claude-warren | ACF migration stage 4b: the defenders move onto ACF |
| 293 | done **UNOBSERVED** | claude-warren | Weapon placement: the data asset drives the ACF weapon, and a live console loop to tune it by eye |
| 294 | done **UNOBSERVED** | claude-warren | Uriel is replaced by a Knight in the prototype levels |
| 295 | done **UNOBSERVED** | claude-warren | ACF migration stage 5: retire whatever of the mesh path is provably dead |
| 296 | done | claude-corruption | Re-file 252: the world corruption rulings, and civilians corrupt the land faster than soldiers do |
| 297 | done | claude-audio | Horn pops: CC_GS_Signal max 1 denies the horn's own end sample mid-fade |
| 298 | done | claude-audio | Audio: the variation cue layer - random containers with pitch/volume jitter over the pack waves |
| 299 | done | claude-warren | Demo staging on L_Tutorial_Island: the seal, the gate, the resources |
| 300 | done | claude-warren | Livestock become loot in place: MakeActorCarryable, animations untouched |
| 301 | done | claude-audio | Audio reaches gameplay: loot containers get smash, break, loot and ignite sounds via Blueprint delegate binds |
| 302 | done | claude-warren | "The statue wards the ground: no portal while it stands" |
| 303 | done | claude-warren | Goblins steal cargo off each other and are eaten as couriers |
| 304 | done | claude-warren | Fire jumps between houses: building-to-building spread |
| 305 | done **UNOBSERVED** | claude-warren | The objective set: a percentage of houses, the statue counts, the mill does not |
| 306 | done | claude-ui | The statue joins the objective list, and completing one says so |
| 307 | done | claude-warren | The forest closes the map: an impassable treeline along the drawn boundary |
| 308 | done | claude-warren | Goblins hate water: past waist deep you drown |
| 309 | done | claude-corruption | World corruption 1/6 - the scalar, the sky, and a two-ended post-process grade from blown-out to gritty |
| 310 | done | claude-ui | The generated HUD art gets real alpha and comes into the project |
| 311 | done | claude-ui | M opens the objective board, with a seam for the map it becomes |
| 312 | done | claude-corruption | World corruption 2/6 - objectives, destruction, clock and horde presence drive the scalar |
| 313 | done **UNOBSERVED** | claude-ui | L_Tutorial_Island onto World Partition, objective actors always loaded |
| 314 | abandoned | claude-ui | The objective list becomes rows with type and state icons |
| 315 | done | claude-corruption | Corruption instrument: dump lines go to LogGSCorruption not LogTemp, and the objectives line shows its per-carrier working |
| 316 | done | claude-corruption | The corrupted sky loses its blue and the sun goes red: ozone absorption and AtmosphereSunDiskColorScale |
| 317 | done | claude-crumble | One destroyed state, three ways in: UGSCrumbleComponent unifies topple, burn-down and smash |
| 318 | done | claude-audio | Footsteps: surface-aware Blueprint notifies on the goblin locomotion set |
| 319 | abandoned | unassigned | Weapon-switch GUI stops working after respawning from a fire death |
| 320 | done | claude-corruption | 67 houses stop swamping the objectives term: weight per TYPE not per instance, plus the ozone zero-baseline bug and the 71-carrier tick cost |
| 321 | abandoned | claude-ui | A horde readout: how many goblins are out, against the cap and the reserve |
| 322 | done | claude-crumble | Clean up the treeline: trees climb the moved walls and their roots float in mid air |
| 323 | done | claude-crumble | The windmill can actually be lit: window trigger, lit-torch tag, and it counts as an objective |
| 324 | done | claude-crumble | The windmill sinks instead of detonating: anchored base, burning top half comes down |
| 325 | done | claude-corruption | Stage 3a - AGSGameMode gets its first delegate: OnCharacterKilled, so a defender death is reportable at all |
| 326 | abandoned | unassigned | The windmill has no cap in game: SM_RoofTIles2 renders in the editor and not at runtime |
| 327 | done | claude-corruption | Stage 3b - human kills drive corruption, and civilians corrupt more than soldiers (ruling 62) |
| 328 | done | claude-crumble | Treeline generator: regenerate the GS_ForestWall treeline as a repeatable level-building command |
| 329 | done | claude-crumble | Road network as splines: GS_Road/GS_Junction actors, ACF spline-following, and a Road Tools panel |
| 330 | done | claude-crumble | Defenders AI content moves onto ACF: ACF blackboard, ACF behaviour tree, ACF patrol/combat components |
| 331 | done | claude-acf | Defenders never start patrolling: TargetLocation seeds to homeLocation so the first MoveTo is a no-op |
| 332 | done | claude-corruption | World corruption handoff: fold stages 0-3 into AGENT_STATE so the next session does not rediscover them |
| 333 | done | claude-acf | Human locomotion + attacks move onto CombatMasterBundle PowerfulSword (retarget Manny_UE5 -> SK_Human) |
| 334 | done | claude-acf | Locomotion states: stop disarming ACF bands for AI so Patrol walks and Combat runs |
| 335 | done **UNOBSERVED** | claude-frontend | Front-end stage 1: the ruling, and ACF UI config reaches the project |
| 336 | abandoned | claude-corruption | Stage 4 - UGSCorruptionDataAsset: weights, knees, curves and both grade ends leave C++ |
| 337 | done | claude-acf | Stage 4 - UGSCorruptionDataAsset: weights, knees, curves and both grade ends leave C++ |
| 338 | done | claude-acf | GS.Corruption.Debug - the live on-screen bar the plan specified and stage 1 never built |
| 339 | done | claude-acf | #331 fix: call ACF StartPatrolLoop on possess so placed guards seed their own patrol |
| 340 | done | claude-skills | Mine ACF FullExample for wiring our 40 skill packs do not cover, emit GS-specific skills |
| 341 | done | claude-acf | Fix GS_Road C_13: spline points float above the navmesh so 5 guards deadlock on point [2] |
| 342 | done | claude-corruption | Icebox world corruption: free the gate, record where stages 4-6 stand and what is unbuilt in the tree |
| 343 | done | claude-combat | Defenders cannot attack: ACF CombatBehaviour unconfigured after #330 (BT_Militia/BT_Archer orphaned) |
| 344 | done | claude-combat | Defender ability set: humans need an ACF AbilitySet so CanExecuteAbility resolves (supplement to 343) |
| 345 | done | claude-combat | Rune stage 1: light attack fires on press, heavy moves to a short hold, dodge travels and grants i-frames, plate deflect reads as a deflect |
| 346 | done | claude-combat | Editor crashes on Play: Knight/Archer/Brawler archetype rows still assign BT_Militia/BT_Archer, whose 5-key BB_Human collides with ACF's cached 13-key blackboard indices |
| 347 | done | claude-combat | Guard AGSAIControllerBase::OnPossess against swapping ACF's blackboard out from under its cached key indices (supplement to 346) |
| 348 | done | claude-combat | Raise the MoveSpeedMultiplier ceiling from 3 to 6 so the dodge roll can exceed 3x walk speed (supplement to 345) |
| 349 | done | claude-combat | Rune stage 2: real melee hit detection - replace the overlap sweep with a swept trace carrying bone name, impact point and physical material |
| 350 | done | claude-combat | Attacking in mid-air freezes the goblin: root-motion montages override gravity - ignore root motion while falling so jump-attacks work |
| 351 | done | claude-combat | Impact FX: add UACMEffectsDispatcherComponent to AGSGameState and author an impacts FX data asset so PlayImpactEffect has something to play through |
| 352 | done | claude-combat | Locomotion bands: ApplyMoveSpeed clobbers the band with a stale BaseWalkSpeed snapshot, and Knight/Archer/Civilian still disarm their bands entirely |
| 353 | done | claude-combat | Rune stage 2: hitstop - a brief time-dilation dip on attacker and victim when a swing connects, scaled by the weight of the blow |
| 354 | done | claude-combat | Archers do not shoot: they share the melee-only combat behaviour, so give them native Actions.Defender.* tags, a ranged behaviour asset and their own controller |
| 355 | done | claude-combat | Rune stage 2 close-out: camera shake on the player's landed hits, and a deflect cue when a knight's plate turns a blow aside |
| 356 | abandoned | claude-anim | Horn blast raise: slerp arm ramp into confirmed frame-29 hold pose |
| 357 | done | claude-fire | Held-torch flame: attach TorchFire Niagara to HeldTorchMeshComponent |
| 358 | done | claude-fire2 | Fire ground work: replace N_MeteorSpawn with a real fire system |
| 359 | done | claude-fire | Wire GS_CrumbleDemo_House to burn and crumble; add UGSCrumbleComponent auto-crumble-on-burn |
| 360 | done | claude-fire | Windmill sink fails silently: MillGeometryNameFilter case-sensitive mismatch against SM_WIndmill_Base |
| 361 | done | claude-fire | Mill: exterior-fire-immune rule retired, char redirected to real geometry |
| 362 | done | claude-fire | Mill exterior fire still unreachable: ContainsWorldLocation never overridden |
| 363 | done | claude-fire | Wheat burn char: fix disconnected CharMask, add height-based chaff/stalk mask |
| 364 | done | claude-fire | Fire spread: add diminishing-returns hop decay + hard cap to stop unbounded chain spread |
| 365 | done | claude-fire | Windmill draw distance: base/roof/sail cull inconsistently, roof invisible from a distance |
| 366 | done | claude-fire | Instrument AGSTorchProjectile::OnProjectileHit to diagnose mill exterior-ignite difficulty |
| 367 | done | claude-fire | Field objectives were intercepting torch hits meant for structures - fix FindObjectiveAtLocation priority |
| 368 | done | claude-fire | Debris damage: released crumble pieces can kill on high-impulse collision |
| 369 | done | claude-fire | Wire debris damage on to mill/building crumble |
| 370 | done | claude-fire | Fire visual: scale pooled field fire volumes to overlap along the burn front |
| 371 | done | claude-fire | Raise field MaxFireVolumes to close visible gaps between fire patches |
| 372 | abandoned | claude-fire | Fire visual: per-instance random seed offset to break the stamped-copy look |
| 373 | abandoned | claude-fire | Test swap: Niagara Fluids fire system for field fire volumes |
| 374 | done | claude-fire | Hill mill cap/roof still culls at 350m - Merge Actors proxy ignores AllowCullDistanceVolume |
| 375 | done | claude-anim | Horn blast loop: freeze intro pose instead of blend-race to Loop montage |
| 376 | done | claude-fire | On-fire status effect: slow burn that clings until you roll |
| 377 | done | claude-town | Town garrison: 8 -> 24 defenders, first real archer placement, patrol splines reused |
| 378 | done | claude-ai | Defenders react to threats: empty ACF DefaultThreatMap means sight-based engagement never worked; victim never self-targets on hit |
| 379 | done **UNOBSERVED** | claude-loot | Design: Loot command - goblins search a pointed area for anything lootable |
| 380 | done **UNOBSERVED** | claude-anim | Design: hook animations onto Pig, Sheep, Chicken so they become raidable livestock |
| 381 | done **UNOBSERVED** | claude-overnight | Pig/Sheep/Chicken: AnimBPs built, sheep+chicken actors created, all placed as raid targets on Tutorial Island |
| 382 | done | claude-loot2 | Loot order: goblins must break crates open and actually carry loot, not just walk to it |
| 383 | abandoned | claude-savegame | Design: persist score across sessions and close the raid loop (end panel is a dead end) |
| 384 | done | claude-loot2 | Loot polish: chest/pouch swap, revert-to-follow when nothing left to loot |
| 385 | done | claude-loot2 | Close the raid loop: EndPanel buttons, score persistence, minimal main menu |
| 386 | abandoned | claude-loot2 | Close raid loop: boot menu, complete-all debug cmd, gold/xp profile |
| 387 | done | claude-package | Package for itch: fix cook-blocking AIPerceptionComponent Error log |
| 388 | done **UNOBSERVED** | claude-package | Fix packaged-build OpenLevel short-name bug: New Raid does nothing outside PIE |
| 389 | done **UNOBSERVED** | claude-shiplog | Enable logging in Shipping builds so packaged-game logs are readable |
| 390 | done | claude-grapple | Fix grapple rope scaling + add right-click release input |
| 391 | done **UNOBSERVED** | claude-loadingscreen | Enable loading screen for menu->raid transition (release day) |
| 392 | done | claude-grapple | Buildings with no fracture asset now break: generic rubble fallback for the 41 of 42 SM_MERGED_House_* meshes missing a GC_ |
| 393 | done | claude-grapple | Bulk Chaos fracture generation: custom editor module calling FractureEngine/PlanarCut C++ directly, real GC_ assets for all 42 house meshes |
| 394 | done | claude-gdd | GDD: Hold and Loot horde orders confirmed working, update roster and inert-order status |
| 395 | done | claude-grapple | Perf: cap simultaneous smolder FX, extend real fracture to kitbashed wall pieces, fix Inn's PieceNameFilters |
| 396 | abandoned | claude-grapple | Building collapse debris deals too much damage after clustering - raise MinImpulseToDamage |
| 397 | done | claude-repack | Repackage for itch under 1GB: drop bCookAll, scope DirectoriesToAlwaysCook, strip dead content |
| 398 | done | claude-fracture-fix | Fix headless GC_ regeneration crash (IsFullyLoaded) |
| 399 | done | claude-chaos-bounds | Chaos ensure: NaN world-space inflated bounds on mass building destruction (completeAllObjectives) |
| 400 | done | claude-smoke-water | Smoke by proximity, and field fire stops at water/grass/props |
| 401 | abandoned | claude-smoke-water | Smoke by proximity, and field fire stops at water/grass/props |
| 402 | done | claude-settle | Freeze collapse physics after it settles; calm the collapse and the market |
| 403 | done | claude-trees | Trees vanish during field fire and mass collapse |
| 404 | done | claude-fracture-guard | Validate generated fracture assets against the source mesh so #399 cannot recur |
| 405 | done | claude-villagelag | General lag in the middle of the village during a raid |

<!-- BOARD:END -->

*Regenerated by `gsqueue.ps1`. Edit above this line, never inside the markers.*
