# AGENT_STATE — Goblin Siege Code Architect

*Single-file agent memory (Michael's ruling 2026-08-04: one distilled state file, not 69 docs).
Human-editable; the agent reads it at run start and rewrites NEXT + appends BUILT/RUNS at run
end. Deep context lives in the Claude project docs; this is the distillation. Seeded 2026-08-04
from the status-and-rebaseline doc, the decision queue, and a live scan.*

## BUILT
- pre-seed: player character complete (third-person rig, soft-lock, crouch, sprint/stamina, dodge, block, guard-break, hit-reacts, ragdoll death), traversal (vault/mantle/climb) PIE-verified, sword combat vs target dummy (GSGA_SwordLight/Heavy, weapon component, DA_Weapon_Scout), fire system large (flammable, fire volumes, field-fire grid, mill dust-fuse, market, burn-mask/char materials village-wide), burn objective base with Required/Optional/Complete, alarm types, GA_GS_* ability BPs, adversary BPs placed (CastleGuard/Archer/Knight/Peasant), Tutorial_Island playable with BP_GSGameMode.
- 2026-08-04 [mvp-001] STAGED Interact framework — hold-E channels + carry: 8 files at out/runs/mvp-001/staging/ — awaiting Michael's review, then an editor-closed full build (new UCLASS types; Live Coding cannot register them)
- 2026-08-05 [raid-loop-001] **BUILT + COMMITTED (108c49e) — the raid loop closes.** `UGSRaidDirector` (world subsystem: carrier roster by world sweep AND idempotent self-announce, Q-32/Q-37 demotion pass, one-of-each-type win, StartRaidClock, BeginObjective dispatch, EndRaid), `AGSRunicSite` (overlap auto-bank extraction, replicated portal driving the Portal 4 mesh+Niagara), `AGSRaidMarker` + 9 `Marker.*` tags, `UGSRaidLibrary` (scripted-placement bridge). GSGameMode: ChoosePlayerStart → runic site, respawn offset OUTSIDE the extraction sphere, out-of-lives now ends the raid. HUD: objective list (names only, no arrows), clock, lives, alarm + 5 BP hooks. Build clean, 54s, no warnings from these files.

## DECISIONS

- Canonical class name is SCOUT (amends decision 36); sword ⇄ bow.
- Win = burn one of each TYPE (mill/field/market) then extract; siblings demote to Optional (Q-32).
- Horde: ground-bound but CAN VAULT (41-a); never climbs/mantles; pool debits on spawn only (40); light fire avoidance + giggle barks.
- Burn visual mandate (2026-08-01): anything burnable chars black + smoulders; mill sails spin while burning, stop at Detonated; breaking meshes later.
- Interact verbs for the slice: loot / takedown / foul-well / extract + carry state; hold-E channels, abort on damage/release/range-facing break; takedown 1.2s channel, 120° behind-cone (placeholders signed).
- Single-player slice, co-op-ready: replicate cheap root state only (Q-36 pattern).
- Human gate: agent output is STAGED for Michael's review before entering Source/ — post-run review, Bark Foundry pattern.

## NEXT

### Queue: Tutorial Island content (raid-loop-001 follow-on, 2026-08-05)
*The C++ loop is done and committed; what remains is MAP CONTENT. A survey of L_Tutorial_Island found
it far thinner than "Tutorial_Island playable" implied: ONE burn objective (`GS_MillField`, 33x33 @640,
**ObjectiveTypeTag EMPTY**), no mill/market/granary objectives, no mission objectives, no runic site,
no spawners, 3 loose enemies. Landmarks to build against: 4x SM_WIndmill_Base ~(12712, 79797);
28x SM_MarketStallStructure ~(-8858, 54481); field at (4500, 75000); PlayerStart (-14200, 59000);
1 NavMeshBoundsVolume; 1 Landscape.*

- [BLOCKED-ON-BUILD] u=10.0 **Editor-closed rebuild** — `UGSRaidLibrary` + `SetObjectiveIdentity` +
  `AGSRaidMarker::Configure` are written but NOT compiled. Everything below needs them.
- [QUEUED] u=9.0 **Tag the carriers** — `GS_MillField` → `Objective.Burn.Field` "The Wheat Field".
  Nothing in C++ sets this tag; an untagged carrier is invisible to the demotion pass, so the map is
  literally unwinnable until this is done. The director logs an Error naming it on Play.
- [QUEUED] u=8.0 **Second field** (`GS_FarmField`, 12x12) — with one carrier per type nothing ever
  demotes and Q-37, the whole reason the director exists, is never exercised by a playtest.
- [QUEUED] u=8.0 **Windmill objective** at the SM_WIndmill_Base cluster → `Objective.Burn.Mill`.
  Testable immediately via `GS.Burn.IgniteAll` (ForceIgniteVerbose calls `Mill->IgniteInterior()`,
  bypassing windows). A PLAYER still cannot light it until BP_GS_Windmill has `Window`-tagged
  primitives — `AGSMillObjective` refuses exterior fire by design.
- [QUEUED] u=7.0 **Market objective + flammable stalls** — `AdoptCluster` only adopts actors carrying
  a `UGSFlammableComponent`, and all ~130 stall/market actors are plain StaticMeshActors, so it
  adopts ZERO and says so only in a log. Fix with `UGSRaidLibrary::MakeActorFlammable` over the
  28 SM_MarketStallStructure actors (uses AddInstanceComponent, so it survives a level save).
- [QUEUED] u=7.0 **Place GS_RunicSite** at the PlayerStart + author BP_GS_RunicSite composing
  SM_Portal4 / M_Portal4 / N_Portal4_V2 (decided 2026-08-05; the pack ships no Blueprints).
- [QUEUED] u=6.0 **WBP_GSPlayerHUD widgets** — add `ObjectiveListText`, `ClockText`, `LivesText`,
  `AlarmText`. BindWidgetOptional means a name mismatch fails SILENTLY; NativeConstruct logs each
  unbound one.
- [QUEUED] u=6.0 **Playtest all three end states** — Extracted, LeftBehind (clock), OutOfLives.
- [QUEUED] u=4.0 **Capture Tutorial Island as the generator template** (Michael's 2026-08-05 framing:
  the finished map is the reference the generator is DERIVED from, not a thing the generator
  replaces). `terrain/t1_static_geo.json` already has `stairs`/`bridge` but `patrol`,
  `village_fence`, `road_fence`, `river_bounds` are empty arrays awaiting exactly this.

### Pre-existing queue
- [ELIGIBLE] u=10.0 **Interact framework — hold-E channels + carry** (block A) — missing: UGSInteractableComponent, UGSInteractionComponent, UGSGA_Interact, UGSCarryComponent
- [EDITOR] u=6.0 **Death & hit-reaction clips retargeted ('nothing can die on screen')** (block B) — missing: AM_GS_Death
- [ELIGIBLE] u=5.75 **Someone to fight — human race data, BT_Militia, enemy attack path** (block B) — missing: DA_Race_Human, BT_Militia, DA_Weapon_Greatclub
- ~~[ELIGIBLE] u=4.0 **Lives / respawn on PlayerState**~~ — **DONE, and this entry was stale**:
  `AGSPlayerState` + `Lives` already existed (Core/GSPlayerState.h:38-42). What was genuinely missing
  was the out-of-lives CONSEQUENCE, added 2026-08-05. Verify against source, not this file.
- ~~[BLOCKED] u=4.0 **Runic site**~~ — **DONE 2026-08-05** (`AGSRunicSite`): spawn/respawn target,
  objective-gated portal, overlap auto-bank. The 90s collapse window still belongs to the clock
  (`AGSGameState`), which the director now listens to for `Expired` → `LeftBehind`.
- [BLOCKED] u=4.0 **Score system — deeds/loot two-kind tally + end screen** (block G) — missing: UGSScoreSubsystem, GSScore
- [BLOCKED] u=3.2 **Horn & horde (subsystem, pool, BT, point command)** (block D) — missing: UGSHordeSubsystem, AGSHordeSpawnMarker, UGSGA_Horn, BT_HordeGoblin, AGSHordeGoblin
- [BLOCKED] u=3.0 **The stealth five (noise, crouch-detect, takedown, corpse-suspicion, coin toss)** (block F) — missing: ReportGSNoise, Takedown, CoinToss, DT_NoiseEvents
- [BLOCKED] u=2.5 **Gore/gib system (intensity scalar, feather-poof)** (block G) — missing: UGSGibComponent
- [ELIGIBLE] u=2.5 **Barks + Overlord whispers (runtime side)** (block H) — missing: UGSBarkSubsystem, DT_Barks
- [BLOCKED] u=2.33 **Patrol director — 5-7 min cadence + castle reinforcements** (block F) — missing: UGSPatrolDirector
- [BLOCKED] u=2.25 **Loot couriers — sacks + livestock cargo, point-to-courier** (block G) — missing: UGSCarryComponent
- [BLOCKED] u=1.75 **Civilians + livestock (routines, disbelief, brigade, flee)** (block G) — missing: BT_Civilian, DA_Race_Livestock

*(ranking from run mvp-001, 2026-08-04)*

## FAILED

- pre-seed (from build log / decision queue — do not rediscover): Live Coding cannot register new UCLASS/UPROPERTY — full editor-closed build required. In-editor Compile gives zero feedback; stranded-UBT bug = `Launching UnrealBuildTool...` with no `HotReload took` → kill orphaned dotnet. `.bat` written from a Linux sandbox needs CRLF. PowerShell over the MCP bridge: no `$`, quote every path, `--%` for native args. VibeUE `list_expressions` inlines nested material functions — connecting to an inlined node writes an illegal cross-package ref that blocks saving. `NS_GS_SmokeColumn` must use `M_Smoke_01`, never `M_Smoke_02` (broken). Content/GoblinSiege + DreamscapeSeries paths are untracked in git — agent-side edits there have no backup.

- 2026-08-05 (raid-loop-001, do not rediscover — each of these cost real time):
  - **`gs_ue.ps1` was unusable; use `gs_ue.py` instead.** Every call hung forever with NO output at
    all, not even the script's own failure line — which reads exactly like "the editor is busy" and
    is not. THREE stacked causes: (a) `Accept: text/event-stream` makes the MCP server choose an SSE
    stream and hold it open, so the client blocks forever — ask for `application/json` only;
    (b) `Invoke-WebRequest` hangs on this endpoint under PS 5.1 anyway and `-TimeoutSec` does not
    fire, while `curl.exe` with identical URL/headers/body answers in 0.34s; (c) nested
    `powershell -File ...` stalls regardless of what the script does. `gs_ue.py` (stdlib urllib)
    has none of these. Launch the editor with `-ExecCmds=ModelContextProtocol.StartServer`.
  - **PowerShell mangles a JSON string passed to a native exe** — inner double quotes are stripped
    and the server answers `-32700 Invalid JSON body!`. Write the body to a file, send `@file`.
  - **`execute_python_code` DISCARDS all buffered stdout when the script raises.** Every print before
    the exception is lost, so a script that dies halfway looks like it printed nothing. Accumulate
    into a list and print once at the end, with each step in its own try/except.
  - **FGameplayTag cannot be built in editor Python** in this build: no `request_gameplay_tag` on
    `GameplayTagLibrary`, `tag_name` is read-only, `GameplayTag("Some.Tag")` does not construct,
    `GameplayTagContainer.gameplay_tags` is read-only, and `make_literal_gameplay_tag` needs a tag
    you already have. Use `UGSRaidLibrary::MakeTagByName` / `SetObjectiveIdentity`.
  - **`FGameplayTag` has no `to_string()` in Python** — `str(tag)` on an empty tag prints
    `<Struct 'GameplayTag' ... {}>`, which is easy to misread as a populated value.
  - **During PIE, `UnrealEditorSubsystem.get_editor_world()` returns None and
    `EditorActorSubsystem.get_all_level_actors()` returns 0.** This looks exactly like "the level
    unloaded" or "the editor died". Check `LevelEditorSubsystem.is_in_play_in_editor()` first and
    use `get_game_world()` + `GameplayStatics.get_all_actors_of_class` instead.
  - **A component added from Python does not survive a level save** unless it goes through
    `AActor::AddInstanceComponent` — the level looks dressed until you reload. Hence
    `UGSRaidLibrary::MakeActorFlammable`.

## RUNS
- 2026-08-04 **mvp-001** — picked 'Interact framework — hold-E channels + carry' (u=10.0); ok
- 2026-08-05 **raid-loop-001** — closed the raid loop (start/win/lose/extract) + HUD; built clean,
  committed 108c49e. Map content queued above, blocked on one editor-closed rebuild.
