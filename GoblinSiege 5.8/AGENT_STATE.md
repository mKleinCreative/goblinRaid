# AGENT_STATE — Goblin Siege Code Architect

*Single-file agent memory (Michael's ruling 2026-08-04: one distilled state file, not 69 docs).
Human-editable; the agent reads it at run start and rewrites NEXT + appends BUILT/RUNS at run
end. Deep context lives in the Claude project docs; this is the distillation. Seeded 2026-08-04
from the status-and-rebaseline doc, the decision queue, and a live scan.*

> ## ⚠ READ `HANDOFF.md` BEFORE PICKING UP RANGED / TORCH / BUILDING WORK
>
> `GoblinSiege 5.8/HANDOFF.md`, written 2026-08-06. It holds **nine outstanding code-review
> findings that nobody has fixed** — including a `TypeError` that kills `gs_buildings.py` on the
> first non-mesh actor, a hole that lets `set -Status done` bypass every close check in
> `gsqueue.ps1` (ticket 028 already slipped through it), and an adopt-radius double-count that can
> make a building objective unwinnable. It also carries the ranged/torch state, the settled radial
> weapon-wheel design, and the gotchas that cost this session the most time.
>
> Those findings live in closed tickets otherwise, and **nothing reads closed tickets at run start.**

**Coordination lives in `AgentQueue/QUEUE.md`, not here.** Before editing any file, claim it:
`& ".\AgentQueue\gsqueue.ps1" claim -Agent <slug> -Title "<t>" -Files "a,b"`. Lower ticket number
has right of way on a shared file; nobody compiles until `gsqueue.ps1 buildgate` exits 0. Tickets
are committed and keep their Generate/Evaluate/Refine as the review record, but **nothing reads
old tickets at run start** — fold anything durable into BUILT / DECISIONS / FAILED below, or the
next agent rediscovers it.

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

## NEXT

*Refreshed 2026-08-06. Every `missing:` symbol below was re-checked against the tree that day; an
item whose symbols all now exist was removed rather than left to rot. Three were: **interact
framework** (u=10.0 — all four components exist in `Interaction/` and `Weapons/Abilities/`),
**lives / respawn** (`AGSPlayerState` exists, `EGSRaidResult::OutOfLives` ends the raid, #009), and
**runic site** (`AGSRunicSite` + `BP_GS_RunicSite` exist, #009/#011). The NEXT list had carried all
three as outstanding for two days while they were being built.*

**Ranked — u carried from run live-003, 2026-08-04. Not re-scored; treat the order as two days old.**

- [EDITOR] u=6.0 **Death & hit-reaction clips retargeted ('nothing can die on screen')** (block B) — missing: AM_GS_Death
- [ELIGIBLE] u=5.75 **Someone to fight — the last piece** (block B) — DA_Race_Human and BT_Militia now EXIST and are PIE-verified; missing: DA_Weapon_Greatclub only
- [BLOCKED] u=4.0 **Score system — deeds/loot two-kind tally + end screen** (block G) — missing: UGSScoreSubsystem, GSScore
- [BLOCKED] u=3.2 **Horn & horde (subsystem, pool, BT, point command)** (block D) — AGSHordeGoblin EXISTS; missing: UGSHordeSubsystem, AGSHordeSpawnMarker, UGSGA_Horn, BT_HordeGoblin
- [BLOCKED] u=3.0 **The stealth five (noise, crouch-detect, takedown, corpse-suspicion, coin toss)** (block F) — Takedown EXISTS (interact framework); missing: ReportGSNoise, CoinToss, DT_NoiseEvents
- [BLOCKED] u=2.5 **Gore/gib system (intensity scalar, feather-poof)** (block G) — missing: UGSGibComponent
- [ELIGIBLE] u=2.5 **Barks + Overlord whispers (runtime side)** (block H) — missing: UGSBarkSubsystem, DT_Barks
- [BLOCKED] u=2.33 **Patrol director — 5-7 min cadence + castle reinforcements** (block F) — missing: UGSPatrolDirector
- [ELIGIBLE] u=2.25 **Loot couriers — sacks + livestock cargo, point-to-courier** (block G) — was BLOCKED on UGSCarryComponent, which now EXISTS; nothing else gates it
- [BLOCKED] u=1.75 **Civilians + livestock (routines, disbelief, brigade, flee)** (block G) — missing: BT_Civilian, DA_Race_Livestock

**Unranked — raised after the live-003 ranking run, so they carry no u.** Do not read the order
below as priority; it is grouped by kind. The first item is the only one anyone has called urgent.

- [ELIGIBLE] **The nine outstanding code-review findings** — `HANDOFF.md` Part 1, full file:line. One is a **crash**: `gs_buildings.py:110` raises `TypeError` on the first actor with no static mesh, which is a PlayerStart or a light on every level. Two more are silent-failure holes in `gsqueue.ps1` itself (`set -Status done` bypasses every close check; the #024 stale-Evaluate gate resolves paths against the wrong root and fails open). Nobody has picked these up; both original authors closed their tickets.
- [ELIGIBLE] **Radial weapon wheel** — `HANDOFF.md` Part 3. Michael's design, two decisions already settled and **not to be re-litigated**: Q opens a hold-drag-release wheel (top torch / bottom-left sword / bottom-right bow), and the torch becomes a real held weapon fired by the ATTACK button, retiring `IA_ThrowTorch`. Open: who builds the UMG. Note `bRangedMode` is a **bool**, so three slots is a type change (`EGSWeaponSlot`) touching every `IsInRangedMode()` caller. Subsumes the two torch items below.
- [ELIGIBLE] **Torch throw has no animation and is barely visible in hand** — `AM_GS_ThrowTorch.uasset` EXISTS and is referenced by **nothing** in C++; `UGSGA_TorchToss` has no `PlayMontage`, the throw is a 0.25s timer. The held torch is un-readied by `EndAbility` the instant the projectile spawns, which is Michael's "it's never in your hand" — not a missing socket or mesh, both verified fine.
- [ELIGIBLE] **Arrows ignore RaceTag** — zero occurrences of `RaceTag` in `GSArrowProjectile.cpp`. An arrow will hit allied goblins. Melee friendly-fire was added by a later pass and ranged was never brought in line.
- [EDITOR] **Building burn duration** — Michael, 2026-08-06, after the first raid that worked end to end: *"burning buildings. Maybe it can take a little longer."* The knob is `UGSFlammableComponent::BurnDurationSeconds` (currently 12s). Data, no rebuild.
- [ELIGIBLE] **Melee `AttackCooldownSeconds` is dead** — the twin of the bow bug closed in #034. `UGSWeaponDataAsset::AttackCooldownSeconds` (1.0s default) has **no reader anywhere**. Check first whether melee is already rate-limited by its montage before adding a second gate on top.
- [ELIGIBLE] **The two lose paths have never been exercised** — `EGSRaidResult::LeftBehind` and `OutOfLives` are wired and neither has run; only `Extracted` is verified (#009). `GS.Raid.ExpireClock`, `GS.Raid.Kill` and `GS.Raid.SetLives` exist to drive them (#018).
- [EDITOR] **`BP_GS_Arrow` subclass** — `AGSArrowProjectile::ArrowMeshOffset` is `EditDefaultsOnly` on the C++ class that is also the class spawned, so with no Blueprint subclass there is no CDO to edit and it is a rebuild-to-change knob. Only worth creating if the arrow is seen flying wrong (#034).

## FAILED

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
  Note `Content/*` is gitignored (`GoblinSiege 5.8/.gitignore:32`), so these assets have **no git
  history** - the backup folder is the only way back.
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
