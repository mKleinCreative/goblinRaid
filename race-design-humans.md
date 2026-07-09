# Humans — Race & Combat Design Reference

**Role in the game:** the baseline defender faction, already implemented in the browser prototype as the militia/archer/knight enemies. Humans are generalists — no extreme strength or weakness, disciplined formations, and the only race (so far) with a spawner building (Barracks).

---

## Design Agent Brief

If you are balancing new human content (new archetypes, the eventual Unreal AI, weapon drops), hold this identity:
- **Humans are the control group.** Every multiplier in their matrix row is close to 1.0x on purpose — they are what "average" looks like, so Elves and Dwarves can be defined as deviations from them.
- **Formation and numbers, not individual power.** No single human archetype should be a hard counter to any one goblin class. Their threat comes from garrisoning (Barracks) and mixed-unit waves (archer + militia + knight together), not from any unit being individually oppressive.
- **Fire is their real weakness.** Wooden granaries, thatch roofs, and wooden cover are all human-built — lean into "their own city is flammable" as the thematic and mechanical answer to goblin torches, rather than giving humans a personal fire vulnerability stat.
- When adding a new human archetype, give it a clear formation role (see table) before giving it new numbers — a "what does this unit do that militia/archer/knight don't" test.

## Archetypes (militia/archer/knight are implemented in the current prototype)

| Archetype | HP | Speed | Damage | Attack CD | Range | Role |
|---|---|---|---|---|---|---|
| Militia | 30 | 95 | 8 | 1.0s | 32 (melee) | Basic frontline filler, dies fast, arrives in numbers |
| Archer | 20 | 75 | 6 | 1.5s | 190 (ranged) | Backline harasser, kites at range, priority target |
| Knight | 75 | 72 | 16 | 1.3s | 36 (melee) | Heavy tank, arrives once Alarm escalates (horde waves) |
| *Captain (future)* | ~90 | 80 | 12 | 0.9s | 40 | Buffs nearby militia (faster attack speed), priority kill to weaken a wave |

## Spawner Building: Barracks

- Stone construction, **not flammable** — must be melee'd/ranged down (220 HP in the prototype), which is the intended human-specific counterplay to goblin torches (you can't just burn this one).
- Spawns a fresh militia/archer on a fixed interval (9s in the prototype) while it stands.
- Destroying it doesn't reduce the current Alarm meter, but does raise it briefly (a Barracks falling is loud) — this is a deliberate risk/reward: kill it early to stop the bleed, but expect a small alarm spike as payment.

## Weapon Matchup Multipliers (goblin damage type → vs Human, baseline = 1.0x)

| Goblin weapon/damage type | Multiplier vs Human | Why |
|---|---|---|
| Dagger | 1.0x | No modifier — baseline |
| Bow | 1.0x | No modifier — baseline |
| Greatclub | 1.1x | Breaks formation/stagger works well on drilled but lightly-armored troops |
| Shadow magic | 1.0x | No modifier — baseline |
| Blood magic | 1.0x | No modifier — baseline |
| Torch/Fire | 1.3x | Wooden granaries, thatch, and morale break under fire — humans fear burning their own city |

## Roadmap notes for the Unreal build

- Barracks-style spawners generalize well to a `ASpawnerActor` base class other races' spawn buildings (Elf watch-towers, Dwarf forges) can subclass, sharing the "destroy to silence" pattern.
- Captain buff-aura archetype is a good first AI-Direction test for Behavior Tree + EQS group coordination once you're in Unreal.
