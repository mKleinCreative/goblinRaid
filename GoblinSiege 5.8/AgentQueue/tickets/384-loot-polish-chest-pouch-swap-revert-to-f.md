---
id: 384
title: Loot polish: chest/pouch swap, revert-to-follow when nothing left to loot
agent: claude-loot2
status: done
claimed: 2026-08-30T20:46Z
build: none
waiting_on:
evaluated: 2026-08-30T23:48:08Z
observed: 2026-08-30T23:48:45Z | Two rounds of confirmation. Round 1 (earlier): Michael watched live - a goblin looted gold, died, dropped a recoverable pouch, and a second goblin picked it up and delivered it to the portal for banking; smash-then-loot completing on both a barrel and crate. Round 2 (final, after fixing the IsStillLootable regression): I ran a clean hands-off test myself - ordered a goblin to loot a barrel, watched [GS.Smash] and [GS.Loot] fire back-to-back with no order collapse between them, then teleported that same living goblin into the portal and captured the new log line: 'BP_HordeGoblin_C_1 made it back to the portal and banked its personal purse for 15 loot.' Michael then independently watched the same full flow himself and confirmed: 'Ok, verified.'
scenario: Live PIE, freshly rebuilt binaries, freshly reloaded L_CombatArena, real player-driven horde orders and horn summons throughout - both my own controlled test and Michael's independent live playthrough.
files: 
  - Source/GoblinSiege/Interaction/GSInteractableComponent.h
  - Source/GoblinSiege/Interaction/GSInteractableComponent.cpp
  - Source/GoblinSiege/Horde/GSHordeSubsystem.h
  - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
  - Source/GoblinSiege/Horde/GSHordeAIController.h
  - Source/GoblinSiege/Horde/GSHordeAIController.cpp
  - Content/Blueprints/Interactables/BP_LootChest.uasset
  - Content/Blueprints/Interactables/BP_LootSack.uasset
---

## Goal

Loot polish: chest/pouch swap, revert-to-follow when nothing left to loot

## Generate

Five iterations, each responding to Michael watching the previous one live (full detail in the
timestamped log below):

1. Swapped BP_LootChest/BP_LootSack carry-vs-loot-in-place roles, then refined to "only livestock
   carries home, everything else loots in place onto a personal purse."
2. Fixed the goblin retry-loop on already-looted targets (GetOrderSubjectFor trusted a stale claim
   forever); goblins now revert to Follow when nothing is left to loot.
3. Fixed a real range mismatch (BTTask_SmashOrderTarget's SmashRange=250 vs BTTask_LootInPlace's
   InteractRange=180, nothing re-approaching between them) that let a goblin smash a target and then
   permanently fail to loot what it just broke.
4. Built AGSHordeGoblin::PersonalPurse (AddToPersonalPurse from BTTask_LootInPlace), then iterated
   through auto-bank-on-death, back to a recoverable pouch-drop-on-death, per Michael's own
   back-and-forth on the design.
5. Final spec from Michael, verbatim: "Horde grabs money -> value of money gets stashed on their
   model. model of money disappears. -> goblin that grabbed that money, keeps that money with them,
   until they either die, in which case they'll drop the amount they have, or they make it back to
   spawn and bank it." Added BP_LootChest's missing bDestroyOwnerOnComplete (the pouch already had
   it), and a new live-delivery path: AGSHordeGoblin::BankPersonalPurse() (distinct from the
   death-only ConsumeLootSackDropValue) plus UGSLootBankComponent::BankPersonalPurse(APawn*), called
   from BankFromOverlap alongside the existing BankCarriedLoot - a goblin banks BOTH what it's
   carrying and its personal purse if it walks into the Warren/portal's existing trigger with both.

## Evaluate

Verified by direct PIE observation throughout, not "should work." Michael watched a goblin loot
gold, die, drop a recoverable pouch, and a SECOND goblin pick that pouch up and deliver it to the
portal for banking. He separately confirmed the smash-then-loot range fix on both a barrel and a
crate. After iteration 5's build, Michael reported the live walk-home banking path was "doing the
spawn thing again" instead of auto-banking - a genuine regression, not the expected iteration-5
behavior.

An adversarial code review (fresh agent, no shared context, given the full bug report and asked to
trace `PersonalPurse` end to end) checked `GSLootBankComponent`/`GSRunicSite`'s live-bank wiring
specifically and found it correct - which correctly pointed the remaining investigation away from
the banking code itself. Live-checking the goblin's own blackboard state immediately after a smash
(`OrderVerb=0/None, OrderSubject=None`) found the real cause: `IsStillLootable` (added in iteration 2
for the retry-loop fix) treated "broken" as "no longer lootable," so a goblin's own claim on a target
it had JUST smashed open was dropped by the SAME refresh that should have let it loot - the order
collapsed to None before `UBTTask_LootInPlace` ever ran. This is what "it smashed, but didn't go
ahead" had been all along, including the range-mismatch fix in iteration 3, which was a real but
partial fix for the same symptom.

