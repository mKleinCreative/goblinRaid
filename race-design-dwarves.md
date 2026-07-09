# Dwarves — Race & Combat Design Reference

**Role in the game:** the third defender faction (not yet implemented in the prototype — this is the design/roadmap spec). Dwarves are slow, heavily armored, and fire/blunt-resistant — the counter to goblin torches and greatclub knockback, and vulnerable to the things armor doesn't stop: curses and explosives.

---

## Design Agent Brief

If you are balancing new dwarf content, hold this identity:
- **Armor beats the physical, not the arcane.** Heavy plate should meaningfully reduce dagger and bow damage, and forge-culture should make them notably fire-resistant — but that same armor does nothing against Shadow magic (which ignores physical defense and preys on claustrophobia/dark-fear, a very "dwarf" psychological weak point) or a well-placed explosive barrel (which bypasses armor entirely via blast force).
- **Slow and heavy is the whole point.** Don't give Dwarves human-tier speed "for fairness" — their low mobility is what makes the Slasher's mobility and the Brute's knockback both feel good against them in different ways (Slasher kites them, Brute's Ground Slam concusses through their armor).
- **Ground Slam is intentionally their hard counter.** A Brute's Ground Slam should be the single best goblin tool against Dwarves — concussive AoE ignores the part of their armor rating that stops clean weapon strikes. Don't let a future dwarf archetype no-sell this without a very deliberate, telegraphed "brace" ability.
- **Explosives are a second hard counter.** Barrel chain-reactions should do full or near-full listed damage to Dwarves (no armor mitigation on blast damage) — this rewards players who lure Dwarves near destructible barrels rather than trading blows head-on.

## Archetypes (design targets — not yet coded in the prototype)

| Archetype | HP | Speed | Damage | Role |
|---|---|---|---|---|
| Shieldbearer | 90 | 60 (slow) | 10 (melee, but blocks frontal hits) | Frontline wall, forces goblins to flank or use AoE |
| Hammer-dwarf | 70 | 65 | 20 (melee, heavy stagger) | Their answer to the goblin Brute — trades blows, wins if allowed to close |
| Engineer | 45 | 70 | 8 (melee) / mines & turret placement | Support — plants proximity charges, most dangerous to careless torch-throwers standing near their own fire |

## Spawner Building: Bunker/Forge (future)

- Reinforced stone-and-iron construction — **highly fire-resistant** (forge culture; expect low or no burn damage-over-time to apply), meant to be the "hard" spawner: torches barely dent it, so it must be melee'd/greatclub'd down or, ideally, blown up with a lured barrel explosion for a satisfying counterplay loop.
- Spawns Shieldbearers on a slow interval; if an Engineer is present nearby, it periodically reinforces the Bunker's HP (small heal-over-time) — giving players a reason to prioritize the Engineer first, mirroring the Human Captain's buff-aura role.

## Weapon Matchup Multipliers (goblin damage type → vs Dwarf, baseline = 1.0x)

| Goblin weapon/damage type | Multiplier vs Dwarf | Why |
|---|---|---|
| Dagger | 0.7x | Heavy plate deflects fast, low-force strikes |
| Bow | 0.8x | Armor mitigates piercing damage, though less than it stops blades |
| Greatclub | 1.3x | Ground Slam-style concussive force bypasses armor rating — the intended hard counter |
| Shadow magic | 1.3x | Curses ignore physical armor entirely; dark-fear is a real dwarven psychological weak point |
| Blood magic | 0.8x | Hardy vitality and thick blood make life-drain effects less efficient |
| Torch/Fire | 0.6x | Forge-born fire resistance — the lowest fire vulnerability of the three races |

## Roadmap notes for the Unreal build

- Blast damage (barrels, future goblin explosives) should be flagged as a distinct damage type in GAS that explicitly skips the armor-mitigation calculation, rather than just being "high damage" — this keeps the "explosives bypass armor" rule enforceable in code, not just in design intent.
- The Engineer's Bunker-repair aura and the Human Captain's attack-speed aura should share a common `ABuffAura` component so both races' support units are built the same way under the hood, even though their effects differ.
