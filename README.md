# goblinRaid
# GOBLIN SIEGE — Game Design Document

*Working draft for review · reflects the browser prototype as currently built*

---

## 1. Vision

**Goblin Siege is a co-operative hack-and-slash in which a warband of goblins invades fortified cities to loot, burn, and destroy before the defenders overwhelm them.**

The fantasy is being the monster at the gates: individually weak, collectively terrifying, armed with fire and numbers. Players pick a goblin archetype, drop into a defended city with a squad, tear through waves of militia while smashing and burning the environment, complete a raid objective, and extract under mounting pressure.

### Design pillars

- **Weighty, readable combat.** Attacks commit, dodges commit, enemies telegraph. Every hit has impact — hitstop, knockback, stagger. You win by reading the fight, not mashing.
- **Fire and destruction as a language.** Torches, explosive barrels, and burnable structures are always available and always useful. The city is flammable and the goblins know it.
- **Pressure, not safety.** An alarm/heat system means the world escalates the longer you stay. Greed (more loot, more destruction) fights against survival (get out clean).
- **A squad that feels like a warband.** Co-op is the target experience; even solo, AI companions fight at your side.

### The three touchstones

- **Darktide** — relentless horde combat, class identity, mixed-enemy waves that demand target prioritization.
- **Helldivers** — mission-and-extraction structure, escalating reinforcement pressure, shut-down-the-spawner objectives, friendly-fire-adjacent chaos from your own fire.
- **Elden Ring** — deliberate, stamina-gated melee with dodge i-frames and enemy telegraphs; combat you read rather than out-click.
- *(Environmental touchstone)* **Enshrouded** — smashable, burnable structures that reshape the play space.

---

## 2. Core gameplay loop

1. **Choose a raider** (class, and for the Shaman a magic path) at the camp/loadout screen.
2. **Drop into the city** with a squad (co-op players and/or AI companions), 5 lives each.
3. **Fight and destroy** through the defended streets — militia, archers, knights, and spawner buildings.
4. **Complete the objective** — currently: burn all 3 granaries.
5. **Extract** — the extraction gate unlocks once the objective is done; reach it to win.
6. **Bank rewards** (gold, loot) and upgrade between raids.

Throughout, the **Alarm meter** climbs from combat, fire, and destruction. At maximum it triggers a horde wave and escalates the tier of reinforcements — the core tension that turns a slow, greedy raid into a desperate one.

---

## 3. The goblins — playable classes

All goblins share the universal **torch toss** and the **5-lives** system (see §4). Values below are the current prototype tuning and are expected to move with playtesting.

### Slasher — mobile skirmisher / flex melee-ranged

| Stat | Value |
|---|---|
| Health | 110 |
| Move speed | 190 (fastest) |
| Turn rate | 12 rad/s (nimble) |
| Body / reach | Standard |

- **Daggers (melee):** a 3-hit combo — two quick strikes (12 dmg) into a **lunging finisher** (22 dmg, extra range and knockback) that dashes you forward. Missing the ~0.9s combo window resets the chain. Combo progress shows as pips under the goblin.
- **Bow (press F to swap):** ranged light shot (16 dmg) and a charged **power shot** (34 dmg, pierces through enemies). Trades the daggers' burst for range and safety.
- **Ability — Dash Strike (E):** burst forward and carve everything in the path.
- **Identity:** the flex pick. Swap weapons live to answer whatever the fight needs — close to shred, back off to bow. The weapon swap is the template for future flex classes.

### Brute — frontline breaker

| Stat | Value |
|---|---|
| Health | 170 (highest) |
| Move speed | 118 (slowest) |
| Turn rate | 5 rad/s (heavy, deliberate) |
| Body | 1.5× size — a genuinely bigger creature |
| Reach | 1.5× swing arc |

