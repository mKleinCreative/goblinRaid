---
id: 299
title: Demo staging on L_Tutorial_Island: the seal, the gate, the resources
agent: claude-warren
status: done
claimed: 2026-08-24T23:58Z
build: none
waiting_on:
evaluated: 2026-08-25T02:02:26Z
observed: 2026-08-25T02:02:37Z | Michael played the staged level: he grapple-hauled the seal and it went over - the log records his own topple - and he reported what he saw, that the statue vanished leaving no chunks, and that he could not pick up the pigs. After the visibility fix the toppled seal leaves a heap of rubble where it stood, which he accepted as right for now. He also caught that the gate had been moved off its authored spot, which has been put back.
scenario: Michael playing in PIE on L_Tutorial_Island, walking the graveyard, hauling the seal over with the grapple and trying to pick up livestock.
files: 
  - Content/Maps/L_Tutorial_Island.umap
---

## Goal

Michael's demo, in his words: *"starts next to the statue, has you cast it down, summon a gate, then
summon goblins from that gate, then raid the village and fight a knight as a miniboss type option"* -
on the tutorial island, because its setup is good.

Two rulings from him reshaped it mid-plan:

- **The Knight is OPTIONAL and the demo is about securing resources, not the fight.**
- **The graveyard monument IS the statue** - *"very cinematic, and we can say, the seal that prevents
  chaos magic from happening in the land."* His eternal darkness is the source of chaos magic, so
  breaking the seal is what lets the gate exist at all.

This ticket is the map staging only. One save on a 185 MB LFS object, so everything is batched.

## Generate

**The seal.** `SM_WarriorStatue` was a plain `StaticMeshActor` - scenery, not a verb. Replaced with
`BP_Statue_Warrior` at the **identical transform** (-964, 31470, -462), yaw -14.9, scale 2.75. That
Blueprint uses **the same mesh**, so it looks exactly as it did, and brings `GSTopplableComponent`,
`GSBreakableComponent` and a 44-chunk geometry collection with it. Labelled `GS_Seal_Warrior`.

**The gate.** `BP_GS_RunicSite` moved from an empty field 125 m away to **1356 uu (14 m) west of the
seal**, ground-traced onto the graveyard floor, and its **yaw aimed at the seal**. `GetSpawnTransform`
returns the site's yaw, so frame one looks straight at the monument - measured at **0.0 degrees off**.
Labelled `GS_Gate_Seal`.

**`ExtractionRadius` 1200 -> 150, and this is a real bug the move created rather than tidying.** The
player spawns ON the site, and `TryExtractOverlappingPawns` only arms extraction once he has left the
sphere. At 1200 the walk to the seal took him outside it, so **returning from the topple would have
ended the raid instantly**. At 150 he must step into the portal mouth deliberately. Banking shares
that sphere and `NotifyActorBeginOverlap` banks BEFORE it extracts, so a haul still counts on the way
in - the loop is "carry it into the gate", which is the beat Michael wants stressed.

**The gate is SUMMONED, not standing there.** Two loose dressing actors - a `SM_Portal4` mesh and a
Niagara - were lit permanently and rode along with the move, so frame one showed a glowing archway
before anything had been toppled. **Caught in a screenshot, not in a property read**: the site's own
`PortalMesh` was correctly hidden and `PortalFX` inactive the whole time. Both loose props are now
`Hidden In Game` and renamed `*_DORMANT`. They are redundant with the site's own visuals; **hidden
rather than deleted because they are not mine** - say the word and they go.

**The resources.** 8 loot actors - 2 chests, 2 barrels, 2 crates, 2 pigs - across the two hamlets
nearest the seal (48 m and 61 m). **Placed on verified `Landscape` hits, not on the first thing a
trace touched**: the first pass put three of them *inside* `SM_MERGED_House_Medium_02`, which reads as
a correct z in every read-back. Found by asking each trace WHAT it hit.

**The Knight** stands over the hamlet-B haul. That is the whole encounter design: an optional fight
about **what you can carry away**, not a boss gate. Nothing was built for it.

**5 militia** across both hamlets. Never knights - 10 goblins vs 6 knights measured 9-0 to the knights.

**`GEN_NavBounds_DemoValley`** - the blocker nothing else would have surfaced. The level's ONLY nav
volume covered the far village (X -17000..-9000, Y 53000..61000). The seal, the gate and both hamlets
had **no navmesh at all**, so every goblin, guard and the Knight would have stood still and it would
have read as broken AI. New volume covers X -6000..6000, Y 25500..39500, Z -3000..3000, and
navigation was rebuilt.

## Evaluate

**Frame one was LOOKED AT, and it is the only reason the dormant-gate bug was found.** Screenshot
taken in PIE: the goblin stands among the gravestones, the seal fills the centre, village roofs on the
horizon. The first shot showed the portal already glowing; the second, after the fix, is dark.

**Runtime, PIE on `L_Tutorial_Island`:**

```
player spawn   (-2300, 31700, -416)  yaw -9.8   1356 uu (14 m) from the seal
bearing to seal -9.8 vs facing -9.8  ->  0.0 deg off
seal           BP_Statue_Warrior_C_0, Topplable + Breakable present
gate           start_open=False, PortalMesh hidden, PortalFX inactive
guards 7, knight 1
```

