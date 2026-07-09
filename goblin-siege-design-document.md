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

## 3. The goblins — weapons (formerly "classes")

**A goblin's identity is its weapon.** Rather than fixed classes, the player picks a **weapon-kit**, and *each weapon has its own progression tree* (see §7). What earlier drafts called "classes" are really the starting weapons; the Shaman's two paths are two separate weapons (Shadow Staff, Blood Staff). This keeps the model clean: choose a weapon, grow it down its tree.

All goblins share the universal **torch toss** and the **5-lives** system (see §4). Values below are the current prototype tuning and are expected to move with playtesting.

### Daggers & Bow — mobile skirmisher / flex melee-ranged
*(the "Slasher" — daggers and bow are **one weapon**, not two, with a single shared progression tree; swapping modes is core to the kit, not an unlock)*

| Stat | Value |
|---|---|
| Health | 110 |
| Move speed | 190 (fastest) |
| Turn rate | 12 rad/s (nimble) |
| Body / reach | Standard |

- **Daggers (melee):** a 3-hit combo — two quick strikes (12 dmg) into a **lunging finisher** (22 dmg, extra range and knockback) that dashes you forward. Missing the ~0.9s combo window resets the chain. Combo progress shows as pips under the goblin.
- **Bow (press F to swap):** ranged light shot (16 dmg) and a charged **power shot** (34 dmg, pierces through enemies). Trades the daggers' burst for range and safety.
- **Ability — Dash Strike (E):** burst forward and carve everything in the path.
- **Identity:** the flex pick. Swap modes live to answer whatever the fight needs — close to shred, back off to bow. Because daggers and bow share one weapon and one tree, investing in this weapon deepens *both* modes together.

### Great-club — frontline breaker
*(the "Brute")*

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

### Shadow Staff & Blood Staff — ranged & support specialists
*(the "Shaman" — these are **two separate weapons**, each with its own progression tree; the player picks one)*

The Shaman weapons are staff-wielding casters: light on melee, built around ranged pressure and controlling the fight for the squad.

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

### Friendly fire — **ON**
Goblin torches, fire zones, and exploding barrels **damage allies** (companions and, in co-op, other players), just as they damage you. This is a deliberate Helldivers-style lever: fire is powerful and area-denying, so it carries real risk when the squad is packed together. It makes positioning and torch discipline part of the skill, not a free-fire button. (Tuning caveat: watch that it lands as *tension*, not *frustration* — friendly-fire damage may be scaled below enemy-source damage if playtests feel punishing.)

### Alarm / heat, and the burning city
A shared meter that rises from fighting, fire, and destroyed objectives, plus a slow passive tick. At **100** it triggers a **horde wave** and bumps the reinforcement tier (tougher mixes — more knights, more archers). It resets partway and climbs again. Every greedy extra second and every fire raises the heat.

**The city reacts to being burned.** Two connected behaviors define the late-raid state:

