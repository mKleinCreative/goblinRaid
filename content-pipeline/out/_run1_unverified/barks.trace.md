# Retrieval trace — NPC bark sheet — livestock & stealth extension pass

**Gap this fills.** GDD 3.2 (The Writer): the approved bark sheet is 'now due a livestock-and-stealth extension pass'. The livestock system (2.7) and the lean-five stealth layer (2.4) both landed after the sheet was approved, so the states they introduce — a startled flock, a coin scramble, a discovered corpse, an overdue patrol — have no lines.

## 1. Queries and what they retrieved

**Query:** `guard perception confirm suspicious corpse discovered investigate`

- `GDD§2.4#1` (BM25 12.419) — 2. Game Mechanics > 2.4 Stealth — the quiet half of the raid
- `GDD§2.4#0` (BM25 10.663) — 2. Game Mechanics > 2.4 Stealth — the quiet half of the raid
- `GDD§2.10#0` (BM25 8.714) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic
- `GDD§2.6#1` (BM25 8.284) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees

**Query:** `livestock chickens sheep pigs pens startled flock squawking noise alarm`

- `GDD§2.7#1` (BM25 27.303) — 2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system
- `GDD§2.7#2` (BM25 15.305) — 2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system
- `GDD§2.7#0` (BM25 14.349) — 2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system
- `GDD§revision-growth#2` (BM25 10.193) — 1. Executive Summary > Revision & growth — what changed since Assignment #1

**Query:** `civilians routines panic bucket brigade well watchman doze bell`

- `GDD§2.6#2` (BM25 15.089) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§2.6#3` (BM25 15.042) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§3.1#4` (BM25 14.861) — 3. AI Architecture > 3.1 The game-system agents
- `GDD§3.1#5` (BM25 14.308) — 3. AI Architecture > 3.1 The game-system agents

**Query:** `tone comedy barks propaganda goblins extinct badger denial shearing day`

- `GDD§2.10#0` (BM25 34.887) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic
- `GDD§2.10#1` (BM25 26.977) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic
- `GDD§2.10#2` (BM25 23.57) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic
- `GDD§2.10#3` (BM25 22.859) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic

**Query:** `coin toss lure greedy guards scramble distraction`

- `GDD§2.4#1` (BM25 13.801) — 2. Game Mechanics > 2.4 Stealth — the quiet half of the raid
- `GDD§2.4#2` (BM25 13.432) — 2. Game Mechanics > 2.4 Stealth — the quiet half of the raid
- `GDD§4.5#4` (BM25 9.612) — 4. Technical Strategy > 4.5 Schedule and honest scoping
- `GDD§2.10#0` (BM25 8.681) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic

**Query:** `forest patrol torches archer signal horn overdue check in waypoint`

- `GDD§2.6#1` (BM25 17.017) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§2.6#2` (BM25 14.634) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§2.1#1` (BM25 12.56) — 2. Game Mechanics > 2.1 The core loop
- `GDD§3.4#0` (BM25 10.616) — 3. AI Architecture > 3.4 In-game AI (for clarity: conventional, not LLM)

**Query:** `alarm phases QUIET SUSPICIOUS RAID RAZED escalation soft signal`

- `GDD§2.6#1` (BM25 20.089) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§2.6#2` (BM25 17.114) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§1#3` (BM25 8.418) — 1. Executive Summary
- `GDD§2.4#0` (BM25 8.024) — 2. Game Mechanics > 2.4 Stealth — the quiet half of the raid

## 2. Retrieved chunk -> generated line

### Barks_01_WatchmanDozeStartPerceptionC

**Output:** Nothing ever comes over that treeline... nothing... ever...

**Retrieved `GDD§2.10#0`** (2. Game Mechanics > 2.10 Tone — the comedy is systemic):

> Humans are greedy first and brave second, and greed is an exploitable AI stat: the coin toss works on guards *because* they're guards in this economy. The watchman's doze is a timer on his perception cone. Text **barks** carry the cheapest comedy per byte ("A goblin? Here? Don't be daft, it's shearing day."). Violence is cartoony-with-a-hint-of-gore — party-favor gibs, clean cartoon bones, an exploding cushion of fea…

