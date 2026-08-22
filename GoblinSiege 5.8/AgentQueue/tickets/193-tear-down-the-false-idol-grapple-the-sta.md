---
id: 193
title: Tear down the false idol: grapple the statue and haul it over by walking away
agent: claude-idol
status: done
claimed: 2026-08-19T00:23Z
build: none
waiting_on:
evaluated: 2026-08-19T03:45:07Z
observed: 2026-08-19T03:44:44Z | Michael played the full chain and confirmed it works: hook the statue, walk backwards, the rope stays attached and holds him inside its reach, the ring fills, and the monument topples. His words: 'it finally works.' The rope renders as a spline mesh running hook to goblin, and the intact statue swaps to the fractured collection at the moment it goes over.
scenario: PIE in L_CombatArena, BP_Statue_Warrior_TEST at (-1700,380,-70) with GC_Statue_Warrior (44 chunks), grapple thrown from the Q wheel slot, hauled by walking away past 120uu of slack against a 260uu rope limit
files: 
  - Source/GoblinSiege/Destruction/GSTopplableComponent.h
  - Source/GoblinSiege/Destruction/GSTopplableComponent.cpp
  - Source/GoblinSiege/Weapons/GSGrappleHaulComponent.h
  - Source/GoblinSiege/Weapons/GSGrappleHaulComponent.cpp
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Content/Blueprints/Grapple/BP_GrappleHook.uasset
  - Content/Blueprints/Destructibles/BP_Statue_Warrior.uasset
---

## Goal

Tear down the false idol: grapple the statue and haul it over by walking away

## Generate

Michael: *"It pulls it down. I want to give people the idea of tearing down false idols in favour of
their dark lord."*

That sentence rules out the obvious build. Hitting a statue four times with a sword is demolition work;
iconoclasm needs resistance - a rope, a heave, a lean, and the thing coming down under its own weight.
Two new components, split along the line the project already uses for breakables:

**`UGSTopplableComponent`** (on the monument) owns what toppling MEANS. `Topple(Toppler, PullDirection,
AnchorPoint)` promotes the collection to dynamic and applies an impulse **at the rope's anchor**, not at
the centre of mass - that moment arm is the difference between rotating about the base and sliding
sideways. `ToppleLift` (0.35) tilts the push upward so the mass clears its balance point instead of
scraping.

**It deliberately does NOT call `Break()`.** `Break()` delegates to ACF's `ForceDestruction`, which
shatters the collection where it stands - a statue exploding at the top of its lean. The fall is the
point, so the shatter is left to `bEnableDamageFromCollision` on landing.

**`UGSGrappleHaulComponent`** (on the player) owns the act of pulling. `NotifyHookAttached` records the
anchor and the distance at that instant; the tick measures whether the player has walked past
`AnchorDistanceAtAttach + TautSlackUU` (120uu) and fills or decays progress accordingly. At 1.0 it calls
`Topple` with a pull direction of anchor-to-player, so **the monument comes down towards whoever hauled
it**.

Michael chose walking away over holding a button: it needs no binding and puts the player's body into
the act. The 120uu slack exists because a zero threshold would tick from idle sway and topple statues
because somebody shuffled. Decay-not-reset on slack means a stumble does not throw away the heave.

**The haul lives on the player, not the statue**, because progress is a property of the puller: it
drives their ring, must survive the statue dying mid-haul, and two goblins hauling two monuments must
not share a counter. It is added via `BP_GSPlayerCharacter` rather than natively because
`GSPlayerCharacter.h/.cpp` are claimed by #186 - the HUD finds it by `FindComponentByClass`, exactly as
it already finds the interaction and horde-command components.

**The ring is reused, not reinvented** - `HandleHaulStarted/Progress/Ended` call the same
`SetChannelProgress` / `ShowChannelRing` the interaction channel uses, and the ended handler defers to
`HandleChannelEnded` so the completion hold is shared rather than reimplemented.

## Evaluate

**OBSERVED AND WORKING.** Michael played the whole chain - hook, haul, rope, topple - and confirmed it:
*"it finally works."*

**The statue not falling took FOUR wrong guesses before anyone measured it**, and that is the finding
worth keeping from this ticket:

| guess | what it changed | result |
|---|---|---|
| `Chaos_Object_Static` | collection had no dynamic proxy to wake | still nothing |
| `Chaos_Object_Kinematic` | gave it a proxy | still nothing |
| `MaxSimulatedLevel = 0` | fall as a figure, not gravel | still nothing |
| defer the impulse one tick | promotion lands before the shove | still nothing |

**The actual cause, found in one PIE probe: the collection's particles were ANCHORED.** An anchored
geometry collection is pinned regardless of everything else - it reported `is_active=true`,
`object_type=Dynamic`, `simulating=true`, `root_broken=false`, and would not move or even fall under
gravity. `RemoveAllAnchors()` plus the SAME impulse produced 185.9 uu/s and 61.9 deg/s immediately.

Every property changed before that was already correct. That is precisely why each fix "worked" and
nothing happened, and why the log kept reporting a successful topple - **the old log line announced the
impulse being dispatched, not the statue moving.** It said so three times while the monument stood
there. The SWAP / OUTCOME split now reports those as two separate facts, and it is what finally located
this.

A second real defect surfaced in the same probe: `auto_activate` was false on the collection, so the
component never activated.

**Michael's design change is what made the feature tractable**, and it should be recorded as his: an
intact static mesh swapped for the collection at the moment of the topple. A standing monument that is
just a mesh cannot crumble at level start, cannot be caught in the wrong Chaos state, and costs nothing
to render. It is the same pattern the crate and barrel already used - which this agent had written -
and it was not reached for until he asked for it.

**Damage done to BP_GrappleHook along the way, and it was avoidable twice over:** a pinless orphan node
left behind after three failed attempts to create a self-reference, which stopped the Blueprint
compiling and left HookMesh/Movement/RopeMesh invalid at runtime; and an exec cycle that hung the game,
created by moving a node without clearing its old outgoing link. Both stem from one property - **an exec
output holds exactly one connection, so wiring to it silently replaces what was there** - and both were
unrecoverable, because the asset was untracked and autosave is disabled project-wide (#160). It is now
committed (37c2362).

## Refine

**Changed after self-review:** `Topple` first applied its impulse at the actor origin, which on a 588uu
statue is near the base - a shove there slides the monument rather than tipping it. Moved to the rope
anchor, which is both physically right and free, since the haul already knows where the rope bit.

**Deliberately not doing: the two-level fracture.** It is very likely needed for the fall to read as a
topple rather than a mid-air disintegration, but changing the collection AND introducing the haul at once
means an ugly result has two candidate causes. Watch the single-level version first.

**Deliberately not doing: `AGSObjective_ToppleStatue` integration.** `OnToppled` is broadcast and
unbound; #189 renamed that class and is still at review, and wiring into another agent's unclosed rename
is how two agents collide.

**Deliberately not doing: rope tension visuals.** The rope is instanced segments drawn by the hook; making
it visibly taut is worth doing and is not needed to prove the mechanic.
