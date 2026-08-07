# Retrieval trace — Tutorial hamlet — first-time prompt rows

**Gap this fills.** GDD 2.8 specifies a skippable teaching layer — every objective and mechanic carries 'a bark from His Eternal Darkness plus a one-line HUD note naming the objective, the payoff, and the one control that does it' — and 3.1 lists it as a Settlement Generator Agent deliverable for week 4. Only one instance (the windmill Stage-1 bark) is actually written.

## 1. Queries and what they retrieved

**Query:** `tutorial hamlet prompts first time dismissible HUD note one control payoff`

- `GDD§2.8#4` (BM25 21.389) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§2.8#3` (BM25 21.219) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§2.8#1` (BM25 9.125) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§2.8#2` (BM25 8.873) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of

**Query:** `burn objectives granary field windmill how each burns telegraphs`

- `GDD§2.8#3` (BM25 20.023) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§2.8#2` (BM25 16.993) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§2.1#1` (BM25 13.988) — 2. Game Mechanics > 2.1 The core loop
- `GDD§1#1` (BM25 11.696) — 1. Executive Summary

**Query:** `takedown bind civilians hold E channel unaware from behind`

- `GDD§2.4#1` (BM25 12.897) — 2. Game Mechanics > 2.4 Stealth — the quiet half of the raid
- `GDD§4.5#4` (BM25 12.881) — 4. Technical Strategy > 4.5 Schedule and honest scoping
- `GDD§2.4#2` (BM25 11.274) — 2. Game Mechanics > 2.4 Stealth — the quiet half of the raid
- `GDD§4.5#3` (BM25 10.583) — 4. Technical Strategy > 4.5 Schedule and honest scoping

**Query:** `courier point command horde pig loot sack rejoins pool`

- `GDD§2.7#3` (BM25 26.415) — 2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system
- `GDD§2.5#0` (BM25 22.572) — 2. Game Mechanics > 2.5 The horn and the horde — the go-loud button
- `GDD§2.7#2` (BM25 18.977) — 2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system
- `GDD§2.7#0` (BM25 12.054) — 2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system

**Query:** `patrol soft signal escalation suspicious overdue decay`

- `GDD§2.6#1` (BM25 16.288) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§2.6#2` (BM25 15.421) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§1#4` (BM25 8.126) — 1. Executive Summary
- `GDD§3.1#2` (BM25 7.802) — 3. AI Architecture > 3.1 The game-system agents

**Query:** `war horn horde treeline summon go loud on purpose`

- `GDD§2.5#0` (BM25 19.842) — 2. Game Mechanics > 2.5 The horn and the horde — the go-loud button
- `GDD§2.3#1` (BM25 13.439) — 2. Game Mechanics > 2.3 The Scout — one class this slice, three more waiting
- `GDD§2.1#1` (BM25 10.492) — 2. Game Mechanics > 2.1 The core loop
- `GDD§1#2` (BM25 8.962) — 1. Executive Summary

**Query:** `universal kit torch toss dodge roll crouch interact controls`

- `GDD§2.3#1` (BM25 17.99) — 2. Game Mechanics > 2.3 The Scout — one class this slice, three more waiting
- `GDD§4.5#1` (BM25 10.435) — 4. Technical Strategy > 4.5 Schedule and honest scoping
- `GDD§4.5#2` (BM25 9.925) — 4. Technical Strategy > 4.5 Schedule and honest scoping
- `ELF§roadmap-notes#0` (BM25 9.5) — Elves — Race & Combat Design Reference > Roadmap notes for the Unreal build

**Query:** `Warren plant respawn banking loot prisoners dug hole`

- `GDD§2.6#3` (BM25 21.196) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§3.1#5` (BM25 13.446) — 3. AI Architecture > 3.1 The game-system agents
- `GDD§2.7#3` (BM25 9.921) — 2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system
- `GDD§2.2#0` (BM25 8.355) — 2. Game Mechanics > 2.2 Win, lose, and the three pressures

## 2. Retrieved chunk -> generated line

### Prompts_01_FirstSightGranary

**Output:** Granary — burn it. One of your three burn targets. Torch toss: Q.

