---
id: 076
title: "Stamina migration: BP SprintStamina retires to UGSStaminaComponent; freeze on the wall; HUD reads the component"
agent: claude-climbrebuild
status: done
claimed: 2026-08-07T22:30Z
build: none
waiting_on:
evaluated: 2026-08-07T23:03:30Z
files: 
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
  - Content/UI/WBP_GSPlayerHUD.uasset
---

## Goal

Retire the Blueprint `SprintStamina` float onto `UGSStaminaComponent`, so one pool drives climbing,
sprinting, vault and mantle - and so the HUD stops being written by two things at once.

## Generate

| system | was | now |
|---|---|---|
| climb drain | BP float subtraction | `SetDrainRate(move/idle rate)` + `SetRegenSuppressed(true)` |
| climb exhaustion | `float <= 0` | `IsExhausted()` |
| sprint drain | signed rate added to the float on Tick | `SetDrainRate(max(0, -rate))` |
| vault / mantle | `FMax(0, stamina - cost)` | `TryConsume(cost)` |
| sprint gate + speed tiers | stale `bExhausted` bool | `IsExhausted()` |
| HUD bar and text | BP Tick poll of the float | C++ `OnStaminaChanged`; poll disconnected |

Call sites now: 5x `SetDrainRate`, 4x `SetRegenSuppressed`, 2x `TryConsume`, 2x `IsExhausted`,
9x `GetStamina`. All four old writers verified orphaned (`execute` unconnected). `SET bExhausted`
still carries an exec wire but is fed by the orphaned Tick write, so it is unreachable.

**Exits clear both the drain rate AND the freeze.** `SetDrainRate(0)` + `SetRegenSuppressed(false)`
on all three of `ClimbTopOut` / `ClimbDropOff` / `ClimbToGround`. Without the first you keep draining
while walking; without the second the pool never refills again.

**Sprint drain now requires movement** (Michael, same session): the test was `bWantsSprint AND NOT
exhausted`, so standing still with shift held burned the pool. Now `AND VSizeXY > 50`. `VSizeXY`
rather than `VSize` so falling does not count as sprinting.

## Evaluate

**NOT PLAYED.** Compile results and graph audits only. Whether the pool now *feels* right - and in
particular whether freeze-on-wall makes tall buildings too easy or too punishing - needs Michael.

**Verified by audit, not assertion:** every old writer's `execute` pin read back as unconnected; the
component call-site census is above; both Blueprints compile `UpToDate` (704 and 25 nodes).

**The HUD fix is a deletion, not a rewrite, and that is the point.** I first tried to repoint the
widget's Tick poll at `GetComponentByClass -> GetStaminaNormalised`. **`set_node_pin_value` reported
success on the `ComponentClass` pin four times and the readback was empty every time** - a class pin
will not take a default through that API. Had I trusted the return value the bar would have read a
null component and sat at zero. Then I read `GSPlayerHUDWidget.cpp` and found
`HandleStaminaChanged` already binds `OnStaminaChanged` and sets both `StaminaBar->SetPercent` and
`StaminaText`. The correct change was to delete the poll, which is what #064 diagnosed in the first
place. The orphaned nodes were removed and Event Tick disconnected.

**Deliberately behaviour-neutral on vault/mantle.** `TryConsume` returns false when you cannot
afford the move; I did NOT gate the montage on it. The float version consumed and continued, so this
one does too. Gating means a vault can refuse - a real feel change that should be Michael's call, not
a side effect of a port.

**The old nodes are orphaned, not deleted.** `SprintStamina`, `MaxStamina` and `bExhausted` still
exist as variables with dead maths hanging off them. That is deliberate for one session - reverting
is reconnecting four exec wires. They should be deleted once this has been played.

**Owed AGENT_STATE.md** - DECISION: `UGSStaminaComponent` is the single owner of the pool; the BP
`SprintStamina` float is retired. Stamina FREEZES on the wall (no drain, no regen). Sprint charges
for movement, not for holding the key.

## Refine

- **Read the C++ before rewiring the widget.** The whole HUD task turned out to be one disconnect,
  because the parent class already did the work. I nearly rebuilt it in Blueprint instead.
- **Checked the pin readback instead of the return value.** `set_node_pin_value` returned `True` on a
  class pin it silently ignored - the same "returns True and changes nothing" trap this project has
  hit before with `set_variable_default_value`.
- **Cleared the drain rate on exit, not just the freeze.** `SetDrainRate` is last-writer-wins with no
  stacking, so leaving the climb rate set would have drained the player while walking around.
- **Reused the existing rate computation rather than re-deriving costs.** The climb's move/idle
  `SelectFloat` and the sprint's signed rate already existed; both feed the component directly, so
  the tuning values keep their old meaning and feel does not shift on the day of the port.

**Deliberately left undone:** deleting the orphaned float and its dead maths; gating vault/mantle on
`TryConsume`'s result; the `MaxWalkSpeed` direct writes in the sprint tiers, which `CLAUDE.md`
forbids and which are a separate ticket.
