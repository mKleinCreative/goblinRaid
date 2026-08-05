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
- 2026-08-04 `UGameplayAbility::AbilityTags` is deprecated in 5.8 (C4996) — used by `GSGA_Block` and
  `GSGA_Interact`. Compiles today, will NOT compile after the next engine upgrade. Migrate both to
  `SetAssetTags()` in the constructor when that comes.

## RUNS
- 2026-08-04 **mvp-001** — picked 'Interact framework — hold-E channels + carry' (u=10.0); ok
- 2026-08-03 **live-001** — scan-only: 13 open gaps
- 2026-08-04 **live-002** — picked 'Interact framework — hold-E channels + carry' (u=10.0); FAILED
- 2026-08-04 **live-002** — picked 'Interact framework — hold-E channels + carry' (u=10.0); FAILED
- 2026-08-04 **live-003** — picked 'Interact framework — hold-E channels + carry' (u=10.0); ok
