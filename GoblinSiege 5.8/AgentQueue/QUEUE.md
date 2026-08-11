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

_Queue is empty. The build gate is OPEN._

**BUILD GATE: OPEN - and 54 finished ticket(s) asked for a build.**

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

<!-- BOARD:END -->

*Regenerated by `gsqueue.ps1`. Edit above this line, never inside the markers.*