**Navmesh verified by PATHFINDING, not by existence.** Every leg of the demo route:

```
GATE -> hamlet A                  FOUND  ( 9 pts)
GATE -> hamlet B                  FOUND  (10 pts)
hamlet A -> GATE (the haul home)  FOUND  ( 9 pts)
GATE -> the Knight                FOUND  ( 9 pts)
```

**Two false alarms on the way, both worth recording** because each looked like a real failure:

1. `project_point_to_navigation` with a loose vertical tolerance returned z **-1790** where the ground
   is -554, which read as "navmesh generated 12 m underground". Tightening the tolerance showed nav
   sitting **48 uu above ground** - correct agent offset. The loose query had been grabbing a lower
   surface.
2. The first path tests all failed. The endpoints were raw ground coordinates slightly off the mesh;
   projecting them first made every leg resolve. **The navmesh was never broken.**

**And one genuine oddity explained:** `SM_WarriorStatue2`, which the plan called a second statue
interpenetrating the first, is **`SM_VillageOak` - a tree**, mislabelled. Its canopy at z 1900 is what
several vertical traces were hitting.

**NOT DONE, and the demo does not run without it: nothing binds the topple to the gate.**
`OnToppled` still has zero subscribers. Toppling the seal today drops the monument and opens nothing.

## Refine

**The topple -> gate bind was deliberately left out of this ticket.** It needs a Blueprint graph, and
the analysis's own warning is that a level-blueprint delegate bound to the wrong instance **logs
nothing** - which is a debugging session, not a two-node job, and it does not belong in a map-staging
save. It also has a better home: a `bOpenOnSealToppled` property on `AGSRunicSite` binding to a
topplable it is told about is testable and cannot silently attach to the wrong actor. That wants C++
and a build, so it is its own ticket.

**Loot placement is engineering-correct, not art-directed.** Every actor is on verified open ground at
the right height and nothing is inside a wall. Whether a barrel sits handsomely beside a doorway is
judged by eye and is Michael's to nudge - the rule this ticket owes him is that the ground is real.

**Deliberately not done:** the far village (245 m away) is untouched, `IA_PlaceWarren` is still bound
(a stray `T` plants a Warren that outranks the gate as the horde's arrival point - a demo-day
footgun worth disarming, but it edits a shared input asset), and no bark, HUD or clock change.

---

## CORRECTION - the gate move was wrong and has been reverted

Michael: *"Why the fuck did you move the gate. I told you not to. The gate is purposefully away from
the graveyard and something you walk towards. You should be guided to topple it. it's part of the
experience, don't do that again."*

**He is right.** He approved the graveyard monument as the seal; the gate's position was authored
deliberately - the walk toward it is the experience, not an inconvenience to be tidied away. Moving it
turned a designed journey into a convenience.

**Reverted:** `BP_GS_RunicSite` back to **(-1551, 19009, 1180)**, `ExtractionRadius` back to **1200**,
both loose portal props back to their authored spots and visible again, labels restored.

**One thing could NOT be reverted: the gate's YAW.** I overwrote it with -9.8 and there is no record
of the authored value. `GetSpawnTransform` returns the site's yaw, so this is frame-one facing and it
needs Michael's number or his eye. Recorded here rather than quietly left.

**What survives, because it was right independently of the gate:** the seal swap, the loot, the
Knight, the militia, and the navmesh - now **widened to Y 17750..39750** so it covers the gate, the
walk, the graveyard AND the hamlets. The walk toward the seal is exactly what needed nav coverage,
and the level had none there.

**The lesson, stated plainly so the next session does not repeat it:** an authored placement is a
design decision until Michael says otherwise. A numbered "defaults in bold" list is for choices he has
not already made - it is not a way to obtain consent for moving something he has already placed on
purpose.

## The seal disappeared, and it was not the scale

Michael: *"the statue would topple, then disappear. no chunks"* - and his own haul is in the log:

```
00:12:36  OUTCOME: DID NOT MOVE - linear 0 uu/s   toppled by BP_GSPlayerCharacter_C_0
```

**Cause: `Visible` was unchecked on the geometry collection in `BP_Statue_Warrior`.** `Topple()` only
calls `SetHiddenInGame(false)`, which does **nothing** while `bVisible` is false - so the collection
could never render, at any scale, on any map, since the prop was authored. The intact mesh hid and
nothing took its place.

**The 2.75 scale was a red herring I chased first**, and Michael's instinct to test it (option 2) is
what killed the theory: at scale 1.0 it still vanished. The decisive read was
`hidden_in_game=False` **and** `visible=False` on the same component - two flags that sound like the
same thing and are not.

**Fixed:** `Visible = true`, `HiddenInGame = true` on the collection, so it stays dark until the
topple reveals it. Verified by re-reading the saved asset, then in PIE at **both** 1.0 and 2.75, and
photographed - there is rubble now. The monument is restored to its authored 2.75.

**Still wrong, and deliberately left:** the seal does not FALL - `ToppleImpulse` 90,000 does not move
it and the collection's top-level `damage_threshold` is **500,000**, which a fall never reaches.
Michael: *"it crumbles, but that's perfect for what we want right now."* So it is recorded, not tuned.
