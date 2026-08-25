---
id: 302
title: "The statue wards the ground: no portal while it stands"
agent: claude-warren
status: done
claimed: 2026-08-25T03:35Z
build: required
waiting_on: Michael: hold T near the statue (red), topple it, hold T again (green).
evaluated: 2026-08-25T05:16:32Z
observed: 2026-08-25T05:16:16Z | Michael played it: with the statue standing he cannot summon a portal anywhere - holding T refuses - and once he casts the statue down the ground opens and he can plant his gate where he chooses. Before this the T ability was free from the moment he spawned, so the statue could be ignored entirely.
scenario: Michael playing in PIE on L_Tutorial_Island: holding T at and around the spawn with the statue up, then toppling it and placing the portal.
files: 
  - Source/GoblinSiege/Destruction/GSTopplableComponent.h
  - Source/GoblinSiege/Raid/GSWarrenPlacementComponent.h
  - Source/GoblinSiege/Raid/GSWarrenPlacementComponent.cpp
  - Content/Maps/L_Tutorial_Island.umap
---

## Goal

Michael, correcting me: *"by the portal opening, I mean the T ability being bound to the idea that if
there's a statue in the area, you can't use T to summon a portal."*

**I built the wrong thing first and this ticket records both.** My reading was "topple the statue ->
the gate opens itself". His design is the opposite shape and better: the statue is a **suppressor**.
While it stands you cannot summon a gate in the ground it protects; casting it down is the price of
opening one, and the player still chooses where.

## Generate

**Reverted first.** The `AGSRunicSite::SealActor` pin and `HandleSealToppled` are gone -
`GSRunicSite.h/.cpp` are byte-identical to the committed version, confirmed with `git diff`. That was
not tidiness: the pin opened the **extraction** portal on topple, so with it set on Tutorial Island
the player could walk out and end the raid seconds after casting the statue down. Leaving it as dead
opt-in code would have left that trap armed.

**`UGSTopplableComponent::WardRadius`** (default **0**, meaning wards nothing) and
`IsWarding()` = standing AND warding.

**On the MONUMENT, not on the placement component.** A village shrine and a great seal should not ward
the same distance, and a level with two of them needs two answers. The 0 default also makes warding
**opt-in**, so every existing topplable behaves exactly as before rather than silently starting to
block placement.

**`EGSWarrenPlacementBlock::WardedByStatue`** - a new reason in the enum that already existed, so the
ghost turns red and *stays* red with a readable cause. That enum is the whole point: the component was
already built to explain refusals rather than just refuse.

**Checked beside the cooldown, not with the ground tests**, because it is the same KIND of refusal -
the spot is fine, the world says no. The player reads the rule off a red ghost instead of pressing a
dead key.

**Wards are gathered when T goes DOWN**, not per tick and not once at BeginPlay. Per tick would walk
all 9,000+ actors of Tutorial Island every frame the ghost is up. Once at BeginPlay would go stale in
the one case that matters - a statue toppled mid-raid. `IsWarding()` is asked at test time, so a statue
that falls while the key is held stops warding immediately.

**2D distance, deliberately.** The statue sits in a graveyard well below the hamlet it guards, and a 3D
test would let the player climb the slope and plant a gate 900 uu away on the map but "outside" the
ward purely on height.

Set on Tutorial Island: `GS_Seal_Warrior` wards **6000 uu**.

## Evaluate

**Build succeeded in 4:34.** Runtime, PIE on `L_Tutorial_Island`, same spot 1500 uu from the statue:

```
BEFORE topple   reason=WARDED_BY_STATUE   valid=False   warding=True
AFTER  topple   reason=NONE               valid=True    warding=False
```

**The ward is bounded, not global** - probed from the gate 12,465 uu away and the reason there is
`OBSTRUCTED` (the portal props), not `WARDED_BY_STATUE`. So the statue refuses its own ground and
nothing else.

**NOT ESTABLISHED: Michael has not held T himself.** Every probe above drives
`BeginPlacement`/`GetBlockReason` from script. What nobody has seen is the thing that matters - the
ghost going red near the statue, the player casting it down, and the ghost going green.

## Refine

**The misread is worth keeping in the record.** "Topple opens the gate" and "the statue forbids gates"
produce the same sentence in a plan - *the statue controls the portal* - and completely different
games. One is a cutscene trigger; the other is a rule the player can discover, test and route around.
I built the first from a plan bullet and Michael had to say it twice.

**Deliberately not done:** no HUD string for the new block reason (the enum carries a DisplayName,
"A standing statue wards this ground", but nothing renders it yet), no ward visualisation in play, and
no second warded monument. The ward radius of 6000 is a first guess, not a tuned number - it covers the
graveyard and the hamlet above it, and is one field on the statue to change.

---

## The radius was wrong, and Michael found it in one play

*"I was able to put the portal down."* The rule fired correctly and meant nothing: the statue sits
**12,475 uu** from the player's spawn and the ward was **6,000**, so T was free from the moment he
started and the statue could be walked past entirely.

**Now global.** `bWardsEntireLevel`, default true - while the monument stands, the land is sealed.
Michael's own fiction had already said so and I had not read it as a specification: *"the seal that
prevents chaos magic from happening IN THE LAND"*. Not in a circle around itself.

The placement check now asks the monument `WardReaches(Spot)` rather than measuring distance itself,
so the reach rule lives in one place and cannot drift. `WardRadius` survives for levels with several
shrines and is consulted only when the global flag is off.

**Verified at the distance that broke it:**

```
statue STANDING, at spawn 125 m away   reason=WARDED_BY_STATUE   valid=False
statue TOPPLED,  same distance         reason=NONE               valid=True
```

**The lesson, recorded because it is general:** a rule the player can trivially stand outside is not a
rule. The first version was tested at 1,500 uu - inside its own radius - where it passed perfectly.
Testing it where the player actually starts would have caught it before Michael did.