**Retrieved `GDD§2.6#1`** (2. Game Mechanics > 2.6 The town that fights back — what the player sees):

> The hamlet is small — **four posted guards** *(placeholder split: one in the watchtower, one on the barracks door, two wandering)*, the **watchtower** itself (an archer with a bell and a nap schedule; his vision cone visibly droops as he dozes), a small **barracks** (stone-based, unburnable, beatable), and 6–10 civilians on daily routines. The defense is legible on screen, not hidden in numbers, through four visible …

### Barks_02_PenChoresFlockAmbient

**Output:** Settle down, you feathered idiots, nobody's shearing you — it's the sheep's day.

**Retrieved `GDD§2.6#1`** (2. Game Mechanics > 2.6 The town that fights back — what the player sees):

> The hamlet is small — **four posted guards** *(placeholder split: one in the watchtower, one on the barracks door, two wandering)*, the **watchtower** itself (an archer with a bell and a nap schedule; his vision cone visibly droops as he dozes), a small **barracks** (stone-based, unburnable, beatable), and 6–10 civilians on daily routines. The defense is legible on screen, not hidden in numbers, through four visible …

**Retrieved `GDD§2.10#0`** (2. Game Mechanics > 2.10 Tone — the comedy is systemic):

> Humans are greedy first and brave second, and greed is an exploitable AI stat: the coin toss works on guards *because* they're guards in this economy. The watchman's doze is a timer on his perception cone. Text **barks** carry the cheapest comedy per byte ("A goblin? Here? Don't be daft, it's shearing day."). Violence is cartoony-with-a-hint-of-gore — party-favor gibs, clean cartoon bones, an exploding cushion of fea…

### Barks_03_CoinTossHeardScrambleBegins

**Output:** Is that a penny? That's my penny, I've been looking for that penny.

**Retrieved `GDD§2.4#1`** (2. Game Mechanics > 2.4 Stealth — the quiet half of the raid):

> Stealth is deliberately *lite*: the whole detection model is one readable rule — a guard's sighting of you must **hold for about 1.5 seconds to confirm**. Duck behind a haycart before the confirm lands and you were never there; get confirmed and the town moves to Suspicious. On top of that foundation sit five systems, and only five: 1. **Noise.** Every action has an audible radius the player learns by feel: walking i…

**Retrieved `GDD§2.10#0`** (2. Game Mechanics > 2.10 Tone — the comedy is systemic):

> Humans are greedy first and brave second, and greed is an exploitable AI stat: the coin toss works on guards *because* they're guards in this economy. The watchman's doze is a timer on his perception cone. Text **barks** carry the cheapest comedy per byte ("A goblin? Here? Don't be daft, it's shearing day."). Violence is cartoony-with-a-hint-of-gore — party-favor gibs, clean cartoon bones, an exploding cushion of fea…

### Barks_04_PatrolWaypointCheckIn

**Output:** Waypoint's clear, horn's dry, back in five— six if the mud's bad. Mark us in.

**Retrieved `GDD§2.1#1`** (2. Game Mechanics > 2.1 The core loop):

> One raid, capped at 30 minutes: 1. **Step through as the Scout.** This slice's one playable kit — daggers and bow, live-swapped — is where every raid starts: no class-select screen this slice, just a straight drop into the **runic site** — a ring of glowing standing stones in a forest clearing. This is spawn, respawn, and the extraction point, all in one. Humans are too superstitious to enter the circle; it is the on…

**Retrieved `GDD§3.4#0`** (3. AI Architecture > 3.4 In-game AI (for clarity: conventional, not LLM)):