- **Great-club (melee):** slow, wide, devastating. Light 14 dmg (0.62s cadence); heavy 34 dmg with heavy knockback (70) after a 0.52s windup and a long 0.72s recovery. The big body and long reach let him hit multiple enemies and clip foes through gaps.
- **Ability — Ground Slam (E):** a wide concussive shockwave (30 dmg, ~128 radius) that staggers and launches everything around him and shatters nearby cover.
- **Identity:** power for commitment. He hits harder and reaches further than anyone, but turns slowly, moves slowly, and each swing leaves a punishable gap. A bigger body also means a bigger target. He breaks a line; he can't dance through one.

### Shaman — ranged & support specialist

The Shaman is a staff-wielding caster: light on melee, built around ranged pressure and controlling the fight for the squad. At raid start the player commits to one of two **paths**.

| Stat | Value |
|---|---|
| Health | 85 (most fragile) |
| Move speed | 160 |
| Turn rate | 10 rad/s |

#### Shadow path — summoner / crowd control

- **Shadow Dagger (LMB):** a fast conjured blade thrown at range (13 dmg). Being magic, it **ignores physical armor**.
- **Shadow Bolt (RMB):** a heavier, slower bolt (26 dmg) that **pierces through ranks** and ignores armor.
- **Ability — Shadow Grasp (E):** summons clutching shadow hands at the cursor that **root and stun a whole group** for ~2.2s, plus minor damage. The squad's setup tool — freeze a pack, let allies punish. This is the support core of the path.

#### Blood path — orb economy / conjured arsenal

The Blood Shaman fuels everything from **blood orbs** — a pool of up to **9**, drawn from the enemy:

- **Defeated enemies** bleed orbs that magnetize to the Shaman.
- **Siphon (F)** draws orbs out of nearby *living* enemies (small drain, up to 3 orbs per cast).

Orbs are spent on conjured weapons, creating a moment-to-moment economy decision:

| Weapon | Cost | Damage | Role |
|---|---|---|---|
| **Blood Dagger** (LMB) | 1 orb | 14 | Cheap ranged spam; *blocked by armor* — best on light infantry |
| **Blood Spear** (RMB) | 2 orbs | 26 | *Armor-piercing*, punches through ranks — the anti-knight/anti-heavy tool |
| **Blood Lance** (E) | 4 orbs | 42 | A great conjured lance sweeping a wide melee arc; heavy stagger + knockback, drinks a little health back |

- **Identity:** a resource caster who must keep killing to keep casting. Rewards target selection (dagger the weak to bank orbs, spear the armored, lance a cluster). Running dry mid-fight is the failure state — a red ring flashes when you try to cast without the orbs.

> **Universal to all paths/classes:** every goblin still carries **torches** (Q) regardless of build. Fire is the racial equalizer.

---

## 4. Universal systems

### Torch toss (all classes)
Throw a torch (Q) that **sticks where it lands** — lodging in a wall or building and burning on that surface rather than passing through. On impact it spawns a fire zone, ignites flammable props, and fire **spreads** to nearby wooden structures. Torches also damage enemies directly. This is the signature goblin trait and the answer to armored, high-HP defenders.

### Lives & respawn
Each goblin — player and each AI companion — has **5 independent lives**. Losing all health costs one life and respawns you after ~2s at partial health with brief invulnerability. Losing the **last** life ends that goblin's raid (for the player, that's the run). Respawn is deliberately frictionless: this is a horde-attacker power fantasy, not permadeath.

### Alarm / heat
A shared meter that rises from fighting, fire, and destroyed objectives, plus a slow passive tick. At **100** it triggers a **horde wave** and bumps the reinforcement tier (tougher mixes — more knights, more archers). It resets partway and climbs again. This is the central risk/reward dial: every greedy extra second and every fire raises the heat.

