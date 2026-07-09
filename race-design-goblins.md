# Goblins — Race & Combat Design Reference

**Role in the game:** playable raider race. Goblins are the "attacker" faction — fast, fragile, numerous, and dependent on fire, terrain abuse, and squad tactics to punish the slower, better-armored defender races (Humans, Elves, Dwarves).

---

## Design Agent Brief

If you are balancing new goblin content (weapons, classes, abilities), hold this identity:
- **Individually weak, collectively dangerous.** No goblin archetype should out-tank a defender archetype 1v1 at equal gear tier. Their edge is mobility, numbers (companions/co-op), and the torch/fire economy.
- **Every kit answers "how does this class use fire or terrain?"** If a new class or weapon doesn't interact with the torch/destructible-prop system in some way, reconsider it before shipping.
- **Lives, not permadeath.** Goblins get up again — this is a horde-attacker power fantasy, not a Souls-like permadeath game. Keep respawn frictionless (short timer, partial HP) so downtime never exceeds ~15% of a raid.
- When changing a number here, check it against the race matrix at the bottom of this doc — a buff to goblin fire damage must be re-checked against every race's fire resistance value, not just Humans.

## Core Identity

- **Lives system:** 5 lives per goblin (player and each AI companion), independently tracked. Losing all HP costs one life and respawns after a short delay; losing the last life ends that goblin's raid (player: raid over: full game-over; companion: gone for the rest of the raid).
- **Universal torch toss:** every class can throw torches regardless of build. This is the goblins' signature racial trait — fire is cheap, plentiful, and the great equalizer against armored, high-HP enemies.
- **Alarm/heat pressure:** goblins are the aggressors; the world reacts to them, not the other way around. This asymmetry (goblins have an alarm meter, defenders do not) is intentional and should stay unique to the goblin campaign.

## Classes & Weapons

| Class | Weapon(s) | Base HP | Speed | Role |
|---|---|---|---|---|
| Slasher | Dual daggers ⇄ hunting bow (swap live) | 110 | 190 (fast) | Mobile skirmisher, flex melee/ranged |
| Brute | Great-club | 140 | 135 (slow) | Frontline breaker, best at destroying cover |
| Shaman (Shadow path) | Staff — Blink Strike | 90 | 160 | Repositioning striker, picks off backline |
| Shaman (Blood path) | Staff — Blood Nova | 90 | 160 | Self-sustain AoE, high-risk/high-reward |

**Weapon-swap design rule:** the Slasher's melee/bow toggle is the template for future flex-weapon classes. When adding a new swappable weapon: the ranged mode should trade damage-per-second for range/safety, and the swap itself should have a brief (0.1–0.2s) input lock so it can't be used as a frame-perfect combat cancel.

## Damage & Utility Types (used in the cross-race matrix)

- **Dagger** — fast, low-per-hit, bleed-flavored (future: apply a damage-over-time bleed stack)
- **Bow** — precise, moderate single-target, no AoE
- **Greatclub** — heavy, staggers, best vs cover/armor
- **Shadow magic** — curse/mobility, debuffs, ignores physical armor
- **Blood magic** — drain/AoE, self-cost, ignores physical armor
- **Torch/Fire** — AoE damage-over-time, ignites flammable terrain, the goblins' core equalizer

## Resistances (how hard it is for defenders to hurt goblins back)

Goblins have **no innate resistances or weaknesses** — they're the baseline the other three races are balanced against. This keeps the matrix legible: every entry in the Human/Elf/Dwarf docs is a multiplier relative to "how this attack performs against an unarmored, average-HP goblin."

## Roadmap notes for the Unreal build

- Lives/respawn should live on PlayerState (per-player) so co-op respawns don't block other players.
- The alarm meter should be a single GameState-replicated value shared by the whole squad, not per-player, to preserve the "we alerted the city together" co-op tension.
