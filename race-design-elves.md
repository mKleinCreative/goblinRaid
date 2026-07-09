# Elves — Race & Combat Design Reference

**Role in the game:** the second defender faction (not yet implemented in the prototype — this is the design/roadmap spec). Elves are fast, evasive, and magic/ranged-leaning — the counter to slow, heavy-hitting goblin classes (Brute), and vulnerable to fast, precise, or draining attacks.

---

## Design Agent Brief

If you are balancing new elf content, hold this identity:
- **Evasive glass cannons.** Low HP across the board, but high effective evasion against slow/telegraphed attacks. A Brute's Ground Slam or heavy greatclub swing should frequently miss or graze an Elf that's actively moving; a Slasher's fast dagger combo should land reliably.
- **Nature-attuned, so shadow-resistant — but life-force is their real weak point.** Elves are spiritually resistant to curses and shadow magic (it reads as "unnatural" to them and their own magic partially wards it), which is why Blood magic — a direct life-force drain — should hit them harder than any other race.
- **Their homes are wood and canopy.** Like humans, they're vulnerable to fire, arguably more so (tree-perches, wooden watch-structures) — lean into this for their spawn building.
- Don't give Elves high HP to "compensate" for their evasion — the fantasy is "hard to hit, easy to kill once you land the hit," not "tanky." If an Elf archetype is surviving too many hits, tune its evasion/behavior, not its HP pool.

## Archetypes (design targets — not yet coded in the prototype)

| Archetype | HP | Speed | Damage | Role |
|---|---|---|---|---|
| Scout | 16 | 130 (fast) | 5 (melee) / 7 (bow) | Skirmisher, kites, calls in reinforcements when it spots you |
| Ranger | 18 | 100 | 9 (bow, longer range than human archer) | Backline, highest single-target ranged DPS of the three races |
| Warden | 26 | 95 | 14 (nature magic AoE root/entangle) | Support/control — roots goblins in place, ideal Ground Slam bait |

## Spawner Building: Watch-Station (future)

- Wooden tree-perch or canopy platform — **highly flammable**, meant to be the "easy" spawner to shut down with a single well-placed torch rather than a melee fight, contrasting with the Human Barracks (stone, must be melee'd) and the Dwarf Bunker (fire-resistant, needs explosives).
- Spawns Scouts on a short interval; Rangers spawn less often and only while a Watch-Station is undamaged (i.e., damaging it — even before destroying it — should throttle Ranger spawns first, as a partial-progress reward).

## Weapon Matchup Multipliers (goblin damage type → vs Elf, baseline = 1.0x)

| Goblin weapon/damage type | Multiplier vs Elf | Why |
|---|---|---|
| Dagger | 1.3x | Fast, precise strikes land well against low-armor, evasive targets once you connect |
| Bow | 1.1x | Ranged precision slightly favored vs elves, who otherwise out-range goblins |
| Greatclub | 0.8x | Elves are mobile enough to dodge slow, telegraphed heavy swings |
| Shadow magic | 0.7x | Nature-attuned spiritual resistance to curses/shadow effects |
| Blood magic | 1.3x | Life-force drain is especially effective against a race defined by low HP/high vitality-dependence |
| Torch/Fire | 1.4x | Wooden dwellings and canopy homes, highest fire vulnerability of the three races |

## Roadmap notes for the Unreal build

- Evasion should be implemented as a genuine dodge/side-step behavior in the Elf Behavior Tree (triggered by detecting a telegraphed heavy attack), not a hidden to-hit-roll modifier — it needs to be visible and readable to the player so "elves dodge Brute swings" is something players learn, not a stat they never see.
- Watch-Station's "partial damage throttles Ranger spawns" behavior is a good candidate for a Blueprint-exposed threshold (e.g., below 50% HP, disable Ranger spawn calls) so designers can tune it without touching C++.