### Combat feel layer ("the juice")
The systems that make hits land:
- **Enemy telegraphs** — melee enemies flash a red windup ring and danger arc, then commit to the strike in the direction they locked; archers project a locked aim line down their shot lane. This is what makes dodging a *skill*: read the tell, roll through melee or step out of the archer's lane.
- **Dodge with weight** — a 0.22s i-frame roll that commits to its direction, followed by a ~0.28s sluggish recovery. Not a twitch; a decision.
- **Hitstop** — a few frames of freeze when your blow connects, scaled by weight.
- **Stagger & knockback** — light hits flinch and interrupt enemy windups; heavy hits crumple and launch, with knockback that decays over distance.
- **Screen shake & damage numbers** — impact feedback scaled to the hit; floating numbers make damage differences legible.
- **Turn rate** — facing eases toward the cursor per-class rather than snapping, so aiming and positioning have weight.

---

## 5. The enemies — four races

The world has **four races**. Goblins are the playable attackers; **Humans, Elves, and Dwarves** are the defender factions each raid is themed around. Each race has a full design/balance brief (see the companion race docs); summarized here.

A shared **weapon-vs-race multiplier matrix** keeps the whole roster consistent as weapons are added. The goblin damage types are: dagger, bow, greatclub, shadow magic, blood magic, and torch/fire. A light **armor** stat lets piercing attacks matter (knights currently have armor 6; the Blood Spear and Bow power-shot pierce it).

### Humans — the baseline defenders *(implemented)*
Generalists with no extreme strength or weakness — the control group the other races deviate from. Threat comes from formations and numbers, not individual power. **Weak to fire** (their own city burns).

| Archetype | HP | Role |
|---|---|---|
| Militia | 30 | Frontline filler, arrives in numbers, telegraphs a 0.5s swing |
| Archer | 20 | Backline harasser, locked aim-line telegraph, priority kill |
| Knight | 75 (armor 6) | Heavy tank, escalates in with the alarm tier |

**Barracks** are stone spawner buildings that pump out militia/archers on a ~9s timer. Not flammable — they must be beaten down rather than torched, the human-specific counter to your fire.

### Elves — evasive glass cannons *(designed, not yet in prototype)*
Fast, low-HP, magic/ranged-leaning. **Dodge slow/telegraphed attacks** (hard-countering the Brute), but fragile once hit. Nature-attuned, so **resistant to shadow magic** — yet especially **vulnerable to blood drain** and the highest **fire** weakness of the three. Spawner: a flammable wooden **Watch-Station**.

### Dwarves — armored anvils *(designed, not yet in prototype)*
Slow, heavily armored, **fire-resistant** (forge culture). Armor beats blades and arrows but does nothing against **shadow curses** or **explosives** (barrels bypass armor entirely). The Brute's concussive **Ground Slam** and lured barrel blasts are their intended hard counters. Spawner: a fire-resistant **Bunker/Forge** best cracked with explosives.

---

## 6. Missions, map & environment

- **Objective (current):** destroy all 3 granaries, then reach the extraction gate at the south wall.
- **Extraction:** locked until the objective completes, then lights up as the go-home point — Helldivers-style "finish and get out."
- **Destructible environment:** granaries (burnable objectives), barracks (spawners), and scattered fences, crates, and **explosive barrels** (chain-react, bypass armor, hurt everything nearby including you). Stone walls and buildings are indestructible cover.
- **Map (current):** a single fixed arena. The full-game vision is larger, more open city districts; the prototype arena is a combat sandbox, not the final level design.

**Future objective variety** (roadmap): steal-the-treasury, assassinate-a-captain, timed burn quotas, escort/carry loot to extraction — each reusing the alarm/spawner/extraction backbone.

---

## 7. Progression *(roadmap)*

- **Gold & loot** collected mid-raid (from kills, smashed props, burned granaries) carry out on successful extraction.
- **Between raids:** spend gold on weapon upgrades and goblin gear at the camp.
- **Direction to define:** per-class weapon trees, unlockable abilities, and whether the Slasher's weapon-swap model extends to other classes as an unlock. Left deliberately open for review.

