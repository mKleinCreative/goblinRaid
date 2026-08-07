# Retrieval trace — Tutorial hamlet — first-time prompt rows

**Gap this fills.** GDD 2.8 specifies a skippable teaching layer — every objective and mechanic carries 'a bark from His Eternal Darkness plus a one-line HUD note naming the objective, the payoff, and the one control that does it' — and 3.1 lists it as a Settlement Generator Agent deliverable for week 4. Only one instance (the windmill Stage-1 bark) is actually written.

## 1. Queries and what they retrieved

**Query:** `tutorial hamlet prompts first time dismissible HUD note one control payoff`

- `GDD§2.8#4` (BM25 21.44) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§2.8#3` (BM25 21.271) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§2.8#1` (BM25 9.151) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§2.8#2` (BM25 8.898) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of

**Query:** `burn objectives granary field windmill how each burns telegraphs`

- `GDD§2.8#3` (BM25 20.069) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§2.8#2` (BM25 17.039) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§2.1#1` (BM25 14.029) — 2. Game Mechanics > 2.1 The core loop
- `GDD§1#1` (BM25 11.722) — 1. Executive Summary

**Query:** `takedown bind civilians hold E channel unaware from behind`

- `GDD§4.5#4` (BM25 12.907) — 4. Technical Strategy > 4.5 Schedule and honest scoping
- `GDD§2.4#1` (BM25 12.393) — 2. Game Mechanics > 2.4 Stealth — the quiet half of the raid
- `GDD§2.4#2` (BM25 11.305) — 2. Game Mechanics > 2.4 Stealth — the quiet half of the raid
- `GDD§4.5#3` (BM25 10.61) — 4. Technical Strategy > 4.5 Schedule and honest scoping

**Query:** `courier point command horde pig loot sack rejoins pool`

- `GDD§2.7#3` (BM25 26.467) — 2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system
- `GDD§2.5#0` (BM25 22.615) — 2. Game Mechanics > 2.5 The horn and the horde — the go-loud button
- `GDD§2.7#2` (BM25 19.016) — 2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system
- `GDD§2.7#0` (BM25 12.072) — 2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system

**Query:** `patrol soft signal escalation suspicious overdue decay`

- `GDD§2.6#1` (BM25 15.728) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§2.6#2` (BM25 14.928) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§2.4#0` (BM25 10.457) — 2. Game Mechanics > 2.4 Stealth — the quiet half of the raid
- `GDD§2.4#1` (BM25 8.313) — 2. Game Mechanics > 2.4 Stealth — the quiet half of the raid

**Query:** `war horn horde treeline summon go loud on purpose`

- `GDD§2.5#0` (BM25 19.877) — 2. Game Mechanics > 2.5 The horn and the horde — the go-loud button
- `GDD§2.3#1` (BM25 13.469) — 2. Game Mechanics > 2.3 The Scout — one class this slice, three more waiting
- `GDD§2.1#1` (BM25 10.523) — 2. Game Mechanics > 2.1 The core loop
- `GDD§1#2` (BM25 8.985) — 1. Executive Summary

**Query:** `universal kit torch toss dodge roll crouch interact controls`

- `GDD§2.3#1` (BM25 18.037) — 2. Game Mechanics > 2.3 The Scout — one class this slice, three more waiting
- `GDD§4.5#1` (BM25 10.463) — 4. Technical Strategy > 4.5 Schedule and honest scoping
- `GDD§4.5#2` (BM25 9.951) — 4. Technical Strategy > 4.5 Schedule and honest scoping
- `ELF§roadmap-notes#0` (BM25 9.51) — Elves — Race & Combat Design Reference > Roadmap notes for the Unreal build

**Query:** `Warren plant respawn banking loot prisoners dug hole`

- `GDD§2.6#3` (BM25 21.25) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§3.1#5` (BM25 13.479) — 3. AI Architecture > 3.1 The game-system agents
- `GDD§2.7#3` (BM25 9.939) — 2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system
- `GDD§2.2#0` (BM25 8.37) — 2. Game Mechanics > 2.2 Win, lose, and the three pressures

## 2. Retrieved chunk -> generated line

### Prompts_01_FirstSightGranary

**Output:** Granary — burn it: one of three targets done. Torch: Q

**Retrieved `GDD§2.8#2`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> - **The core village** — the hamlet's social heart: houses, the well, the watchtower, the barracks, and the **market stalls with lootable gold** (the map's loot concentration, and where the strongbox spawns). Its burn objective is the **village granary**, sitting where the guards are thickest. - **Farmsteads** — the working edge: a farmhouse, **livestock pens** (the chickens, sheep, and pigs of §2.7), and ripe **whea…

**Retrieved `GDD§2.8#3`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> This hamlet carries **all three burn objectives in a single fixed layout — one granary, one field, one windmill** — rather than the randomized two-of-three mix a procedural version will eventually roll; a new player sees every objective type, and how each one burns, in their very first raid instead of maybe two of them. **Trees and greenery** (hedgerows, copses, garden plots) fill the seams between modules and double…

### Prompts_02_FirstSightWheatField

**Output:** Wheat field — burn the crop: fire spreads row to row. Torch: Q

**Retrieved `GDD§2.8#2`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> - **The core village** — the hamlet's social heart: houses, the well, the watchtower, the barracks, and the **market stalls with lootable gold** (the map's loot concentration, and where the strongbox spawns). Its burn objective is the **village granary**, sitting where the guards are thickest. - **Farmsteads** — the working edge: a farmhouse, **livestock pens** (the chickens, sheep, and pigs of §2.7), and ripe **whea…

