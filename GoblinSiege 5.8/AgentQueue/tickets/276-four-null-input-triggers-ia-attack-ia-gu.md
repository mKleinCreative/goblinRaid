---
id: 276
title: Five null input triggers, and why filling them would have broken blocking and the heavy charge
agent: claude-warren
status: done
claimed: 2026-08-24T03:24Z
build: none
waiting_on:
evaluated: 2026-08-24T03:30:00Z
observed: 2026-08-24T03:30:02Z | A launch that had been printing four null-trigger errors printed none, and no Enhanced Input errors of any kind. All 19 key mappings still answer, Traverse and Climb kept their hold triggers, and the game came up and ran as before.
scenario: PIE on L_CombatArena with the log length marked beforehand so only the new run was counted. NOTE: nobody has pressed attack, block or dodge since - that is the check still owed.
files: 
  - Content/Input/IA_Attack.uasset
  - Content/Input/IA_GuardBreak.uasset
  - Content/Input/IA_Block.uasset
  - Content/Input/IMC_Default.uasset
---

## Goal

Every PIE start logged `LogEnhancedInput: Error: Null input trigger detected in mapping to input
action '<X>'` for `IA_Attack`, `IA_GuardBreak`, `IA_Dodge` and `IA_Traverse` - four errors, on core
combat inputs, on every launch.

## Generate

**There were FIVE nulls, not four.** Sweeping every `InputAction` in the project rather than only the
ones the log named turned up `IA_Block` in the same state. It does not appear in the error list
because the message is emitted per *mapping context entry*, and Block is bound in C++ but is not one
of `IMC_Default`'s 19 rows - so its null sat there silently.

Two different kinds of null, in two different places:

- **On the action asset** (`triggers` array): `IA_Attack`, `IA_Block`, `IA_GuardBreak` - one empty
  array element each, an entry someone added and never assigned a class to.
- **On the mapping context** (`IMC_Default.DefaultKeyMappings.Mappings[n].Triggers`): row 12
  `IA_Traverse`, row 17 `IA_Dodge`.

### The important part: the obvious fix would have broken combat

The tempting repair is to fill the empty slots with `InputTriggerPressed`. It is what **40 of ACF's
own input actions do**, so it reads as the house style, and it silences the error just as well.

**It would have broken hold-to-block and the heavy-charge attack.** From
`Characters/GSPlayerCharacter.cpp:448-464`:

```cpp
EIC->BindAction(AttackAction, ETriggerEvent::Started,   ... Input_AttackPressed);
EIC->BindAction(AttackAction, ETriggerEvent::Completed, ... Input_AttackReleased);
EIC->BindAction(BlockAction,  ETriggerEvent::Started,   ... Input_BlockStart);
EIC->BindAction(BlockAction,  ETriggerEvent::Completed, ... Input_BlockStop);
```

Attack and Block are **hold-and-release** bindings - press starts the charge or raises the guard,
release ends it. `InputTriggerPressed` fires `Started` and `Completed` on the *same frame*, so the
guard would drop instantly and the charge would never build. `GuardBreak` and `Dodge` bind `Started`
only and would have survived it, which is exactly the sort of partial success that makes a wrong fix
hard to spot.

**The right fix is to remove the empty entries**, which is also precisely what the engine already
does at runtime - it detects the null, logs, and skips it. So this change is
**behaviour-preserving by construction**: what runs after it is what has been running all along,
minus the error.

That also matches this project's own convention rather than ACF's. Of our 20 `IA_*` assets, only
`IA_Climb` and `IA_Traverse` carry a trigger at all (`InputTriggerHold`); every other one runs on the
default `Down`.

## Evaluate

**Runtime, one controlled run** - log length marked, PIE started, only the new lines counted:

```
Null input trigger detected     0      (was 4 a launch)
LogEnhancedInput: Error         0
```

The only Enhanced Input line left is `Enhanced Input local player subsystem has initialized the user
settings!`.

**Nothing else was lost in the edit**, checked explicitly because the `IMC_Default` fix rebuilds the
whole mappings array and a silent row loss would have been invisible and catastrophic:

- 19 mappings before, **19 after**, all 16 distinct actions still present
- `IA_Traverse` and `IA_Climb` still carry `InputTriggerHold` - the action-level trigger was a
  separate thing from the mapping-level null and had to survive
- `IA_Attack`, `IA_Block`, `IA_GuardBreak`, `IA_Dodge` now sit on the default `Down`, like the rest
  of the project

**NOT ESTABLISHED, and it needs Michael: that attack, block and dodge still FEEL right.** The
argument that behaviour is unchanged is sound - the engine was already discarding these nulls - but
nobody has pressed the buttons since. The specific things worth trying: **hold attack to charge a
heavy**, **hold right mouse to keep the guard up**, and a **dodge**. If any of those changed, this
ticket is the cause.

## Refine

**Changed from the plan:** the ticket was claimed for four nulls on three assets. The sweep found a
fifth on `IA_Block`, and the claim was widened before touching it. Worth recording *why* the log
undercounted - the error is per mapping-context entry, so a null on an action that no context maps
can never be reported. **There is no guarantee the log has named every one**; the sweep is what
found the truth, and a future check should sweep rather than grep.

**Deliberately not done:** no trigger was *added* anywhere. Deciding that Attack ought to be
`Pressed`, or that Dodge ought to be `Hold`, is input-feel design and Michael's call - and as above,
getting it wrong on Attack or Block breaks them in a way that reads as a combat bug rather than an
input change.
