---
id: 212
title: BT_HordeGoblin's AcquireTarget service has bSelectTarget ON, so it overwrites the ordered target every rescan and the goblin flickers between attacking and following
agent: claude-hordetarget
status: done
claimed: 2026-08-20T21:02Z
build: none
waiting_on:
evaluated: 2026-08-20T21:05:16Z
observed: UNOBSERVED 2026-08-20T21:05:16Z - Verified as data - bSelectTarget reads False from a reloaded asset - but nobody has watched an ordered goblin commit instead of staring. Closes so 213 can claim the same file; Michael tests the behaviour next.
scenario: none - never run
files: 
  - Content/AI/BT_HordeGoblin.uasset
---

## Goal

BT_HordeGoblin's AcquireTarget service has bSelectTarget ON, so it overwrites the ordered target every rescan and the goblin flickers between attacking and following

## Generate

One property on `BT_HordeGoblin`'s root-selector service: `BTService_AcquireTarget.bSelectTarget`
**True -> False**. Asset data, no build. Backup at
`_PristineBackups/BT_HordeGoblin_pre212_20260820.uasset`.

**The service was configured against its own documented contract.** `BTService_AcquireTarget.cpp`
says, verbatim: *"With bSelectTarget off, whatever wrote TargetActor owns it (AGSHordeAIController
writes it from the horde's threat registry). Read it, never clear it - clearing here would fight that
writer every tick and the goblin would flicker between Frenzy and Follow."* It was ON, so every
`ReacquireInterval` the service rescanned for the nearest hostile and overwrote - or **cleared** - the
`TargetActor` the controller had just published from the standing order.

That is precisely Michael's report: *"some went right away to attack, but goblins facing me tend to
not want to attack."* Goblins with a hostile in range happened to agree with the order and committed.
Goblins near the player had their target cleared, fell through to `Follow Summoner`, and stood facing
him.

It also contradicted the architecture beyond this order: horde goblins are **designed to have no
senses of their own**, with the subsystem feeding them stimuli (GDD 3.1 - the horde agent's "quietest
performance trick", and the reason ten concurrent goblins are cheap). A service selecting its own
targets undoes that whether or not an order exists.

## Evaluate

**Found by reading the tree, not by guessing.** Walked `BT_HordeGoblin`'s root node in the live
editor and printed the service's flag: `bSelectTarget=True`. The C++ comment then explained the
symptom without any inference on my part - it names the exact failure mode, "flicker between Frenzy
and Follow", as the consequence of this flag being on.

**Verified against a reloaded asset**, not a read-back of the in-memory object: True before, False
after, confirmed after `collect_garbage` and a fresh load.

**NOT verified: the behaviour.** Nobody has watched an ordered goblin commit. This closes as data,
not as a fix - Michael tests it next.

**A trap worth recording:** the property is `bSelectTarget`, **not** `b_select_target`. An earlier
probe tried both names and printed a hardcoded label, which hid which one had actually worked and
cost a round trip when the setter used the wrong one.

**This is the second half of the same bug as #210** and they should be read together: #210 stopped
the CONTROLLER publishing a follow target during an order; this stops the SERVICE clearing the attack
target. Either one alone leaves a goblin with a reason to oscillate.

## Refine

Nothing changed on review. Considered whether turning selection off would leave Frenzy blind - it does
not: the controller writes `TargetActor` from the horde threat registry in both the ordered and the
auto-engage case, which is exactly the arrangement the service's comment describes as correct.

**Left undone and raised as #213:** `Hold` still lands nowhere. The tree is a flat Selector of Block,
Melee Attack, Menace Orbit, Chase Target, Follow Summoner, Wait - **no branch reads `OrderVerb` at
all**. Attack only ever appeared to work because it writes `TargetActor`, which Chase Target already
consumes.