> The NPCs the player fights are classical game AI, *built by* the agents above, not powered by them at runtime: behavior trees for guards, civilians, livestock, and the horde; a perception system whose entire stealth model is one number (a sighting must hold ~1.5 seconds to confirm); a **patrol director** that spawns forest patrols on a 5–7-minute cadence, tracks whether each one checks back in at its next waypoint (t…

### Barks_05_SightingUnconfirmedDecayedTo

**Output:** Little green fella by the haycart... no. No. Badger. Big badger. Long day.

**Retrieved `GDD§2.6#1`** (2. Game Mechanics > 2.6 The town that fights back — what the player sees):

> The hamlet is small — **four posted guards** *(placeholder split: one in the watchtower, one on the barracks door, two wandering)*, the **watchtower** itself (an archer with a bell and a nap schedule; his vision cone visibly droops as he dozes), a small **barracks** (stone-based, unburnable, beatable), and 6–10 civilians on daily routines. The defense is legible on screen, not hidden in numbers, through four visible …

**Retrieved `GDD§2.10#0`** (2. Game Mechanics > 2.10 Tone — the comedy is systemic):

> Humans are greedy first and brave second, and greed is an exploitable AI stat: the coin toss works on guards *because* they're guards in this economy. The watchman's doze is a timer on his perception cone. Text **barks** carry the cheapest comedy per byte ("A goblin? Here? Don't be daft, it's shearing day."). Violence is cartoony-with-a-hint-of-gore — party-favor gibs, clean cartoon bones, an exploding cushion of fea…

### Barks_06_FlockStartledNearestGuardInv

**Output:** Hens are off — right, who's been in the pen, and did they leave the gate again?

**Retrieved `GDD§2.7#1`** (2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system):

> **Livestock are loot that runs away.** The hamlet's pens hold chickens, sheep, and pigs (kitbashed from the same stylized packs as the humans). *Killing* an animal is a petty deed — 2 points for a chicken, 5 for a sheep, 8 for a pig, party-favor feathers included — permitted and pointless, exactly like civilian kills. *Stealing* one is the real prize: a live pig is a squirming over-the-shoulder carry worth a fat 40 l…

**Retrieved `GDD§2.6#1`** (2. Game Mechanics > 2.6 The town that fights back — what the player sees):

> The hamlet is small — **four posted guards** *(placeholder split: one in the watchtower, one on the barracks door, two wandering)*, the **watchtower** itself (an archer with a bell and a nap schedule; his vision cone visibly droops as he dozes), a small **barracks** (stone-based, unburnable, beatable), and 6–10 civilians on daily routines. The defense is legible on screen, not hidden in numbers, through four visible …

## 3. Critic findings

### Round 1 — 2 finding(s)

- **LORE_BREAK** in `Barks_04_PatrolWaypointCheckIn` (by critic)
  - offending: Waypoint's clear, horn's dry, back before the hour — mark us in.
  - canon: Forest patrols sweep and are expected back at the hamlet on a 5–7-minute cadence, with a couple of minutes' grace before the missing-check-in soft signal fires — not an hourly rotation. 'Back before the hour' sets the wrong cadence in the player's ear, which breaks the row's own stated purpose of making the later overdue-patrol signal legible. — evidence `GDD§2.1#1`
  - correction: Waypoint's clear, horn's dry, back inside the half hour— call it five minutes. Mark us in.
- **LORE_BREAK** in `Barks_02_PenChoresFlockAmbient` (by critic)
  - offending: Settle down, you feathered idiots, it's shearing day.
  - canon: The hamlet's pens hold chickens, sheep, and pigs as distinct livestock; shearing is a sheep chore, so addressing the feathered flock as the subjects of shearing day misreads the pen roster the whole livestock system is built on. — evidence `GDD§2.7#0`
  - correction: Settle down, you feathered idiots, nobody's shearing you — it's the sheep's day.

### Round 2 — 1 finding(s)

- **LORE_BREAK** in `Barks_04_PatrolWaypointCheckIn` (by critic)
  - offending: Waypoint's clear, horn's dry, back inside the half hour— call it five minutes. Mark us in.
  - canon: Forest patrols sweep and are expected back at the hamlet on a 5–7-minute cadence; a patrol that misses that window files the overdue soft signal. 'Half hour' is the raid clock, not the patrol cadence, and stating it in the bark teaches the player the wrong interval the overdue signal is measured against. — evidence `GDD§2.1#1`
  - correction: Waypoint's clear, horn's dry, back in five— six if the mud's bad. Mark us in.