**Retrieved `GDD§2.8#2`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> - **The core village** — the hamlet's social heart: houses, the well, the watchtower, the barracks, and the **market stalls with lootable gold** (the map's loot concentration, and where the strongbox spawns). Its burn objective is the **village granary**, sitting where the guards are thickest. - **Farmsteads** — the working edge: a farmhouse, **livestock pens** (the chickens, sheep, and pigs of §2.7), and ripe **whea…

**Retrieved `GDD§2.8#3`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> This hamlet carries **all three burn objectives in a single fixed layout — one granary, one field, one windmill** — rather than the randomized two-of-three mix a procedural version will eventually roll; a new player sees every objective type, and how each one burns, in their very first raid instead of maybe two of them. **Trees and greenery** (hedgerows, copses, garden plots) fill the seams between modules and double…

### Prompts_02_FirstSightWheatField

**Output:** Wheat field — fire spreads row to row. One of your three burn targets. Torch toss: Q.

**Retrieved `GDD§2.8#2`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> - **The core village** — the hamlet's social heart: houses, the well, the watchtower, the barracks, and the **market stalls with lootable gold** (the map's loot concentration, and where the strongbox spawns). Its burn objective is the **village granary**, sitting where the guards are thickest. - **Farmsteads** — the working edge: a farmhouse, **livestock pens** (the chickens, sheep, and pigs of §2.7), and ripe **whea…

**Retrieved `GDD§2.8#3`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> This hamlet carries **all three burn objectives in a single fixed layout — one granary, one field, one windmill** — rather than the randomized two-of-three mix a procedural version will eventually roll; a new player sees every objective type, and how each one burns, in their very first raid instead of maybe two of them. **Trees and greenery** (hedgerows, copses, garden plots) fill the seams between modules and double…

### Prompts_03_FirstSightWindmill

**Output:** Windmill — torch the frame to set it Ablaze. Torch toss: Q.

**Retrieved `GDD§2.8#2`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> - **The core village** — the hamlet's social heart: houses, the well, the watchtower, the barracks, and the **market stalls with lootable gold** (the map's loot concentration, and where the strongbox spawns). Its burn objective is the **village granary**, sitting where the guards are thickest. - **Farmsteads** — the working edge: a farmhouse, **livestock pens** (the chickens, sheep, and pigs of §2.7), and ripe **whea…

**Retrieved `GDD§2.8#3`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> This hamlet carries **all three burn objectives in a single fixed layout — one granary, one field, one windmill** — rather than the randomized two-of-three mix a procedural version will eventually roll; a new player sees every objective type, and how each one burns, in their very first raid instead of maybe two of them. **Trees and greenery** (hedgerows, copses, garden plots) fill the seams between modules and double…

### Prompts_04_WindmillStage1Ablaze

**Output:** Windmill: Ablaze — needs a window shot. Aimed torch: Q.

**Retrieved `GDD§2.8#2`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> - **The core village** — the hamlet's social heart: houses, the well, the watchtower, the barracks, and the **market stalls with lootable gold** (the map's loot concentration, and where the strongbox spawns). Its burn objective is the **village granary**, sitting where the guards are thickest. - **Farmsteads** — the working edge: a farmhouse, **livestock pens** (the chickens, sheep, and pigs of §2.7), and ripe **whea…

**Retrieved `GDD§2.8#4`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> **A hamlet built to be skipped, one prompt at a time.** Every objective and mechanic this hamlet introduces — the granary, the field, the windmill, the patrol and its soft-signal escalation (§2.6), a civilian pair offering both the takedown and the bind choice (§2.4), and a livestock pen for a first courier run (§2.7) — carries a short, dismissible prompt the first time the player meets it: a bark from His Eternal Da…

### Prompts_05_FirstSightPatrol

**Output:** Patrol ahead — break their sightline to stay unconfirmed. Crouch.

**Retrieved `GDD§2.4#1`** (2. Game Mechanics > 2.4 Stealth — the quiet half of the raid):

> Stealth is deliberately *lite*: the whole detection model is one readable rule — a guard's sighting of you must **hold for about 1.5 seconds to confirm**. Duck behind a haycart before the confirm lands and you were never there; get confirmed and the town moves to Suspicious. On top of that foundation sit five systems, and only five: 1. **Noise.** Every action has an audible radius the player learns by feel: walking i…