---

## 8. Co-operative play

Co-op is the **target experience**: a warband of players hitting a city together, sharing one alarm meter (you alerted the city *together*), each with their own lives. The prototype simulates this with **AI companions** that fight alongside you, but real multiplayer networking is not present in the browser build and is the single biggest jump to the full game (see roadmap).

---

## 9. Prototype status — what's real today

**Implemented and playable (browser prototype):**
- All three classes with full kits; both Shaman paths with their distinct resource systems
- Universal torch toss with stick-on-impact and fire spread
- 5-lives/respawn for player and companions
- Alarm/heat meter with escalating horde waves
- Full combat-feel layer (telegraphs, hitstop, stagger, knockback, shake, damage numbers, weighted dodge, turn rate)
- Human enemies (militia/archer/knight) + Barracks spawners + light armor system
- Destructible props, explosive barrels, burnable granaries
- The burn-3-granaries → extract mission loop
- 2 AI companions

**Designed but not yet built:**
- Elf and Dwarf enemy factions (specs written, not coded)
- Real co-op multiplayer (simulated via companions)
- Open-city levels (prototype is a single arena)
- Progression/upgrade economy (gold is collected but not yet spent)
- Objective variety beyond granary-burning

---

## 10. Road to Unreal — the 7-week target

The end goal is to rebuild this in **Unreal Engine 5**. The browser prototype is the **design reference** — mechanics, tuning numbers, and feel — not portable code. The prototype's source carries inline notes mapping each system to its UE5 equivalent. Summary of the translation:

| Prototype system | UE5 equivalent |
|---|---|
| Player classes | `ACharacter` subclasses + Gameplay Ability System (GAS) |
| Class kits / weapon swap | GAS ability sets; equipped-item component |
| Lives & respawn | `PlayerState` (per-player) + `GameMode` respawn |
| Companions / enemies | `AIController` + Behavior Trees + EQS |
| Destructible props | Chaos Destruction (Geometry Collections) |
| Barracks / spawners | Shared `ASpawnerActor` base (destroy-to-silence) |
| Torches & fire | Projectile w/ surface hit + Niagara FX + radial damage volume |
| Alarm meter | Replicated float on `GameState` driving an AI Director |
| Combat feel | Time dilation (hitstop), `UCameraShakeBase`, montage windups (telegraphs), GAS stun tags (stagger) |
| Armor / pierce / magic | Distinct damage types in GAS that skip or apply mitigation |
| Co-op | Real replication (Actors, RPCs, session subsystems) — the biggest single lift |
| HUD | UMG widgets bound to GameState/PlayerState |

**Honest scoping note:** a co-op, fully-destructible hack-and-slash is ambitious even for funded teams. Realistic 7-week targets are a **vertical slice** — one race (Humans), core combat and classes, one polished district, and either functional co-op *or* deep single-player, but likely not both at full fidelity. The prototype de-risks the design so Unreal time goes to the things that don't come free there: networking, 3D destruction, and the animation/feel layer.

---

## 11. Open questions for review

1. **Objective variety** — is burn-the-granaries the flagship mission, or one of a rotating set from day one?
2. **Progression depth** — light gear upgrades, or full per-class weapon/ability trees?
3. **Weapon swap** — Slasher-only, or a system other classes unlock into?
4. **Co-op vs. solo priority for the slice** — which gets the fidelity if we can't fully do both in 7 weeks?
5. **Friendly fire** — do goblin torches/barrels hurt allies? (Big lever for Helldivers-style tension vs. frustration.)
6. **Alarm tuning** — should extraction itself spike the alarm (a climactic fighting retreat), or stay a clean getaway?

---

*Companion documents: `race-design-goblins.md`, `race-design-humans.md`, `race-design-elves.md`, `race-design-dwarves.md`. Playable reference: `goblin-siege-prototype.html`.*
