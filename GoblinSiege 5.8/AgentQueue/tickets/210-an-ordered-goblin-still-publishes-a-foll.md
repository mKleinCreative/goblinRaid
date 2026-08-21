---
id: 210
title: An ordered goblin still publishes a FollowTarget, so Follow Summoner and Chase Target both pass and the tree oscillates
agent: claude-hordeorder
status: done
claimed: 2026-08-20T20:51Z
build: required
waiting_on:
evaluated: 2026-08-20T20:52:23Z
observed: UNOBSERVED 2026-08-20T20:52:23Z - Written, never run - the fix needs a build and this ticket's own claim shuts the gate. The MECHANISM is evidenced from both sides (the unconditional FollowTarget write in C++, and the Follow Summoner / Chase Target branch names and decorators extracted from BT_HordeGoblin), but nobody has watched an ordered goblin stop oscillating. Michael tests it after the build.
scenario: none - never run
files: 
  - Source/GoblinSiege/Horde/GSHordeAIController.cpp
---

## Goal

An ordered goblin still publishes a FollowTarget, so Follow Summoner and Chase Target both pass and the tree oscillates

## Generate

One conditional in `AGSHordeAIController`'s blackboard refresh: **a goblin under a standing order no
longer publishes a `FollowTarget`.**

```cpp
BB->SetValueAsObject(FollowTargetKey, bHasStandingOrder ? nullptr : FollowTarget);
```

The order verb is now read once, above the writes, instead of twice — two reads of the same source in
one refresh could in principle disagree mid-frame.

**The mechanism, confirmed from both sides before touching anything.** `BT_HordeGoblin` carries two
branches that could both pass at once:

| Branch | Gate |
|---|---|
| `Follow Summoner` | `Has A Follow Target` — a blackboard decorator on `FollowTarget` |
| `Chase Target` | `Has A Target` — on `TargetActor` |

The controller wrote **both** keys unconditionally, so a goblin under an Attack order held a live
`TargetActor` *and* a live `FollowTarget`. The Selector had two valid children and flip-flopped
between them on re-evaluation. On screen: a goblin that stares at you, breaks off, stares again —
which is exactly how Michael reported it.

**Fixed in the controller rather than the tree, deliberately.** The branch ordering in
`BT_HordeGoblin` is fine, and the tree *cannot* be where this is decided — the controller is the only
thing that knows an order exists. A decorator cannot out-vote a key that should never have been set.

**This is not a cancel.** `UGSHordeSubsystem` still holds the summoner and the follow slot, so the
moment the order clears the next refresh republishes the target and the goblin drops back into the
scamper on its own.

## Evaluate

**Evidence gathered from both halves before the edit**, not inferred from one:
- C++: `GSHordeAIController.cpp` wrote `FollowTargetKey` unconditionally, with `bCommanded` computed
  *after* it.
- Assets: string extraction from `BT_HordeGoblin.uasset` and `BB_HordeGoblin.uasset` confirmed the
  branch names `Follow Summoner` / `Chase Target`, the decorators `Has A Follow Target` /
  `Has A Target`, and the keys `FollowTarget` / `TargetActor` all exist as the theory required.

**A string in a package proves a reference exists, not a wiring.** The branch/decorator pairing is
inferred from names, not from reading the tree structure — the editor was closed. If the tree turns
out to be shaped differently, the C++ change is still correct on its own terms (an ordered goblin has
no business publishing a follow target) but it might not be the whole cause.

**NOT verified: nothing has run.** Written, needs a build, and this ticket's own claim shuts the gate.
No behaviour is claimed.

**The identical shape exists in Frenzy and is deliberately untouched.** A goblin auto-engaging a
threat also has `TargetActor` and `FollowTarget` live at the same time, so the same two branches can
compete with no order involved. Michael reported the Attack-order case; fixing both at once would
make it impossible to tell which change settled what — the discipline #184 used when it refused to
move windup and chain speed in one pass. If the stare-and-break-off persists with no order issued,
that is the next line to change and it is one word.

## Refine

Hoisted the order read above the writes rather than duplicating the call, after noticing the original
asked the subsystem for the verb twice in one refresh.

Left `FollowSlot` publishing unconditionally: it is an int with no decorator on it, so it cannot gate
a branch, and clearing it would only churn the ring index for no benefit.