Touched outside the original ticket scope: BTTask_LootInPlace.h's InteractRange and
GSLootBankComponent.h/.cpp (both new scope, pulled in as Michael's live bug reports and final spec
made them necessary) and this final fix to GSHordeSubsystem.cpp's IsStillLootable.

DECISION for AGENT_STATE.md: the "grab it, keep it, die-and-drop-or-return-and-bank" personal purse
model is now the intended design for all non-livestock loot (chest, coin pouch); livestock keeps the
older carry-and-deliver path unchanged. A broken-but-not-yet-looted container counts as still
in-progress for area-forage/claim purposes - only the actual loot (CompleteInteraction) ends a claim,
not the smash. Worth recording so a future session doesn't reintroduce either regression.

## Refine

Fixed `IsStillLootable` to also accept a broken container whose interactable is still available (not
yet looted), alongside the existing carryable and unbroken-container cases. Rebuilt, fresh editor +
PIE. Ran one clean, hands-off test myself: ordered a goblin to loot a barrel, watched smash and loot
fire back-to-back in the log with no collapse between them, then teleported that same still-alive
goblin directly into the portal's overlap sphere and captured the new confirming log line -
"BP_HordeGoblin_C_1 made it back to the portal and banked its personal purse for 15 loot." Michael
independently watched the full flow end to end and confirmed: "Ok, verified."

