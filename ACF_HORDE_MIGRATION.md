# Moving the horde onto ACF

**Decision (Michael, 2026-08-12): full adoption.** The horde reparents onto the ACF hierarchy and
orders go through ACF's command system. Ticket #141's hand-rolled order board is parked.

**Scope widened (Michael, 2026-08-12): the defenders come too.** His reasoning: *"Defenders are
eventually going to need to do patrols, I believe it's worth having them follow."* That is the right
call and it makes the migration cheaper, not dearer — see *Why the defenders coming along helps*.

So: **both** the summoned allied goblins (`AGSHordeGoblin`) and the human defenders
(`AGSEnemyCharacter`) end up on the ACF hierarchy. One AI stack, not two.

---

## What ACF gives us the moment we adopt it

- **Commands as gameplay tags**, already registered in `Content/Configuration/CommandTags`:
  `AICommand.AttackMyTarget`, `ComeHere`, `FollowMe`, `StayThere`, `GoThere`, `FollowSpline`.
- **`UACFCommandsManagerComponent`** — a per-AI queue of instanced `UACFBaseCommand` objects with
  `OnCommandFinished(bool)`, `TriggerCommand`, `ExecutePendingCommand`, `TerminateCurrentCommand`,
  `HasPendingCommands`. Server-authoritative with validation.
- **`UACFGroupAIComponent::SendCommandToCompanions(FGameplayTag)`** — the group fan-out, Server Reliable.
- **`UACFCompanionGroupAIComponent`** — roster, `groupLead`, `MaxDistanceFromLead`, despawn-when-far,
  centroid, reacts to the lead being hit. This is our follow behaviour and most of our pool bookkeeping.
- **BT command branch already wired into `ACFBT`**: `BTTask_ExecuteCommand`, `BTTask_ResetCommand`,
  `BTDecorator_HasPendingCommands`.
- **Ready-made command Blueprints**: `ACFComeHereCommandBP`, `ACFGoThereCommandBP`,
  `ACFAttackMyTargetCommandBP`, and `ACF_SendCommandAction_BP` to issue them.
- The blackboard we already inherit (`ACFAIBB`) stops being half-dead: `GroupLeader`,
  `GroupLeadDistance`, `CommandDuration`, `HomeDistance` get real writers.

## Why the defenders coming along helps

Bringing them in is not extra work bolted on — it removes the plan's worst risk and deletes a
scheduled feature.

**It gives us patrols and routines outright.** `AIFramework` ships:

| ACF | What it covers for us |
|---|---|
| `UACFAIPatrolComponent`, `ACFUpdatePatrolBTService`, `BTTask_GoToNextWayPoint` | Waypoint patrols — the guard rounds GDD §2.6 wants |
| `UACFSplineFollowerComponent`, `UACFPatrolSplinePathTask`, `ACFFollowSplineCommandBP` | Patrol routes as splines rather than hand-placed points |
| `UACFAIRoutineComponent`, `ACFCheckRoutineBTService`, `ACFNPCRoutineControllerBP` | **Daily routines** — "6–10 civilians on daily routines" (§2.6), and the "hamlet gone conspicuously quiet" soft signal has something real to be measured against |
| `UACFInteractSmartObjectsTask` (+ `SmartObjectsModule`) | Civilians actually using props — field work, pen chores, the bucket line |
| `UACFAIManagerSubsystem::PauseNonBattleAIs(Reason)` | Freezes non-combat AI via `UBrainComponent::PauseLogic`. With a hamlet full of routines this is the difference between a village and a frame-rate problem |
| `AACFAIGroupSpawner`, `UACFAIWavesMasterComponent` (`FWaveConfig`) | The barracks and the Highpurse Keep reinforcement waves (§2.6) |

**It deletes work already on the board.** `AGENT_STATE.md`'s NEXT list carries *"Patrol director (5–7
min cadence + castle reinforcements) — missing `UGSPatrolDirector`"* as `[BLOCKED]`, ranked 2.33. That
item does not need building; it needs configuring. The civilians entry (`BT_Civilian`,
`DA_Race_Livestock`, ranked 1.75) is substantially covered by the routine component too.

**It kills the two-stacks risk.** With the defenders on ACF as well there is one AI hierarchy, one
blackboard schema, one command vocabulary — which is the whole point of the ruling that prompted this.

**And it makes Phase 1 pay for itself twice.** `AGSHordeAIController` and the defender controllers
*both* derive from `AGSAIControllerBase`, so whatever we do with `TickFacing` / `TickSeparation`
serves both classes. Solving it once is now mandatory rather than optional, and it benefits everything.

