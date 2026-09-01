---
id: 382
title: Loot order: goblins must break crates open and actually carry loot, not just walk to it
agent: claude-loot2
status: done
claimed: 2026-08-30T15:34Z
build: none
waiting_on:
evaluated: 2026-08-30T20:40:58Z
observed: 2026-08-30T20:40:22Z | Fresh PIE session, pristine unbroken barrel and crate. Issued GS.Horde.Order Loot at the barrel. Log showed [GS.Smash] BP_HordeGoblin_C_0 smashed BP_LootBarrel_C_0 open, then [GS.Loot] BP_HordeGoblin_C_0 looted BP_LootBarrel_C_0 in place for 15 - both Log-level success lines, first time either has ever fired this entire session. A second goblin independently smashed and looted BP_LootCrate_C_0 for 15 in the same window (area-forage correctly split them across two targets). Earlier in the same session, GS.Horde.Order Loot on a pristine BP_LootSack_C_1 produced BP_HordeGoblin_C_1 picked up BP_LootSack_C_1, then LogGSLootBank: the portal on BP_GS_RunicSite_C_1 swallowed BP_LootSack_C_1 for 20 loot - the full carry-and-deliver path also completing end to end.
scenario: Live PIE, freshly rebuilt binaries and freshly reloaded L_CombatArena map (not a stale long-running session), goblins spawned via GS.Horde.SpawnTest and ordered via GS.Horde.Order Loot at camera-traced targets - the real player-facing order path, not a synthetic BT-only test. Michael separately confirmed he would watch a run himself.
files: 
  - Source/GoblinSiege/Horde/GSHordeSubsystem.h
  - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
  - Source/GoblinSiege/Horde/GSHordeOrderTypes.h
  - Source/GoblinSiege/Interaction/GSInteractableComponent.h
  - Source/GoblinSiege/Interaction/GSInteractableComponent.cpp
  - Source/GoblinSiege/Raid/GSRaidLibrary.h
  - Source/GoblinSiege/Raid/GSRaidLibrary.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_LootInPlace.h
  - Source/GoblinSiege/AI/Tasks/BTTask_LootInPlace.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_PickUpCargo.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_SmashOrderTarget.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.h
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Source/GoblinSiege/Combat/GSGameplayTags.h
  - Source/GoblinSiege/Combat/GSGameplayTags.cpp
  - Content/AI/BT_HordeGoblin.uasset
  - Content/AI/BB_HordeGoblin.uasset
  - Content/Blueprints/Interactables/BP_FoodHeal.uasset
  - Content/Maps/Test/L_CombatArena.umap
---

## Goal

Loot order: goblins must break crates open and actually carry loot, not just walk to it

## Generate

Michael, live: "when pointing to loot at a crate, they need to attack the crate then interact and
steal what's inside. and loot whatever's nearby, which they're not right now." Investigated and
built the whole chain; found and fixed one real BT infrastructure failure along the way; then hit a
level-geometry blocker that stops the whole thing from being observably confirmed tonight.

**C++ built (all compiled, one clean build covering everything below):**
- `UBTTask_LootInPlace` (new) - the "interact and steal" half. Reads `OrderSubject`, range-checks,
  requires the target's `UGSInteractableComponent` to be non-carryable and `CanInteract`-eligible
  (i.e. already broken open), calls `NotifyChannelStarted`/`CompleteInteraction`, and credits
  `GetLootValue()` straight to `UGSScoreSubsystem::AddLoot` - no carry trip needed, matching "steal
  what's inside" rather than "shoulder the whole box home."
- `UBTTask_PickUpCargo` hardened: it never checked `IsCarryable()` before this - `StartCarry` itself
  has no such gate, so an ordered crate was getting shouldered whole and unbroken. Now fails fast on
  a non-carryable subject, which is what lets the tree's smash-then-loot branch have it instead.
- `UGSHordeSubsystem::FindNearestLootable`'s area search (from tonight's earlier `#379` pass) extended
  to also match an unbroken breakable container, not just `IsCarryable()` actors - aiming Loot near a
  cluster of containers now finds the nearest one instead of requiring a precise hit on bare ground.
