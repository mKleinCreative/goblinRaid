---
id: 110
title: Bigger warband, and start the player across the arena from the patrol
agent: claude-gobkit
status: done
claimed: 2026-08-10T00:43Z
build: none
waiting_on:
evaluated: 2026-08-10T00:47:55Z
files: 
  - Config/DefaultGame.ini
  - Content/Maps/Test/L_CombatArena.umap
---

## Goal

Bigger warband, and start the player across the arena from the patrol

## Generate

Michael: "give me the ability to summon more troops and stop spawning us so close together. On the
other end of the arena so I can summon troops first would be preferable." Then, mid-work: **"when I
said troops, I meant human troops."**

**`GS.Combat.SpawnPatrol [militia=4] [archers=2] [knights=0] [distance=2000]`** - a new console
command that adds ordinary human defenders in front of the player. Run it repeatedly to mass a force.

- Defaults are his own squad ruling: 3-4 militia and 1-2 archers. Knights default to **0** because he
  set them as elites, "at most 3-4 on the entire map".
- **No race re-badging.** `GS.Combat.Duel` exists but is a different tool - it spawns two sides and
  re-badges one onto a hostile race so AI fight AI. A human defender is already hostile to a goblin
  player through `RaceTag`, so this spawns them plain.
- Militia alternate `BP_CastleGuard01`/`02` so a line is not six of the same man; archers stand 260uu
  behind the line, knights 160uu in front.
- Ground-snapped per class using each one's OWN capsule half-height, reusing the reasoning already
  written into `GS.Combat.Duel`: a flat additive height spawns them embedded and the spawn handler
  then shoves them to an arbitrary altitude.
- **Distance 2000uu by default, chosen against `BTService_AcquireTarget`'s `AcquireRadius` of 1500** -
  a squad lands outside its own notice range, so it can be stacked up and looked at before it reacts.

**Spacing.** `Arena_PlayerStart` moved from (-400, 0) to **(-2600, 0)**, facing the patrol. The arena
floor is 6000x6000, so the old start was 1000uu from the patrol - inside their 1500uu acquire radius,
which is why the fight began before he could do anything. Now **3040uu**.

**Arrival markers moved** so a summoned warband gathers around him instead of trickling in:
`HordeArrival_01/02` to (-2100, +/-600); all three are now 781-800uu from the player start and
2400-2766uu from the patrol.

## Evaluate

**Distances verified against the actual thresholds they have to beat, not eyeballed:**

```
player start -> nearest patrol member   3040uu   (acquire radius 1500 - out of notice)
HordeArrival_01   781uu from you, 2766uu from the patrol
HordeArrival_02   781uu from you, 2766uu from the patrol
HordeArrival_03   800uu from you, 2400uu from the patrol
```

**I built the wrong thing first and reverted it.** On "summon more troops" I raised the horde config -
`ActiveCap` 10->20, `ReserveMultiplier` 2->3, `SummonsPerBlast` 4->6 - which is *goblins*, and a
departure from GDD §2.5 he had not asked for. Reverted in full; `git diff` on `DefaultGame.ini` now
shows only the pre-existing `HordeGoblinClassPath` line. Recorded rather than quietly dropped, because
the config is exactly where an unrequested design change would survive unnoticed.

**NOT COMPILED OR RUN YET.** A new console command that has never been executed is a command that does
not exist.

## Refine

**Made it repeatable rather than parameterising a single big number.** "More troops" wants a knob he
can turn again, not a bigger constant - so the command spawns a squad and can be run as many times as
he likes, which also keeps each squad readable as a squad.

**Kept the archer rank behind the militia.** Trivial, but a mixed group spawned on one line reads as a
crowd rather than a patrol, and the whole point of the command is to look at a fight.

**Deliberately left undone:** the horn's own summon counts. If he does also want bigger goblin
warbands, those three config values are the place, and they are a one-line edit with no build.
