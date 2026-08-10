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
- pre-seed (from build log / decision queue — do not rediscover): Live Coding cannot register new UCLASS/UPROPERTY — full editor-closed build required. In-editor Compile gives zero feedback; stranded-UBT bug = `Launching UnrealBuildTool...` with no `HotReload took` → kill orphaned dotnet. `.bat` written from a Linux sandbox needs CRLF. PowerShell over the MCP bridge: no `$`, quote every path, `--%` for native args. VibeUE `list_expressions` inlines nested material functions — connecting to an inlined node writes an illegal cross-package ref that blocks saving. `NS_GS_SmokeColumn` must use `M_Smoke_01`, never `M_Smoke_02` (broken). Content/GoblinSiege + DreamscapeSeries paths are untracked in git — agent-side edits **there** have no backup (the rest of `Content/` IS tracked via LFS — see the corrected note above; 465 files). **Build times: the UnrealBuildAccelerator permission problem is FIXED as of 2026-08-07** — `Build-GoblinSiege.ps1` now reports "UBA cache is writable - parallel compilation available" and a full editor-closed build of the GoblinSiege module ran **4:45 with 6 actions across 12 physical cores** (#069). Anywhere this file or `CLAUDE.md` still says to expect ~6-minute near-serial builds because UBA is crippled on `C:\ProgramData\Epic\UnrealBuildAccelerator`, that is stale.
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
