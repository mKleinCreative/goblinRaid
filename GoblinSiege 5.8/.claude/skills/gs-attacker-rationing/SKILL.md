---
name: gs-attacker-rationing
description: Goblin Siege — why ACF's UACFAIManagerComponent attacker tickets are the wrong thing to adopt here, and what GSEngagementComponent does that they do not.
globs: []
alwaysApply: false
---

# GS — keep the engagement token ledger; do not adopt ACF's attacker tickets

This is the rare inverted case. The standing rule says adopt what ACF ships, and the instinct — "ACF
already does crowd control, delete `UGSEngagementComponent`" — is wrong here for three measurable
reasons. Read this before touching `UGSEngagementComponent`, `UBTDecorator_HasAttackToken`, or adding
`UACFAIManagerComponent` to `AGSGameState`.

`UACFAIManagerComponent` appears in exactly one of the 40 ACF packs (`music-manager:19`) and only as a
battle-state source for adaptive music. `ai-framework` never mentions tickets, `MaxAttackersPerTarget`,
`RequestTicket` or `bRequiresTicket` at all.

---

## 1 — What ACF actually ships

| Fact | Source |
|---|---|
| `int32 MaxAttackersPerTarget = 1;` | `AIFramework/Public/Components/ACFAIManagerComponent.h:143` |
| `RequestTicket` counts live tickets whose `Ticket.Target == Target` and refuses at `CurrentCount >= MaxAttackersPerTarget` | `ACFAIManagerComponent.cpp:76-93` |
| `HasTicket(AIController)` is `ActiveTickets.Contains(AIController)`, and `FACFAITicket::operator==` compares **only** `AIController` | `ACFAITypes.h:430-433` |
| Tickets expire only by `TimeRemaining -= DeltaTime` in `UpdateTickets` | `ACFAIManagerComponent.cpp:62-69` |
| `ReleaseTicket` is declared and defined and **called from nowhere in the plugin** (grep across all of `Source`) | `ACFAIManagerComponent.cpp:96-99` |
| Sole consumer is `UACFCombatBehaviourComponent::EvaluateTicket`, reached only when `FActionChances::bRequiresTicket` is set — default **false** | `ACFCombatBehaviourComponent.cpp:132-152`; `AscentCombatFramework/Public/Game/ACFTypes.h:182`, `:199` |

**NOT VERIFIED:** FullExample contains no ACF content, so there is no shipped `MaxAttackersPerTarget`
value or ticket-enabled `FActionChances` row to read as intended tuning.

---

## 2 — Three defects for a siege

1. **The default clamps a siege to one attacker per defender.** Dropping the component onto
   `AGSGameState` at its defaults does not "add crowd control", it strangles the fight.
2. **The ticket is per-controller, not per-target.** `operator==` compares only the AIController, so an
   AI that got a ticket against defender A is `HasTicket == true` and swings freely at B and C for the
   whole `TicketDuration`. The cap is not actually per-target once fights overlap — which is precisely
   the situation a raid is made of.
3. **Nothing releases on death.** With no `ReleaseTicket` caller, a target killed mid-window keeps its
   attackers' tickets held until they time out.

It would also **stack with**, not replace, the existing budget.

---

## 3 — What Goblin Siege has instead

`AGSGameState` registers only `UACFTeamManagerComponent` (`Core/GSGameState.h:38`); there is no
`UACFAIManagerComponent` anywhere. Rationing lives on the **victim**:

- `UGSEngagementComponent`, `TokenBudget = 6` (`Combat/GSEngagementComponent.h:321` — raised from 4 by
  Michael, 2026-08-21)
- `TryAcquireToken` / `ReleaseToken` / `SetTokenLocked` / `HoldsToken` (`:95-105`)
- `TokenWatchdogSeconds = 3.f` sweep (`:408`)
- `UBTDecorator_HasAttackToken` acquires on `OnBecomeRelevant`, releases on `OnCeaseRelevant`
  (`AI/Tasks/BTDecorator_HasAttackToken.cpp:32-33`)

That is a per-target ledger with explicit release and a watchdog — the two things ACF's ticket lacks.
The two systems solve different problems; this is not a workaround around ACF.

**If you are asked to adopt the AI manager anyway:** it is still the right component for `EBattleState`
/ `OnBattleStateChanged` (that is what `music-manager` uses it for). Adopt it for that, leave
`bRequiresTicket` false, and leave the token ledger alone.
