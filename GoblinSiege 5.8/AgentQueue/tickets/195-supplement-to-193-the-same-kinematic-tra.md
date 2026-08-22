---
id: 195
title: Supplement to 193 - the same kinematic trap in GSBreakableComponent::Break(): SetSimulatePhysics does not make a collection dynamic
agent: claude-idol
status: done
claimed: 2026-08-19T02:46Z
build: none
waiting_on:
evaluated: 2026-08-19T03:45:32Z
observed: 2026-08-19T03:44:45Z | Rode along with 193's build and is exercised by the same topple path: the collection is promoted to Chaos_Object_Dynamic before simulation is enabled, which is what the statue needed. The GSBreakableComponent half remains pre-emptive - no smashable prop carries a geometry collection yet, so that branch has still never fired in anger.
scenario: Same PIE session as 193; the topple path proves the ObjectType-then-simulate ordering, the smash path is unexercised by design
files: 
  - Source/GoblinSiege/Destruction/GSBreakableComponent.cpp
---

## Goal

Supplement to 193 - the same kinematic trap in GSBreakableComponent::Break(): SetSimulatePhysics does not make a collection dynamic

## Generate

One line in `UGSBreakableComponent::Break()`:

```cpp
GC->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
GC->SetSimulatePhysics(true);
```

The smash path had the same trap the topple path was stuck in for hours. `SetSimulatePhysics(true)`
does NOT change `ObjectType` - the component then reports `is_simulating_physics == true` while
remaining Kinematic, and a kinematic body ignores every impulse and every strain, silently.

Found while debugging #193's statue, which logged three successful topples and never moved.

## Evaluate

**OBSERVED indirectly, and honestly labelled.** The ordering itself is proven - the identical fix in
`UGSTopplableComponent::Topple()` is what let the statue respond at all, watched by Michael in the same
session. But **this** copy of it, in the smash path, has never executed: no smashable prop in the
project carries a geometry collection. The crate and barrel work by mesh swap, and the only collection
that exists belongs to the statue, which topples rather than smashes.

So it is a fix applied ahead of the bug rather than after it, which is worth stating plainly rather
than dressing up as verification.

**Why fix it pre-emptively at all:** the moment anyone gives a smashable prop a fracture asset - the
explicit next step in the smashables plan - `Break()` would have delegated to ACF's `ForceDestruction`
against a kinematic collection and produced exactly the failure the statue produced: a log line
announcing destruction, and a prop standing untouched. That cost most of a session to diagnose once.
Paying one line to not pay it twice is the trade.

## Refine

**Changed after self-review:** nothing in the code. The comment was rewritten to record the measurement
(`is_active=true, object_type=Dynamic, simulating=true` and still zero velocity) rather than just
asserting the ordering matters, so the next person meets the evidence and not an instruction.

**Deliberately NOT also calling RemoveAllAnchors here**, even though that turned out to be the real
blocker for the statue. The anchoring came from how `GC_Statue_Warrior` was authored through the
Dataflow template; whether a future smashable prop's collection is anchored is unknown, and stripping
anchors from every breakable on the assumption it might be would be a guess applied to props that do
not exist yet. When one does, the SWAP/OUTCOME logging added in #193 will say so in a line.