**What does NOT come for free:** the check-in / overdue-patrol logic (§2.6's "a patrol that's overdue
and hasn't checked in") is ours — ACF patrols walk waypoints, they do not report to a stealth
director. The alarm-phase coupling stays `UGSStealthSubsystem`'s job.

## What stays ours — ACF has none of it

- The **radial order wheel** (`UGSHordeCommandComponent`) and **`WBP_HordeOrderWheel`**.
- The **world-space order marker** (`AGSHordeOrderMarker`) — the thing you actually asked for.
- The **courier verb**: `BTTask_PickUpCargo`, `BTTask_DeliverCargo`, `UGSCarryComponent`, and the
  pool credit-back through `NotifyCourierDelivered`.
- **Formations / ring slots.** ACF has zero formation code (`grep Formation` → nothing). Our
  `MenaceOrbit` + engagement-token work is a genuine addition, not duplication.
- The **horn, the finite raid pool of 20, and the arrival markers** — ACF spawns groups, it has no
  concept of a shared depleting pool.

## What dies

`GSHordeOrderTypes.h` · `ActiveOrders` + `IssueOrder`/`ClearOrder`/the four `Get*For` accessors on
`UGSHordeSubsystem` · `EGSHordeState::Commanded` as a hand-rolled state · `GetFollowTargetFor` /
`GetFollowSlotFor` (superseded by `groupLead` / `GroupLeadDistance`) · probably
`BTTask_SmashOrderTarget`, if a `UACFBaseCommand` subclass can carry it.

---

## The migration, in verifiable phases

Each phase ends somewhere you can watch something and say yes or no. Do not start the next until the
current one is watched.

### Phase 0 — make the module reachable (no behaviour change)

- Add `AIFramework` (and whatever it transitively demands — expect `AscentCombatFramework`,
  `ActionsSystem`, `AscentGASRuntime`, `AscentTargetingSystem`, `AscentTeams`, `InventorySystem`,
  `AdvancedRPGSystem`, `CharacterController`) to `GoblinSiege.Build.cs`.
- Add `AscentCombatFramework` explicitly to `MyProject.uproject`'s `Plugins` array — today it loads
  only because project plugins default on, which is fragile for a dependency we are about to rely on.
- **Watch:** the editor still opens and a horn blast still summons four goblins. Nothing else changes.
- **Risk if this fails:** module cycles or a link error. Cheap to discover, and it is the whole cost
  of finding out before we commit.

### Phase 1 — the shared controller base moves

Do **not** reparent any pawn yet. Reparent only the brain, and do it at the base:

- **`AGSAIControllerBase : AACFAIController`** (today: `AAIController`).
- Reparenting the *base* rather than `AGSHordeAIController` alone is what carries the defenders with
  us — AGENT_STATE records that defenders possess `AGSAIControllerBase` directly, so one edit moves
  both populations. It is also why this cannot be staged one class at a time.
- Keep `BT_HordeGoblin` / `BB_HordeGoblin` and the defender trees as they are for now.
  `AACFAIController` expects `ACFAIBB`'s keys and we already inherit them.
- The hard part: `AGSAIControllerBase` owns `TickFacing` (the single facing authority, signed off
  2026-08-11 and explicitly not to be re-opened) and `TickSeparation`. Those must survive the change
  of base. Hoist them into a `UActorComponent` both can carry rather than duplicating them —
  **two facing authorities is the exact failure #108 is named after.**
- `AACFAIController` brings its own `UACFCommandsManagerComponent`, `UACFCombatBehaviourComponent`,
  `UATSAITargetComponent` and `UACFThreatManagerComponent`. Expect overlap with
  `UGSEngagementComponent` and our threat registry; do not wire the ACF ones up yet, just let them
  exist.
- **Watch, on both populations separately:** summoned goblins still face their target and keep out of
  each other's capsules (`GS.Horde.SpawnTest` in `L_CombatArena` — `GS.Combat.Duel` provably cannot
  see this class of bug, #135), *and* a militia patrol still fights normally.

### Phase 2 — the pawns join the ACF hierarchy

**This is the expensive step and it has a fork in it that needs deciding before any code moves:**

- **Option A — move the base.** `AGSCharacterBase : AACFCharacter`. Everything follows in one edit:
  horde, defenders **and the player**. This is how ACF is designed to be used — its player is an
  `AACFCharacter` under an ACF player controller, and it is what makes `AACFCompanionsPlayerController`,
  the command abilities and the shared damage model line up without adapters.
- **Option B — move the two AI classes only.** `AGSEnemyCharacter` and `AGSHordeGoblin` each reparent
  to `AACFCharacter`, and the player stays on `AGSCharacterBase`. Smaller blast radius, but the combat
  verbs (`TryLightAttack`, `IsAlive`, `IsHostileTo`, `RaceTag`, `IsRecoiling`, the engagement hooks)
  live on `AGSCharacterBase` and are called by BT tasks and by `UGSHordeSubsystem` — so they would have
  to be duplicated or hoisted, and the player would be on a different damage path from everything he
  fights.

Whichever is chosen:

- Audit the five class-identity checks AGENT_STATE flags: `GSFireVolume`'s `FriendlyFireScalar`,
  `GSTargetingComponent`'s soft-lock candidates, `GSBuffAuraComponent`'s aura targets, and two more.
  The standing ruling was "do not reparent `AGSHordeGoblin` to `AGSEnemyCharacter`"; this is a
  different move with the same class of hazard, and it needs the audit rather than an assumption.
- Decide the attribute story: ACF ships `AdvancedRPGSystem`; we run stock GAS with
  `UGSAttributeSetBase` and `DA_Race_Goblin`. **Two health pools is the failure mode to avoid.**
- **Watch:** a goblin takes damage, dies once, and the pool logs `Reserve N (unchanged)` as today.
  A militiaman still blocks, and a blocked swing still recoils. Fire still kills our own horde.

### Phase 2b — patrols and routines (defenders only, once Phase 2 lands)

The payoff for bringing them along. Add `UACFAIPatrolComponent` to the guard archetypes, author
waypoints or splines, and hang `UACFAIRoutineComponent` on the civilians. Retire the
`UGSPatrolDirector` entry from AGENT_STATE's NEXT list rather than building it.

**Watch:** a guard walks a round unprompted, breaks off to investigate, and returns to it — with no
`UGSPatrolDirector` in the codebase.

### Phase 3 — orders become ACF commands

- Player controller gains a `UACFCompanionGroupAIComponent` (ACF's own
  `AACFCompanionsPlayerController` is the reference; we may not want to reparent our PC, in which
  case we add the component ourselves).
- `UGSHordeSubsystem::SummonWave` registers each spawned goblin into the group component
  (`AddExistingCharacterToGroup`) instead of only its own `ActiveGoblins` map. The **pool stays ours** —
  ACF has no depleting reserve — but the roster becomes ACF's.
- The order wheel stops calling `IssueOrder` and starts calling `SendCommandToCompanions(FGameplayTag)`:
  - Attack → `AICommand.AttackMyTarget`
  - Hold → `AICommand.StayThere`
  - Follow → `AICommand.FollowMe`
  - Loot → **a new `UACFBaseCommand` subclass of ours**, `UGSCourierCommand`, wrapping the two cargo
    BT tasks. This is how the courier verb survives adoption: as an ACF command, not beside one.
- `AGSHordeOrderMarker` is spawned by whatever issues the command — it is presentation, so it does
  not care which system underneath.
- **Watch:** the wheel issues a real `AICommand` tag; `BTDecorator_HasPendingCommands` passes;
  `BTTask_ExecuteCommand` runs it; the warband converges. And the beacon appears where you pointed.

### Phase 4 — reconcile the trees

Once commands run, `BT_HordeGoblin` and `ACFBT` overlap. Either graft ACF's command branch into ours,
or move to `ACFBT` and graft our `MenaceOrbit` / attack-token / block work into it. **Do not decide
this in the abstract** — decide it once Phase 3 is watched and it is obvious which tree is carrying
more of the behaviour we care about.

---

## Risks, stated up front

1. ~~**Two combat stacks.**~~ **Retired 2026-08-12** — the defenders are coming too, so there is one
   hierarchy. What replaces it as a scheduling risk is *ordering*: the horde and the defenders share
   `AGSAIControllerBase`, so Phase 1 cannot be done for one and not the other. Plan for both classes
   to move in the same window rather than discovering the coupling halfway through.
2. **The facing authority.** `AGSAIControllerBase::TickFacing` is signed-off, watched work. Losing it
   in Phase 1 would be invisible in a duel and obvious in a raid. Named here so it cannot be dropped
   quietly.
3. **Attributes.** ACF's RPG stats vs our GAS attribute set. Pick one owner per concept before Phase 2
   rather than discovering a second health bar at runtime.
4. **Dependency weight.** `AIFramework` pulls most of ACF in. That is the price of the reuse and it is
   probably the right price — but it should be a decision, not a surprise at link time.
5. **The pool is ours and must stay ours.** `SummonWave`, the reserve of 20, debit-on-spawn,
   credit-on-delivery, `OnPoolDry` — ACF has no equivalent. Do not let group spawning quietly replace it.

## What is already done and should not be reverted

`BB_HordeGoblin` now inherits `ACFAIBB` (16 keys visible) and `BT_HordeGoblin` points at it; two
selectors that had silently rebound to `SelfActor` were repointed to `FollowTarget` and
`TargetIsAttacking`. That was the first real step toward ACF and it stands.

`IA_HordeOrder` on R, `WBP_HordeOrderWheel`, `BP_HordeOrderMarker` + `M_HordeOrderMarker`, and the
class assignments on `BP_GSPlayerCharacter` all survive the migration untouched.