**Retrieved `GDD§2.4#2`** (2. Game Mechanics > 2.4 Stealth — the quiet half of the raid):

> 1. **Noise.** Every action has an audible radius the player learns by feel: walking is quiet, running less so, smashing stalls and fences is loud, and fire is a town-wide announcement. Guards investigate noise; the squawking chickens investigate *you*. 2. **Crouch.** A stealth stance that tightens every guard's effective detection range and slows you to a creep. Hedgerows, fences, haycarts, and pen walls are the haml…

### Prompts_06_FirstSightUnawareGuard

**Output:** Unaware human — take him down in silence. Hold E behind him.

**Retrieved `GDD§2.4#1`** (2. Game Mechanics > 2.4 Stealth — the quiet half of the raid):

> Stealth is deliberately *lite*: the whole detection model is one readable rule — a guard's sighting of you must **hold for about 1.5 seconds to confirm**. Duck behind a haycart before the confirm lands and you were never there; get confirmed and the town moves to Suspicious. On top of that foundation sit five systems, and only five: 1. **Noise.** Every action has an audible radius the player learns by feel: walking i…

**Retrieved `GDD§2.4#2`** (2. Game Mechanics > 2.4 Stealth — the quiet half of the raid):

> 1. **Noise.** Every action has an audible radius the player learns by feel: walking is quiet, running less so, smashing stalls and fences is loud, and fire is a town-wide announcement. Guards investigate noise; the squawking chickens investigate *you*. 2. **Crouch.** A stealth stance that tightens every guard's effective detection range and slows you to a creep. Hedgerows, fences, haycarts, and pen walls are the haml…

## 3. Critic findings

### Round 1 — 2 finding(s)

- **LORE_BREAK** in `Prompts_01_FirstSightGranary` (by critic)
  - offending: Granary — burn it for objective 1 of 3. Torch toss: Q.
  - canon: The three burn objectives are not a fixed, numbered sequence. First-time prompts fire "the first time the player meets" each thing, and the player cases the hamlet and chooses which target to torch first, so no objective can be labelled a fixed ordinal. — evidence `GDD§2.8#3`
  - correction: Granary — burn it. One of your three burn targets. Torch toss: Q.
- **LORE_BREAK** in `Prompts_02_FirstSightWheatField` (by critic)
  - offending: Wheat field — fire spreads row to row. Objective 2 of 3. Torch toss: Q.
  - canon: Objectives are named, not ordered: the HUD names the raid's three targets and the player decides which burns first, and these prompts trigger on encounter order, so "2 of 3" asserts a sequence the slice does not have. — evidence `GDD§2.1#1`
  - correction: Wheat field — fire spreads row to row. One of your three burn targets. Torch toss: Q.

### Round 2 — 1 finding(s)

- **LORE_BREAK** in `Prompts_09_FirstSightLivestockPen` (by critic)
  - offending: Send a lad — the pig walks itself home to me.
  - canon: Livestock are "loot that runs away" — a pointed courier goblin chases, catches, and physically carries the pig home ("waddle off with a pig on its back"; a live pig is a squirming over-the-shoulder carry worth 40 loot). Only prisoners are "loot that walks itself" (§2.7). Teaching the player that the pig self-walks confuses the courier verb with the rope chain. — evidence `GDD§2.7#0`
  - correction: Their wealth has trotters, goblin. Send a lad — he'll chase it down and carry it home on his back.

### Round 3 — 1 finding(s)

- **TONE_DRIFT** in `Prompts_06_FirstSightUnawareGuard` (by critic)
  - offending: Behind him. Closer. He is thinking about his supper. Let that be the last thing he does.
  - canon: His Eternal Darkness is a comic-satire Overlord whose register is pompous, whispered, chronically underwhelmed condescension (canon sample: "Adequate. I have seen rats do better. …adequate."), not generic assassin menace. "Let that be the last thing he does" is a straight grimdark threat line with no deflation or disdain in it. — evidence `GDD§2.1#1`
  - correction: Behind him. Closer. He's dreaming about his supper — the most ambitious thought he'll have all day. Take it from him quietly.