- **A razed city stops producing defenders.** Once the objective structures are destroyed / the district is sufficiently ablaze, local **spawners go quiet** — the barracks can't pump out militia if they're rubble and smoke. The internal garrison thins out.
- **…but outside patrols come to the rescue.** In place of endless internal spawns, **external relief patrols** march in from the map edges — a different, finite pressure that turns the back half of a raid into holding off reinforcements from outside rather than an inexhaustible internal horde.
- **Defenders fight the fires.** Non-combat and off-duty NPCs will **attempt to put out fires** when they can — running to burning structures to douse them. This gives the player a live counter-pressure (your destruction can be *undone* if you don't defend it or move on) and creates readable, emergent AI behavior: the city visibly trying to save itself. Firefighting NPCs are vulnerable while doing it — an optional target.

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

**Missions are a small rotating set from day one — not a single flagship.** Two objective types are defined:

- **Burn the Granaries** *(implemented):* destroy all 3 granaries. A destruction/arson objective spread across the map that naturally spikes the alarm as you go.
- **Kill the Landlord** *(designed):* a named human authority figure (the district's landlord) holds up somewhere in the city, guarded. Assassination objective — find him, cut through his guard, and put him down. He should react to the raid: flee toward a safehouse or the far gate under escort once alarmed, so dawdling lets him slip away and forces a chase. This reuses the same alarm/spawner/extraction backbone with a target-and-hunt flavor instead of area destruction.

Both funnel into the same **extraction** step below. More types (steal-the-treasury, timed burn quotas, escort-loot-out) remain on the roadmap.

- **Extraction:** locked until the objective completes, then lights up as the go-home point — Helldivers-style "finish and get out."
- **Destructible environment:** granaries (burnable objectives), barracks (spawners), and scattered fences, crates, and **explosive barrels** (chain-react, bypass armor, hurt everything nearby including you). Stone walls and buildings are indestructible cover.
- **Map (current):** a single fixed arena. The full-game vision is larger, more open city districts; the prototype arena is a combat sandbox, not the final level design.

**Future objective variety** (roadmap): steal-the-treasury, assassinate-a-captain, timed burn quotas, escort/carry loot to extraction — each reusing the alarm/spawner/extraction backbone.

---

## 7. Progression — a tree per weapon *(roadmap)*

**Progression is organized by weapon, not by character.** Each weapon (Daggers & Bow, Great-club, Shadow Staff, Blood Staff, and any future weapons) has **its own progression tree**. Investing in a weapon deepens that kit — new nodes, stat upgrades, and ability modifiers specific to it. The player's power comes from how far they've grown the weapon they're wielding.

- **Gold & loot** collected mid-raid (kills, smashed props, burned granaries, the landlord's coffers) carries out on successful extraction.
- **Between raids:** spend earnings at the camp to unlock and upgrade nodes on the tree of whichever weapon(s) you're building.
- **What a tree contains (direction to define):** raw stat upgrades (damage, cadence, reach), kit-specific unlocks (e.g. the Daggers & Bow tree might deepen *both* modes — a bleeding-dagger node here, a multi-shot bow node there; the Blood Staff tree might raise the orb cap or cheapen the Lance), and ability modifiers. Because Daggers & Bow is **one weapon**, its single tree serves both melee and ranged — no separate unlock to "gain the bow"; it's baked in and grows together.
- **Cross-weapon feel:** trees are deep rather than wide, so mastering a weapon is a commitment. Swapping to a fresh weapon means starting its tree — a meaningful choice, not a free menu.

---

## 8. Solo first, co-op later

**Decision: solo is the priority for the 7-week slice.** Co-op remains the long-term north star — a warband of players hitting a city together under one shared alarm — but real multiplayer networking is the single biggest, riskiest lift, and it will *not* be built inside the slice. Instead the slice targets a **polished single-player experience**.

- **Goblin companions are minimal AI.** You still fight alongside 1–2 AI goblins for the warband feel, but they are deliberately **simple** — follow, engage nearby enemies, respawn on their own lives. No elaborate squad-command layer. Just enough to sell "you're not alone," cheaply, so engineering time goes into combat feel and the single-player loop.
- **Build co-op-ready, don't build co-op yet.** Where it's low-cost, structure systems so real replication can be added later (shared alarm on a game-state object, per-actor lives), but do not spend slice time on networking.

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
- **Kill-the-Landlord mission** (decided; not yet coded)
- **Friendly fire** on torches/barrels/fire (decided; prototype currently spares allies)
- **Burnt-city alarm model** — spawners going quiet once razed, external relief patrols, and fire-fighting NPCs (decided; not yet coded)
- **Per-weapon progression trees** (decided; gold is collected but not yet spent)
- Elf and Dwarf enemy factions (specs written, not coded)
- Real co-op multiplayer (solo is the slice priority; companions are minimal AI)
- Open-city levels (prototype is a single arena)

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
| Alarm meter | Float on `GameState` driving an AI Director; switches spawns from internal → external patrols when razed |
| Firefighting NPCs | Behavior Tree task: seek nearest fire volume, extinguish over time; interruptible/vulnerable |
| Combat feel | Time dilation (hitstop), `UCameraShakeBase`, montage windups (telegraphs), GAS stun tags (stagger) |
| Friendly fire | Damage volumes affect all pawns; optional team-scalar to reduce ally damage |
| Armor / pierce / magic | Distinct damage types in GAS that skip or apply mitigation |
| Progression trees | Per-weapon data assets + save profile; nodes modify the weapon's GAS ability set |
| Co-op *(out of slice scope)* | Real replication (Actors, RPCs, session subsystems) — the biggest single lift; deferred past the slice |
| HUD | UMG widgets bound to GameState/PlayerState |

**Honest scoping note:** a fully-destructible hack-and-slash is ambitious even for funded teams. The realistic 7-week target is a **single-player vertical slice** — one race (Humans), core combat and the starting weapons, one polished district, both mission types, friendly fire, and the burnt-city/patrol/firefighting alarm behavior. **Co-op networking is explicitly out of slice scope** (see §8), which frees that time for combat feel, the two objectives, and 3D destruction. The prototype de-risks the design so Unreal time goes to the things that don't come free there: 3D Chaos destruction, the animation/feel layer, and enemy AI (including firefighting behavior).

---

## 11. Resolved decisions

The review questions from the prior draft are now settled:

1. **Objective variety → a small rotating set from day one.** Ship with **two** mission types: *Burn the Granaries* and *Kill the Landlord* (a fleeing, guarded assassination target). More types remain roadmap. *(§6)*
2. **Progression → a tree per weapon.** Each weapon has its own deep progression tree; the player grows the weapon they wield. *(§7)*
3. **Weapon model → each weapon is the "class."** Daggers and Bow are **combined into a single weapon** with one shared tree; swapping modes is core to the kit, not an unlock. The Shaman's two staves are two separate weapons. *(§3, §7)*
4. **Co-op vs. solo → solo priority for the slice.** Polished single-player; goblin companions are **minimal AI**. Co-op stays the long-term goal but is out of slice scope. *(§8)*
5. **Friendly fire → ON.** Torches, fire, and barrels hurt allies (tunable down if it reads as frustration rather than tension). *(§4)*
6. **Alarm / extraction → the city burns out, then patrols respond.** A razed district **stops producing internal defenders**; finite **external relief patrols** arrive from the map edges instead. Defender **NPCs fight the fires**, so unattended destruction can be undone. *(§4)*

### Next open questions *(new, for the next pass)*

- **Landlord chase:** how aggressively should he flee — a slow retreat you can cut off, or a real footrace with an escort?
- **Firefighting balance:** can NPCs fully save a structure, or only slow the burn? How much does a doused fire lower the alarm?
- **Patrol pacing:** how large/frequent are external relief patrols, and do they escalate the longer you linger post-objective?
- **Friendly-fire scaling:** full damage to allies, or a reduced percentage?

---

*Companion documents: `race-design-goblins.md`, `race-design-humans.md`, `race-design-elves.md`, `race-design-dwarves.md`. Playable reference: `goblin-siege-prototype.html`.*
