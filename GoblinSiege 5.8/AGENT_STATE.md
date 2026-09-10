# AGENT_STATE — Goblin Siege Code Architect

*Single-file agent memory (Michael's ruling 2026-08-04: one distilled state file, not 69 docs).
Human-editable; the agent reads it at run start and rewrites NEXT + appends BUILT/RUNS at run
end. Deep context lives in the Claude project docs; this is the distillation. Seeded 2026-08-04
from the status-and-rebaseline doc, the decision queue, and a live scan.*

> ## `HANDOFF.md` — the nine findings are FIXED. Read it for the gotchas, not for a work list.
>
> **Corrected 2026-08-07 (#073).** This banner said for a full day that `HANDOFF.md` held "nine
> outstanding code-review findings that nobody has fixed". All nine were closed by **#036** (four,
> one of which #033 had already deleted as a side effect) and **#037** (the five C++ ones) — and
> neither ticket updated this banner or the heading in `HANDOFF.md`, so the first thing every agent
> read at run start was an invitation to redo finished work. Re-verified before this edit, not taken
> on trust.
>
> `HANDOFF.md` is still worth reading for **Part 2** (ranged/torch state), **Part 3** (the settled
> radial weapon-wheel design) and **Part 4** (gotchas that cost real time). Part 1 is now history.
>
> **The lesson, which is the reason this paragraph is long:** a finding lives in three places — the
> ticket, `HANDOFF.md`, and this file. Closing the ticket closes one of them. If you fix something
> off a list, close the list, or the next agent pays for it. This cost roughly a session.

**Coordination lives in `AgentQueue/QUEUE.md`, not here.** Before editing any file, claim it:
`& ".\AgentQueue\gsqueue.ps1" claim -Agent <slug> -Title "<t>" -Files "a,b"`. Lower ticket number
has right of way on a shared file; nobody compiles until `gsqueue.ps1 buildgate` exits 0. Tickets
are committed and keep their Generate/Evaluate/Refine as the review record, but **nothing reads
old tickets at run start** — fold anything durable into BUILT / DECISIONS / FAILED below, or the
next agent rediscovers it.

## FAILED
- 2026-09-09 (#399) **A GENERATED ASSET THAT LOADS, COOKS AND SHIPS CAN STILL BE GEOMETRICALLY
  CORRUPT - and none of #398's checks could see it.** The Chaos NaN ensure Michael hit on
  `completeAllObjectives` was ONE bad fracture asset: `GC_MERGED_House_Medium_07` carried rest
  transforms 270,634uu / 144,992uu from the house (siblings: ~2,800 / ~4,000), so Chaos solved
  constraints spanning kilometres and the accumulated impulse went NaN. Three of three placed
  instances failed; every other asset was clean, including four with MORE pieces - piece count,
  mass and the collapse-ring impulse were all ruled out by that. The source mesh
  (`SM_MERGED_House_Medium_07`, 2336x2588x2128uu) is fine - **fracture GENERATION introduced it**.
  Regenerating that one asset fixed it: rest extent 270,634 -> 2,323, and the identical repro then
  produced no ensure and zero NaN transforms across all 366 collections.
  **Three lessons, in order of how much they will cost the next person:**
  1. **`Content/*` is gitignored, so `Content/Destruction/GC_*` is NOT in the repo, and the same
     source mesh with the same parameters produced a corrupt asset on 09-01 and a clean one on
     09-09.** Voronoi site placement is random: this is a NON-DETERMINISTIC failure, roughly 1 in 40
     on that batch, and every future regeneration is another roll. `GenerateFractureAsset` should
     validate its output's rest extent against the source mesh's bounding box before saving and
     refuse or retry - not written, needs a compile. **The shipped itch build contains the corrupt
     asset.**
  2. **#398 verified 84 regenerations by file size (1.8GB -> 1.2GB), non-crashing, and a successful
     repackage, and concluded they were "structurally valid, not just present on disk".** They were.
     A corrupt-geometry collection loads, cooks, packages and ships perfectly well - not one of
     those three checks looks at where the pieces ARE. When a tool generates geometry, the
     verification has to measure the geometry.
  3. **A tight temporal correlation in one log is a hypothesis, not a cause.** This ticket's second
     pass pinned it on `SweepStragglers` - 147 stragglers swept 0.35s before the ensure, via an
     unguarded `ApplyExternalStrain(..., 1000000.f)`, which was genuinely suspicious and read as
     convincing. The reproduction showed zero `swept` lines in the frame it fired. Reproducing and
     counting took minutes and killed a wrong answer that a third round of reasoning would have kept
     building on.

- 2026-08-30 (#389 then #388) **`bUseLoggingInShipping = true` is a DEAD END on this engine
  install, REVERTED.** Landed to fix a real problem (a packaged Shipping .exe writes a totally
  empty `Saved/Logs/MyProject.log`, NO_LOGGING=1 being Shipping's default, which is why the
  "New Raid does nothing" packaged-build bug had zero log evidence even though the identical click
  logs plenty in PIE) - but setting it makes UBT require `BuildEnvironment = TargetBuildEnvironment.Unique`
  (a target whose rules differ from `UnrealGame`'s cannot use the default Shared environment), and
  **"Targets with a unique build environment cannot be built with an installed engine"** - this is
  an installed/Rocket 5.8 build, not source, and that restriction is absolute here. Confirmed by
  actually trying to package with it: `RulesError`, build refused outright. Reverted in full.
  **If a live-readable Shipping log is needed again**, the real options are: package as
  `DevelopmentClient` instead of Shipping (keeps logging/console without a Unique build environment,
  at the cost of Shipping's size/perf), or build the engine from source. Do not re-attempt
  `bUseLoggingInShipping` on this install without one of those.
- 2026-08-30 **The real reason the mill was hard to exterior-ignite: fields were stealing the hit
  (#366 + #367, watched).** #361/#362 (below) were real, necessary fixes but didn't fully explain
  why exterior ignition still felt unreliable - two sessions of theories (reach/height, then
  collision) went nowhere. Michael asked for instrumentation instead of another guess:
  `UE_LOG` added to `AGSTorchProjectile::OnProjectileHit` printing every hit and which
  `AGSBurnObjectiveBase` claimed it (#366). One test run showed the actual bug immediately: torches
  that visibly landed on the mill's own tower - and separately one that hit an unrelated house -
  were BOTH being claimed by `GSFieldFireObjective_0`. `AGSBurnObjectiveBase::FindObjectiveAtLocation`
  returns the FIRST objective (arbitrary `TActorIterator` order) whose `ContainsWorldLocation`
  matches, and a field's version of that check is a coarse rectangle over its whole grid with no
  idea a structure sits inside it. Independently confirmed by Michael before the fix even built:
  the mill only ignited once its overlapping field objective had already COMPLETED (completed
  objectives are skipped by the lookup) - an accidental, unintended dependency.
  Fix (#367): `FindObjectiveAtLocation` now runs structures first, fields only as a fallback when
  no structure claims the point. `AGSFieldFireObjective`/`AGSMillObjective`/`AGSBuildingObjective`
  themselves untouched - purely a dispatch-priority fix in the shared base. Verified live: torched
  the mill while a field was still actively burning (previously-broken exact scenario) - "it burns
  now before the Field." **Lesson for next time a fix "should work" but doesn't fully land**: this
  is the second time this session a plausible, well-reasoned fix (#362) turned out to be necessary
  but not sufficient, with the actual remaining blocker somewhere the reasoning hadn't looked -
  instrumentation beat a third round of theorizing. The `TORCH HIT` diagnostic log lines are left in
  place (cheap, Warning-level, marked temporary) rather than stripped immediately.

- 2026-08-30 **Windmill roof (`SM_RoofTIles`) was invisible always, not just from a distance (#365,
  watched, both mills).** Root cause: the roof's three material instances (`MI_TowerRoofTiles`,
  `MI_TowerWood`, `MI_TowerSpear`) all inherit `dithered_lod_transition = true` from their shared
  master `M_Props_Master` - a property whose own doc string says it's for the foliage system. The
  roof is placed as a plain `StaticMeshActor` (ground mill) or a `ChildActorComponent`-spawned one
  (hill mill), never as foliage/HISM, so the shader never gets the per-instance dither data it
  expects and renders fully dithered-out. Fixed with an instance-level `base_property_overrides`
  override on the three affected instances only; the shared master is untouched. Found by
  elimination after every other property (geometry, position, visibility, scale, blend mode, WPO,
  ground-coverage switches, textures, compile errors) checked out clean - swapping the whole
  material to `DefaultMaterial` was the first thing that actually changed anything. **Also
  confirmed NOT caused by anything this session touched** - reverted the level to the last commit
  and it was still broken, and a subagent sweep of every burn/crumble/fracture C++ file found zero
  references to this mesh or property. `MaterialInstanceBasePropertyOverrides.to_dict()` returns
  `{}` even when correctly set - verify via the individual named fields instead; see the CLAUDE.md
  gotcha.

- 2026-08-30 **Mill exterior fire, actually fixed (#361 + #362, watched with the correct repro).**
  #361 retired the "stone base, no purchase" rule by rewriting `IgniteAtLocation` to call
  `IgniteInterior()` - and was closed `done` on evidence (Michael watched both mills burn
  cinematically) that turned out to have tested the WRONG path: both mills that session were lit via
  the window-overlap route, not the exterior torch route #361 claimed to fix. **`IgniteAtLocation`'s
  body was never the gate** - `AGSTorchProjectile::OnProjectileHit` only calls it on whatever
  `AGSBurnObjectiveBase::FindObjectiveAtLocation` returns, which depends entirely on
  `ContainsWorldLocation` answering true, and the base class's version unconditionally returns false.
  `AGSMillObjective` never overrode it - documented in three separate comments as the actual
  "windows only" mechanism, all of which #361 left unread. #362 added the override (true within the
  resolved geometry actor's bounds, padded 300uu) and corrected the three stale comments. **Verified
  correctly this time**: Michael threw an actual torch at the specific mill that had failed before
  and watched it catch and collapse. Lesson for next time: "a human watched it" still has to be
  watching the SPECIFIC path a change claims to fix, not an adjacent success - isolate what actually
  triggered ignition before calling it verified.
  - Separately, same session: `UGSBurnFXComponent` gained `SetCharTargetActor(AActor*)` so its
    char/MID cache can target a DIFFERENT actor than its owner - the mill's `MillMesh`/`SailMesh` are
    empty placeholders, the real geometry is a separate `StaticMeshActor` resolved at runtime (see
    #360). `AGSMillObjective::BeginPlay` now resolves and caches it once (`CachedMillGeometry`,
    shared by `ContainsWorldLocation` and `SinkTower` too) and redirects char to it. Confirmed
    working in the same verified test.
  - **Two leaked-PowerShell-process memory incidents hit this session** (one crashed the Epic Games
    Launcher via OOM) - both were MY OWN timed-out background tool calls still running/buffering, not
    an engine/content leak. `Get-Process powershell | Sort WorkingSet64 -Descending` before chasing
    an in-engine memory leak after a string of timed-out MCP calls. Written up in
    `GoblinSiege 5.8/CLAUDE.md`'s Editor Python gotchas section.
  - **Follow-ups opened, not fixed:** (1) some `AGSBuildingObjective` clusters appear to count as more
    than one building and spawn multiple full-size fires each (Michael observed this live during a
    full-map burn) - worth a ticket on per-cluster fire/smolder instance count. (2) the mill still
    logs a stale "has a UGSBurnFXComponent but no UGSFlammableComponent - it will never char" warning
    at BeginPlay that is no longer true - cosmetic, worth a wording/gating pass. (3) `SM_RoofTIles`
    (and presumably the tower base) has a much shorter authored max-draw-distance than the sail mesh,
    so from across the map only the sail is visible - not a bug, just an inconsistency worth tuning if
    the windmill should read as a landmark from farther out. (4) `AGSMillObjective::DropSails`'s loose-
    part name matching (`LoosePartNameFilters`) still uses case-SENSITIVE `Contains`, unlike
    `SinkTower`'s fixed `ContainsWorldLocation`/geometry search - same class of bug as #360, not yet
    known to have bitten anyone, worth the same `ESearchCase::IgnoreCase` fix opportunistically.

- 2026-08-27 **WORLD CORRUPTION, STAGES 0-3 OF 6 - the land turns as you raid** (#296/#309/#312/#315/#316/#320/#325/#327, all watched). Ledger rulings **40-45** (2026-08-21) and **62** (2026-08-24). Un-iceboxes #252 - see the 2026-08-23 icebox note below, now superseded.
  - **`UGSCorruptionSubsystem`** (`World/`, `Config = Game`, 10 Hz, one timer): a constant-rate follower (`FInterpConstantTo`, never `FInterpTo` - an exponential settles a big step and a small one in the same time, which is backwards for "lurch on an objective, creep on a kill") over five weighted terms. Monotonic high-water ratchet. Public shape deliberately mirrors `AGSGameState`'s alarm meter (`GetCorruption01`, `OnCorruptionStageChanged(New, Old)`).
  - **All five drivers live**: objectives 0.45 · kills 0.20 · structures 0.15 · clock 0.10 · horde 0.10, plus a razed floor at 0.85. 1.00 is reachable from gameplay alone.
  - **`AGSCorruptionDirector`** (found-or-spawned, never placement-dependent): captures the level's authored sky/fog/sun values as baselines and lerps FROM them; spawns an `ExponentialHeightFog` and its own unbound `APostProcessVolume` (prio 1000, `BlendWeight` fixed at 1) when the map has none. Restores baselines on teardown so a PIE stop does not leave the editor sky red.
  - **The grade is two-ended and live at all times** (Michael, 2026-08-25): clean = bloom 3.0 / threshold -0.5 / exposure +0.75, gritty = bloom 0.35 / threshold +1.2 / grain 0.6 / sat 0.55. **The `BloomThreshold` sweep is the best thing in it** - as the world darkens, only genuinely hot pixels bloom, so *the fires the player set* become the only things in frame that glow. A tuning pass that flattens that curve loses it.
  - **`AGSGameMode::OnCharacterKilled`** (#325) is that class's FIRST delegate. Broadcast as the first statement of `HandleGoblinDeath`, **above both early returns** - a defender whose controller is already destroyed returns on `!Controller`, and every AI defender returns on the missing `AGSPlayerState`, so a broadcast placed after either reports PLAYER deaths only. Before this, killing the entire garrison reported to nothing.
  - **Civilians corrupt more than soldiers** (ruling 62): `Weighted = Soldiers + Civilians x GS.Corruption.CivilianWeight` (2.5). Discriminator is `AGSEnemyCharacter::GetArchetypeRowName() == "Civilian"` - a `DA_Race_Human` row key, NOT a class-name match. Watched: 2 soldiers + 1 civilian = 4.5 weighted, meter 0.02 -> 0.06 unforced.
  - **`GS.Corruption.Set / .Release / .Step / .Dump / .Refresh`**, cvars `.RiseRate .FallRate .RatchetFraction .CivilianWeight`.

- 2026-08-21 **ACF PHASE 2A: `AGSCharacterBase : AACFCharacter`, ONE ASC** (#223, watched). Our ASC is
  gone; ACF's `ActionsComp` is the only one, cached under the old member name so ~20 call sites were
  untouched. `UGSAttributeSetBase` stays a default subobject of the ACTOR, which is how the ASC adopts
  it (`InitializeComponent` walks the owner's subobjects). `bAutoInit = false` keeps ACF's initialiser
  off - no `UACFCharacterDataAsset` is authored, and with it on every character gets zero health.
  `AscentSaveSystem` and `CharacterController` are now LINKED in `Build.cs` (deriving is not linking).
  - **`UGSCharacterMovementComponent` exists to disarm ACF's locomotion state machine.** ACF's
    movement component owns `MaxWalkSpeed` and rewrites it from `LocomotionStates`
    (Idle 0 / Walk 250 / Jog 500 / Sprint 650, `DefaultState = EJog`) on every band transition, which
    makes sprint STRUCTURALLY IMPOSSIBLE - the Sprint band needs velocity above 505 while the cap is
    500. Our subclass empties the bands in `BeginPlay` and **restores the authored `MaxWalkSpeed`
    afterwards**; that restore is load-bearing, because with no bands ACF's
    `Internal_ApplyLocomotionState` resolves to `0.0f` and every character stands still. **Deleting
    this class is part of Phase 2b** (ruling 27).
  - **ACF's AI machinery is now awake.** `AACFAIController::OnPossess` used to early-return on our
    pawns because they failed `Cast<AACFCharacter>`; they pass now, so its blackboard init runs and it
    reaches the tree check - hence six `should be assigned with a behavior Tree` warnings.
    **CORRECTED 2026-08-27 (#330): the rest of this bullet is now false and it cost a session.** It
    used to say `StartTree()` stays quiet ONLY because no ACF BehaviorTree is assigned, and that our
    own `RunBehaviorTree()` calls remain load-bearing. `BP_GSAIController_Militia` now carries
    `BehaviorTree = BT_Defender`, so ACF's `StartTree()` DOES run - and our `RunBehaviorTree()` at
    `GSAIControllerBase.cpp:137-152` then runs a SECOND tree on a SECOND
    `UBehaviorTreeComponent` (ACF never assigns `BrainComponent`, `ACFAIController.cpp:53`), and
    re-initialises the blackboard to `BB_Human`, invalidating every key ID ACF cached at `:87-97`.
    **Two trees on two blackboards driving one movement component** is the cause of the patrol
    misbehaviour chased all of 2026-08-27, not anything to do with locomotion states.
    Also false, same ticket: `GSCharacterMovementComponent.cpp:18-20` claims the emptied bands make
    `Super::BeginPlay()` write MaxWalkSpeed to ZERO so the restore at `:23` is not optional. The miss
    branch at `ACFCharacterMovementComponent.cpp:677-692` is **log-only** - it writes nothing, and
    `"Locomotion State inexistent"` is a marker, not a cause. `GSAIControllerBase.h:13-16` carries the
    same stale "load-bearing" claim. Consequence: ACF's attacker ticketing
    (`MaxAttackersPerTarget = 1`) is now one asset assignment away from clamping the horde to one
    attacker - ruling 33's insurance was never implemented and is a live debt.
  - **NOT DONE, do not mistake it for done:** ACF's `StatisticsComp` is present and uninitialised, so
    rulings 25/35 are unsatisfied; ACF's own `IsAlive()` answers "alive" for a corpse because it reads
    a `UACFDamageHandlerComponent` nothing drives, so OURS is the truth until 2b routes death through
    it; fourteen unconfigured ACF components now sit on every character.
  - `ACFLog: Warning: Invalid Character - ActionsManager` is an **ACF logging bug** - it fires in the
    `else` branch, when the statistics component IS found. Ignore it.
- pre-seed: player character complete (third-person rig, soft-lock, crouch, sprint/stamina, dodge, block, guard-break, hit-reacts, ragdoll death), traversal (vault/mantle/climb) PIE-verified, sword combat vs target dummy (GSGA_SwordLight/Heavy, weapon component, DA_Weapon_Scout), fire system large (flammable, fire volumes, field-fire grid, mill dust-fuse, market, burn-mask/char materials village-wide), burn objective base with Required/Optional/Complete, alarm types, GA_GS_* ability BPs, adversary BPs placed (CastleGuard/Archer/Knight/Peasant), Tutorial_Island playable with BP_GSGameMode.
- 2026-08-04 [mvp-001] STAGED Interact framework — hold-E channels + carry: 8 files at out/runs/mvp-001/staging/ — awaiting Michael's review, then an editor-closed full build (new UCLASS types; Live Coding cannot register them)
- 2026-08-04 [live-003] STAGED Interact framework — hold-E channels + carry: 8 files at out/runs/live-003/staging/ — awaiting Michael's review, then an editor-closed full build (new UCLASS types; Live Coding cannot register them)
- 2026-08-04 [live-003] **PROMOTED + BUILDS CLEAN.** Interact framework is in Source/: `Interaction/`
  (Interactable/Interaction/Carry components), `Weapons/Abilities/GSGA_Interact`,
  `Combat/GSGE_MoveSpeedScalar`, plus 8 patched files (tags, player character, Block, TorchToss,
  SwordLight). Editor-closed Build.bat, 81.75s, zero errors, two C4996 warnings. Reviewed before
  promotion; four rulings applied to the generated code. **Still untested — nothing has run.** Next:
  IA_Interact + E in IMC_Default, CarrySocket on GOB_Scout_v2_Skeleton, HUD channel-bar bindings, a
  test chest in L_CombatArena, then PIE.

- 2026-08-04 **Defender handoff GOAL 1 DONE, PIE-verified.** All six `/Game/Blueprints/Adversaries/BP_*`
  reparented `Character` -> `AGSEnemyCharacter`; capsule/mesh/anim/max_walk_speed all survived
  byte-identical; `ai_controller_class` corrected from stock `AIController` to `GSAIControllerBase`
  and `auto_possess_ai` to `PLACED_IN_WORLD_OR_SPAWNED` (the BPs were overriding the C++ defaults);
  four ability classes assigned from `BP_GS_TargetDummy`'s set. In PIE on L_Tutorial_Island all three
  placed defenders are possessed by `GSAIControllerBase`, `TryLightAttack()` returns True, and the
  log shows `[GS.Damage] BP_CastleGuard01_C_0 -> BP_GSPlayerCharacter_C_0 ... = 25.0 (HP 50/100)`.
  Backups of all seven adversary BPs at `D:\goblinRaid\BP_Backup_20260804\`.

- 2026-08-04 **Defender handoff GOAL 2 (movement) DONE, PIE-verified.** Correct path taken: new
  `/Game/AI/DA_Race_Human` (Militia 30HP, Archer 20HP, Knight 75HP/armor 6 - GDD canon), `BB_Human`
  (TargetActor Object + TargetLocation Vector), `BT_Militia` (Selector: MeleeAttack -> MoveTo ->
  Wait, with an AcquireTarget service), and two new C++ nodes in `AI/Tasks/`
  (`BTService_AcquireTarget`, `BTTask_MeleeAttack`). All six BPs carry RaceData + row. **PIE: a
  defender chases the player at HU_Speed 1210.4** (DoD #2 needed >10), and melee fires autonomously.
  Archetype `MoveSpeed=0` now means "no opinion", so each BP keeps its height-derived speed.

- 2026-08-04 **Ranged combat framework — BUILDS CLEAN, UNTESTED.** Interactive session with Michael
  (branch `interact-framework`), plan approved before implementation. New: `Combat/GSAimComponent`
  (aim state + trajectory prediction + the arc ribbon + the aim-rotation RPC),
  `Weapons/GSArrowProjectile`, `Weapons/Abilities/GSGA_BowShot`. Patched:
  `Characters/GSPlayerCharacter`, `Weapons/Abilities/GSGA_TorchToss`, `Combat/GSDebugCommands`.
  Editor-closed Build.bat, zero errors, the same two pre-existing C4996s (Block, Interact) and no new
  ones. Built twice (89.63s, then 62.83s): the `RaceTag` / friendly-fire pass landed on
  `GSCharacterBase`, `GSEnemyCharacter`, `GSPlayerCharacter`, `GSGA_SwordLight` and `GSGameplayTags`
  four minutes after the first link, so the first result described a tree that no longer existed. The
  second build is the one that counts — **the two passes coexist and compile clean together.** Worth
  knowing for next time: two agents were editing `GSPlayerCharacter.cpp` in the same window, and the
  only reason it was caught was comparing source mtimes against the DLL.
  - **The muzzle has one definition now.** `GSGA_TorchToss::ThrowTorch` and the arc preview both call
    `UGSAimComponent::GetMuzzleTransform()`. `SpawnForwardOffset` and the `+50` hand-height literal
    are deleted; they previously existed in two files kept in step by a comment.
  - **The arc ships.** `PredictProjectilePath` + `DrawDebug*` (compiled out of a packaged build)
    replaced by a pooled `USplineMeshComponent` ribbon and a `UDecalComponent` landing marker — a
    decal so the marker lies on a slope instead of hovering over it. Built only on the locally
    controlled pawn, never replicated. Old debug draw survives behind `GS.Aim.Debug` (default 0).
  - **Bow is on the attack button**, reinterpreted by `IsInRangedMode()` — press to draw, release to
    loose, heavy-charge timer suppressed in ranged mode. Arrow is a real projectile (6000uu/s, 0.2
    gravity) so it can be dodged, so one arc system serves both verbs, and so AI archers get it free
    (`BP_ErikaArcher` wants exactly this). Damage via the existing `GSGA_SwordLight` path:
    `UGSGE_WeaponDamage` + `Damage.Bow` as both dynamic asset tag and SetByCaller key.
  - **Aim camera** blends to the left shoulder over 0.2s — arm 450→250, SocketOffset +55→−55,
    FOV 90→70 — on an explicit eased alpha, not `FInterpTo` (asymptotic, so the Tick early-out could
    never fire). Hip values captured from the components in `BeginPlay`, not duplicated as constants.
    Also deleted the dead `CameraBoom->SetRelativeRotation(-10 pitch)`: `bUsePawnControlRotation`
    overwrites it every tick, so the "low pitch by default" its comment promised was never in effect.
  - **Fixed a live bug**: the torch aim never called `UpdateRotationMode()`, so the arc was drawn
    along the control rotation while the goblin faced his movement direction.
  - **Ruling made during implementation**: facing and camera are now two predicates, not one.
    `WantsAimFacing()` keeps the ranged-mode term (unchanged §16 behaviour); `WantsAimCamera()` drops
    it, because pinning the camera at 250/70° for as long as the bow is equipped means the player
    never sees the hamlet again. Zoom is something you do while aiming a shot, not a property of what
    you are holding.
  - **Multiplayer**: ranged abilities no longer trust the server's `GetControlRotation()` for a
    remote pawn — `APawn::RemoteViewPitch` is byte-quantised (~1.4°), which is metres of drift across
    a 3s torch lob and would make the preview lie. `UGSAimComponent::Server_SetAimRotation` sends the
    exact rotation, pushed BEFORE activation; falls back to control rotation for standalone/AI.
    `EGSAimMode` replicates for remote aim poses; the arc never does.
  - **Still untested — nothing has run.** Blocked on editor-side work: author `M_GS_AimArc`,
    `M_GS_AimLanding` and an arc segment mesh (`/Engine/BasicShapes/Cylinder` is fine to start) and
    assign all three plus `BowShotAbilityClass` on `BP_GSPlayerCharacter`. Without the materials the
    aim works and warns once but draws nothing; without the ability class ranged mode falls through
    to melee rather than dead-keying the attack button. Then PIE on `L_CombatArena`: torch lands on
    its decal (test on a slope), body faces the aim while strafing, camera returns on release, bow
    hits `BP_GS_TargetDummy` — verify with `GS.Combat.LogDamage 1`, NOT `GS.Combat.Debug`.
  - Also added the two toggles that were silently missing from the `GS.PlayerView` table
    (`GS.Combat.LogHitReact`, `GS.Interact.Debug`) alongside the new `GS.Aim.Debug`.

- 2026-08-08 **NPC-vs-NPC MELEE WORKS AND IS PIE-VERIFIED** (#083, #085, #086). Two AI of opposing
  races acquire each other, read each other's wind-ups, raise a guard in response, and kill each
  other. A 4v4 on `L_CombatArena`: 44 telegraphs seen, 17 block rolls (8 accepted vs an intended
  0.55), 7 hits resolving `BLOCKED - armor 6.0 = 0.0`, **6 of 10 dead**. The proof pair is a
  `block ACCEPTED (telegraph, p=0.55)` line immediately followed by a `[GS.Damage] ... BLOCKED
  ... = 0.0` on that same defender, with the player nowhere in it.
  - **Almost none of this was new combat code.** Damage, the 140° block arc, guard-break and death
    were already faction-symmetric; what was missing was three decisions no AI could make. Who is my
    enemy: `BTService_AcquireTarget` was one hardcoded `GetPlayerPawn(0)` and is now nearest-live-
    hostile by `IsHostileTo`. Is he winding up: `State.Attacking.Windup`, a loose tag spanning
    exactly `FGSSwingStage::WindupSeconds`. Should I guard: `UBTTask_Block`, the only new UCLASS.
  - **`GS.Combat.Duel [n] [classA] [classB] [raceBAsset] [raceBRow]`** is how you stand a fight up.
    It exploits `InitializeFromArchetype` being public: side B is an ordinary human defender BP
    re-badged onto `DA_Race_Goblin` AFTER spawn, so **no goblin pawn or behaviour tree needs to
    exist** to exercise the melee. New: `DA_Race_Goblin` (rows `HordeGoblin` 40HP, `Brawler` 75/6
    arena-only), `BB_Human` += `TargetIsAttacking`, `BT_Militia` root Selector is now
    `[Block, MeleeAttack, MoveTo, Wait]` with a `TargetIsAttacking` LowerPriority-abort decorator on
    the Block edge. Debug: `GS.Combat.LogAI 1` beside `GS.Combat.LogDamage 1`.
  - **The block must be able to INTERRUPT.** Without the LowerPriority abort the Selector re-reaches
    Block once per ~2.3s cycle and a 0.22s wind-up never lands inside it — the guard goes up on a
    timer instead of on a read, which looks identical to "the AI does not block".
  - **Still unproven: the guard break has never once fired** (0 in 28 damage events). And both PIE
    runs needed the combatants teleported together — spawned 600uu apart they acquire correctly and
    then do not close, so `MoveTo` against the arena navmesh is suspect. Combat is tested; approach
    is not. Nothing ran on `L_Tutorial_Island`.
  - Also cleared #069's owed step 2: all seven adversary BPs kept all four ability slots across the
    hoist to `AGSCharacterBase`. Read off every CDO. #048's precedent was not needed.
  - **CORRECTION (#087): the "approach is not tested / MoveTo is suspect" line above is WRONG.**
    Combatants spawned 600uu apart close and pair off on their own. The earlier failure was the Block
    decorator observing an *invalid* blackboard key (before #086) and aborting the lower-priority
    `MoveTo` branch. The arena navmesh is 7000x7000 over a 6000x6000 floor and was never implicated.

- 2026-08-09 **A BLOCKED SWING IS TURNED ASIDE** (#087, Michael's ruling). New `State.Recoil`:
  `UGSDamageExecCalculation` calls `AGSCharacterBase::NotifyAttackWasBlocked` on the attacker, which
  cancels the swing in flight (by tag, deferred one frame - it runs inside that swing's own sweep
  loop) and blocks BOTH the attack and the block abilities for `GS.Combat.RecoilSeconds` (0.6).
  `UBTTask_MeleeAttack` bypasses its own cooldown against a recoiling target, and `UBTTask_Block`
  drops the guard to go and punish. Dodging stays legal so a player can escape their own mistake.
  **PIE: 30 damage events, 4 blocked, 26 landed, no deadlock**, one pair 75→56→37→18→dead. This is
  the anti-turtle answer, and it is what allied goblins have instead of a guard break.

- 2026-08-09 **THE HORN SUMMONS, AND THE HORDE FIGHTS** (#088). Everything #069 could not build with
  the gate shut, all editor-side, no C++: `BP_HordeGoblin` (duplicate of `BP_GS_TargetDummy`
  reparented to `AGSHordeGoblin` - it already had the Scout rig, anim BP and all four abilities;
  `GuardBreakAbilityClass` cleared on purpose), `BB_HordeGoblin`, `BT_HordeGoblin`,
  `BP_GSHordeAIController` (exists solely to hold `CompanionBehaviorTree`), the
  `[/Script/GoblinSiege.GSHordeSubsystem]` section with `HordeGoblinClassPath`, three
  `Marker.HordeArrival` in `L_CombatArena`, and `IA_Horn` on **MiddleMouseButton** with `HornAction`
  assigned. `Horn: 4 answered. Reserve 16, active 4/10`, then summoned goblins read knights'
  wind-ups, blocked them, and punished the recoil. Pool accounting matches decision 40 exactly:
  `Horde goblin died. Reserve 10 (unchanged)`.
  - **`UGSGA_Horn` has still never been fired by a human pressing MMB** - only via
    `GS.Horde.SpawnTest`. The input path is the most likely thing still broken.
  - **`L_Tutorial_Island` has no arrival markers**, so the horn summons nothing there.
  - Goblins die to two knight hits (40 HP vs 25). Archetype numbers behaving, but a warband melts
    against knights - wants more goblins or fewer knights before it reads as a battle.

## DECISIONS

### CORRECTION, 2026-09-10 — WE DO HAVE ROOT MOTION. THE FLAG IS OFF, THAT IS ALL.

I told Michael "ACF's montages expect root motion we deliberately stripped" while planning the
animation spine. **That was wrong, and he caught it as a hallucination.** It was repeated from
`gs-anim-adoption-gaps`'s summary line without checking a single asset.

**Measured, 2026-09-10, in `/Game/CombatMasterBundle/Animations/DynamicAxe/Manny_UE5/RootMotion/`:**

- **25 `_RM` anim sequences exist** — 16 combo attacks, 3 common attacks, 3 hit/die, 2 idle, 1 buff.
- **All 25 have `enable_root_motion = False`** (and `force_root_lock = False`).
- **The root track genuinely carries motion**, sampled at 21 points across each:

| asset | length | max root displacement |
|---|---|---|
| `Anim_DA_CommonAttack_A_RM` | 1.60 s | **364.56 uu** |
| `Anim_DA_Combo_A1_RM` | 1.25 s | **94.56 uu** |
| `Anim_DA_Idle_A_RM` | 2.08 s | 2.44 uu (static, correct for an idle) |

**So the animation spine's supposed blocker does not exist.** Adopting ACF's root-motion montages
needs `enable_root_motion` enabled on the attack/combo/hit sequences — a per-asset boolean, batchable
from Python — not new animation work. Idles should stay off.

**Expect a real feel change when it goes on:** a common attack will carry the character 364 uu. That
weight is the point, and it is what ACF's combat is built around, but it will need tuning against
capsule collision and the engagement distances the AI uses.

**`gs-anim-adoption-gaps` needs correcting** — its "root motion we deliberately removed" line is what
produced the false claim. The data was never removed; the flag was.


### RULING, 2026-09-10 — ACF IS THE SYSTEM. GS CODE IS THE EXCEPTION, AND IT NEEDS PERMISSION.

**Michael, in full, after a session spent fighting GS-authored systems:** *"We have been CONSTANTLY
fighting against the bugs in systems you created because you were convinced you could do more than
what ACF has already implemented... we would be better off if I had just not listened to you at the
start of this... Now is the time for us to start fresh using the lessons we learned from the
prototype we built... I want us to be ACFU's showcase. So let's use the system to the maximum of
its' potential instead of fucking around with bush-league half realized systems you created. This is
the last time I tell you this."*

**He has now said this THREE times.** The earlier two are already recorded ("check the vendor plugin
before building custom", the second time angrily). This is not a preference. It is the standing
architecture of the project.

**The rule, operationally:**

1. **Default to ACF for every system.** Not "borrow from", not "wrap" - adopt, and let it own the
   behaviour.
2. **Writing GS code that overlaps an ACF feature requires Michael's explicit permission FIRST,**
   with the ACF feature named and the specific reason it cannot serve. "Ours is more flexible",
   "theirs doesn't quite fit", "faster to write it here" are not reasons.
3. **The existing GS systems are a PROTOTYPE.** Their value is the lessons, not the code. Treat
   them as scheduled for replacement, not as an asset to defend.
4. **Read the ACF skill pack and the GS gap skill before touching a domain.** Both exist. This
   session proved they work: `gs-teams-damage-spawning` named a real permanently-invulnerable-corpse
   bug in one read, and `gs-character-data-asset` stopped a change that would have disarmed five of
   seven characters.
5. **The bar is ACFU's showcase project**, not "works well enough".

**Symptoms he named, all downstream of the same cause and all still open:** the grapple/climb never
reached its potential (ACF's CharacterController owns climbing, ladders and vaulting); combat is
boring and haphazard (GS abilities are plain `UGameplayAbility`, so ACF's Actions System, combo
graph, chooser actions and collision manager are idle); weapons grip oddly and **the bow is held
backwards right now** (no GS AnimBP is a `UACFAnimInstance`, so ACF's equipment-to-pose chain never
runs).


- **2026-08-31 (#391): `EnableLoadingScreen` FLIPPED TO `True` - STALE #335 TODO, UNOBSERVED.**
  Michael's report on release day: "there's no loading time after New Raid, so you can't tell if
  it's broken or not." `Config/DefaultPlugins.ini`'s `ALSLoadingScreenSettings` had
  `EnableLoadingScreen=False`, set by #335 specifically because `L_MainMenu`/the New Raid button
  did not exist yet - the block's own comment said a later "stage 8" would flip it once they did.
  `L_MainMenu`, `WBP_MainMenu` and New Raid all shipped since (#385/#388); stage 8 never ran, so the
  flip was just forgotten, not a decision anyone reversed. Flipped it. Confirmed the widget it
  points at (`ANS_LoadingScreen_WB`) exists on disk in the ACF plugin before flipping. **UNOBSERVED:
  config-only change, no rebuild needed, but nobody has watched a loading screen actually appear on
  a live New Raid click yet - if it still doesn't show, the next place to look is whether ACF's
  loading-screen subsystem is even initialized for this project (it may need more than the ini flag).**
  Left alone, adjacent and still stale from the same #335 plan: `WidgetRegistryAsset` /
  `DefaultMenuMap` / `DefaultNewGameMap` in the same file, still commented out.

- **2026-09-01 (#395): SMOLDER FX HAS NO PER-BUILDING COORDINATOR - GLOBAL CAP ADDED (`MaxGlobalSmolderFX
  = 40`).** Follow-up to #393: clustering fixed the collapse-piece-count cost, but a live re-measure
  (`stat dumpframe`, after clustering landed and frame time had NOT moved - 182ms -> 202ms) found the
  real culprit was `UGSBurnFXComponent::SpawnSmolder()` - permanent (`bSmolderForever=true`), uncapped,
  and per-PIECE. The Inn (`GSBuildingObjective_293`, 440 pieces, genuinely kitbashed and never
  mesh-merged like the rest of the village) had every burning piece independently spawn its own
  smolder system via `SpawnGenericRubbleFallback`: 577 simultaneous Niagara instances, ~70ms of
  game-thread time from particle-collision checks alone. Fixed with a file-scope static counter
  (`GActiveSmolderCount`) since this component has no building-level coordinator the way
  `AGSBuildingObjective::MaxFireFX` does. **Same session, two more Inn-specific fixes**: real fracture
  assets extended to kitbashed wall pieces (`GC_House_Wall_*`, same #393 tool, different NamePrefix -
  no code change, this is exactly the reusable process it was built for) and the Inn's
  `PieceNameFilters` cleared to empty on that ONE placed instance only (its furniture - `SM_Bar_Main`,
  `SM_Fireplace_Base`, etc. - matched none of the structural name filters and was never adopted at
  all, left floating once walls were destroyed; an empty filter adopts everything within
  `AdoptRadius`, per this class's own header). All three confirmed live: "the framerate was amazing,"
  "i watched it and the collapse looked great." **NEXT: the Inn's adoption footprint after clearing
  its filter was never audited for over-adoption** (the class's own documented risk - "drags in
  barrels and market tables"); roofs/floors/foundations/corners of the kitbashed kit still have no
  fracture assets, only walls do.

- **2026-09-01 (#393): BULK CHAOS FRACTURE GENERATION - ALL 42 HOUSE MESHES HAVE REAL `GC_` ASSETS
  NOW, VIA A REUSABLE EDITOR TOOL, NOT HAND-AUTHORING.** Followed #392's rejected physics-topple
  fallback (both versions of it failed live - see that entry). Built a NEW editor-only module,
  `GoblinSiegeEditor` (`Source/GoblinSiegeEditor/`), with `UGSFractureToolsLibrary::
  GenerateFractureAsset`/`BulkGenerateMissingBuildingFractures` - both `BlueprintCallable` and
  Python-callable from VibeUE. **The key discovery: `Fracture` and `PlanarCut` are
  `"EnabledByDefault": false` plugins in this engine install, but their compiled binaries (`.dll`/
  `.lib`) are already on disk** - enable them in `MyProject.uproject` (`TargetAllowList: ["Editor"]`
  so they never reach a packaged build) and a project module CAN link against `FractureEngine`/
  `PlanarCut` directly, bypassing the fact that neither module has a Python-exposed
  create-and-fracture entry point (`unreal.FractureEditorLibrary` does not exist; Dataflow's Python
  surface authors graphs but cannot evaluate them). The actual calls used are the same ones the
  project's own hand-made Dataflow graph (`DF_GS_HouseFracture`) already wraps:
  `FGeometryCollectionEngineConversion::AppendStaticMesh` then
  `FFractureEngineFracturing::UniformFracture`.
  **Two real bugs, each found by an actual live burn, not guessed:** (1) using the OTHER conversion
  entry point, `ConvertStaticMeshToGeometryCollection`, and manually assigning its output materials
  array afterward, scrambled every piece's material - `AppendStaticMesh` operating directly on the
  destination asset (`bAddInternalMaterials=true`) keeps geometry and materials in sync by
  construction and is the fix. (2) neither conversion nor fracturing generates collision -
  `FGeometryCollectionConvexUtility::CreateNonOverlappingConvexHullData` after fracturing is the
  missing step; without it the resulting debris has no collision at all ("I can just walk through
  it"). **NEXT: only 2 of the 42 regenerated houses were watched burning live** (the full bulk pass
  ran clean against the fixed code and every mesh shares the same code path, but that is
  "representative," not "watched" - see #393's own ticket for the honest distinction).
  `NumVoronoiCells` is a flat 8 for every mesh regardless of size; if a specific house's fracture
  density looks wrong later, that is the number to revisit, not the algorithm.

- **2026-09-01 (#394, ruling 73): ALL FOUR HORDE ORDER-WHEEL VERBS CONFIRMED WORKING - GDD AND
  LEDGER UPDATED, NOT JUST ATTACK/FOLLOW.** Michael, in session: Hold and Loot both land now, not
  just Attack and Follow. `BT_HordeGoblin`/`BB_HordeGoblin.uasset` show as modified, uncommitted,
  in the working tree - consistent with #213's spec (an `OrderVerb`-gated Hold branch, positioned
  above Menace Orbit/Chase/Follow, below Block/Melee Attack, `Observer Aborts: Both`) having been
  hand-authored in the BT editor, since #213 itself concluded a Python-injected node would not
  survive the asset next being opened. GDD §5 and §12.1 rows 6/17 re-graded WIRED -> BUILT/WIRED;
  full ruling in `docs/decisions-ledger.md` (2026-08-31 entry). **UNOBSERVED BY THIS AGENT: this is
  Michael's report, not a PIE session or log line this agent watched** - the doc change is
  accurate to what he said, not independently re-verified. **Smash is NOT part of this claim** -
  `BTTask_SmashOrderTarget` was never a wheel command (the wheel is Attack/Hold/Loot/Follow, #141)
  and stays uncalled. Also left alone: livestock (#379-381) stays UNOBSERVED, not folded into this.

- **2026-08-31 (#392): ONLY 1 OF 42 `SM_MERGED_House_*` MESHES HAS A FRACTURE ASSET - GENERIC RUBBLE
  FALLBACK ADDED, NOT REAL FRACTURES.** Michael, right after #390 closed: "not every building has
  their collapse mesh cached." `AGSBuildingObjective::SpawnCollectionProxy`/`CrumblePieces` already
  treat a missing `GC_<name>` fracture asset as the expected, silent case (see that method's own
  header) - so 41 of 42 house shapes simply stood untouched on `HandleCompleted`, with no error.
  A Geometry Collection bakes in the specific geometry it was fractured from, so the one existing
  `GC_MERGED_House_Small_03` cannot stand in for the other 41 meshes. Given the choice between
  bulk-authoring 41 real fractures (better result, unverified Python surface for Chaos Fracture Mode,
  large content lift) and a same-day generic fallback, Michael picked the fallback:
  `SpawnGenericRubbleFallback` hides+disables collision on a piece with no `GC_` match, plays a
  reused existing dust burst (`N_PebbleDust`) and debris sound, then destroys it.
  **SUPERSEDED same day - see #393, below.** The "close, watched, ship it" verdict here did not
  survive Michael's own follow-up watch: in the SAME live session he called this exact result
  unacceptable ("the building completely disappeared... a puff of smoke was there hanging in mid
  air"). A physics-topple rewrite was tried next and ALSO failed live two different ways (see #393).
  The real fix that stuck was bulk-generating actual fracture assets, not a better fallback -
  `SpawnGenericRubbleFallback` is back to exactly this hide+dust+destroy shape today, kept only as a
  rare safety net for a mesh the bulk tool has not covered yet. Read #393 before touching this
  function again; this entry is left here as the record of what was tried and rejected, not as
  current behavior to extend.

- **2026-08-31 (#390): `BP_GrappleHook` RETIRED FOR A NATIVE CLASS; `ActionsSystem` ADDED TO
  `GoblinSiege.Build.cs`.** #388 had already found `BP_GrappleHook`'s `HookMesh`/`RopeISM`/`RopeMesh`
  carried the same SCS-drop corruption as `BP_Statue_Warrior` - the engine silently drops corrupted
  components on every load, and this Blueprint's had real EventGraph logic wired to them. Replaced
  wholesale with `AGSGrappleHookProjectile` (native `UPROPERTY` subobjects - no SCS tree for the
  validator to drop). **The module gotcha to remember**: `UGSGA_GrappleThrow` was rebased onto
  `UACFGameplayAbility` to wire the throw into ACF's Actions System (arms `CurrentPriority`/combo
  buffering per `gs-abilities-outside-acf-asc`), which compiled fine through `AscentCombatFramework`'s
  transitive include path and then failed to LINK every `UACFGameplayAbility` symbol - the exact
  `#166`/`#280` trap already documented in that file, now with `ActionsSystem` a fourth confirmed
  case of it. **Next agent that derives from or calls into a new ACF module for the first time: add
  it to `PublicDependencyModuleNames` explicitly, do not trust the transitive include.**
  **NEXT: `AGSGrappleHookProjectile::HookMeshAsset` is still unset** - the hook head has no visual in
  flight (functional, same invisible-but-working degrade `AGSTorchProjectile` uses for a missing
  `TorchMesh`). Needs a mesh assigned in the editor; nobody has done this yet.

- **2026-08-20 (#207–#210): THERE WERE TWO DODGE SYSTEMS, AND FOUR PASSES WERE SPENT DEBUGGING THE
  ONE THAT WORKED.** Michael reported "the dodge plays the forward roll in every direction". The C++
  `UGSGA_DodgeRoll` was correct the entire time. **`BP_GSPlayerCharacter` contained a SECOND, complete
  dodge implementation** — `DodgeMontage_Fwd/Back/Left/Right`, `DodgeDistance`, `DodgeWarpLoc`, and
  motion warping — reached by a different key. **All four of its montage variables pointed at the same
  clip, `AM_GS_Dive_RM`**, so it could only ever play one dive whichever way you went.
  - **He was pressing `E`, which was `IA_Traverse`, not dodge.** Dodge was on `LeftAlt` and had always
    worked. Every "the dodge is broken" observation, mine included, was of the wrong system. **Check
    which key the tester is actually pressing before believing a report about an ability.**
  - **A Hold trigger does NOT stop the `Started` pin firing.** `E` was split Tap→`IA_Dodge`,
    Hold→`IA_Traverse` (#207) and the bug survived, because Enhanced Input fires `Started` on the
    initial press regardless of whether the hold completes. The Blueprint's traversal chain hangs off
    `Started`, so a tap still ran both systems into `DefaultSlot`. Fixed in #208 by moving that chain
    onto `Triggered` behind a `K2Node_ExecutionSequence` — **moved, not cut, because vault and mantle
    live on it and nothing else calls them.**
  - **`GS.Combat.LogDodge` is the dodge instrument** (#204), and it is what finally settled this: it
    logs the direction, the actor frame, both dot products and the chosen montage. Correct picks in
    the log beside a wrong animation on screen is a contradiction that can only mean two systems.
    Before it existed, three separate theories — clip finishing early, a 1.5 play rate, swapped root
    motion axes — all died on measurement. **Every one was reasoned from source and wrong.**
  - **WATCHED AND SIGNED OFF by Michael, 2026-08-20: dodge, climb, vault and mantle all work.**
  - `LeftAlt` is retired. Tap `E` dodges, hold `E` traverses and climbs — which is the first time the
    input asset has actually matched the 2026-08-08 ruling that "Hold-E climbs".

- **2026-08-20 (#209): A STAMINA COST BELOW THE REGEN-DURING-THE-MOVE IS FREE.** `DodgeStaminaCost`
  is 30, and the number is sized against regen rather than against the other verbs. The pool is 100,
  regen is 25/s with `RegenDelaySeconds` 0, and a dodge commits for its montage length (0.833s
  fwd/back, 1.000s left/right) because `bCommitForFullMontage` is true — so **21–25 stamina comes back
  during the roll itself**. The obvious first choice, something between vault's 8 and mantle's 18,
  would have cost nothing at all. 30 nets about −8 per dodge. **It is a throttle, not a wall**; if it
  must bite harder the sharper lever is `RegenDelaySeconds`, but that also hits sprint and climb.
  Deliberately NOT using `SetRegenSuppressed`: it is last-writer-wins and climb already uses it
  (#072/#076), so a dodge toggling it could hand the player free stamina on a wall.

- **2026-08-20 (#210): AN ORDERED GOBLIN USED TO PUBLISH A FOLLOW TARGET, AND THE TREE OSCILLATED.**
  `BT_HordeGoblin` has two branches that could both pass at once — `Follow Summoner` behind
  `Has A Follow Target`, and `Chase Target` behind `Has A Target`. `AGSHordeAIController` wrote
  **both** keys unconditionally, so a goblin under an Attack order had a live `TargetActor` AND a live
  `FollowTarget`, and the Selector flip-flopped. On screen: a goblin that stares at you, breaks off,
  stares again. Now `FollowTargetKey` is written as `bHasStandingOrder ? nullptr : FollowTarget`.
  - **Fixed in the controller, not the tree, on purpose.** The controller is the only thing that knows
    an order exists; a decorator cannot out-vote a key that should never have been set.
  - **Not a cancel** — the subsystem still holds the summoner and slot, so the goblin rejoins the
    scamper on the first refresh after the order clears.
  - **THE SAME SHAPE EXISTS IN FRENZY AND IS UNFIXED.** A goblin auto-engaging a threat also holds
    both keys. Left alone so the Attack-order fix could be attributed on its own. **If the staring
    happens with no order issued, that is this, and it is a one-word change.**

- **2026-08-20: WHAT THE UNOBSERVED CLOSES OF #204, #205, #207, #209 AND #210 LEAVE UNPROVEN.** Each
  was closed written-but-never-run because an open ticket shuts the build gate, so a ticket cannot
  watch its own change compile. Specifically:
  - **#209's stamina cost has never been seen to refuse a dodge.** It compiled 2026-08-20 13:40 and
    Michael has played dodging since, but "dodge works" is not "the cost bites" — the refusal path and
    its log line are still unwitnessed.
  - **#210 is written and compiled but nobody has watched an ordered goblin stop oscillating.**
  - **#205's ACF swap** is proven only to the extent that builds now run; no ACF *behaviour* has been
    checked against 4.4.2, and the migration's Phase 2 is still ahead of us.

- **2026-08-19 (#198): THE GDD MOVED, AND IT IS NOW THE CANONICAL ONE. `docs/goblin-siege-gdd.md`.**
  What used to be a derived export under `Tools/CodeArchitect/docs/` is now **v1.0, LOCKED**, and the
  repo-root `goblin-siege-design-document.md` is **frozen** as the submitted Assignment #02 artifact
  — do not edit the root doc to record a design change. The move needed no code change:
  `ca/config.py:48-54` already probed `project_root/docs/` first.
  - **The decisions ledger now EXISTS: `docs/decisions-ledger.md`.** Both documents had pointed at a
    "§13 ledger" for weeks that was in neither of them, which is why settled questions kept being
    re-opened. Today's 23 rulings plus the horde, climbing, ACF, ranged and interact rulings are
    there with their sources. **Add to it by ticket, not by memory.**
  - **`Tools/check_gdd.py` is the drift gate.** Nine checks: the parser contract, `features.json`
    coverage, the GDD roster vs the generator's `REQUIRED_KINDS`, and banked CSV vocabulary. Rules
    are **imported** from `gsstyle.py` and `gslevelgen.generate`, never restated. Exit 0 clean, 1
    drift, **2 incomplete** — a check that could not run does not exit 0. **It is WIRED into
    `Build-GoblinSiege.ps1`** (#200), immediately after the queue gate, and refuses the build with
    **exit 5** on drift — watched refusing one. `-IgnoreGddDrift` is the escape hatch, and it is the
    same kind of knowingly-loud bypass as `-IgnoreQueue`. The four banked rows that named the granary
    were regenerated first, so the gate went in clean rather than blocking the build window.
  - **A rule I derived by READING the regex was false, and the break test caught it.** A fourth
    column in §12.1 does **not** drop the row — it is absorbed into the status string, which is worse,
    because a drop is loud in the id list. Rows are actually dropped by a non-numeric id (`5b` is the
    one legal suffix), an id prefix like `#14`, a missing trailing pipe, two columns, or **any leading
    whitespace before the first pipe**. The contract in the GDD is now written from mutation tests.
  - **`gdd.py` uses `re.search` for the never-cut line, so the FIRST match in the file wins** — my own
    prose mentioning the heading hijacked the parse. And that line must stay **unwrapped**: it had
    been silently truncating five items to three.
- **2026-08-19 (#179/#189/#190 closed UNOBSERVED): THE STATUE SCORES, BUT IT IS NOT AN OBJECTIVE
  YET.** Michael watched a toppled statue award +100 deeds twice (#196). That does **not** mean the
  statue objective works, and the difference matters before Block C:
  - **`AGSObjective_ToppleStatue` is instantiated NOWHERE.** Its only references outside its own
    files are comments. The +100 came from `GSTopplableComponent.cpp:176` calling
    `UGSScoreSubsystem::AddDeeds` **directly**, bypassing the mission-objective class entirely. So
    nothing yet makes the statue a *required raid objective* that gates extraction — it is a prop
    that pays deeds.
  - **`Marker.ObjectiveAnchor.Statue` is defined and read by nothing.** No map places an
    ObjectiveAnchor marker of any kind, so `AGSRaidMarker::GatherByType` has never been asked for
    one.
  - Both are in the 2026-08-18 20:49 binary. Neither has run. **Do not read #196's green tick as
    "the statue objective is done"** — that inference is exactly what these three tickets were closed
    UNOBSERVED to prevent.

- **2026-08-19 (#198): TWO FINDINGS THAT MUST NOT BE REDISCOVERED.**
  - **THE TUTORIAL MAP AND THE ARENA HAVE DIVERGED.** `L_Tutorial_Island` holds the **2026-08-05**
    world — 67 `GSBuildingObjective`, 2 field objectives, market, mill, runic site, 3 defenders,
    `GEN_NavBounds_Village`. It contains **ZERO** raid markers, horde arrival points, interactables,
    loot, breakables, grapple anchors or the topplable statue. **Every system shipped since
    ~2026-08-07 exists only in `L_CombatArena`**, whose actors are labelled `BP_*_TEST`. Its objective
    roster is also a revision behind (Mill/Market/**Field**). **Any claim that something "works in the
    tutorial" is false by default until re-checked.**
  - **THE ×1.5 EXTRACTION MULTIPLIER DOES NOT EXIST ANYWHERE IN SOURCE.** Not stubbed, not unwired —
    absent. The rule that makes deeds provisional, and the whole reason the run home is a decision,
    has never been implemented. `UGSScoreSubsystem::AddLoot` still has zero callers beside it.

- **2026-08-11 (#136): A TICKET CANNOT CLOSE UNLESS SOMEBODY WATCHED THE WORK RUN.** `gsqueue.ps1`
  gained `observed:` and `scenario:` fields, an `observed -Id <n> -What "..." -Scenario "..."` verb,
  and a refusal in `done`. It is the first gate in that script that asks whether the work RAN — every
  other one inspects the ticket's text and timestamps.
  - **`-What` is phrase-scanned** and rejects `compil`, `read back`, `should work`, `no errors`,
    `looks correct` and friends: those describe the artifact, not its behaviour. The scan runs on
    that one line only, **never on the Evaluate prose** — a good Evaluate quotes those phrases in
    order to disown them.
  - **`-Scenario` is the one that matters most.** "In the editor" is not a scenario. Wrong-scenario
    evidence is this project's most repeated failure: #132/#133 tested with `GS.Combat.Duel` (which
    spawns *defenders*) and never ran on the horde; #133's blendspace was signed off from the player
    pawn, which exercises one of its five direction columns.
  - **`done -Id <n> -Unobserved "<reason>"` is the honest escape** — a broken editor must never
    deadlock the queue — and marks the board `**UNOBSERVED**` permanently.
  - Why mechanical and not another rule: "verify with evidence" already existed in **5 normative
    places and ~12 case-law restatements**, and #120 — the ticket that exists to record *"three fixes
    shipped without anyone watching them run"* — was **itself closed unwatched**, passing every gate.
    Prose is not a gate.
- **2026-08-11 (#137): `GS.Anim.Snapshot [radius]` IS THE ANIMATION INSTRUMENT.** Before it, this
  module had 18 cvars and 26 commands and **not one reported anything about animation** (no
  `UAnimInstance` subclass; no line had ever printed a speed).
  - One row per live pawn: `pawn | controller | speed | direction | rotation mode | REFPOSE |
    playing | blackboard target`. `Direction` uses `UKismetAnimationLibrary::CalculateDirection`,
    the same call the AnimBPs use, so it cannot drift from what the graph sees.
  - **The `REFPOSE` column answers "did this pose evaluate to nothing".** T-pose, "no locomotion at
    all" and A-pose were all one condition and nothing could state it. **Validated against a
    known-BAD case**: 0/18 in reference pose on the working blendspace, **9/15 on the dead one** —
    exactly the pawns running the goblin AnimBP.
  - **It prints EVERY pawn on purpose**, so a player-only test cannot hide an AI-only failure.
    Measured in one table: player `Direction 0.0 / OrientToMove`, horde goblins spanning −138° to
    +122°.
  - Gotcha: the header is `"KismetAnimationLibrary.h"` — **directly in `AnimGraphRuntime/Public/`,
    NOT under `Kismet/`** — and needs the `AnimGraphRuntime` private module dependency.
- **2026-08-11 (#133): COMBAT FACING HAS EXACTLY ONE AUTHORITY — `AGSAIControllerBase::TickFacing`**
  (`SetFocus` at `EAIFocusPriority::Gameplay` + `bUseControllerDesiredRotation`), behind
  `GS.Combat.FaceTarget`. **WATCHED AND SIGNED OFF by Michael, 2026-08-11: *"the combat animation
  looks good now."*** This is settled work, not a pending fix — do not re-open it on suspicion.
  It only became true once **#135** re-enabled the horde controller's tick, without which
  `TickFacing` had never executed on a single horn-summoned goblin. `UBTTask_MenaceOrbit`, `UBTTask_MeleeAttack` and `UBTTask_Block` keep their
  `SetActorRotation` code **only as the switch-off path** — deleting it would turn
  `GS.Combat.FaceTarget 0` into #089's deadlock rather than a comparison.
  - **CORRECTION to a comment repeated across the codebase:** `BTTask_Block.h:121` claims these pawns
    run `bOrientRotationToMovement` with `bUseControllerRotationYaw` false. **Both are inverted** —
    `BP_CastleGuard01` and `BP_ErikaArcher` ship `bOrientRotationToMovement=FALSE` /
    `bUseControllerRotationYaw=TRUE`. That is why the three nodes' rate-limited turns were being
    overwritten every frame by `APawn::FaceRotation`, and it is probably what #109 and #124 were
    really chasing. Do not trust that comment where it is repeated.
  - Priority is `Gameplay`, **not** `Move`: path following parks its own focus at `Move`
    (`AAIController::SetMoveFocus`), so a combat focus there is overwritten by every MoveTo.
    `UBTTask_RangedAttack` focuses the same blackboard actor at the same priority, so they agree.
- **2026-08-11 (#133): DIRECTIONAL LOCOMOTION IS NOT SHIPPED. BOTH RIGS ARE ON THEIR ORIGINAL
  LOCOMOTION.** `ThirdPerson_AnimBP_Gob` is back on the 1D `ThirdPerson_IdleRun_2D_Gob` (Speed->X)
  and `ABP_Human` is back on its Idle/Walk/Run state machine. **Do not read the two new blendspaces
  as working assets.**
  - `BS_GS_Locomotion_Gob` / `BS_GS_Locomotion_Hu`, `A_HU_Std_RunL`/`RunR`, `HU_Direction` and both
    `CalculateDirection` graphs all EXIST and are correct by every check the tooling can perform.
    They are simply not wired in, and the blendspaces are **not known-good** — see the FAILED entry.
  - `ThirdPerson_IdleRun_2D_Gob` is a **`BlendSpace1D`** despite the `_2D_` in its name.
  - The goblin's `Direction` variable existed for months and **nothing ever set it**; it is now set,
    harmlessly, whether or not anything reads it.
  - **The trap for the next agent:** the player goblin runs `bOrientRotationToMovement=true`, so its
    `Direction` is pinned at ~0 and it only ever samples the FORWARD column. Every other AI pawn
    (`BP_HordeGoblin`, the defenders) runs `bUseControllerRotationYaw=true` instead and uses the full
    ±180 range. **Testing directional locomotion as the player proves nothing** — it exercises one
    column out of five. That is how a broken blendspace got signed off as "looks good".
- **2026-08-11 (#133, open): `BP_CastleGuard01` has MaxWalkSpeed 1210 against a ~420 uu/s run
  animation.** A 2.9x mismatch that no blendspace can absorb — a charging guard must foot-slide or
  play at ~3x rate. Pre-existing and equally true of the single `A_HU_Std_RunF` before #133. It is a
  speed-vs-animation balance decision for Michael, not a bug.
- **2026-08-09 (#101): THE HUMANS HAVE NO COMBAT ANIMATIONS, AND UE WILL HAPPILY PLAY A GOBLIN
  MONTAGE ON THEM ANYWAY.** All 36 montages in the project live on `GOB_Scout_v2_Skeleton` under
  `/Game/Characters/ScoutV2/Montages/`; the six human defenders run `SK_Human_Skeleton` with
  `ABP_Human`, which references three animations total (Idle, WalkF, RunF).
  `PlayAnimMontage(AM_GS_Atk_Light)` on a guard **returns 1.150 and drives the slot** - the tracks map
  to nothing, so the slot evaluates to the REFERENCE POSE. Every attack, block and guard break
  T-posed a guard for the montage's length, and took his sword arm out sideways with it, which is why
  the sword read as "not in his hand" (measured: hand 104uu from body centre in idle, 149uu during
  the montage - and 149uu was exactly what a guard measured mid-combat).
  **`AGSCharacterBase::PlayAnimMontage` now refuses a montage whose skeleton differs from the
  character's, returning 0.** One gate for every caller, and only when BOTH skeletons are known.
  **This is a stopgap: it trades a T-posing guard for an unanimated one.** The real fix is human
  combat animations - retargeted from the goblin set or authored - and that is a content decision.

- **2026-08-09 (#101) CORRECTION to #097: all four human defenders SHARE `SK_Human_Skeleton`.** #097
  recorded that each guard has its own skeleton and chose bone attachment on that basis. It is wrong.
  A weapon socket added once to `SK_Human_Skeleton` would serve every human.

- **2026-08-09 (#100): A WEAPON MESH'S PIVOT IS NOT ALWAYS AT ITS GRIP.** `GS_Sword`'s pivot is at
  the hilt; `GS_Sword_Guard`'s is at the **tip** (settled from vertex data: crossguard XY radius peaks
  at 10.7-11.8 around z 60-80, tapering to 3.0 at z=0). Rotation cannot fix a tip-pivot - the pivot is
  what sits on the socket - so `DA_Weapon_Guard` carries a translation of
  `-(MeshLength * Scale)` along `Rotation.RotateVector(0,0,1)`, derived from the scale so the two
  cannot drift. **"0.0uu from the socket" proves nothing about orientation**; measure both mesh
  endpoints.

- **2026-08-09 (#097/#098): EVERY COMBATANT NOW HOLDS ITS WEAPON, and `UGSWeaponComponent` stays off
  `AGSCharacterBase`.** The player has had one for months; the defenders got one in #097 and the horde
  in #098, both as their own member rather than hoisted to the shared base - `AGSPlayerCharacter`
  creates its own in its constructor and hoisting would give it two. **The horde carries the player's
  own `GS_Sword` on `hand_r_weapon` with the player's tuned offset**, which is exact rather than
  reasoned: same mesh, same socket, same skeleton. The guards cannot do that - **no human mesh has any
  weapon socket at all** (checked all eight on all six; each guard has its OWN skeleton, not a shared
  one), so `DA_Weapon_Guard` attaches `GS_Sword_Guard` to the **`RightHand` bone** and its rotation is
  still unverified by eye. **Build weapon data assets FRESH, never duplicated:** the soft-object mesh
  fields cannot be cleared from Python (`None` no-ops, `SoftObjectPath('')` throws), and a duplicate of
  `DA_Weapon_Scout` hangs a bow and quiver on anything whose rig has those sockets - which the goblins'
  does.

- **2026-08-09 (#093, Michael): MITIGATION MAY NEVER ZERO A HIT.** `GS.Combat.MinimumDamage` (1.0),
  applied once at the end of `UGSDamageExecCalculation` so it catches every path - block, plate,
  armour, race matchup, NPC-vs-NPC scalar - which compound and produced literal 0.00 hits. Gated on
  `RawDamage > 0` so it lifts real blows only. **This resolved the #091 open question**: a knight is
  no longer invulnerable to the warband frontally (measured chipping 75 -> 67), while staying
  enormously resistant. **Two consequences beyond knights: blocks now CHIP (a perfect block was 0.0,
  now 1.0 - a real change to player feel), and fire shares this exec so every fire tick has a floor,
  which stops armour quietly making a knight fireproof.**
- **2026-08-09 (#091, GDD §217): ARMOUR IS DIRECTIONAL, and a bow ignores it.** Plate is no longer a
  flat number that helps equally from every angle. Straight-on (within `GS.Combat.PlateArc` 150 deg)
  the damage is scaled by `GS.Combat.PlateFrontalScalar` (0.3) and THEN flat armour is subtracted, so
  plate compounds; from the flank, back, or a takedown the flat armour is skipped **entirely**; and
  `Damage.Bow` bypasses it at any angle. Only ever applies to a character with Armor > 0, so it is a
  knight rule and touches nothing else. Measured: a 25/30/45 frontal combo takes a knight 75 -> 63;
  the same combo from his flank takes him 75 -> 20.
  - **It composes with #090's ring slots for free.** Goblins were measured at 174.9 and 92.9 degrees
    off a knight's front - in the gaps - purely because slots are claimed around the target. Nobody
    wrote flanking; the crowd system produced it.
  - **Open question for Michael: a goblin's FRONTAL hit is exactly 0.00** (25 x 0.55 NPC x 0.3 plate
    - 6 armour). From the gaps it is 13.75. So a knight facing one goblin is invulnerable to it.
    GDD §76 does name the Shaman's armour-ignoring magic as the answer to knight armour, so this may
    be correct - but raise `GS.Combat.PlateFrontalScalar` to ~0.6 for chip damage instead of nothing.
- **2026-08-09 (#092): an arrow to the head does 2.5x.** Resolved in `AGSArrowProjectile`, the only
  place that still holds the `FHitResult`. Two traps closed that would each have shipped a dead
  feature: the arrow stops on the CAPSULE so `Hit.BoneName` is always `None` (falls back to
  `FindClosestBone` on the impact point rather than changing mesh collision), and the two skeletons
  spell bones differently (case-insensitive SUBSTRING match on `head`/`neck`, not an exact name).
  **Never observed firing** - it needs a human on the mouse.
- **2026-08-09 (Michael): KNIGHTS ARE ELITES - at most 3-4 on the entire map at this difficulty.**
  A knight is a set-piece, not rank-and-file; the garrison a raid meets is militia. Measured on
  `L_CombatArena`: 10 goblins vs 6 knights is 9-0 to the knights (a boss fight, not a raid), while
  10 goblins vs 6 militia + 2 knights trades 9 goblins for 5 defenders. Balance the horde against
  MILITIA and treat every knight in an encounter as a large difficulty increment.
- **2026-08-09: `FGSArchetypeDefinition::Damage` (8.0) IS READ NOWHERE.** All melee damage comes from
  `FGSSwingStage::Damage` (25) on the ability, so a militiaman and a knight hit for exactly the same
  number - confirmed in PIE, both resolving 13.8 against a goblin. Today "elite" means ONLY more HP
  (75 vs 30) and armor (6 vs 0); they share a behaviour tree, so they also share block chance and
  both carry the guard break. Anyone tiering fodder-vs-elite needs to know the damage lever is not
  connected, and that armor is currently the whole of a knight's offence-side identity.
- **2026-08-09 (#090): permission and position to attack are rationed BY THE VICTIM.**
  `UGSEngagementComponent` on `AGSCharacterBase` owns a weighted token budget (two jabs or one heavy,
  never both), an engagement capacity (how many may be assigned at all), and the exclusive ring
  slots. An attacker reserves from the thing it is attacking - N attackers each rationing themselves
  still produces N simultaneous swings, so only the victim can hold the number. **The player has the
  same component**, so the crowd takes turns on him too; a rationing system he was exempt from would
  be visible immediately. `CanBeAttacked()` gates all of it on the victim not being staggered,
  guard-broken, recoiling or dead - one check that applies to every attacker at once, and the reason
  a stumbling defender is not deleted by four simultaneous sweeps.
- **2026-08-09 (#087, Michael): a block does not merely reduce damage - it TURNS THE SWING ASIDE.**
  The attacker's attack is cancelled and they are open (no attacking, no re-guarding) for
  `GS.Combat.RecoilSeconds`. This is the answer to "nobody is ever punished", it replaces the guard
  break as the AI's anti-turtle tool, and it is symmetric: it applies to the player's blocked swings
  exactly as it does to an AI's. The opening IS the punish - deliberately no damage multiplier on
  top, since the target cannot block the counter anyway.
- **2026-08-08 (#083): `State.Attacking.Windup` is the ONLY sanctioned channel by which an AI may
  learn a hit is coming.** No BT node, service or decorator may read an opponent's ability internals,
  montage position, blackboard or timers instead. The tag is raised on the frame the player sees the
  arm go back and cleared when the damage window opens, so a defender reacting to it is reacting to
  something the player also saw — that is the whole line between a fair fight and a psychic one, and
  it is exactly the shortcut that gets added at 1am to make an encounter "read better".
- Canonical class name is SCOUT (amends decision 36); sword ⇄ bow.
- Win = burn one of each TYPE (mill/field/market) then extract; siblings demote to Optional (Q-32).
- Horde: ground-bound but CAN VAULT (41-a); never climbs/mantles; pool debits on spawn only (40); light fire avoidance + giggle barks.
- Burn visual mandate (2026-08-01): anything burnable chars black + smoulders; mill sails spin while burning, stop at Detonated; breaking meshes later.
- Interact verbs for the slice: loot / takedown / foul-well / extract + carry state; hold-E channels, abort on damage/release/range-facing break; takedown 1.2s channel, 120° behind-cone (placeholders signed).
- Single-player slice, co-op-ready: replicate cheap root state only (Q-36 pattern).
- Human gate: agent output is STAGED for Michael's review before entering Source/ — post-run review, Bark Foundry pattern.
- 2026-08-04 (review of live-003 staging, four rulings):
  - **E with full hands: the focused interactable wins.** `BeginChannel` refreshes focus first; put-down
    only when nothing is focused. Carry-to-extract has to work, so "hands full = drop" is wrong.
  - **Carry stays one slot, attacks blocked, for the slice.** GDD §9's "chickens weightless (carry two,
    fight one-handed)" needs weight classes — deferred until livestock (loot couriers, block G) lands.
  - **MoveSpeedMultiplier gets wired into CharacterMovement once**, and Carry *and* `GSGA_Block` both
    convert to it. Retires the cache-and-restore pattern that `OnStartCrouch` silently defeats.
  - **The co-op server path goes in now** (refines "replicate cheap root state only"): server RPC for
    begin/abort, payout + `SetAvailable` behind `HasAuthority`. Cheaper before five systems hook the
    completion delegate than after.
- 2026-08-06 (Michael, on the ranged pass — **settled, do not re-litigate**):
  - **An arrow STICKS in an ally; it does not pass through.** #038 stopped arrows damaging allied
    goblins (`IsHostileTo`, the same rule melee has used since 2026-08-04), and the question was
    whether they should also stop being blocked by them. They should not: an arrow is stopped by an
    allied body, deals nothing, and the shot is wasted. Positioning is the player's problem. This was
    asked with the horde case on the table (firing past your own line) and answered anyway — so a
    future agent finding "every shot eaten by a friendly" is looking at intended behaviour, not a bug.
  - **The radial weapon wheel is built C++-first**: `EGSWeaponSlot` and the selection maths in code
    with `BlueprintReadOnly` state plus open/close/changed events; the UMG widget comes after, built
    against a working backend. This supersedes fixing the torch throw directly — that work is
    subsumed (see `HANDOFF.md` Part 3).
- 2026-08-04 (three further rulings):
  - **No interacting or blocking while staggered.** Block already refused `State.GuardBroken`;
    interact now does too, and `BreakGuard` cancels an in-flight channel by tag so the kick stops a
    loot already in progress rather than only refusing the next one.
  - **Full hands cannot throw a torch** — `UGSGA_TorchToss` blocks on `State.Carrying`. Drop the sack
    first.
  - **Extraction is an auto-bank circle for now** (GDD §9), NOT a hold-E verb — resolves the §9 vs
    §12.1 contradiction. `Interact.Extract` stays declared but unused, reserved for if it ever
    becomes channelled.
- 2026-08-07 (Michael, the horde rulings — **settled, do not re-litigate**). Sixteen decisions taken
  while #069 was built. The first four are the ones a later agent is most likely to "fix" back:
  - **The war-horn is on MIDDLE MOUSE (wheel click), not G.** Both GDDs say G; G is `IA_Block` from
    the #058 remap, which was Michael's own ask. Verified against the live `IMC_Default`: 17 rows,
    17 distinct keys, no duplicates. **F is also taken** (`IA_Interact`), so "move it to F" is the
    same collision. GDD §2.3's "G" is an erratum — as is its "torch toss (Q)", since `IA_ThrowTorch`
    has had no IMC row since the torch became a held weapon on 2026-08-06.
  - **The horde arrives from a PORTAL / Warren mouth, not the treeline.** GDD §2.5's "sprint in from
    the treeline (never popping into existence — watching them arrive is the joke)" is superseded.
    Emerging from a hole satisfies the same no-pop-in mandate and sidesteps `GEN_NavBounds_Village`
    being 4000x4000 uu. A placeholder portal stands in until the Warren is built — and it is built
    as the Warren-to-be (one actor that grows the dig/banking/respawn behaviour later), not a
    throwaway. **Do not "restore" the treeline.**
  - **Combat verbs live on `AGSCharacterBase`.** Reparenting `AGSHordeGoblin` to `AGSEnemyCharacter`
    is the tempting one-liner and silently flips five class-identity checks: `GSFireVolume.cpp:394`
    would stop burning the horde, `GSTargetingComponent.cpp:50` would soft-lock the player onto his
    own goblins, `GSBuffAuraComponent.cpp:48` would let defender auras buff them. None fail loudly.
  - **`AGSHordeSpawnMarker` will NEVER be built.** The `Marker.HordeArrival` tag and
    `AGSRaidMarker::GatherByType` already do the whole job, and `GSRaidMarker.h:20-26` argues
    against new marker UCLASSes. It was listed as missing work on the NEXT line for two days.
  - **AI vault is DEFERRED** until the climb rebuild (#067/#070) settles. Decision 41-a still stands
    — horde goblins vault, never climb or mantle — but nothing in the project can vault from code
    (it is Blueprint-only, entered through Enhanced Input an AI cannot press) and there are zero
    `NavLink`/`NavArea` hits repo-wide. `Stranded` degrades to "unreachable → idle → re-horn free →
    trudge home", which repo GDD §5 already describes.
  - **Active cap is PER PLAYER (10 each); the raid pool of 20 is SHARED.** `ActiveGoblins` keyed by
    summoning controller; `ReserveRemaining` is one counter. In co-op two players at full cap empty
    the reserve — intended, and what keeps the 20-pool comparable to the defenders' 15 (§2.6).
  - **Fire kills your own horde and that is intended.** ~10s at 40 HP. Do not "fix" it in
    `AGSFireVolume` — that would make the player fire-immune too. Any fix belongs in horde steering.
    Note the 40 HP lives in `DA_Race_Goblin`: whoever authors that row sets fire lethality with it.
  - **Corpses are never destroyed** (`CorpseLifespan` stays 0, horde included) — "we want to see
    where things died". Accept the cost knowingly: ten dead goblins is ten permanently simulating
    ragdolls, and the week-3 exit test is *ten goblins at frame rate*. If that test fails on render
    thread, this is the first dial to revisit — but it is a deliberate choice, not an oversight.
  - **The horn raises the alarm straight to `Raid`**, not Suspicious. §2.5 calls it "the formal end
    of the quiet half"; §2.6's Suspicious-tier horn is a *patrol's* horn, a different event sharing
    a noun. New `EGSAlarmSource::HornBlast`, **appended** to the enum, never inserted.
  - **`SummonsPerBlast` is a fixed 4**, not a random 3-4 — a player counting his pool should not
    have to guess.
  - **A dead horde goblin is shown as a stat, not scored** (no deed, no loot, no penalty).
  - **`ThreatMemorySeconds` = 8** — how long Frenzy stays committed to a guard who ran away.
  - **Arrival markers go in `L_CombatArena` first**, then `L_Tutorial_Island` once it works.
    `L_Tutorial_Island` is 185 MB and every save is a 185 MB LFS object.
  - **The navmesh gets WIDENED, not switched to invokers** — measure the bake first. Invoker-based
    generation gives no navmesh where no invoker is standing, which would break the planned
    `UGSPatrolDirector` (patrol routes across the village, soldiers marching from a distant castle).
    A fixed-size hand-authored map is the case a static bake is good at. Folded into the Tier 2
    editor pass.
  - **Nothing summons until `BP_HordeGoblin` exists and `HordeGoblinClassPath` points at it** in
    `DefaultGame.ini` under `[/Script/GoblinSiege.GSHordeSubsystem]`. Until then `SummonWave`
    correctly refuses and logs which knob is empty — that message is the design, not a failure.
- 2026-08-08 (Michael, the climbing rulings — **settled, do not re-litigate**). The climb went from
  "stalls at the same lip every time, four sessions running" to working; these are the calls that got
  it there, and the first three are the ones a later agent is most likely to undo:
  - **Ledge detection is a SEARCH, not a tuned constant.** `UGSClimbLibrary::FindClimbLedge` sweeps
    insets **80..340 step 20** and takes the first surface that is walkable AND has open sky above it.
    Measured on 56 roof lips across 14 houses: a fixed inset tops out at **88%** (best single value is
    80; the 120 that shipped briefly scored 82%), the search gets **96%**. The art-pack roofs are
    ribbed at ~40uu, so the answer alternates between deck and rib as the inset moves — **no constant
    can work, and "just tune it" is the trap this cost a day to escape.**
  - **The sky check is load-bearing.** A candidate deck with a roof above it is an INTERIOR FLOOR, and
    since #077 made `Medium_11` `UseComplexAsSimple` there is no wall left to stop a downward probe
    finding one. 15 of 22 sampled heights find interior floors at `nz 1.00`, which passes any
    walkable gate. Deleting the sky test puts the player inside the house.
  - **Goblins have claws: `WalkableFloorAngle` is 65 degrees** (`WalkableFloorZ` 0.4226), and that
    angle is *simultaneously* the steepest walkable surface and the boundary above which a surface
    must be climbed. Michael's rule, from `Medium_02`'s 63.3-degree roof: *"that should be the limit
    on what you can actually climb."* Do NOT add a separate looser gate inside the ledge search — it
    would mantle the goblin onto a roof the movement component then slides him off.
  - **Climb animation play rate is DERIVED per frame**, not set: `|ΔactorZ| / DeltaTime / 46.3`,
    clamped 0.60..9.00, where 46.3 uu/s is the clip's own root-track speed. A fixed rate was wrong
    three times (7.0, then 6.33, against sessions that climbed at 324/293/201 uu/s).
    **`GetVelocity()` is the wrong input** — during the climb it reports what `ClimbTick` commanded,
    not what moved: the log shows `vel=Z=420` held steady while `dZ` was 0.00 against the eave.
  - **Braced hops stay** (#075, reaffirmed). The target feel is the Moria scene — *"smooth and a
    little hectic"*. Smooth means no sliding; it does NOT mean a continuous climb cycle.
  - **Hold-E climbs.** Michael likes it; do not repurpose E as a release verb.
  - **Stamina FREEZES on the wall** — no drain, no regen (#072/#076). Constant stamina while climbing
    is correct behaviour, not a stuck tick. I misread it as evidence of a halted `ClimbTick`.
  - **The guards are gone and should stay gone** (#081): `ClimbBlockedSeconds`, `ClimbLastZ`, the
    lean-out, and the old single-inset probe. The accumulator's cost was never cycles — it read
    **0.000 for 1285 ticks while the character was visibly stuck**, and sent three sessions down wrong
    diagnoses. A lying instrument is worse than dead code.
  - **`GS.Climb.LogLedge 1` is the instrument** — it prints what the search decided and why
    (`found`/`inset`/`rise`/`nz`/`steep`/`interior`/`empty`). Every climbing claim gets checked
    against Michael's play log, never against my own trace simulation. Measuring the WRONG FACE of the
    right house (north instead of south, 1210uu apart) invalidated a day of "verified" numbers.
  - **ROOF CONTINUATION IS CLOSED, NOT DEFERRED** (Michael, 2026-08-08: *"the roofs are fine,
    consider it closed"*). `Small_8` (82.5 deg) and `Small_10` (79.6 deg) stay unclimbable and that is
    the intended answer — **98% of roof lips resolved across 20 houses is done, not 98% of the way to
    done.** Do not build a "climb onto a pitch too steep to stand on" feature; the earlier plan listed
    it as owed work and it is not. If a specific roof ever needs to be climbable, the lever is the
    walkable angle or that building's collision, not a new traversal mode.
  - **Still open:** per-goblin cadence jitter for horde climbing, **parked at Michael's request**
    until the combat/summoning windows finish; Stage 4 plane transition — 42% of climb columns turn
    >30 deg in one 40uu step, but there is NO evidence it breaks anything, so it waits on observation
    rather than a fix. **Neither is a known defect.** Climbing is finished work.

- **THE INTERACTION FRAMEWORK — plan of action, and the ACF comparison (2026-08-14).**

  **How it went missing.** The NEXT refresh of 2026-08-06 *removed* the interact framework from the
  ranked list — at u=10.0, the highest-utility item on it — with the note *"all four components exist
  in `Interaction/` and `Weapons/Abilities/`"*. The symbols existed, so the item was deleted as done.
  **It has never executed once.** `BP_GSPlayerCharacter`'s `InteractAction` is unset on the CDO, so
  `GSPlayerCharacter.cpp:454` skips the bind and `Input_InteractStart` — the only caller of the
  interact ability — has never fired. #061 recorded this in July and it is still true. Nothing in the
  project carries a `UGSInteractableComponent` either, so `ResolveChannelTarget` always returns null.
  **1409 lines of written, compiling, replicated interaction code have never run.** This is the exact
  failure the BUILT/WIRED/SKELETON grading was introduced to prevent, and it happened to the single
  most load-bearing system in the slice: five other systems sit behind it.

  **What ACF ships, checked before building anything (Michael's standing rule).** ACF has the whole
  stack, not just the interface: `IACFInteractableInterface` (contract), `UACFInteractableComponent`
  (Free/Busy state, per-item montage override, display name, examination texts),
  `UACFInteractionComponent` (sphere-overlap detection with a camera-forward offset, best-target
  selection, replicated `CurrentInteractingActor`, `ServerInteractOnBehalf` for possession swaps),
  `UACFInteractActionAbility` (**motion-warps the pawn to the interactable**, camera lock, montage
  driven), `AACFBaseInteractableActor`, and `ACFInteractSmartObjectsTask` — **a behaviour-tree task so
  AI can interact**, which is the missing consumer for the courier run's Loot order.

  **Three ACF capabilities we do not have and want:** motion warping (the goblin *steps to* the sack
  instead of looting it at arm's length), camera lock during the interaction, and the AI BT task.

  **Two things ours has that ACF's does not, and both are load-bearing design:** a real
  **hold-to-channel with abortable progress** (`BeginChannel` / `AbortChannel` /
  `ReleaseInteractInput` + `ChannelProgress`) — ACF's "duration" is just a montage length, with no
  release-to-cancel — and **abort-on-damage** (`HandleOwnerHealthChanged`). Hold-F takedowns, 1.5s
  loot channels and "get hit and you lose the channel" are all specified in the GDD and none of them
  survive a straight swap to ACF. Ours also gates focus on **facing cone + distance + line of sight**
  where ACF uses sphere overlap.

  **Why adopting ACF is not a drop-in.** `UACFInteractActionAbility` derives from `UACFActionAbility`
  and triggers by tag on the ACF ability system; the interactable expects an ACF-shaped interactor.
  Our characters are `AGSCharacterBase` on plain `UGameplayAbility`. #142/#143 completed ACF Phase 0
  and 1a only. Adopting the interaction stack means pulling the character-and-ability migration
  forward into the critical path of a seven-week slice.

  **THE PLAN, in order:**

  1. **Assign `IA_Interact` to `InteractAction`** and add the missing `else` log beside the guard at
     `GSPlayerCharacter.cpp:454`, copying the `HordeOrderAction` pattern at `:488-494`. Half an hour.
     This is the whole reason the comparison below is currently unanswerable: we are choosing between
     a vendor system we have not tested and our own system that has never run.
  2. **Build one interactable and watch a channel.** The livestock MVP's pig is the natural first.
     Requires the same three fixes the livestock plan names: `CarrySocket` on
     `GOB_Scout_v2_Skeleton` (it exists on **none** of the project's 25 skeletons, so cargo silently
     attaches at the carrier's feet) with a `DoesSocketExist` guard that warns; and the
     `CompleteChannel` ordering fix, which consumes the interactable *before* `StartCarry`, so a
     put-down object can never be picked up again. Exit test: hold F, a bar fills, a pig ends up on a
     shoulder, put it down, pick it up again.
  3. **Then decide, with evidence.** If the channel works, keep it and take ACF's three ideas
     piecemeal — motion warp on channel start, camera lock, and the AI interact task as the Loot-order
     branch. If it is broken in ways that are not cheap, price the ACF migration honestly *then*,
     knowing what would be given up.

  **Deliberately not doing:** adopting ACF's interaction stack sight-unseen, to replace a system that
  may already work, at the cost of the two mechanics the design actually specifies.

  **Open questions for Michael:** whether piecemeal borrowing is acceptable against a cleaner all-ACF
  future; and whether the AI-interact half is in scope this slice at all — a player-only courier run
  satisfies GDD 2.7's first-courier-run beat, and the AI courier is 4-6 days with the order-wheel work
  bundled in.

- **2026-08-30 (#385): `GameInstanceClass` WAS NEVER SET ANYWHERE IN CONFIG — `UGSGameInstance` HAD
  BEEN DEAD CODE AT RUNTIME SINCE IT WAS WRITTEN.** Found while wiring the main menu's gold/XP
  display. `Config/DefaultEngine.ini` had no `GameInstanceClass=` line under
  `[/Script/EngineSettings.GameMapsSettings]`, and no Blueprint child or other config set it either —
  so the engine instantiated the base `UGameInstance` at runtime, and every
  `World->GetGameInstance<UGSGameInstance>()` call (including the score/gold/XP banking added earlier
  this same session in `GSRaidDirector::EndRaid`) was silently returning null and no-opping. Nothing
  errored; the save file just never got written. **Fixed by adding
  `GameInstanceClass=/Script/GoblinSiege.GSGameInstance` to `DefaultEngine.ini`.** Confirmed live in
  PIE afterward: `GameplayStatics.get_game_instance(world).get_class()` now reads
  `/Script/GoblinSiege.GSGameInstance`, and two consecutive raids banked additively (0/0 → 25g/3180xp →
  90g/6360xp). **If a `UGameInstance` subclass's state ever appears to silently not persist or not
  exist, check `GameInstanceClass` in Config before assuming the C++ logic is wrong** — the class can
  compile clean, compile into a Blueprint child, and still never run if nothing points the project at
  it.

- **2026-08-31 (#388): PACKAGED-BUILD PLAYTEST FOUND GRAPPLE HOOK/CRATE LOOT/BUILDING FRACTURES ALL
  MISSING - ROOT CAUSE WAS SOFT-REFERENCED CONTENT NEVER GETTING COOKED, NOT GAMEPLAY BUGS.** After
  fixing the New Raid map-cooking bug (see the `-allmaps` entry above), a live playtest of the
  packaged build found: grapple hook doesn't work (can't leave the map), goblins fixate on crates
  that never break or drop loot, no building or windmill ever burns down/collapses, and fire volumes
  cause a severe frame-rate spike after starting. All four trace to ONE cause: `TSoftObjectPtr`/
  `LoadObject`-by-path content (fracture `GC_*` collections in `GSCrumbleComponent`/
  `GSBuildingObjective`, fire/smoke/ember Niagara systems in `GSFireVolume`, the grapple hook's
  projectile class in `GSGA_GrappleThrow`) is never auto-included by the cooker unless something
  hard-references it - and nothing did, since only `L_MainMenu`+`L_Tutorial_Island` were explicitly
  listed to cook. `LoadSynchronous()` on an asset that was never cooked returns null silently, every
  call - which also explains the frame-rate spike, since fire volumes call it every tick. **Fixed by
  setting `bCookAll=True` in `DefaultGame.ini`** (forces the whole `Content/` directory to cook
  regardless of reference type) to unblock testing immediately. **Before a real itch upload this
  needs trimming back down** - either list the specific soft-loaded folders (`/Game/Destruction`,
  the VFX folders) in `DirectoriesToAlwaysCook` and drop `bCookAll`, or register them as Primary
  Asset Types via the Asset Manager - `bCookAll` currently also ships the test/scratch maps
  `MapsToCook` deliberately excluded.

- **2026-08-31 (#388): `BP_GrappleHook` HAS BEEN SILENTLY LOSING ITS OWN MESH COMPONENTS ON EVERY
  LOAD, PROBABLY FOR A WHILE.** Found while chasing why grapple hook "doesn't work" in the first
  ever packaged build. Root cause is the SAME class of SCS corruption `BP_Statue_Warrior` had
  (`IntactMesh`/`Collection` nested under a non-root parent - see the #388 ticket for that fix) -
  `HookMesh`, `RopeISM`, `RopeMesh` were all nested under `Sphere`. Unlike the statue, this one
  couldn't be fixed by making the children independent: `Sphere` needs them ATTACHED so the rope
  visuals actually follow the flying hook, and the same "make it independent" recipe would have
  broken that relationship on purpose. **Worse: the engine's own auto-repair-on-load doesn't just
  reparent the malformed nodes, it DROPS them entirely** - a fresh load leaves the Blueprint with
  only `Sphere`/`Movement`, and the EventGraph's `Get RopeMesh`/`Get HookMesh` nodes then log
  `Could not find a variable named "RopeMesh"`/`"HookMesh"` and `The property associated with ...
  could not be found`. This means the rope/hook mesh components - and whatever the EventGraph does
  with them - have likely been silently gone at runtime on every load (editor, PIE, cook) for a
  while now, not something this session broke.
  - **Scanned ALL 126 Blueprints in the project for this exact SCS pattern** (force-reload each one,
    grep the log for the "Reparenting... cyclic linkage" warning) - **only `BP_Statue_Warrior`
    (already fixed) and `BP_GrappleHook` are affected.** Nothing else in the project has this defect.
  - **Deliberately did NOT reconstruct the EventGraph logic or re-author HookMesh/RopeISM/RopeMesh
    blind** - that's real gameplay/content work needing Michael's knowledge of what those nodes were
    supposed to do, not something to guess at overnight. Saved the Blueprint in its current
    auto-repaired (components-dropped) state ONLY to stop it hard-failing the cook (any Error:-level
    log line fails a package build regardless of whether the cook itself finishes - see the
    AIPerceptionComponent entry in ticket #387's history for the same UAT behavior).
  - **FOLLOW-UP NEEDED, NOT DONE**: re-author `BP_GrappleHook`'s `HookMesh`/`RopeISM`/`RopeMesh`
    components and check whatever the EventGraph does with them - this is very likely THE reason the
    grapple hook doesn't work in the packaged build, separate from (and on top of) the
    soft-reference-cooking issue also found the same night (see the `bCookAll` decision above/below).

## NEXT

### UI costs 4.60 ms a frame — folded into the ACF conversion, not its own ticket

**Michael's ruling, 2026-09-09: "let's lump UI work into the ACF conversion."** The HUD is expected
to move onto ACF's UI framework (Ascent UI Tools + the UI Navigation System, and ACF's own
`WBP_FullHUD` now that #406 has installed FullSample), so optimising the bespoke
`WBP_GSPlayerHUD` in place is throwaway work. **No standalone UI perf ticket. Read this before
authoring the ACF-side HUD** — the cheapest moment to avoid a 4.6 ms per-frame UI cost is while
building the replacement.

**The measurement (Michael's 2,374-frame capture, `Saved/Profiling/CSV/Profile(20260909_165845).csv`).**
After #405 cut shadows, `Exclusive/GameThread/UI` at **4.60 ms** is the largest identified
game-thread cost in the game — bigger than Animation (2.65), TickActors (1.81) and
CharacterMovement (1.67). The frame is 21.77 ms median and game-thread bound.

**The clue that matters: Slate is cheap.** `DrawPrePass` 1.75, `TickPlatform` 0.21,
`DrawWindows_Private` 0.15, `PaintFastPath` 0.05, `SObjectWidget_Tick` 0.03,
`AllWorkers/Slate` 0.14, `RenderThread/Slate` 0.09, `GPU/SlateUI` 0.10. **Slate is ~2.2 ms of the
4.60 — over half the cost is outside Slate's own timers entirely.** `DrawCall/SlateUI` is 159,
which is a lot of batches for a health bar, stamina bar, clock, lives row, objective list and
reticle.

**Prime suspect: UMG property Bindings.** A `Binding` on any widget property is a Blueprint
function evaluated every frame, on the game thread, per bound property, and its cost lands in the
UI bracket rather than in any Slate stat — which is exactly the gap above. Checked first, and the
C++ side is NOT the problem: `UGSPlayerHUDWidget::NativeTick` only runs bind-retries plus
`TickRefusalShake`, and the expensive rebuilds (`RebuildObjectiveList`, `RefreshLivesRow`, which
`ClearChildren()` and re-`CreateWidget`) are event-driven, so they do not run on the measured
frames. **Do not start by optimising the rebuild** — it looks heavy and is not what the measurement
is pointing at. `WBP_GSObjectiveRow` is the one to watch, since a binding on it is multiplied by
the number of rows on screen.

**Whoever builds the ACF HUD:** count the Bindings before you ship it, prefer event-driven pushes
(the C++ HUD is already shaped for this — `RefreshLivesRow` and `RebuildObjectiveList` are called
from delegates), and re-measure against the 4.60 ms baseline. Refuse to land a HUD worse than the
one it replaces. If `DrawPrePass` turns out to dominate instead, the levers are
`Slate.EnableGlobalInvalidation 1` as a blunt A/B and Invalidation Boxes as the targeted fix.

**Two measurement rules, both learned by getting them wrong on 2026-09-09 (#405):**
1. **The editor throttles PIE when its window is not focused** — `render=0.0` and FrameTime pinned
   at 333.33 ms are the tell, and every number taken that way is meaningless. Michael's captures
   are trustworthy because he plays focused; an agent driving the editor over HTTP must set
   `Slate.bAllowThrottling 0` **in the same script as the measurement** (the ini setting did not
   hold).
2. **Use `csvprofile start`/`stop` and read the CSV, not `stat dumpframe`.** Single frames misled
   twice in one session — once naming `FTicker_Tick` (it was the throttle's idle time) and once
   pointing at translucency (the GPU breakdown then showed shadows at 77%). Add
   `r.GPUCsvStatsEnabled 1` before starting for the per-pass `GPU/` columns; `-csvGpuStats` is a
   command-line switch, not a console command.


- **2026-09-01: Class deadline shipped (packaged build uploaded to itch); Sept 8 is the FINAL
  deadline for polish.** Two known issues from the live packaged playtest were deliberately NOT
  fixed before shipping (Michael's call - "that's what matters," ship now, fix after):
  - **Foliage (including apple trees) disappears entirely once a field fire starts spreading**,
    with a lag spike right before it. Never diagnosed - no editor/PIE access to a packaged .exe, and
    this session ran out of runway before switching back to the editor to reproduce it. The
    on-screen "[VSM] Nanite Marking Job Queue overflow" warning has been visible repeatedly all
    session (building fires too, not just field fire) and is the most likely shared root cause -
    start there with `PerformanceService.frame_timing()`/`stat dumpframe` in PIE, matching the
    method that found the smolder-FX and clustering costs in #393/#395.
  - **General lag "around the middle of the village"** during a full raid loop, likely village
    density (many buildings/pieces in view at once) rather than any single system - not isolated to
    one building or mechanic.
  Both are real regressions to chase before the 8th, not accepted as final quality.


### World corruption - ICEBOXED 2026-08-27 (#342), stages 5-6 remain

> **Parked by Michael. Do not pick this up without him saying so.** Stages 0-4 are BUILT and watched
> (see the entry at the top of this file) - this is a working feature stopped four-sixths through,
> not an abandoned one. **The full handover is `AgentQueue/ICEBOX.md`.** Nothing was reverted, all
> corruption tickets closed properly, and the board is clear of them, so this holds no build gate.
>
> **Two things that will look like faults and are not.** Every run logs `No corruption tuning asset
> at '/Game/Data/World/DA_Corruption_Default...'` - the asset is not authored yet and the warning is
> the designed fallback, not a break. And the four `UCurveFloat` slots on that asset are declared but
> unconsumed by the director.
>
> **`GS.Corruption.Debug 0|1` closed UNOBSERVED (#338)**: it registers and logs `live overlay ON`,
> but nobody has looked at the screen with it enabled, and the bar draws through on-screen debug
> messages that no log can confirm. If it draws nothing, suspect the line filter - it string-matches
> `DescribeState()`'s wording.
>
> The design plan stays machine-local at `C:/Users/Michael/.claude/plans/i-want-you-to-abstract-newell.md`
> by ruling; **this file and the icebox are the durable record.** Two passages in that plan are stale
> - stage 6's audio predates #249's mixer spine, and stage 4 split into class-vs-asset.

### World corruption, stages 4-6 - superseded by the icebox note above (written 2026-08-27, #332)

**Read first:** the plan is `C:/Users/Michael/.claude/plans/i-want-you-to-abstract-newell.md`; the
rulings are `docs/decisions-ledger.md` 40-45 and 62. Stages 0-3 are BUILT (top of this file).

**Stage 4 - `DA_Corruption_Default` + curves.** Tuning leaves C++. Precedent: `GSWeaponDataAsset` ->
`Content/Data/Weapons/DA_Weapon_*`. Carries the five weights, both soft knees, the razed floor, the
type-weight map, both `FGSCorruptionGrade` ends, and the civilian multiplier. **Two numbers in it are
knowingly unfounded and must be set from a watched raid, not from a fresh guess:**
- `KillSoftKnee` = 12, drafted against ruling 19's 15-defender pool BEFORE the 2026-08-23 roster
  ruling made castle guards Militia with *"a decent amount of them"*. Counting the roster from
  `.umap` files gives reference counts, not instances - it needs the editor or a full raid.
- `GS.Corruption.CivilianWeight` = 2.5. Ruling 62 says "more"; it deliberately does not say how much.

**Stage 5 - `MPC_GSCorruption` + the ground.** No MPC exists in the project (verified: an earlier
survey claiming ~20 Dreamscape materials reference one was reading `ParameterCollectionInfos`, a
field serialized into every `UMaterial`, which hits 172 files). **The `CollectionParameter` node into
`M_GS_Crop_Master` must be hand-authored** - assume Python cannot be trusted with a material graph,
same risk class as the 2D blendspace. Do NOT edit the Dreamscape landscape masters: marketplace
content, shared across three maps, and ground char already routes through `GS_BurnMask`.

**Stage 6 - ash, embers, ambience.** `Content/VolcanoEnvironmentVFX/VFX/Niagara/NS_AtmosphereAsh` and
`NS_AtmosphereEmbers`; **their user-parameter names are unknown until someone opens them** - a
read-and-report step, not a guess. Ambience routes through #249's mixer spine (`Content/Audio/Mix/`).
Ship the GDD 12.1 row and the `features.json` entry TOGETHER - `check_gdd.py` pins ids to 1-20+5b and
fails on both `missing` and `extra`.

**Debts, small, fold into stage 4:**
- The kill log line is `Verbose`, so a real kill leaves no trace in the file. Raise to `Log` - this
  cost a round trip on 2026-08-27 where a working hook could not be told from a dead one.
- A failed `Cast<AGSEnemyCharacter>` is silent: any human that is not one counts as a soldier.
- `ObjectiveRecomputeIntervalSeconds` is not in `DefaultGame.ini` (C++ default 0.5 applies).
- Corruption hooks the three callers UPSTREAM of `UGSCrumbleComponent` rather than `OnCrumbled`.
  #317 made Crumble the unified destroyed state; `GSBuildingObjective.cpp:684` crumbles directly and
  is not counted. Not broken - buildings feed the objectives term - but the wrong shape.

**Open design question nobody has answered:** with per-TYPE weighting, burning one house of 67 moves
the term by almost nothing. Should razing an entire street feel like an achievement? Today it reads
mainly through the structures term, which has its own knee.

**Deferred by ruling, not forgotten:** `MD_GS_Corruption` post-process material; Epic's `DaySequence`
(`UDaySequenceModifierComponent::SetUserBlendWeight` is literally this feature's output stage -
revisit when a hand-authored level exists, it needs an `ADaySequenceActor` per level); the co-op
replicated byte; cross-raid persistence (note `IALSSavableInterface` is actor-shaped, so a
`UWorldSubsystem` cannot be saved by ALS - the float would have to move onto the director).


*Refreshed 2026-08-06. Every `missing:` symbol below was re-checked against the tree that day; an
item whose symbols all now exist was removed rather than left to rot. Three were: **interact
framework** (u=10.0 — all four components exist in `Interaction/` and `Weapons/Abilities/`)
— **THIS REMOVAL WAS WRONG AND COST EIGHT DAYS (corrected 2026-08-14).** "Every symbol exists" is
not "the system runs": the interact framework's components all exist and it has **never executed**,
because `InteractAction` is unset on the player CDO. It is restored to the list below at the top.
The refresh rule that produced this deletion — *remove an item once its `missing:` symbols exist* —
is unsafe on its own and should be read as *remove it once something has been observed running*, —
**lives / respawn** (`AGSPlayerState` exists, `EGSRaidResult::OutOfLives` ends the raid, #009), and
**runic site** (`AGSRunicSite` + `BP_GS_RunicSite` exist, #009/#011). The NEXT list had carried all
three as outstanding for two days while they were being built.*

**Ranked — u carried from run live-003, 2026-08-04. Not re-scored; treat the order as two days old.**

- [EDITOR] u=10.0 **Interact framework — make it RUN** (block A) — **RESTORED 2026-08-14 after being wrongly removed on 08-06.** Nothing is missing in code; 1409 lines exist and have never executed. Blocking five other systems (loot, takedown, foul-well, extract, carry). Step 1 is one CDO property: assign `IA_Interact` to `InteractAction` on `BP_GSPlayerCharacter`, plus the `else` log at `GSPlayerCharacter.cpp:454`. Then one interactable to channel against, `CarrySocket` on `GOB_Scout_v2_Skeleton`, and the `CompleteChannel` consume-ordering fix. Full plan and the ACF comparison in DECISIONS. **Do not remove this item again on the strength of symbols existing.**
- [EDITOR] u=6.0 **Death & hit-reaction clips retargeted ('nothing can die on screen')** (block B) — missing: AM_GS_Death
- [ELIGIBLE] u=5.75 **Someone to fight — the last piece** (block B) — DA_Race_Human and BT_Militia now EXIST and are PIE-verified; missing: DA_Weapon_Greatclub only
- [BLOCKED] u=4.0 **Score system — deeds/loot two-kind tally + end screen** (block G) — missing: UGSScoreSubsystem, GSScore
- [EDITOR] u=3.2 **Horn & horde** (block D) — **C++ HALF DONE AND PIE-VERIFIED, #069/#071 (2026-08-07).** `UGSHordeSubsystem` (pool, three exits, stimulus bus), `UGSGA_Horn` (middle mouse), `AGSHordeGoblin`, `AGSHordeAIController` (runs a BT, no perception), combat verbs hoisted to `AGSCharacterBase`, `GS.Horde.SpawnTest`/`.Status`. Build clean, `LogGSHorde: Pool reset: 20 in reserve, cap 10 active` in PIE, and three blasts against a missing goblin class correctly debited **nothing**. **`AGSHordeSpawnMarker` is NOT missing work — it will never be built** (see DECISIONS); the tag and `AGSRaidMarker::GatherByType` already do the job. Remaining is ALL EDITOR: `BP_HordeGoblin` (+ `HordeGoblinClassPath` in DefaultGame.ini — nothing summons until this exists), `DA_Race_Goblin`, `BB_HordeGoblin` + `BT_HordeGoblin` (hand-author; `BT_Militia` has no EdGraph and **must not be duplicated** — its AcquireTarget service hardcodes the player as the ATTACK target), `IA_Horn` on middle mouse, the placeholder arrival portal, and widening `GEN_NavBounds_Village`. Then the behaviours (Arriving/Follow/Frenzy) and the point command. AI vault deferred — see DECISIONS.
- [BLOCKED] u=3.0 **The stealth five (noise, crouch-detect, takedown, corpse-suspicion, coin toss)** (block F) — Takedown EXISTS (interact framework); missing: ReportGSNoise, CoinToss, DT_NoiseEvents
- [BLOCKED] u=2.5 **Gore/gib system (intensity scalar, feather-poof)** (block G) — missing: UGSGibComponent
- [ELIGIBLE] u=2.5 **Barks + Overlord whispers (runtime side)** (block H) — missing: UGSBarkSubsystem, DT_Barks
- [BLOCKED] u=2.33 **Patrol director — 5-7 min cadence + castle reinforcements** (block F) — missing: UGSPatrolDirector
- [ELIGIBLE] u=2.25 **Loot couriers — sacks + livestock cargo, point-to-courier** (block G) — was BLOCKED on UGSCarryComponent, which now EXISTS; nothing else gates it
- [BLOCKED] u=1.75 **Civilians + livestock (routines, disbelief, brigade, flee)** (block G) — missing: BT_Civilian, DA_Race_Livestock

**Unranked — raised after the live-003 ranking run, so they carry no u.** Do not read the order
below as priority; it is grouped by kind. The first item is the only one anyone has called urgent.

- ~~[ELIGIBLE] **The nine outstanding code-review findings**~~ — **DONE, #036 + #037.** Removed from NEXT 2026-08-07 (#073). See the banner at the top of this file: this line survived a full day after the work landed because closing a ticket does not close the list the ticket was working from.
- [ELIGIBLE] **Radial weapon wheel** — `HANDOFF.md` Part 3. Michael's design, two decisions already settled and **not to be re-litigated**: Q opens a hold-drag-release wheel (top torch / bottom-left sword / bottom-right bow), and the torch becomes a real held weapon fired by the ATTACK button, retiring `IA_ThrowTorch`. Open: who builds the UMG. Note `bRangedMode` is a **bool**, so three slots is a type change (`EGSWeaponSlot`) touching every `IsInRangedMode()` caller. Subsumes the two torch items below.
- [ELIGIBLE] **Torch throw has no animation and is barely visible in hand** — `AM_GS_ThrowTorch.uasset` EXISTS and is referenced by **nothing** in C++; `UGSGA_TorchToss` has no `PlayMontage`, the throw is a 0.25s timer. The held torch is un-readied by `EndAbility` the instant the projectile spawns, which is Michael's "it's never in your hand" — not a missing socket or mesh, both verified fine.
- ~~[ELIGIBLE] **Arrows ignore RaceTag**~~ — **FALSE, and was false when written.** #038 landed this. `GSArrowProjectile.cpp:150` calls `ShooterChar->IsHostileTo(OtherActor)` and gates the damage on it. Verified directly 2026-08-07 (#073). What arrows still do is **STICK** in an allied body, dealing nothing and wasting the shot — that is Michael's settled ruling of 2026-08-06, taken with the horde case explicitly on the table (see DECISIONS). A future agent finding "every shot eaten by a friendly" is looking at intended behaviour.
- [EDITOR] **Building burn duration** — Michael, 2026-08-06, after the first raid that worked end to end: *"burning buildings. Maybe it can take a little longer."* The knob is `UGSFlammableComponent::BurnDurationSeconds` (currently 12s). Data, no rebuild.
- [ELIGIBLE] **Melee `AttackCooldownSeconds` is dead** — the twin of the bow bug closed in #034. `UGSWeaponDataAsset::AttackCooldownSeconds` (1.0s default) has **no reader anywhere**. Check first whether melee is already rate-limited by its montage before adding a second gate on top.
- [ELIGIBLE] **The two lose paths have never been exercised** — `EGSRaidResult::LeftBehind` and `OutOfLives` are wired and neither has run; only `Extracted` is verified (#009). `GS.Raid.ExpireClock`, `GS.Raid.Kill` and `GS.Raid.SetLives` exist to drive them (#018).
- ~~[EDITOR] **`BP_GS_Arrow` subclass**~~ — **RESOLVED 2026-08-09 (#096).** The arrow WAS seen flying wrong (head down, launched from its own middle). Cause found by measuring the asset: `GS_Arrow` is 59.5uu long, its long axis is **+Z**, and its pivot is at the **tail**, while the actor's +X follows velocity — so with the identity default the shaft rendered at 90° to its own flight path. Corrected in the C++ constructor (`Pitch -90`, `X -59.5`), verified live at 0.0° shaft-vs-travel with the head on the collision sphere. A Blueprint subclass is no longer needed to make arrows look right, only to make them look *different*.

## FAILED
- 2026-08-27 **A TERM CAN BE CORRECTLY WEIGHTED AND STILL WRONG IN AGGREGATE.** Corruption's objective term weighted each carrier by type (Mill 1.00, House 0.15) and normalised by total weight. Correct per instance; on `L_Tutorial_Island` 67 houses x 0.15 = 10.05 of a 13.40 total, so **houses were 75% of the term** - GDD 12.1 row 12's complaint reappearing one level up. Fixed by averaging within a type and weighting the four TYPE averages (houses -> 6%). **It was closed as verified twice before this surfaced**, on `GS_BurnTest`, which has three objectives and could never have shown it. **Test driver maths on the map with the most objects, not the tidiest one.**
- 2026-08-27 **THE SUN HAS TWO COLOURS AND THE SKY HAS TWO BLUES.** `SetLightColor` changes what the sun DOES to the scene; `UDirectionalLightComponent::AtmosphereSunDiskColorScale` changes what it LOOKS like in the sky. `RayleighScattering` recolours the dome; ozone (`OtherAbsorption`) keeps its blue-cyan at the zenith regardless. Driving one of each pair produces a world that looks half-corrupted and reports as fully working. Only a human looking up caught it.
- 2026-08-27 **A MULTIPLIER CANNOT LIFT A ZERO BASELINE.** Ozone strength was `Lerp(Base, Base * 2.0, T)`; `L_Tutorial_Island` authors `OtherAbsorptionScale` at 0.0, so the whole ozone fix was silently inert there. Absolute targets for anything a level may legitimately author at zero.
- 2026-08-27 **AN INSTRUMENT THAT ONLY PRINTS TO SCREEN IS HALF AN INSTRUMENT.** `GS.Corruption.Dump` logged to `LogTemp` and never reached `MyProject.log`, so it could only be read over Michael's shoulder. Log to the feature's own category. The same shape bit twice more: the dump printed a status (`SkyAtmosphere found`) where it needed a VALUE, and the kill line is still `Verbose`. **Print the number, not the state.**

- 2026-08-21 **THE SAME TICK TRAP AS #135, FROM THE OTHER DIRECTION - THIS TIME THE BASE CLASS DID
  IT** (#223, ACF Phase 2a). `AACFCharacter`'s constructor sets
  `PrimaryActorTick.bStartWithTickEnabled = false` (`ACFCharacter.cpp:83`) and **nothing in ACF ever
  turns it back on**. The engine default is `true`. `AGSPlayerCharacter` sets `bCanEverTick = true`,
  which is a DIFFERENT FLAG and did not help - our constructor also runs second, after ACF's.
  - **Symptom: sprint and stamina both stopped, and nothing else did.** Both live in
    `BP_GSPlayerCharacter`'s Event Tick. Every event-driven system - attacks, dodge, interaction,
    blocking, death, the whole horde - kept working perfectly, so it presented as "sprint is broken"
    rather than "the actor is not ticking".
  - **Cost: three wrong fixes.** Two of them were about `MaxWalkSpeed` and ACF's locomotion state
    machine, which is a real hazard (see BUILT) but was not this. The first of those fixes was
    **inert and the build was green** - `SetDefaultSubobjectClass<UCharacterMovementComponent>` is
    REFUSED at runtime because an override must DERIVE from the class the parent chose; it logged
    `is not a legal override for component CharMoveComp` once per character and used ACF's component
    anyway. Read the log, not the diff.
  - **What ended it was a question, not an investigation:** "does nothing happen, or does it happen
    weakly?" The answer "nothing at all", plus the volunteered "stamina doesn't work either", named
    the system in one step. Two symptoms sharing one mechanism beats any amount of tracing.

- 2026-08-11 **A SUBCLASS CONSTRUCTOR SILENTLY DISABLED TWO SHIPPED FEATURES FOR A WHOLE CLASS OF
  AGENT** (#135). `AGSHordeAIController` set `PrimaryActorTick.bCanEverTick = false`;
  `AGSAIControllerBase` sets it true. The subclass runs second, so **#132's separation steer and
  #133's facing authority never executed on a single horn-summoned goblin** — while both tickets
  claimed the horde was covered, and #132's Evaluate said so in as many words. Nothing failed loudly:
  no log, no warning, no compile error, just an absent feature.
  - **`GS.Combat.Duel` CANNOT SEE THIS CLASS OF BUG**, and it is what every crowd ticket from #105
    onward has tested with. It re-badges human defender BPs onto `DA_Race_Goblin`, so its "goblins"
    possess `AGSAIControllerBase` directly and tick normally. **Anything added to
    `AGSAIControllerBase::Tick` must be exercised on a HORN-SUMMONED goblin** (`GS.Horde.SpawnTest`),
    not in a duel.
  - The header's *"the blackboard refresh is a timer, not Tick"* paragraph was read as a blanket ban
    and implemented as one. It has been rewritten: the real rule is **no per-agent SEARCH on the
    frame**, which a 4Hz broadphase overlap behind early-outs does not violate.
- 2026-08-11 **`SK_Human_Skeleton` HAS EVERY BONE ON `Animation` TRANSLATION RETARGETING, AND IT HAS
  NOW CAUSED TWO SEPARATE VISIBLE BUGS** (#133, #134). That mode takes each bone's translation from
  the animation and discards the target mesh's own bind pose — which is wrong for this project,
  because all six human defenders wear `_baked` meshes re-bound from their own per-character
  skeletons onto that one shared skeleton.
  - **#133:** importing a Mixamo FBX exported from a differently-proportioned character applied its
    translations verbatim — measured at exactly **0.600x on every bone**, which telescopes the whole
    skeleton inward and reads as *"the spine is collapsing inside the body"*. **`import_uniform_scale`
    does NOT fix it** (it moves only the Hips). Import human animation through `MixamoSource` +
    `RTG_MixamoToHuman` instead — that measures 1.000x on every limb bone.
  - **#134:** Erika's and the Knight's eyes were posed from the shared rig's eye bones (0.185 of head
    height) rather than their own (0.365), so the eyes sat outside the head. The guards were immune
    only because they have **no eye bones at all**. Fixed by setting `Head`/`LeftEye`/`RightEye`/
    `HeadTop_End` to `Skeleton` retargeting. **The rest of the skeleton is still `Animation`, so the
    trap is disarmed only for the head** — the canonical fix (all non-`Hips` bones to `Skeleton`,
    `Hips` to `AnimationScaled`) is deliberately still outstanding.
- 2026-08-11 **A 2D BLENDSPACE AUTHORED FROM PYTHON IS COSMETICALLY PERFECT AND FUNCTIONALLY DEAD,
  AND I NEVER GOT ONE WORKING** (#133). Four passes, four different failures, all shipped to Michael
  as "verified" because **every check this project's tooling can perform passes on a dead asset**:
  the samples read back exactly, the skeletons match, the sample grid is a complete rectangle, the
  AnimBP compiles `BS_UP_TO_DATE`, and the thing still evaluates to the reference pose.
  - **CONFIRMED DEAD BY MEASUREMENT 2026-08-11 (#137), not by inference.** `GS.Anim.Snapshot` on the
    goblin AnimBP pointed at `BS_GS_Locomotion_Gob` reported **9 of 15 pawns in the reference pose** —
    every pawn using that graph — against **0 of 18** on the old 1D asset. Four passes of reasoning
    in #133 could not settle this; one command did.
  - **The strong suspicion, unproven:** `UBlendSpace` builds its grid/triangulation inside
    `PostEditChangeProperty`, and writing `sample_data` from Python does not reliably fire it, so the
    blend surface never exists and only inputs landing EXACTLY on a sample resolve. Passing
    `set_editor_property(name, value, unreal.PropertyAccessChangeNotifyMode.ALWAYS)` did **not** fix
    it. There is no Python API that exposes the triangulation, so this cannot be checked — only
    inferred from behaviour.
  - **If you need a 2D blendspace, author it BY HAND in the editor.** Do not spend another session
    proving the array was written. The clips, the skeletons and the sample maths were all correct
    every time; the asset was not.
  - Genuine sub-findings worth keeping regardless: a direction axis needs an explicit `+180` column
    (`wrap_input` does **not** close the convex hull); per-sample `rate_scale` must not be combined
    with `axis_to_scale_animation` (the working `ThirdPerson_IdleRun_2D_Gob` uses axis scaling alone
    with every rate at 1.000); and `A_MX_Sprint_Gob` lives in `Anims_Climb`, not `Anims_Loco`.
- 2026-08-10 **THREE FIXES IN A ROW SHIPPED WITHOUT ANYONE WATCHING THEM RUN, AND ALL THREE WERE
  WRONG** (#113, #116, #118). The pattern, not the individual bugs, is the entry worth reading:
  - **#113** set `force_root_lock=True` on 9 human clips to stop attacks popping upward, and closed
    saying *"the root lock has not been seen in play"*. It did not fix the pop — it replaced it with
    a collapsed mesh, because locking the root pins it to the ref pose while the hips keep 88uu of
    authored travel. **Michael diagnosed this at the time — "the root node is getting snapped to the
    ground" — and was told no.** He was right. Fixed in #119 by repointing the montages at the clean
    `A_HU_*` import; the `A_MX_*_Gob` copies are the old attempt.
  - **#116** fixed the recoil punish by argument. The first duel run: 17 blocks landed, 0 punishes.
    The veto had simply moved from `State.Recoil` to `State.HitReact`, because the flinch that sells
    a block sets that tag too (#117).
  - A **skeleton-mismatch theory** for the T-pose survived a full code read and died in one run —
    zero `REFUSED montage` lines.
  - **What actually worked, every time: a log line or an eyeball.** `GS.Combat.LogAI 1` +
    `GS.Combat.Duel 3` settled the punish in one run. Michael previewing ONE montage settled #119
    before the other ten were touched. Static reads confirm what is *configured*; they never show
    what the engine *does* with it. Two questions cut the search fastest, both from him: *does
    distance change it?* and *only during the action, or also at rest?*
  - **Two probes that lie.** `SkeletonService.get_bone_transform` returns success for any bone name,
    including `mixamorig1:Hips`. A `.uasset` name-table grep proves a reference exists in the
    package, not that a property is assigned — read the CDO.
- 2026-08-09 **NO HUMAN MESH HAS ANY WEAPON SOCKET** (#097). Checked all eight defender meshes for
  `hand_r_weapon` / `hand_l_weapon` / `back_sword` / `back_bow` / `spine_quiver` — every one missing,
  while `GOB_Scout_v2` has all five. Each guard also has its OWN skeleton (`SK_CastleGuard01_Skeleton`
  etc.), not a shared one, so authoring a socket means doing it per character. **Workaround in use:
  attach to the `RightHand` BONE** — UE attachment accepts a bone name as readily as a socket. Human
  rigs are Mixamo-style: `Hips` / `Spine` / `RightHand`. Holstering will need real sockets.
- 2026-08-09 **`UGSWeaponComponent` was on the PLAYER ONLY** — every guard in the game fought
  bare-handed all along. Added to `AGSEnemyCharacter` in #097 with a `DefaultWeapon` slot.
  `AGSHordeGoblin` still has none: **the goblins are still empty-handed.**
- 2026-08-09 **Soft-object mesh fields on a data asset CANNOT be cleared from Python.**
  `set_editor_property(prop, None)` silently no-ops and `SoftObjectPath('')` throws a conversion
  error, so a duplicated `UGSWeaponDataAsset` keeps meshes you do not want — which put a floating bow
  and quiver at every guard's feet. **Build such an asset fresh rather than duplicating it.**
- 2026-08-09 **THE GOBLIN MESHES ARE A FRACTION OF THEIR CAPSULES** (found in #094, NOT fixed -
  needs Michael). Measured live, head bone above the capsule's feet: Erika (human) **+256** in a 300
  capsule, player goblin **+40** in a 240 capsule, horde goblin **+70** in a 240 capsule. A goblin's
  visible head sits at ankle height of its own collision volume. Arrows aimed at the capsule centre
  pass well above a goblin's actual head and still register a hit because the capsule is what blocks.
  This reaches far past archery - collision, cover, doorways, camera and every trace in the game
  reason about that capsule.
- 2026-08-09 **"Closest bone" is not "the bone you hit".** #092 detected headshots with
  `FindClosestBone(ImpactPoint)`; because an arrow stops on the CAPSULE, the impact point sits on a
  cylinder around the body and the nearest NAMED bone to a chest-height point is the neck/head chain
  - so **every single arrow was a headshot** (a flat 50 damage) the first time anyone watched one
  fly. Fixed in #095 by measuring distance to the head bone instead. The lesson is the shape of the
  bug: a detection heuristic that can only ever answer "yes" is worse than not having one, and #092
  shipped it with its own Evaluate saying "unproven in play".
- 2026-08-09 **A BT Selector RESTARTS when a child succeeds - so any child that succeeds instantly
  starves everything below it.** This has now bitten twice in two days, both times presenting as "the
  AI just stands there". `BTTask_MoveTo` returns Succeeded *immediately* when the agent is already
  inside its acceptable radius, so an agent parked on its stand-off slot busy-loops:
  MeleeAttack fails -> MoveTo succeeds instantly -> tree restarts -> repeat, and nothing below MoveTo
  ever runs. #089 was this with the facing check (fixed by turning instead of refusing); #090 was
  this with the menace orbit (fixed by putting it ABOVE the chase). **Rule: anything that must run
  when an agent is stationary at its destination has to sit above the MoveTo, and any node that can
  refuse must also make progress toward being able to accept.**
- 2026-08-08 **A BT node that only WRITES blackboard keys works fine unresolved — so a missing
  `InitializeFromAsset` is silent until the day something reads `IsSet()`.** `FBlackboardKeySelector::
  IsSet()` tests `SelectedKeyType`, populated only by `ResolveSelectedKey`, which is called only from
  a node's own `InitializeFromAsset` override. `UBTService_AcquireTarget` never had one and did not
  care for its whole life, because `SetValueAsObject(Key.SelectedKeyName, ...)` needs no resolution.
  The moment #083 guarded the attack telegraph on `IsSet()`, the guard was a permanent false: the key
  was never written, the Block decorator never passed, and PIE showed "the AI does not block" with no
  error anywhere. Fixed in #086. **`UBTTask_MeleeAttack` still has no override** — correct today
  because it uses no `IsSet()`, and a trap for whoever adds one.
- 2026-08-08 **`EditorAssetSubsystem.save_loaded_asset` defaults to `only_if_is_dirty=True`, and
  `set_editor_property` on `UBlackboardData.Keys` does not mark the package dirty.** The save
  returned `True` and wrote nothing; reading the key straight back **succeeded**, because that reads
  the in-memory object. `BB_Human.uasset` kept its 2026-08-05 mtime for an hour while a C++ bug that
  did not exist was hunted. **Verify an editor write against the DISK — mtime or the bytes — never
  against a read-back.** Pass `save_loaded_asset(asset, False)` when the edit may not have dirtied
  the package.

- 2026-08-06 **A burn objective's Required/Optional state is ERASED before it tells anyone it
  completed.** `AGSBurnObjectiveBase::HandleCompleted` calls `SetListState(Complete)` *before*
  broadcasting `OnBurnObjectiveCompleted`, so every listener sees `Complete` and cannot tell what the
  objective was a moment earlier. Anything that needs the distinction (scoring, barks, progression)
  must track it itself — asking `UGSRaidDirector` instead makes the answer depend on delegate binding
  order between two subsystems, which is not contractual. `UGSScoreSubsystem` keeps its own
  scored-types set for exactly this reason (#053).
- 2026-08-06 **BOTH lose paths fire correctly and NOTHING HAPPENS WHEN THEY DO.** First execution
  ever of `EGSRaidResult::OutOfLives` and `LeftBehind` (#049). Both log `RAID ENDED: <result>`
  exactly as designed — and then the game carries on. `UGSRaidDirector::EndRaid` sets the result,
  logs, and broadcasts `OnRaidEnded`; the only subscriber is `UGSPlayerHUDWidget::HandleRaidEnded`,
  which forwards to the **`BlueprintImplementableEvent` `OnRaidEnded`** — and `WBP_GSPlayerHUD` does
  not implement it. So a finished raid produced one log line and no screen, no pause, no restart.
  **Correction to my first reading of this:** all five HUD `BlueprintImplementableEvent`s are
  unimplemented, but only `OnRaidEnded` mattered. The other four (`OnLivesChanged`,
  `OnObjectiveListChanged`, `OnRaidClockPhaseChanged`, `OnAlarmPhaseChanged`) are *enrichment hooks*
  over text `UGSPlayerHUDWidget` already writes to bound widgets itself — an unimplemented
  `OnLivesChanged` costs a nicer lives display, not the lives display. `OnRaidEnded` was the only
  output of the raid loop with no C++ fallback behind it. Fixed in #050 by giving it one
  (`EndPanel` / `EndTitleText` / `EndDetailText`, C++-driven, BP event still fires after).
  **Lesson: "the event is unimplemented" is not the same as "the feature is missing" — check whether
  C++ already writes the primary path before counting a hook as a hole.**
- 2026-08-06 **`EndRaid` does not stop the raid clock.** After `OutOfLives` the clock is still
  `Running` and the timer keeps counting down (observed 1769s → 1762s across two `GS.Raid.Status`
  calls *after* the raid had ended). `LeftBehind` looks like it stops the clock, but only because
  the clock expiring is what ended the raid. Ending for any *other* reason leaves it ticking.
- 2026-08-06 **The raid clock expiring does NOT strand you — it starts a 90s collapse.**
  `GS.Raid.ExpireClock` moves phase 1 (Running) → phase 3 (Collapsing, 90s), and only a second
  expiry reaches `LeftBehind`. Worth knowing before "the clock ran out and nothing happened" gets
  filed as a bug: it is a two-stage transition and both stages must be driven.
- 2026-08-06 **`EditorAssetSubsystem.load_asset` returns None WHILE PIE IS RUNNING**, and
  `does_asset_exist` returns False, for assets that demonstrably exist and load fine once PIE stops.
  Same family as `get_editor_world()` returning null during PIE. Stop PIE before inspecting assets,
  or you will conclude an asset is missing when it is merely unavailable.
- 2026-08-06 **A material used on a `USplineMeshComponent` needs `bUsedWithSplineMeshes`, or it
  silently renders as the ENGINE DEFAULT.** `M_GS_AimArc` shipped without it, so the aim ribbon drew
  with the default material from the day it was written — and recompiled the shader on every editor
  launch. The only symptom is one `LogMaterial: Warning ... missing usage flag SplineMeshes` line at
  load. It is a checkbox on the material; no rebuild. Check the flag on any material assigned to a
  spline mesh, ribbon, or instanced mesh.
- 2026-08-06 **A derived table must be recomputed from the values actually SHIPPED.** Ticket #043
  computed camera-clearance figures at `AimArmLength = 250`, then set it to 320 in the same ticket
  and shipped the old table. Drop is `ArmLength * sin(pitch)`, so the change that fixed one complaint
  silently invalidated every row. The Evaluate flagged the wrong risk about the same number. If a
  ticket changes an input to its own arithmetic, redo the arithmetic before closing.
- 2026-08-06 **The aim camera's real obstacle on L_Tutorial_Island is the WHEAT, not the ground.**
  `SM_VillageWheat_01/02` are 158 uu tall at ~91,500 instances each (~275k instances of 117–158 uu
  cover). Any camera height under ~200 uu is inside the canopy regardless of collision, so aim-camera
  clearance is a height problem, not a `bDoCollisionTest` problem.
- 2026-08-06 **Iterating a Python-exposed `Array` of structs yields COPIES.** `for m in arr:
  m.set_editor_property(...)` changes nothing, and the subsequent `save_loaded_asset` still returns
  `True` — the first `IMC_Default` remap "succeeded" and the read-back showed the old action. Assign
  back by index (`rows[i] = m`), then re-read after `collect_garbage()` + `load_asset`. This is the
  concrete, repeatable cause behind "a successful tool call is not evidence".
- 2026-08-06 **A guard that resolves paths against ONE root, when the data carries two conventions,
  fails OPEN — and looks exactly like a guard that passed.** `gsqueue.ps1`'s stale-Evaluate check
  (#024) joined every claimed path to the git root, but 102 of 126 claims across all tickets are
  *project*-relative (`Source/…`, `Content/…`, living under `GoblinSiege 5.8/`) and only 24 are
  repo-relative. `Test-Path` failed, the loop `continue`d, and the check silently examined nothing on
  the majority of files for a full day. Fixed in #036 by resolving against **both** roots and
  **reporting** anything that resolves nowhere. The lesson generalises: when a check skips what it
  cannot resolve, "I could not look" is indistinguishable from "nothing changed".
- 2026-08-06 **A code-review finding is a claim about a MOMENT — re-check it before acting.** The one
  HIGH finding in `HANDOFF.md` (a `TypeError` crashing `gs_buildings.py` on any level) was already
  gone when it was picked up: ticket #033's unrelated rewrite deleted the code, confirmed with
  `git log -S PIECE_KEYS`. It had sat in the handoff as HIGH regardless. Cost of checking: one
  command. Cost of not checking: debugging a bug that does not exist.
- 2026-08-06 **A UPROPERTY with a sensible default and NO READER is invisible from both sides.**
  `UGSWeaponDataAsset::RangedAttackCooldownSeconds` carried a 1.5s default and a design comment from
  the day it was written, and nothing ever read it — so the bow fired as fast as the mouse could
  click while `DA_Weapon_Scout` looked correctly configured, because it was. Found by Michael playing
  it, not by any tool. Fixed in #034. **`AttackCooldownSeconds` (melee) is still dead.** Grep every
  tuning field on a data asset for at least one reader.
- 2026-08-06 **Dreamscape fog cards ship with collision ON.** 12 of them in L_Tutorial_Island
  (`Plane`, `Plane4`-`Plane14`): `/Engine/BasicShapes/Plane` + `MI_Fog_02`, all `ECR_BLOCK` to
  Pawn. A Plane mesh is single-sided and fog is translucent, so you see nothing and still cannot
  walk through - one was a 111m x 8m x 51m slab across the village. Fixed by setting
  `NO_COLLISION`, NOT by deleting (they are atmosphere art). **Expect the same in any new
  Dreamscape scene.** `Plane2` is the village water (`MI_VillageWater`) and was deliberately left
  blocking. Map backup: `D:\goblinRaid\Map_Backup_20260806\`.
- 2026-08-06 **"Mesh is None" is the WRONG way to hunt invisible collision.** An empty
  `StaticMeshComponent` has no collision geometry at all - 12 such actors in this level all report
  `get_actor_bounds(True)` = (0,0,0) and block nothing, and one of them is a live `GSMillObjective`
  that a "delete everything empty" sweep would have destroyed. Sort by `get_actor_bounds(True)`
  instead. Note `PrimitiveComponent.bounds` is not readable from Python here;
  `AActor.get_actor_bounds(bOnlyCollidingComponents)` is.
- 2026-08-06 **For "something is blocking me", get the player's POSITION first.** Five structured
  searches over 9,067 actors found nothing; one `sphere_overlap_actors` at the live PIE pawn
  location found it immediately. Read the position off
  `GameplayStatics.get_player_pawn(...).get_actor_location()`.
- 2026-08-06 **`GEN_NavBounds_Village` is only (4000, 4000, 2998)** - an 80m x 80m navmesh box for
  the entire village, at (-13000, 57000). This is why defenders freeze after chasing the player any
  distance: they path straight out of it, and off the navmesh `move_to_location` returns FAILED.
  Not yet fixed; wants its own ticket.

- 2026-08-05 **The work queue is ADVISORY - it protects only against agents that actually run
  it, and the first one to breach it was the agent that built it.** `AgentQueue/` was created,
  its rules written, and other agents' tickets reviewed against them, across a session in which
  the author claimed nothing and edited seven files. `AGENT_STATE.md` was among them while
  ticket #005 held a claim on it; the collision surfaced as an editor "File has been modified
  since read" rejection, i.e. caught by a tool, not by the queue - the same shape as the
  2026-08-04 `GSPlayerCharacter.cpp` near-miss the queue exists to prevent. Ticket #010 is the
  record. QUEUE.md now states that the orchestrator is not exempt: queue operations need no
  ticket, **writing any repo file does**. Nothing in the system detects an unticketed writer -
  the only reason to trust the board is that every session chooses to use it, so a window that
  never adopted it is invisible. Symptom to watch for: files with recent mtimes that no open
  ticket claims (`gsqueue.ps1 list` against the working tree).
- 2026-08-05 **Marketplace Niagara systems recompile on EVERY load until re-saved once.**
  `N_Portal4_V2` and `N_Portal4Elemental` cost 24.2s + 24.3s every time `L_Tutorial_Island` opened
  (one session paid 320.9s across 22 compiles) because the compiled result was never serialised
  back into the asset. Loading each and re-saving fixed it - verified by a fresh editor showing
  zero `Compiling System NiagaraSystem` lines. **Before blaming gameplay code for a load hitch,
  run `Select-String "Compiling System NiagaraSystem" Saved/Logs/MyProject*.log`.** Backups at
  `D:\goblinRaid\PortalVFX_Backup_20260805\`.
  (Verification confirmed at review: the re-save ran 18:40:07 UTC in the process logging to
  `MyProject_2.log`; the clean load ran 19:25:40 UTC in a *different* process that opened at
  19:15:29 and compiled zero Niagara systems. Different process, so the reading is real and not
  the systems merely being warm in memory.)
  **STILL OWED - the fix reached 2 systems; the folder holds 25.** Within the `N_Portal4*` family
  alone, `N_Portal4` (18.27s) and `N_Portal4Boss` (18.26s) carry the identical latent cost (only
  `N_Portal4Book`, 0.57s, is cheap). They escaped because they are not placed in
  `L_Tutorial_Island`, and ticket 001's "the other three total ~1.2s" was wrong - it is ~37s for
  those two alone. **The runic-site / portal work on NEXT is precisely what will load them**, so
  they must be re-saved before a portal is placed or that cost reappears as a "new" hitch nobody
  connects to this. Ticket 003 is auditing the full 25.
  ~~Note `Content/*` is gitignored, so these assets have no git history.~~ **WRONG - corrected
  2026-08-07 (#073).** `.gitignore` writes `Content/*` and then re-includes the project folders with
  `!Content/Characters/`, `!Content/Blueprints/`, `!Content/UI/`, `!Content/Input/` and more. **465
  files under `Content/` are tracked**, via LFS - including `BP_GSPlayerCharacter.uasset`,
  `BT_Militia.uasset` and `IMC_Default.uasset` (verified with `git ls-files --error-unmatch`).
  Only `Content/GoblinSiege` and `Content/DreamscapeSeries` are genuinely untracked. This matters
  the wrong way round: an agent who believes the old note will skip a `git checkout --` that would
  have worked, and reach for a backup folder instead. Backing up before a large `.uasset` edit is
  still cheap and sensible - just do not think git is unavailable.
- 2026-08-05 **You cannot measure performance by driving PIE from `gs_run.ps1`.** With the editor
  window in the background it throttles to ~3 FPS, and `PerformanceService.frame_timing()` counts
  the idle as game-thread time - it reported a confident "GameThread bound, clear confidence"
  verdict twice while `stat dumpframe` showed `Game thread tick wait time` 287-310ms of a 333ms
  frame and real `World Tick Time` of only 9.2-9.8ms. Setting `Slate.bAllowThrottling 0` did not
  fix the verdict. A real reading needs the window FOCUSED, via
  `PerformanceService.start_trace` -> play -> `stop_trace` -> `analyse`.
- 2026-08-05 `is_actor_tick_enabled()` is the tick census, NOT
  `primary_actor_tick.start_with_tick_enabled` - the latter is true even when `can_ever_tick` is
  false, which made 8,034 non-ticking `StaticMeshActor`s look like 8,034 ticking ones.
  L_Tutorial_Island really has **19** ticking actors out of 9,076.
- pre-seed (from build log / decision queue — do not rediscover): Live Coding cannot register new UCLASS/UPROPERTY — full editor-closed build required. In-editor Compile gives zero feedback; stranded-UBT bug = `Launching UnrealBuildTool...` with no `HotReload took` → kill orphaned dotnet. `.bat` written from a Linux sandbox needs CRLF. PowerShell over the MCP bridge: no `$`, quote every path, `--%` for native args. VibeUE `list_expressions` inlines nested material functions — connecting to an inlined node writes an illegal cross-package ref that blocks saving. `NS_GS_SmokeColumn` must use `M_Smoke_01`, never `M_Smoke_02` (broken). Content/GoblinSiege + DreamscapeSeries paths are untracked in git — agent-side edits **there** have no backup (the rest of `Content/` IS tracked via LFS — see the corrected note above; 465 files). ~~**Build times: the UnrealBuildAccelerator permission problem is FIXED as of 2026-08-07.**~~ **WRONG, AND IT WAS WRONG THE DAY IT WAS WRITTEN — corrected 2026-08-20 (#206).** It was declared fixed on the strength of `Build-GoblinSiege.ps1` printing *"UBA cache is writable"*, and **that check was lying**: it created a BRAND NEW file at the cache root, which always succeeds because ProgramData lets any user create files. UBA never creates fresh files there. It **rewrites** `memgroups`, **replaces** `cas\casdb` and **deletes** stale session folders, and every one of those already exists owned by `BUILTIN\Administrators` from an elevated install — which a non-elevated `doomsday\michael` can create beside and cannot touch. Measured 2026-08-20: `Validated storage (size 0b)`, ~20 `Access is denied` failures, and a **12m17s** build. **`CLAUDE.md`'s "UBA is crippled" section was right all along and this line wrongly told three sessions to ignore it.** The check now probes memgroups, `cas` and session ownership instead, and reports NOT usable. **The permission fix itself still needs an ELEVATED shell and has not been applied** — `icacls "C:\ProgramData\Epic\UnrealBuildAccelerator" /grant "%USERNAME%:(OI)(CI)F" /T` then clear `sessions\*`. Until then every build pays full compile cost. The lesson is the shape, not the ACL: **a probe that tests something easier than what the real code does will pass forever.**
- 2026-08-04 [live-002] generation of interact_framework failed: no JSON object in model output
- 2026-08-04 [live-002] generation of interact_framework failed: Unterminated string starting at: line 20 column 12 (char 44113)
- 2026-08-04 KNOWN, not yet fixed: `UGSGA_SwordLight` still caches `MaxWalkSpeed` directly
  (`ApplyMoveSpeedScale`/`RestoreMoveSpeed`) instead of using `UGSGE_MoveSpeedScalar`. Swinging is
  blocked while carrying so the common case is safe, and `ApplyMoveSpeed` self-heals on the next
  attribute change — but a slow starting or ending MID-SWING makes the swing's restore write a stale
  speed that persists until the next change. Fix is converting it to the GE like Block.
- 2026-08-04 ~~**Defenders kill each other.**~~ **FIXED.** The sword sweep had no friend/foe test and
  Militia on 30 HP died in two swings, which is why placed defenders vanished mid-PIE. There is a
  race concept now: `Race.Goblin` / `Race.Human`, `AGSCharacterBase::RaceTag` +
  `IsHostileTo()`, checked by `UGSGA_SwordLight::DoSweep`. Defenders inherit `Race.Human` from
  `DA_Race_Human`'s RaceTag via `InitializeFromArchetype`, so it is one field on one asset rather
  than six Blueprints. Player and horde goblins are `Race.Goblin` in C++. **An unset race still hits
  everything** - opt-in, so nothing silently became invulnerable. Fire deliberately does NOT check
  race: the torch is the goblin equalizer and burns its owner too.
- 2026-08-05 ~~`GOB_Scout_v3` is wearing another mesh's physics asset~~ **FIXED, and six more with it.**
  `unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).create_physics_asset(mesh, True, 0)`
  generates a fitted asset with FBX-import settings and assigns it in one call - the earlier "not
  scriptable" note was wrong, it was `PhysicsAssetFactory` that is useless from Python, not the
  subsystem. `GOB_Scout_v3` now uses `/Game/Characters/ScoutV3/GOB_Scout_v3_PhysicsAsset` instead of
  the `_Import` asset built for another body: ragdoll bones now sit a median 112uu / max 186uu from
  the pawn, against 1850-3700uu before. The six human `SK_*_baked` meshes had **no physics asset at
  all**, so defenders froze in their last pose instead of ragdolling; each now has one beside it and
  a killed PeasantMan simulates properly (median 114uu, max 193uu, bounds 125x215x189). Backups:
  `D:\goblinRaid\ScoutV3_Backup_20260805\` and `HumanMesh_Backup_20260805\`.
  **Careful:** `is_physics_asset_compatible` only checks bone names, so it returned True for the
  wrong-body asset too - it will not catch a mis-fitted one.
- 2026-08-04 ~~**`GOB_Scout_v3` is wearing another mesh's physics asset — CONTENT FIX STILL OWED.**~~
  `GOB_Scout_v3.PhysicsAsset` = `/Game/_Import/SK_GoblinScout_Rigged_v2_PhysicsAsset`, authored for a
  different body and living in import staging. Bodies that do not fit start interpenetrating and
  depenetration throws them, which is why a corpse measured 1881x1252x609 of bounds centred 1850uu
  away. Mitigated in C++ (`bComponentUseFixedSkelBounds` on death, verified: extent now ~(120,120,130),
  centre within ~100uu) but the ragdoll itself is still simulating on wrong-shaped bodies and will
  look wrong. **Real fix, needs a human in the Physics Asset editor:** generate a physics asset from
  `GOB_Scout_v3` itself (it rides `GOB_Scout_v2_Skeleton`, so `GOB_Scout_v3_240u_PhysicsAsset` is NOT
  a drop-in - that one targets `GOB_Scout_v3_240u_Skeleton`), save it beside the mesh in
  `/Game/Characters/ScoutV3/`, and stop referencing anything under `/Game/_Import/`. Not scriptable:
  `unreal.PhysicsAssetFactory` exposes no target-mesh property and physics asset bodies are unreadable
  from Python.
- 2026-08-04 ~~`BP_GSPlayerCharacter`'s collision is broken for AI.~~ **CORRECTED, and the real
  cause is worse.** The player Blueprint is fine - a freshly spawned one has a QUERY_AND_PHYSICS
  capsule and bounds of (75,58,120). The `NO_COLLISION` capsule and ~5385x4748 bounds were measured
  on a pawn that was **dead**: `AGSCharacterBase::HandleDeath` disables the capsule and ragdolls the
  mesh, and the ragdoll's bounds explode. So `MoveToActor` returns `AlreadyAtGoal` for anything
  within ~50m *of a corpse*, and defenders were mobbing a body forever. FIXED: the acquire service
  ignores targets holding State.Dead. Measuring a live actor and blaming its Blueprint was the
  mistake - check `bIsDead` before trusting any bounds reading off a character.
- 2026-08-04 `BT_Militia` was authored by setting `RootNode` from Python; it has **no editor
  EdGraph**. Opening it in the Behaviour Tree editor and saving may regenerate an empty graph and
  wipe the tree. Rebuild it by hand there if you want to edit it visually.
- 2026-08-04 `TryLightAttack()` returning False right after a swing is NOT a bug - GAS refuses
  re-activation while the ability runs, which UGSGA_SwordLight uses as its combo buffer. Sampling it
  mid-swing looks like a failure and is not one.
- 2026-08-04 `UGameplayAbility::AbilityTags` is deprecated in 5.8 (C4996) — used by `GSGA_Block` and
  `GSGA_Interact`. Compiles today, will NOT compile after the next engine upgrade. Migrate both to
  `SetAssetTags()` in the constructor when that comes.

## RUNS
- 2026-08-04 **mvp-001** — picked 'Interact framework — hold-E channels + carry' (u=10.0); ok
- 2026-08-03 **live-001** — scan-only: 13 open gaps
- 2026-08-04 **live-002** — picked 'Interact framework — hold-E channels + carry' (u=10.0); FAILED
- 2026-08-04 **live-002** — picked 'Interact framework — hold-E channels + carry' (u=10.0); FAILED
- 2026-08-04 **live-003** — picked 'Interact framework — hold-E channels + carry' (u=10.0); ok

---

## Rulings, 2026-08-23 — three animation debts closed by Michael, not by fixing them

Raised as outstanding work at the end of the bow session; he accepted all three as they stand. Do
not re-open these as defects.

- **Erika has a bow recoil but no draw animation.** Her draw window is
  `UBTTask_RangedAttack::DrawSeconds` (0.8s) and the ability only activates at the shot, so a draw
  clip would have to be started from the BT task. Michael: *"looks good for now."* ACCEPTED.

- **The human locomotion bands do not match their clips.** `Walk -> Run` enters at `HU_Speed > 500`
  while `A_HU_Std_RunF` is authored at 406.9 uu/s, so anything in the Run state slides by at least
  1.23x; `BS_GS_Locomotion_Hu` is still an ORPHAN node, so `HU_Direction` is computed every frame and
  consumed by nothing and every direction of travel plays forward clips. Bands were retuned under
  #251 (Idle<->Walk 60/25, Walk<->Run 290/250, guards 450, archers 200) and the measured flicker fell
  from 1.5-2.4/s to 0.4-0.6/s. Michael: *"looks good."* ACCEPTED at that level - the orphaned
  blendspace is a known, deliberate gap, not an unnoticed bug.

- **The bow timing bar art.** I flagged it as watermarked stock and a licensing risk. Michael: *"I
  created it."* NO LICENSING ISSUE - the flag was wrong. The watermark-shaped artefact keyed out of
  the source was not a third party's mark.

## Iceboxed, 2026-08-23 — #249 (audio spine) and #252 (world corruption)

> **SUPERSEDED for #252 as of 2026-08-27.** The blocking question below - *do civilian kills corrupt
> the world as much as knight kills?* - was put to Michael and answered: **civilians count MORE**
> (ruling 62). Corruption was re-filed as **#296** and has since shipped stages 0-3; see the BUILT
> entry at the top of this file. #249's spine did land and is what stage 6's ambience should route
> through, rather than the bare `UAudioComponent` the corruption plan describes.


Michael's ruling: park both, do not work them. Their board status is `abandoned` **only so they stop
holding the build gate shut** — nothing was reverted and all of their work is committed in `main`.
The handover, including what each deliberately left undone, is **`AgentQueue/ICEBOX.md`**. Read that
before treating either as dead or as finished.

The one thing that unblocks #252 is a question for Michael, not a task: *do civilian kills corrupt
the world as much as knight kills do?* The agent drafted it as ruling 46 and then cut it rather than
record a decision Michael had not made.

#249's spine has **never been listened to**. It is configuration that loads; whether the mix sounds
right is unproven.

## #269 closed UNOBSERVED, 2026-08-23

ACF Phase 3 scoping — the analysis of why an unused `UACFEquipmentComponent` rode along since Phase
2a while `UGSWeaponComponent` did the same job. **What is unproven:** nothing, in the sense that
nothing runnable was produced — it is a written argument. Its conclusions were acted on in #270,
which *was* observed (10 of 10 summoned goblins equipping the axe through ACF). If #269 reasoned
wrongly, the symptom appears in #270's behaviour, not in anything #269 shipped. Both components still
coexist on purpose; #270 changed nothing in `UGSWeaponComponent`.

## #277 closed UNOBSERVED, 2026-08-24 — the finite-arrows ruling

Rulings 46-52: arrows become finite for the player, torches stay infinite, AI archers never run dry,
spent arrows stay litter, resupply is walk-over, and ACF supplies the inventory store only.
GDD v1.2, §12.4 IN column.

**What is unproven:** whether finite arrows are any good to play. A ruling ticket cannot settle that.
The mechanical claim underneath it *was* checked at runtime before anything was written - arrows are
genuinely unlimited today, with `CostGameplayEffectClass` and `CooldownGameplayEffectClass` reading
None on both ability CDOs and on the `GA_GS_TorchToss` Blueprint - so the premise is sound even if
the design turns out wrong.

**The specific thing to judge it on**, and the reason ruling 52 exists: an arrow that sticks in an
allied goblin is a wasted shot (2026-08-06, not re-opened). That cost a miss when arrows were free.
With a quiver it costs a consumable, in a game that deliberately puts your own horde between you and
your target. If finite arrows feel unfair, this is almost certainly why - and the levers to fix it
are the starting count, bundle density and drop rate, all data. **Changing what happens when an arrow
hits an ally would be a new ruling, not a quiet edit inside an ammo ticket.**

## #283 closed UNOBSERVED, 2026-08-24 — the wheel goes to ACF, Uriel goes to the demo

Rulings 53-55. The weapon wheel migrates onto ACF equipment (answering #274's open question);
weapons therefore become items and lootable in principle, though whether corpses actually surrender
them is NOT decided; and **Uriel A Plotexia is demo scope, not prototype scope**, which supersedes
the 2026-08-23 note recording his armour as "FUTURE" without saying which future. `BP_UrielAPlotexia`
staying Militia with an arming sword is now correct and is not a defect to fix in passing.

**What is unproven, and it is the whole risk of ruling 53:** that the wheel can move onto ACF without
changing how weapons FEEL. #270 already proved ACF can equip and attach. What it has never had to
preserve is the set of hand-tuned rules `UGSWeaponComponent` carries that ACF has no equivalent for -
the holstered melee weapon hidden for the whole time the bow is out (Michael rejected the aim-only
version by name), the quiver that never moves and is never hidden, the Torch slot outranking an
ability asking to un-ready, and the 0.15s anti-cancel swap lock.

If the migration goes wrong, it will not go wrong by failing to equip. It will go wrong by equipping
perfectly while the sword sits in the wrong place, or reappears mid-bow, or the swap starts feeling
mushy - none of which a compile or a read-back can catch. **Watch the weapons, not the log.**

## #285 closed UNOBSERVED, 2026-08-24 — sneaking is cut from the demo

Rulings 56-58, GDD v1.3. The crouch-and-confirm stealth core, noise, takedowns, corpse-suspicion and
the coin toss all leave the slice. **This removed an item from §12.4's "Never cut" line - the only
time anything has ever come off it** - and the line now carries a blockquote saying so in place.
The bucket brigade stays deferred. The demo is a straight raid: horn, horde, burn, bank, extract.

**What is unproven:** whether cutting sneaking is right for the GAME. It was taken as a demo scope
decision with the costs on the table - the stealth core is the only Never-cut item that had never run
on a single actor (§12.1 row 7: the perception component sits on zero actors), so the sunk cost was
two systems that never executed and the saving was three that do not exist.

**Nothing is deleted.** `Stealth/`, the perception component and `BTTask_Firefight` all stay in the
tree. No ticket should remove them. Whether stealth returns for the full game is explicitly not
decided.

**Also corrected here:** #283 claimed the bucket brigade needed no ruling, which was false - §12.4's
consequences paragraph deferred it along with the well. The lesson is written into that ticket: "is
this in scope?" is not answered by the IN column alone, because the prose removes things the table
never mentions.

## #288 closed UNOBSERVED, 2026-08-24 — the primary player experience is the bar

Ruling 59: a defect is judged by what the primary player experiences. If it does not reach the person
holding the controller it is not urgent, whatever it looks like in an outliner, a log or a details
panel. **A triage rule, not a quality rule** - it decides what gets fixed now, it does not license
shipping things that are wrong.

**Recorded below the line and NOT to be fixed in passing:** the scout carries a visible second axe on
his back while the primary weapon is drawn (#287). The cause was deliberately not investigated.

**What is unproven:** whether this is the right bar for the whole project rather than for a demo. A
rule that is right while cutting toward a demo can be wrong for a shipping game, where an outliner
full of stray actors becomes a performance problem rather than a cosmetic one. Revisiting 59 is
allowed; revisiting it quietly is not.

## #294 closed UNOBSERVED, 2026-08-24 — Uriel is replaced by a Knight

Rulings 60 and 61, superseding 55. Uriel is not a bespoke character any more: `BP_KnightDPelegrini`
already exists, is armoured, is on ACF and has been seen in play, and ruling 58's straight raid has no
boss in it. `BP_UrielAPlotexia` is **deleted** - placed in no level, referenced by no asset, spawned by
no code, and carrying no `CharacterInitDataAsset`, so his ACF stats would never have initialised.

**What is unproven:** nothing about behaviour, because nothing referenced him - there was no runtime
change to watch. What could still be wrong is the DECISION, not the deletion: if the demo later wants a
distinct final opponent, ruling 60 says use a Knight, and somebody may find that a Knight reads as
"another guard" rather than as an ending. That is a design judgement nobody has tested, and the asset
is recoverable from git.

**One thing worth carrying forward:** `EditorAssetSubsystem.delete_asset` AND `delete_loaded_asset`
both returned **True** while leaving the file on disk. Two APIs, two confident lies. Any future ticket
deleting an asset should check the file system, not the return value.

## #293 closed UNOBSERVED, 2026-08-24 — weapon placement has a loop now, and the wrong bow

The weapon data asset is now the single source of truth for placement on BOTH paths. Until #293 the
offset existed twice - in `UGSWeaponDataAsset`, and hand-copied onto each `BP_ACFWeapon_*` - so editing
the data asset moved our mesh and did **nothing** to the weapon ACF was holding. `GS.Weapon.Dump` /
`Set` / `Reapply` make it a change-look-change loop without leaving PIE.

**What is unproven:** whether the loop is usable by the person it was built for. It was driven end to
end from script and the round trip is real (Set moved the weapon, Reapply pulled the asset's value
back), but nobody has tuned anything by eye with it.

**GS_Bow_Only IS THE WRONG BOW MESH.** Michael, 2026-08-24: *"you're using the wrong bow. Stop playing
with Erika and the bow."* Every stage of the ACF migration carried that asset forward faithfully and
every transcription was correct - #289 onto the player, #292 onto Erika - and none of it mattered,
because the asset itself is wrong. **Do not tune Erika's bow and do not assume `GS_Bow_Only` is the
intended mesh for anybody.** Which bow is right is a question for Michael, not an investigation: three
sessions of measuring agreeing with itself is exactly how this stayed invisible.

## #295 closed UNOBSERVED, 2026-08-24 — stage 5 retires nothing, deliberately

The ACF migration's final stage was to delete `UGSWeaponComponent`'s dead mesh path. **It removed no
code, and that is the finding.** `BP_PeasantMan` (the bucket) and `BP_GS_TargetDummy` are still on the
legacy path, and the quiver, torch and horn keep the rest of it alive. `RangedMeshComponent` is the
only component with no live user, and deleting it would turn `bUseACFEquipment` into a switch with one
position - the per-character rollback that made every stage independently revertable, and that proved
Erika's bow was not a migration regression by running her on both paths and comparing.

**Re-open it when** `BP_PeasantMan` and `BP_GS_TargetDummy` are on ACF and the bow question is settled.
Both `MeleeMeshComponent` and `RangedMeshComponent` are genuinely unreachable at that point.

**What is unproven:** that keeping it is right. This is a judgement that dead code is cheaper than a
lost rollback, made while cutting toward a demo. If the legacy path starts causing confusion rather
than just sitting there - two ways to place a weapon, one live - that trade has flipped and the ticket
should be re-opened rather than argued with.

**Also unproven and worth naming:** `GetActiveWeaponMesh()` is `BlueprintPure` with no C++ callers and
returns null for an ACF-held weapon. If a Blueprint melee trace hangs off it, that has been silently
degrading since #287 and nothing would log it. Nobody has checked.

## Accepted below the line under ruling 59 — running list

Ruling 59 says below-the-line items are **recorded, not ignored**. This is that record. None of these
should be "fixed in passing"; each becomes work only if it reaches the primary player.

- **The scout carries a second axe on his back while the primary is drawn** (#287). Cause not
  investigated.
- **The torch's grip angle is slightly off in the right hand** (#290). Michael's diagnosis, and it is
  the useful part: it comes from **how the hand is posed by the animation**, not from the socket. If
  so, `HeldTorchMeshOffset` cannot fully fix it — a constant rotation on a bone whose own rotation
  changes per animation is right in one pose and wrong in the next. The real fix is a torch grip pose
  or an anim overlay. **Recorded so nobody spends an afternoon hunting a number that may not exist.**
