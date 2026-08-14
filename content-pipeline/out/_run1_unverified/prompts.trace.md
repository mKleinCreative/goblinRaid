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

**Output:** Granary: burn it — one of the three fires that flares the stones open for extraction. [Q] Torch toss.

**Retrieved `GDD§2.8#2`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> - **The core village** — the hamlet's social heart: houses, the well, the watchtower, the barracks, and the **market stalls with lootable gold** (the map's loot concentration, and where the strongbox spawns). Its burn objective is the **village granary**, sitting where the guards are thickest. - **Farmsteads** — the working edge: a farmhouse, **livestock pens** (the chickens, sheep, and pigs of §2.7), and ripe **whea…

**Retrieved `GDD§2.1#1`** (2. Game Mechanics > 2.1 The core loop):

> One raid, capped at 30 minutes: 1. **Step through as the Scout.** This slice's one playable kit — daggers and bow, live-swapped — is where every raid starts: no class-select screen this slice, just a straight drop into the **runic site** — a ring of glowing standing stones in a forest clearing. This is spawn, respawn, and the extraction point, all in one. Humans are too superstitious to enter the circle; it is the on…

### Prompts_02_FirstSightWheatField

**Output:** Wheat field: torch the crop — fire spreads row to row and is the hardest blaze in the hamlet to douse. [Q] Torch toss.

**Retrieved `GDD§2.8#2`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> - **The core village** — the hamlet's social heart: houses, the well, the watchtower, the barracks, and the **market stalls with lootable gold** (the map's loot concentration, and where the strongbox spawns). Its burn objective is the **village granary**, sitting where the guards are thickest. - **Farmsteads** — the working edge: a farmhouse, **livestock pens** (the chickens, sheep, and pigs of §2.7), and ripe **whea…

**Retrieved `GDD§2.1#1`** (2. Game Mechanics > 2.1 The core loop):

> One raid, capped at 30 minutes: 1. **Step through as the Scout.** This slice's one playable kit — daggers and bow, live-swapped — is where every raid starts: no class-select screen this slice, just a straight drop into the **runic site** — a ring of glowing standing stones in a forest clearing. This is spawn, respawn, and the extraction point, all in one. Humans are too superstitious to enter the circle; it is the on…

### Prompts_03_FirstSightWindmill

**Output:** Windmill: any torch on the frame starts Stage 1 — Ablaze, the first half of the raid's biggest burn. [Q] Torch toss.

**Retrieved `GDD§2.8#2`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> - **The core village** — the hamlet's social heart: houses, the well, the watchtower, the barracks, and the **market stalls with lootable gold** (the map's loot concentration, and where the strongbox spawns). Its burn objective is the **village granary**, sitting where the guards are thickest. - **Farmsteads** — the working edge: a farmhouse, **livestock pens** (the chickens, sheep, and pigs of §2.7), and ripe **whea…

**Retrieved `GDD§2.3#1`** (2. Game Mechanics > 2.3 The Scout — one class this slice, three more waiting):

> **Waiting on the roadmap, once the Scout has taught the loop:** - **The Brute** (Great-club, 170 / Slow) — the go-loud specialist: worst at hiding, best when hiding stops, and built to carry two pigs at once. Unlocking it turns "go loud on purpose" from a phase every raid passes through into a playstyle a returning player can lean into from the first minute. - **Shaman (Shadow)** (Shadow Staff, 85 / Medium) — armor-i…

### Prompts_04_FirstTimeWindmillAblaze

**Output:** Windmill: Ablaze — needs a window shot; an aimed torch through a window catches the grain dust and starts Stage 2 — Detonation. [Q] Aim, then throw.

**Retrieved `GDD§2.8#2`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> - **The core village** — the hamlet's social heart: houses, the well, the watchtower, the barracks, and the **market stalls with lootable gold** (the map's loot concentration, and where the strongbox spawns). Its burn objective is the **village granary**, sitting where the guards are thickest. - **Farmsteads** — the working edge: a farmhouse, **livestock pens** (the chickens, sheep, and pigs of §2.7), and ripe **whea…

**Retrieved `GDD§2.3#1`** (2. Game Mechanics > 2.3 The Scout — one class this slice, three more waiting):

> **Waiting on the roadmap, once the Scout has taught the loop:** - **The Brute** (Great-club, 170 / Slow) — the go-loud specialist: worst at hiding, best when hiding stops, and built to carry two pigs at once. Unlocking it turns "go loud on purpose" from a phase every raid passes through into a playstyle a returning player can lean into from the first minute. - **Shaman (Shadow)** (Shadow Staff, 85 / Medium) — armor-i…

### Prompts_05_FirstSightForestPatrol

**Output:** Patrol ahead: crouch to break their sightlines — a look has to hold about 1.5 seconds to confirm you; break it sooner and you were never there. [Crouch]

**Retrieved `GDD§2.4#1`** (2. Game Mechanics > 2.4 Stealth — the quiet half of the raid):

