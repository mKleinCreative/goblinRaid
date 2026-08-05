# AGENT_STATE — Goblin Siege Code Architect

*Single-file agent memory (Michael's ruling 2026-08-04: one distilled state file, not 69 docs).
Human-editable; the agent reads it at run start and rewrites NEXT + appends BUILT/RUNS at run
end. Deep context lives in the Claude project docs; this is the distillation. Seeded 2026-08-04
from the status-and-rebaseline doc, the decision queue, and a live scan.*

## BUILT
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

## DECISIONS

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
- 2026-08-04 (three further rulings):
  - **No interacting or blocking while staggered.** Block already refused `State.GuardBroken`;
    interact now does too, and `BreakGuard` cancels an in-flight channel by tag so the kick stops a
    loot already in progress rather than only refusing the next one.
  - **Full hands cannot throw a torch** — `UGSGA_TorchToss` blocks on `State.Carrying`. Drop the sack
    first.
  - **Extraction is an auto-bank circle for now** (GDD §9), NOT a hold-E verb — resolves the §9 vs
    §12.1 contradiction. `Interact.Extract` stays declared but unused, reserved for if it ever
    becomes channelled.

## NEXT
- [ELIGIBLE] u=10.0 **Interact framework — hold-E channels + carry** (block A) — missing: UGSInteractableComponent, UGSInteractionComponent, UGSGA_Interact, UGSCarryComponent
- [EDITOR] u=6.0 **Death & hit-reaction clips retargeted ('nothing can die on screen')** (block B) — missing: AM_GS_Death
- [ELIGIBLE] u=5.75 **Someone to fight — human race data, BT_Militia, enemy attack path** (block B) — missing: DA_Race_Human, BT_Militia, DA_Weapon_Greatclub
- [ELIGIBLE] u=4.0 **Lives / respawn on PlayerState** (block E) — missing: Lives, AGSPlayerState
- [BLOCKED] u=4.0 **Runic site — spawn/respawn, objective-gated portal, staging, 90s collapse** (block E) — missing: AGSRunicSite, Portal
- [BLOCKED] u=4.0 **Score system — deeds/loot two-kind tally + end screen** (block G) — missing: UGSScoreSubsystem, GSScore
- [BLOCKED] u=3.2 **Horn & horde (subsystem, pool, BT, point command)** (block D) — missing: UGSHordeSubsystem, AGSHordeSpawnMarker, UGSGA_Horn, BT_HordeGoblin, AGSHordeGoblin
- [BLOCKED] u=3.0 **The stealth five (noise, crouch-detect, takedown, corpse-suspicion, coin toss)** (block F) — missing: ReportGSNoise, Takedown, CoinToss, DT_NoiseEvents
- [BLOCKED] u=2.5 **Gore/gib system (intensity scalar, feather-poof)** (block G) — missing: UGSGibComponent
- [ELIGIBLE] u=2.5 **Barks + Overlord whispers (runtime side)** (block H) — missing: UGSBarkSubsystem, DT_Barks
- [BLOCKED] u=2.33 **Patrol director — 5-7 min cadence + castle reinforcements** (block F) — missing: UGSPatrolDirector
- [BLOCKED] u=2.25 **Loot couriers — sacks + livestock cargo, point-to-courier** (block G) — missing: UGSCarryComponent
- [BLOCKED] u=1.75 **Civilians + livestock (routines, disbelief, brigade, flee)** (block G) — missing: BT_Civilian, DA_Race_Livestock

*(ranking from run live-003, 2026-08-04)*

## FAILED
- pre-seed (from build log / decision queue — do not rediscover): Live Coding cannot register new UCLASS/UPROPERTY — full editor-closed build required. In-editor Compile gives zero feedback; stranded-UBT bug = `Launching UnrealBuildTool...` with no `HotReload took` → kill orphaned dotnet. `.bat` written from a Linux sandbox needs CRLF. PowerShell over the MCP bridge: no `$`, quote every path, `--%` for native args. VibeUE `list_expressions` inlines nested material functions — connecting to an inlined node writes an illegal cross-package ref that blocks saving. `NS_GS_SmokeColumn` must use `M_Smoke_01`, never `M_Smoke_02` (broken). Content/GoblinSiege + DreamscapeSeries paths are untracked in git — agent-side edits there have no backup.
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