> 2026-08-30T20:55Z GENERATE: Two design changes on top of #382's loot-order fix. (1) BP_LootChest and BP_LootSack initially swapped their carry-vs-loot-in-place roles per Michael's first request. (2) Michael then refined further after watching the swap live: only livestock (BP_Livestock_Pig/Chicken/Sheep, already bIsCarryable=True, untouched) should be physically carried home through GSLootBankComponent; everything else (chest, coin pouch) should loot in place and accumulate on the LOOTING GOBLIN as a personal purse instead of banking straight to the shared score subsystem, and that purse should drop as a recoverable BP_LootSack when the goblin dies. Also fixed the retry-loop Michael flagged live: goblins kept re-approaching an already-looted barrel/crate instead of giving up.
EVALUATE: The personal-purse ask maps directly onto the LootSackDropValue/ConsumeLootSackDropValue/SpawnLootSack framework already built earlier tonight for guards (#382 addendum) - AGSHordeGoblin already derives from AGSCharacterBase and its HandleDeath() already calls Super::HandleDeath(), which already spawns a loot sack from whatever ConsumeLootSackDropValue() returns. No changes needed to the death path itself. Retry-loop root cause: UGSHordeSubsystem::GetOrderSubjectFor's sticky per-goblin claim cache, and its own fallback to the order's raw Subject, both trusted a target forever once claimed - neither ever re-checked whether it was STILL lootable (still carryable, or still an unbroken container) after the first check.
REFINE: BP_LootChest reverted to non-carryable/loot-in-place (Interact.Loot, bConsumeOnComplete=True) after briefly being made carryable; BP_LootSack set to non-carryable/loot-in-place with a new bDestroyOwnerOnComplete=True (added to UGSInteractableComponent) so the pouch model actually disappears on loot, matching "so the model disappears" - verified via full disk reload after each Blueprint save. AGSHordeGoblin gained a private PersonalPurse int32, a public AddToPersonalPurse(int32), and an override of ConsumeLootSackDropValue() that reads-and-resets it - the same pattern AGSPlayerCharacter already uses for the score subsystem. BTTask_LootInPlace now casts its looter to AGSHordeGoblin and calls AddToPersonalPurse instead of UGSScoreSubsystem::AddLoot directly (falls back to the old score-subsystem path defensively if the looter is ever something else - this task only ever runs from BT_HordeGoblin today). GSHordeSubsystem gained a shared IsStillLootable(const AActor*) helper (the exact two-part test FindNearestLootable's own loop already applied) and now runs both the sticky claim and the Order->Subject fallback through it before trusting either - a stale claim is dropped and re-searched instead of returned forever. Rebuilt twice (BUILD SUCCEEDED both times), fresh editor relaunch both times. NOT YET independently observed running - map is freshly loaded and PIE is ready for Michael to press Play and test the new chest/pouch/purse/death-drop behavior himself, as he asked to do for #382.

> 2026-08-30T21:09Z Also touching Source/GoblinSiege/Horde/GSHordeGoblin.h/.cpp - not in the original claim, but checked clear of conflicts.

> 2026-08-30T21:28Z GENERATE (cont.): Michael reported "smashed, but didn't go ahead [and loot]" / "smashed, then wandered" after watching a fresh test. Log showed BP_LootBarrel_C_0 smashed successfully, then LootInPlace failed with "198uu away, need <= 180uu - not in range yet" and never retried; BP_LootCrate_C_0 smashed successfully with no LootInPlace attempt logged at all afterward.
EVALUATE (cont.): Read BTTask_SmashOrderTarget.cpp in full for the first time this session - confirms it never moves the character (pure range check + TryLightAttack + Break()). Its SmashRange=250uu (comment: "Matches UBTTask_MeleeAttack's AttackRange") vs BTTask_LootInPlace's InteractRange=180uu (comment: "Matches UBTTask_PickUpCargo's PickUpRange") - both deliberately chosen to match OTHER tasks, but nothing re-approaches the target between Smash and Loot in Sequence_1. A goblin whose swing or crowd-avoidance slop lands it anywhere between 180-250uu away can smash successfully (within SmashRange) and then permanently fail to loot (outside InteractRange), with MoveTo_2's 150uu AcceptanceRadius already satisfied so it never re-walks closer.
REFINE (cont.): Raised BTTask_LootInPlace's InteractRange from 180 to 300 (past SmashRange with margin, same "generous against what comes before it" reasoning PickUpRange's own comment already uses) rather than lowering SmashRange and losing its intentional match to melee reach. PickUpCargo's own PickUpRange (180, a separate constant) is untouched. The real structural fix - a second MoveTo before LootInPlace in Sequence_1 - would need Michael to add it by hand in the BT editor per the standing rule against structural BT edits via Python; this scalar fix closes the gap without needing that. Rebuilt (BUILD SUCCEEDED), fresh editor relaunch, map reloaded. NOT YET independently observed with this specific fix - ready for Michael to retest the same barrel/crate smash-then-loot sequence.

> 2026-08-30T21:40Z GENERATE (cont.): Michael reversed the auto-bank decision after seeing it work: "I didn't want it to bank on death, I want it to drop its potential points on the ground so you have to go pick it up or have another goblin pick it up."
REFINE (cont.): AGSHordeGoblin::ConsumeLootSackDropValue reverted from the auto-bank version back to returning PersonalPurse (reset to 0) for AGSCharacterBase::HandleDeath to hand to SpawnLootSack - the same recoverable-pouch path the player/guard death drop already uses. InitialiseAsCarryable sets bIsCarryable=true on the spawned pouch, so it is already a legal Loot-order target for another goblin's area-forage without further changes - "or have another goblin pick it up" needed no new code. Removed the now-unused GSScoreSubsystem.h/Engine/World.h includes and updated both header comments (AddToPersonalPurse's and the removed auto-bank block's) to match. Rebuilt (BUILD SUCCEEDED), fresh editor relaunch, map reloaded. NOT YET independently observed with this specific reversal - ready for Michael to retest a goblin dying with a nonzero purse and confirm a physical pouch drops.

> 2026-08-30T23:48Z GENERATE: See full write-up already in this ticket's Generate/Evaluate/Refine sections above - five iterations culminating in the final spec (loot in place -> stashed on the goblin -> die-and-drop-recoverable OR walk-home-and-autobank).
EVALUATE: Final round found a real regression from the earlier retry-loop fix: IsStillLootable treated "broken" as "no longer lootable," so the instant a barrel was smashed the goblin's own claim on it was dropped, OrderVerb collapsed to None via the revert-to-Follow fix, and UBTTask_LootInPlace never got a chance to run - "it smashed, but didn't go ahead" had a real, previously-unidentified cause. An adversarial code review (fresh agent, no shared context) independently checked the live-bank wiring itself and found it correct, which correctly ruled that half out and pointed the remaining investigation at the claim-drop instead.
REFINE: Fixed IsStillLootable to treat a broken-but-not-yet-looted container as still legitimately in-progress (only actually looting it, which flips bIsAvailable false, now ends the claim) - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp. Rebuilt, fresh editor + PIE. Ran one clean, uninterrupted, hands-off test myself: smash and loot fired back-to-back with no collapse in between, then walked the same goblin (alive) directly into the portal and captured the new confirming log line - "BP_HordeGoblin_C_1 made it back to the portal and banked its personal purse for 15 loot." Michael independently watched the full flow and confirmed: "Ok, verified."
