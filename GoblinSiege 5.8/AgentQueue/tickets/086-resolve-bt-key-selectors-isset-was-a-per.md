---
id: 086
title: Resolve BT key selectors: IsSet() was a permanent false, so the telegraph never reached the blackboard
agent: claude-npcfight2
status: done
claimed: 2026-08-08T23:05Z
build: required
waiting_on:
evaluated: 2026-08-08T23:16:23Z
files: 
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.h
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_Block.h
  - Source/GoblinSiege/AI/Tasks/BTTask_Block.cpp
---

## Goal

Resolve BT key selectors: IsSet() was a permanent false, so the telegraph never reached the blackboard

## Generate

`InitializeFromAsset` overrides on `UBTService_AcquireTarget` and `UBTTask_Block`, each resolving its
key selectors against the tree blackboard, plus a `telegraph SEEN` log line on the service. Built
editor-closed with `-IgnoreQueue` on Michael's ruling (#084 was open, `build: none`, Content-only, so
a C++ compile could not disturb it). 49s, zero errors, only the two known C4996s.

## Evaluate

**The bug this fixes was mine, from #083, and it was invisible by construction.**

`FBlackboardKeySelector::IsSet()` tests `SelectedKeyType`, which is populated *only* by
`ResolveSelectedKey` - and that is called only from a node's own `InitializeFromAsset`.
`UBTService_AcquireTarget` never had one. This did not matter for its entire prior life, because
writing a key goes through `SelectedKeyName` and needs no resolution at all: `TargetActor` and
`TargetLocation` worked perfectly while permanently unresolved. The moment #083 guarded a NEW feature
on `IsSet()`, the guard became a permanent false, the telegraph key was never written, the Block
decorator never passed, and PIE showed "the AI does not block" with no error anywhere.

**Verified, with evidence.** Before the fix: no `telegraph SEEN` and no telegraph block rolls in any
run. After it, the engine started emitting the warning that named the *remaining* problem -

```
LogBehaviorTree: Warning: Acquire Target (nearest hostile)[0]> Failed to find key
'TargetIsAttacking' in BB asset BlackboardData /Game/AI/BB_Human.BB_Human.
```

- which is the fix working: resolution is now attempted, and it reported that the key was missing
from the asset (a separate silent-save bug, diagnosed and fixed under #085). With both fixed, a 4v4
produced 44 telegraphs seen, 17 block rolls and 7 hits resolving `BLOCKED - armor 6.0 = 0.0`. Full
numbers in #085.

**Also verified directly** that the writer side was never at fault: calling `TryLightAttack()` from
Python and reading the ASC in the same frame gave `State.Attacking = 2`,
`State.Attacking.Windup = 1`. That measurement is what let me stop suspecting the ability and go
looking at the reader.

**Not changed:** `UBTTask_MeleeAttack` still has no `InitializeFromAsset`. It reads its key by name
and uses no `IsSet()`, so it is correct today - but it carries the same latent trap, and whoever adds
an `IsSet()` check there will lose the same hour. Left alone deliberately rather than widening a
build; recorded here and in the header comment on the override.

**Owed to `AGENT_STATE.md`:** a FAILED line, because this is a trap rather than a one-off - a BT node
that only WRITES blackboard keys works fine unresolved, so the omission stays silent until the day
something reads `IsSet()`.

## Refine

Added the `telegraph SEEN` log line while fixing this, on the EDGE rather than per tick. Without it,
"the AI never blocks" and "the AI never sees the windup" are the same observation with completely
different causes, and I had already spent an hour on exactly that ambiguity. The header comment on
the override is deliberately long and says why the absence was survivable for so long, because the
next person to write a BT node here will hit it.

Nothing else changed. The fix is four lines per node; the diagnosis was the work.