**Retrieved `GDD§2.8#3`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> This hamlet carries **all three burn objectives in a single fixed layout — one granary, one field, one windmill** — rather than the randomized two-of-three mix a procedural version will eventually roll; a new player sees every objective type, and how each one burns, in their very first raid instead of maybe two of them. **Trees and greenery** (hedgerows, copses, garden plots) fill the seams between modules and double…

### Prompts_03_FirstSightWindmill

**Output:** Windmill — light the frame: Stage 1, Ablaze. Torch: Q

**Retrieved `GDD§2.8#2`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> - **The core village** — the hamlet's social heart: houses, the well, the watchtower, the barracks, and the **market stalls with lootable gold** (the map's loot concentration, and where the strongbox spawns). Its burn objective is the **village granary**, sitting where the guards are thickest. - **Farmsteads** — the working edge: a farmhouse, **livestock pens** (the chickens, sheep, and pigs of §2.7), and ripe **whea…

**Retrieved `GDD§2.8#4`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> **A hamlet built to be skipped, one prompt at a time.** Every objective and mechanic this hamlet introduces — the granary, the field, the windmill, the patrol and its soft-signal escalation (§2.6), a civilian pair offering both the takedown and the bind choice (§2.4), and a livestock pen for a first courier run (§2.7) — carries a short, dismissible prompt the first time the player meets it: a bark from His Eternal Da…

### Prompts_04_WindmillStage1Ablaze

**Output:** Windmill: Ablaze — needs a window shot. Aimed torch throw: Q

**Retrieved `GDD§2.8#2`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> - **The core village** — the hamlet's social heart: houses, the well, the watchtower, the barracks, and the **market stalls with lootable gold** (the map's loot concentration, and where the strongbox spawns). Its burn objective is the **village granary**, sitting where the guards are thickest. - **Farmsteads** — the working edge: a farmhouse, **livestock pens** (the chickens, sheep, and pigs of §2.7), and ripe **whea…

**Retrieved `GDD§2.8#4`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> **A hamlet built to be skipped, one prompt at a time.** Every objective and mechanic this hamlet introduces — the granary, the field, the windmill, the patrol and its soft-signal escalation (§2.6), a civilian pair offering both the takedown and the bind choice (§2.4), and a livestock pen for a first courier run (§2.7) — carries a short, dismissible prompt the first time the player meets it: a bark from His Eternal Da…

### Prompts_05_FirstSightForestPatrol

**Output:** Patrol ahead — crouch in cover: a broken look is no alarm. Crouch

**Retrieved `GDD§2.1#1`** (2. Game Mechanics > 2.1 The core loop):

> One raid, capped at 30 minutes: 1. **Step through as the Scout.** This slice's one playable kit — daggers and bow, live-swapped — is where every raid starts: no class-select screen this slice, just a straight drop into the **runic site** — a ring of glowing standing stones in a forest clearing. This is spawn, respawn, and the extraction point, all in one. Humans are too superstitious to enter the circle; it is the on…

**Retrieved `GDD§2.4#1`** (2. Game Mechanics > 2.4 Stealth — the quiet half of the raid):

> Stealth is deliberately *lite*: the whole detection model is one readable rule — a guard's sighting of you must **hold for about 1.5 seconds to confirm**. Duck behind a haycart before the confirm lands and you were never there — a look that breaks early is not a partial signal, it is *no* signal, and it costs you nothing. Get confirmed and that sighting becomes exactly one uncorroborated soft signal (§2.6): the town …

### Prompts_06_FirstSightUnawareGuard

**Output:** Unaware guard — take him down: silent, but the body still talks. Hold E behind him

**Retrieved `GDD§2.4#1`** (2. Game Mechanics > 2.4 Stealth — the quiet half of the raid):

> Stealth is deliberately *lite*: the whole detection model is one readable rule — a guard's sighting of you must **hold for about 1.5 seconds to confirm**. Duck behind a haycart before the confirm lands and you were never there — a look that breaks early is not a partial signal, it is *no* signal, and it costs you nothing. Get confirmed and that sighting becomes exactly one uncorroborated soft signal (§2.6): the town …

**Retrieved `GDD§2.4#2`** (2. Game Mechanics > 2.4 Stealth — the quiet half of the raid):

> 1. **Noise.** Every action has an audible radius the player learns by feel: walking is quiet, running less so, smashing stalls and fences is loud, and fire is a town-wide announcement. Guards investigate noise; the squawking chickens investigate *you*. 2. **Crouch.** A stealth stance that tightens every guard's effective detection range and slows you to a creep. Hedgerows, fences, haycarts, and pen walls are the haml…

## 3. Critic findings

### Round 1 — 2 finding(s)

- **TONE_DRIFT** in `Prompts_03_FirstSightWindmill` (by critic)
  - offending: Her sails first, goblin. Let the county see the smoke and learn to fear it.
  - canon: His Eternal Darkness is a pompous, whispering, chronically underwhelmed patron in a comic-satire register ("Adequate. I have seen rats do better. …adequate."), not a source of epic menace; barks should be smug or disappointed rather than fear-mongering. — evidence `GDD§2.1#1`
  - correction: Her sails first, goblin. A landmark is only useful to me on fire.
- **LORE_BREAK** in `Prompts_06_FirstSightUnawareGuard` (by critic)
  - offending: Unaware guard — take him down: silent, no signal. Hold E behind him
  - canon: A takedown is silent, but corpses talk: a discovered body is itself a soft signal that moves the hamlet to SUSPICIOUS, and there is no body-dragging in this slice. The HUD may not promise "no signal." — evidence `GDD§2.6#1`
  - correction: Unaware guard — take him down: silent, but the body still talks. Hold E behind him

### Round 2 — 0 finding(s)