- Failure-path logging added to `SmashOrderTarget`, `PickUpCargo`, and `LootInPlace` (`[GS.Smash]`,
  `[GS.PickUp]`, `[GS.Loot]`) - none of the three logged anything on failure before tonight, which is
  exactly why the first live test (see Evaluate) went completely silent and looked like "nothing
  happened" instead of showing a diagnosable reason.
- `AGSCharacterBase::Heal(float Amount)` - general-purpose flat heal via `SetNumericAttributeBase` on
  ARS's Health attribute (the #228 pattern `ApplyRespawnState`/`KillOutright` already use). Built for
  Michael's separate ask this session ("the food inside the arena should heal us"), not the loot work,
  but done in the same session/build.
- `Interact.Eat` gameplay tag added (`GSGameplayTags.h/.cpp`) - a sixth interaction verb, costing a
  tag not a class, per the interaction framework's own stated convention.

**Content built:** `BP_FoodHeal` (SM_Ham + `GSInteractableComponent`, `Interact.Eat`, 0.75s channel) -
`OnInteractionCompleted` bound event casts the Interactor to `AGSCharacterBase` and calls
`Heal(25)`, then destroys the actor so it disappears once eaten (Michael: "it just needs to
disappear after it gets consumed"). Placed in `L_CombatArena`, ground-snapped. Confirmed working
live by Michael (`[GS.Heal] ... healed 25.0 (75 -> 100 / 100)` in the log, and the actor is gone
after eating it).

**The `BT_HordeGoblin` incident.** Opening this asset's editor tab was found to zero out its
compiled runtime tree (root selector children 0) EVERY time, independent of any click or save -
confirmed repeatedly by reading `root_node` via script before/after opening the tab, with the tab
closed-without-saving recovering the real tree every single time. This is a worse version of the
exact failure `#153` documented and `#213` warned about ("opening the asset recompiles the graph
over the runtime tree"). The tree's `PickUpCargo`/`DeliverCargo` branch (added sometime before
tonight, presumably via an unsafe direct RootNode script edit rather than hand-authoring, given
`#213`'s explicit warning that this exact failure mode was the reason NOT to) had apparently never
gone through a real save-from-editor, so the visual EdGraph had nothing in it at all - a hand-add of
just the new Smash branch was not survivable; the whole tree needed re-entering from empty.

**Michael hand-rebuilt the full tree live, with me verifying each pass by reading `root_node`
directly via script** (not trusting the visual editor) - 5 verify/fix rounds caught real mistakes a
human clicking through dropdowns made blind: a stray default (`SelfActor` instead of
`TargetIsAttacking`; three separate `Cargo`/`FollowLocation`/`OrderSubject`-adjacent keys silently
defaulting to `TargetActor`), plus one GENUINE pre-existing Blackboard bug found along the way -
`OrderSubject` and `Cargo` (both Object-type keys) had **Base Class = `Object`** instead of `Actor`,
which is why `Move To`'s key picker silently omitted them from the dropdown even though they held
actors correctly at runtime the whole time. Fixed both keys' Base Class to `Actor` on `BB_HordeGoblin`
- a real, permanent, correct fix, not a workaround. Final tree verified byte-for-byte against spec
after save, close, and a hard reload from disk (not trusting the in-memory copy): all 7 root
branches, both Loot sub-sequences, every blackboard key, every decorator's abort mode - all correct.

## Evaluate

**The BT rebuild is solid - verified past the point of reasonable doubt.** Read via script
immediately after save, again after closing the tab, and again after a forced disk reload; identical
each time; nothing dirty. This is the strongest evidence any content edit got tonight.

**The loot mechanic itself is NOT confirmed working, but NOT because of a navmesh problem - that was
my own bad diagnosis and I retracted it.** Michael issued a live Loot order directly on
`BP_LootCrate_C_0` (log: `Order: Loot at V(-2390,-620,135.53) on BP_LootCrate_C_0`) and nothing
happened - the crate is still `is_broken=False`, `is_available=False` after the test. First check was
`NavigationSystemV1.find_path_to_location_synchronously` from the player start to each prop's exact
actor location, which came back `is_valid=False` for the crate, barrel, chest AND the statue - and I
wrote up a whole "disconnected navmesh island" theory (isolated pedestal platforms, a bigger problem
than the loot bug) and nearly filed it as its own ticket.

**That was testing the wrong point.** A prop's own actor location sits INSIDE its solid collision by
definition - you cannot path to a point inside a barrel, and a `False` there proves nothing about
whether the FLOOR AROUND the prop is reachable. Re-tested correctly using
`NavigationSystemV1.project_point_to_navigation` to find the nearest actual walkable point next to
each prop, then pathed to THAT: crate 96uu away and reachable, barrel 81uu and reachable, chest 100uu
and reachable, statue 200uu and reachable - every single one well inside the 180-250uu ranges
`SmashOrderTarget`/`PickUpCargo`/`LootInPlace` all use. Michael caught the flawed methodology
("I think we've established you should not include lootables inside the navmesh and that's the
issue") before I filed a ticket over nothing. **There is no navmesh problem. No `#384` needed.**

So the real explanation for the silent live test is more mundane: the goblins were mid-journey on an
earlier Loot-the-pig order when the crate order landed, and that order got cleared
(`Order cleared - the warband is back on Follow`) only 23 seconds later - probably not enough time to
switch targets, walk over, and execute smash+loot before Michael moved on. The new failure-path
logging (`[GS.Smash]`/`[GS.PickUp]`/`[GS.Loot]`, this same pass) should make the next attempt
unambiguous either way, whatever the actual outcome is.

## Generate (addendum, same morning - Shape B / area-forage)

Michael, after seeing single-target Loot work: "let's [do] the area-forage, it makes it more
interactive to see your group of goblins giggling as they smash and loot" - upgrading from the design
doc's Shape A (nearest-in-radius, one shared Subject for the whole warband) to Shape B (each goblin
finds and claims its own nearby item), which `#379`'s design doc had explicitly deferred as the
larger, riskier half needing a claim/reservation mechanism.

Built entirely inside `UGSHordeSubsystem::GetOrderSubjectFor` - **no BT changes needed**, deliberately,
given tonight's `BT_HordeGoblin` scare; every goblin already re-reads its `OrderSubject` blackboard key
every ~0.15-0.2s refresh, so per-goblin resolution logic there reaches the tree for free:

- New `mutable TMap<TWeakObjectPtr<AGSHordeGoblin>, TWeakObjectPtr<AActor>> LootClaims` on the
  subsystem - a memoization cache, not externally-visible state, which is why `GetOrderSubjectFor` can
  stay `const`/`BlueprintPure`.
- For a Loot order: sticky claim lookup first (once a goblin commits to an item, it keeps returning
  that same item every refresh rather than re-rolling mid-walk); on a cache miss, searches
  `FindNearestLootable` (now takes an `Exclude` set) around the order's `Location`, excluding
  everything any OTHER goblin has already claimed; falls back to the order's own single resolved
  `Subject` if the radius is picked clean of unclaimed items, so a lone goblin (or a late arrival to a
  small cluster) still has something to do rather than idling.
- Claims self-clean: dropped the instant a goblin's order stops being Loot, the order clears/prunes, or
  the claimed actor goes invalid (looted/destroyed) - no separate cleanup pass needed.
- Non-Loot verbs (Attack/Hold) are completely untouched - they still share one `Subject`, which is the
  correct read for "the whole warband dogpiles the one guard you pointed at."

Compiled clean, same build as the failure-logging pass above.

**Not yet re-tested with the new failure logging** - it was added specifically because tonight's one
live test went completely silent (no `[GS.Smash]`/`[GS.PickUp]`/`[GS.Loot]` line at all, because none
of the three logged failures before this pass), which is diagnosable-but-unhelpful evidence rather
than proof of a working chain. The mechanism itself (smash-then-loot, credit-on-interact, the
carryable guard) is code-reviewed and compiled clean but has never run end-to-end against a reachable
target.

## Refine

**Handing back at `review`, unobserved for the actual loot mechanic** - the BT, the area-forage claim
logic, and the rest of the C++ are as solid as script-level verification can make them, but nobody has
yet watched a goblin successfully smash and loot a container end to end. The containers ARE reachable
(navmesh theory retracted above) - what's missing is simply a clean live test with enough time for a
goblin to walk over, plus a look at whether the new `[GS.Smash]`/`[GS.PickUp]`/`[GS.Loot]` log lines
confirm or explain whatever happens.

**Corrected, not left standing:** the navmesh investigation and the `#384` ticket idea were both wrong
and are retracted in the Evaluate/Generate sections above rather than quietly dropped - the mistake
(pathing to a prop's own solid-collision center instead of the walkable floor beside it) is recorded
so it doesn't get repeated.

> 2026-08-30T19:25Z Also touching Source/GoblinSiege/Horde/GSHordeAIController.h/.cpp tonight: added OnRequestFinished diagnostic instrumentation, now adding a MoveTo() override to catch synchronous RequestMove failures. No other open ticket claims these files.

> 2026-08-30T19:44Z Also fixed two real bugs found by adversarial review: GSBreakableComponent BeginPlay now re-syncs intact-mesh/interactable-unlock when bBroken loads already true (Source/GoblinSiege/Destruction/GSBreakableComponent.cpp); GSLootBankComponent::BankCarriedLoot now guards against a PutDown()-reentrancy double-bank (Source/GoblinSiege/Raid/GSLootBankComponent.h/.cpp). Both files not previously claimed by this ticket.

> 2026-08-30T20:40Z GENERATE: Three real bugs found and fixed. (1) BB_HordeGoblin OrderVerb key had EnumType=None, breaking every Enum comparison against it - fixed via Python property set, verified via full disk reload. (2) The Loot selector's root gate (BTDecorator_Blackboard) was comparing OrderVerb against int_value=-1 instead of 4 (Loot, per GSHordeOrderTypes.h ordering None=0/Attack=1/Hold=2/Follow=3/Loot=4) and could never pass - fixed via scalar Python edit on the existing decorator, verified via disk reload. (3) Two MoveTo nodes required an exactly-navigable end location on an actor goal, which UE 5.8 skips ProjectPointToNavigation for on actor-tracked moves, and every lootable prop's pivot sits inside its own solid collision by design - set require_navigable_end_location=False on both. Added durable diagnostics: AGSHordeAIController::HandleMoveRequestFinished (binds PathFollowingComponent::OnRequestFinished) and a MoveTo() override logging the synchronous FPathFollowingRequestResult, both gated behind GS.AI.LogMoveCompletion.
EVALUATE: An adversarial tech-lead review (fresh agent, no shared context) independently checked all three fixes against actual source and confirmed each. It also surfaced two further real bugs from the live log evidence handed to it: GSBreakableComponent never re-synced a broken actor's intact-mesh/interactable-unlock state at BeginPlay if bBroken loaded already true - any legitimately pre-broken actor would be permanently stuck neither looking nor working broken. And GSLootBankComponent::BankCarriedLoot had a real reentrancy double-bank: PutDown()'s RestoreCarriedCollision re-enables the cargo's collision while still inside the bank's own trigger volume, synchronously re-firing the same overlap handler for that actor before BankCarriedLoot ever reaches its own CommitBank - banked one delivered item twice, caught live as "40 banked here across 2 item(s)" for a single sack.
REFINE: Fixed both. GSBreakableComponent::BeginPlay now re-runs RetireIntactMesh plus the interactable unlock when bBroken is already true on load. GSLootBankComponent::BankCarriedLoot now checks IsValid(Cargo) after PutDown() and no-ops if the reentrant BankLooseActor path already committed it - BankLooseActor already guarded IsValid(Actor) at its own entry, so this closes the race from either firing order with no new state needed. Rebuilt (BUILD SUCCEEDED both times), fresh editor relaunch and fresh PIE both times to rule out stale-session artifacts. Final clean test (see observed): both barrel and crate genuinely smashed-and-looted by AI with zero human input, sack genuinely carried-and-banked. Known follow-up, not blocking, not part of the original bug: once a barrel or crate is consumed, other goblins keep re-approaching and re-failing "still locked" instead of giving up - a retry-loop polish item for a separate ticket if wanted.
