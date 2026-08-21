---
id: 231
title: ACF Phase 2b-3: stamina storage moves to ARS, GSStaminaComponent becomes policy
agent: claude-acf
status: done
claimed: 2026-08-21T03:50Z
build: none
waiting_on:
evaluated: 2026-08-21T03:54:37Z
observed: 2026-08-21T03:57:42Z | Michael watched it: sprinting drains stamina, dodging drains stamina, and both refuse once stamina is low enough - so the exhaustion latch and the drain path both survive the move to ARS storage. Earlier automated proof: try_consume(30) took the component 100 to 70 and ARS read 95 mid-regen, where before the change ARS sat untouched at 100.
scenario: Player-driven sprint and dodge on L_CombatArena after the 2b-3 build, plus a PIE TryConsume probe.
files: 
  - Source/GoblinSiege/Characters/GSStaminaComponent.cpp
  - Source/GoblinSiege/Characters/GSStaminaComponent.h
---

## Goal

ACF Phase 2b-3: stamina storage moves to ARS, GSStaminaComponent becomes policy

## Generate

Ruling 35, done as written: **ARS owns the stamina value; `UGSStaminaComponent` keeps the policy.**

`GetStamina`, `GetMaxStamina` and `GetStaminaNormalised` moved out of line and now read
`UACFStatisticsSet::Stamina` / `MaxStamina`. `SetStamina` - still the single writer - writes the ARS
attribute. Every internal read of the raw members became a getter call, including the two exhaustion
latch tests.

**What stayed, because a bare attribute cannot express it:** `SetDrainRate` (continuous drain for
sprint and swim), `bRegenSuppressed` (the climb freeze from #072/#076 - a frozen pool, not merely a
non-draining one), `RegenDelaySeconds`, and `bExhausted` with `OnExhausted`/`OnRecovered` - hysteresis
against `RecoverFraction`, not a threshold. That is the whole point of ruling 35: ARS gets the number,
we keep the feel.

Two guards worth naming:
- `GetMaxStamina` treats an ARS max of **zero** as "not configured" and falls back, rather than
  dividing every ratio and clamp in the component by zero.
- The local float is still written and still replicated, so an owner with no ability system - a
  target dummy, a breakable - keeps working. It is a fallback, not a second pool: once ARS is
  present, nothing reads it.

## Evaluate

**WATCHED, and by a test that discriminates.** In PIE:

```
before: stamina 100.0 / 100.0  exhausted=False
try_consume(30) -> True
after : stamina 70.0 / 100.0
GS.Stats.Dump:  ARS 100/100 hp   95/100 stam
```

The component reports 70 immediately after the spend, and the ARS attribute reads **95** a few
seconds later - mid-regen back up from 70 at `RegenRate = 25/s`. **Before this change ARS stamina
would have sat at 100 untouched**, which is exactly what the first reading of this session showed.
So the write reaches ARS and the read comes back from it.

Deliberately not trusted: the resting `100/100` reading on its own. It is identical whether or not
the migration worked, which is why the spend was forced rather than waited for.

**NOT re-watched by a human:** sprint drain and the dodge cost (#209) go through the same
`TryConsume` and `SetDrainRate` that were just exercised, but nobody has watched the meter on screen
since the change.

## Refine

`MaxStamina 100` was already authored on every row of `DT_GSAttributeInits` in #226, marked
provisional so this ticket would not need a second pass over six rows. It did not.