> Stealth is deliberately *lite*: the whole detection model is one readable rule — a guard's sighting of you must **hold for about 1.5 seconds to confirm**. Duck behind a haycart before the confirm lands and you were never there; get confirmed and the town moves to Suspicious. On top of that foundation sit five systems, and only five: 1. **Noise.** Every action has an audible radius the player learns by feel: walking i…

**Retrieved `GDD§2.4#2`** (2. Game Mechanics > 2.4 Stealth — the quiet half of the raid):

> 1. **Noise.** Every action has an audible radius the player learns by feel: walking is quiet, running less so, smashing stalls and fences is loud, and fire is a town-wide announcement. Guards investigate noise; the squawking chickens investigate *you*. 2. **Crouch.** A stealth stance that tightens every guard's effective detection range and slows you to a creep. Hedgerows, fences, haycarts, and pen walls are the haml…

### Prompts_06_FirstTimePatrolOverdue

**Output:** Soft signal filed: a second one while the hamlet is SUSPICIOUS escalates straight to RAID — stay unseen and it decays back to QUIET. [Crouch]

**Retrieved `GDD§2.6#1`** (2. Game Mechanics > 2.6 The town that fights back — what the player sees):

> The hamlet is small — **four posted guards** *(placeholder split: one in the watchtower, one on the barracks door, two wandering)*, the **watchtower** itself (an archer with a bell and a nap schedule; his vision cone visibly droops as he dozes), a small **barracks** (stone-based, unburnable, beatable), and 6–10 civilians on daily routines. The defense is legible on screen, not hidden in numbers, through four visible …

**Retrieved `GDD§2.6#2`** (2. Game Mechanics > 2.6 The town that fights back — what the player sees):

> - **QUIET:** field work, pen chores, pottering. The watchman dozes. This is the stealth half's playground. - **SUSPICIOUS:** the hamlet is sitting on one uncorroborated **soft signal** — a goblin half-confirmed, a corpse discovered, a squawking flock, a forest patrol's horn carrying in from the treeline, a patrol that's overdue and hasn't checked in at its expected waypoint (§2.1), or a hamlet whose daily routines ha…

## 3. Critic findings

### Round 1 — 2 finding(s)

- **LORE_BREAK** in `Prompts_05_FirstSightForestPatrol` (by critic)
  - offending: a look must hold about 1.5 seconds to confirm you, and unconfirmed keeps the hamlet QUIET
  - canon: An unconfirmed sighting is not free: "a goblin half-confirmed" is explicitly one of the soft signals that moves the hamlet QUIET → SUSPICIOUS. The prompt teaches the player a guarantee the town does not give them; only the confirm timing (~1.5s) is canon. — evidence `GDD§2.6#1`
  - correction: Patrol ahead: crouch to break their sightlines — a look has to hold about 1.5 seconds to confirm you, and even a half-confirmed goblin puts the hamlet on SUSPICIOUS. [Crouch]
- **LORE_BREAK** in `Prompts_07_FirstSightCivilianUnaware` (by critic)
  - offending: hold from behind for a silent takedown — no shout, no bell, no witness
  - canon: Takedowns are silent but not clean: there is no body dragging in this slice, and "a guard who *sees* one goes straight to Suspicious" — a discovered corpse is itself a soft signal. The prompt promises an evidence-free kill the mechanic does not deliver. — evidence `GDD§2.4#1`
  - correction: Unaware human: hold from behind for a silent takedown — no shout, no bell, but the body stays where it drops. [Hold E]

### Round 2 — 2 finding(s)

- **LORE_BREAK** in `Prompts_05_FirstSightForestPatrol` (by critic)
  - offending: a look has to hold about 1.5 seconds to confirm you, and even a half-confirmed goblin puts the hamlet on SUSPICIOUS
  - canon: The detection model is one readable rule: a sighting must hold ~1.5s to confirm, and breaking the look before the confirm lands means you were never seen at all. A partial, unconfirmed look costs nothing; only a confirm (or another listed soft signal) moves the town to SUSPICIOUS. The prompt teaches the opposite and undercuts the core stealth promise. — evidence `GDD§2.4#1`
  - correction: Patrol ahead: crouch to break their sightlines — a look has to hold about 1.5 seconds to confirm you; break it sooner and you were never there. [Crouch]
- **LORE_BREAK** in `Prompts_04_FirstTimeWindmillAblaze` (by critic)
  - offending: an aimed torch inside catches the grain dust and detonates the mill
  - canon: The window shot does not detonate the mill; it catches the grain dust to start Stage 2 — Detonation, which builds until the dust blows. Both stages are deliberately named and visible so the burn reads as two legible steps, and the teaching prompt must not collapse them into one instant result. — evidence `GDD§2.8#2`
  - correction: Windmill: Ablaze — needs a window shot; an aimed torch through a window catches the grain dust and starts Stage 2 — Detonation. [Q] Aim, then throw.
