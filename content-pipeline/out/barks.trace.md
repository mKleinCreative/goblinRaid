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

### Barks_01_WatchmanDozeStart

**Output:** The king slew the last of 'em, so what exactly am I watching for… mmnh.

**Retrieved `GDD§2.10#0`** (2. Game Mechanics > 2.10 Tone — the comedy is systemic):

> Humans are greedy first and brave second, and greed is an exploitable AI stat: the coin toss works on guards *because* they're guards in this economy. The watchman's doze is a timer on his perception cone. Text **barks** carry the cheapest comedy per byte ("A goblin? Here? Don't be daft, it's shearing day."). Violence is cartoony-with-a-hint-of-gore — party-favor gibs, clean cartoon bones, an exploding cushion of fea…

**Retrieved `GDD§2.6#1`** (2. Game Mechanics > 2.6 The town that fights back — what the player sees):

> The hamlet is small — **four posted guards** *(placeholder split: one in the watchtower, one on the barracks door, two wandering)*, the **watchtower** itself (an archer with a bell and a nap schedule; his vision cone visibly droops as he dozes), a small **barracks** (stone-based, unburnable, beatable), and 6–10 civilians on daily routines. The defense is legible on screen, not hidden in numbers, through four visible …

### Barks_02_WatchmanDozeEnd

**Output:** Awake! Watching! Been watching this whole time — what time is it?

**Retrieved `GDD§2.6#1`** (2. Game Mechanics > 2.6 The town that fights back — what the player sees):

> The hamlet is small — **four posted guards** *(placeholder split: one in the watchtower, one on the barracks door, two wandering)*, the **watchtower** itself (an archer with a bell and a nap schedule; his vision cone visibly droops as he dozes), a small **barracks** (stone-based, unburnable, beatable), and 6–10 civilians on daily routines. The defense is legible on screen, not hidden in numbers, through four visible …

**Retrieved `GDD§3.1#5`** (3. AI Architecture > 3.1 The game-system agents):

> **5. The Town Agent.** *Role: builds the living hamlet — the four alarm phases, guard perception, the patrol director, the watchman, civilians (including the bind/capture alternative to takedown, §2.6), livestock behavior, the bucket brigade, and the finite raid-response pool that caps RAID-phase reinforcements.* Player-facing effect: the town that visibly fights back and never calms down — the drooping vision cone o…

### Barks_03_CoinTossScramble

**Output:** Ooh — dropped coin, no owner, that's salvage, that's mine by law.

**Retrieved `GDD§2.4#1`** (2. Game Mechanics > 2.4 Stealth — the quiet half of the raid):

> Stealth is deliberately *lite*: the whole detection model is one readable rule — a guard's sighting of you must **hold for about 1.5 seconds to confirm**. Duck behind a haycart before the confirm lands and you were never there; get confirmed and the town moves to Suspicious. On top of that foundation sit five systems, and only five: 1. **Noise.** Every action has an audible radius the player learns by feel: walking i…

**Retrieved `GDD§2.10#0`** (2. Game Mechanics > 2.10 Tone — the comedy is systemic):

> Humans are greedy first and brave second, and greed is an exploitable AI stat: the coin toss works on guards *because* they're guards in this economy. The watchman's doze is a timer on his perception cone. Text **barks** carry the cheapest comedy per byte ("A goblin? Here? Don't be daft, it's shearing day."). Violence is cartoony-with-a-hint-of-gore — party-favor gibs, clean cartoon bones, an exploding cushion of fea…

### Barks_04_CoinTossScramble

**Output:** Coins in the mud at the king's feet — the statue's paying out!

**Retrieved `GDD§2.4#1`** (2. Game Mechanics > 2.4 Stealth — the quiet half of the raid):

> Stealth is deliberately *lite*: the whole detection model is one readable rule — a guard's sighting of you must **hold for about 1.5 seconds to confirm**. Duck behind a haycart before the confirm lands and you were never there; get confirmed and the town moves to Suspicious. On top of that foundation sit five systems, and only five: 1. **Noise.** Every action has an audible radius the player learns by feel: walking i…

**Retrieved `GDD§2.10#0`** (2. Game Mechanics > 2.10 Tone — the comedy is systemic):

> Humans are greedy first and brave second, and greed is an exploitable AI stat: the coin toss works on guards *because* they're guards in this economy. The watchman's doze is a timer on his perception cone. Text **barks** carry the cheapest comedy per byte ("A goblin? Here? Don't be daft, it's shearing day."). Violence is cartoony-with-a-hint-of-gore — party-favor gibs, clean cartoon bones, an exploding cushion of fea…

### Barks_05_FlockStartledInvestigate

**Output:** Squawk all you like, hens — it's a fox, it's always a fox, it has never once not been a fox.

**Retrieved `GDD§2.7#1`** (2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system):

> **Livestock are loot that runs away.** The hamlet's pens hold chickens, sheep, and pigs (kitbashed from the same stylized packs as the humans). *Killing* an animal is a petty deed — 2 points for a chicken, 5 for a sheep, 8 for a pig, party-favor feathers included — permitted and pointless, exactly like civilian kills. *Stealing* one is the real prize: a live pig is a squirming over-the-shoulder carry worth a fat 40 l…

**Retrieved `GDD§2.6#1`** (2. Game Mechanics > 2.6 The town that fights back — what the player sees):

> The hamlet is small — **four posted guards** *(placeholder split: one in the watchtower, one on the barracks door, two wandering)*, the **watchtower** itself (an archer with a bell and a nap schedule; his vision cone visibly droops as he dozes), a small **barracks** (stone-based, unburnable, beatable), and 6–10 civilians on daily routines. The defense is legible on screen, not hidden in numbers, through four visible …

### Barks_06_FlockStartledNearbyRoutine

**Output:** A goblin? In my pen? Don't be daft, it's shearing day.

**Retrieved `GDD§2.10#0`** (2. Game Mechanics > 2.10 Tone — the comedy is systemic):

> Humans are greedy first and brave second, and greed is an exploitable AI stat: the coin toss works on guards *because* they're guards in this economy. The watchman's doze is a timer on his perception cone. Text **barks** carry the cheapest comedy per byte ("A goblin? Here? Don't be daft, it's shearing day."). Violence is cartoony-with-a-hint-of-gore — party-favor gibs, clean cartoon bones, an exploding cushion of fea…

**Retrieved `GDD§2.7#1`** (2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system):

> **Livestock are loot that runs away.** The hamlet's pens hold chickens, sheep, and pigs (kitbashed from the same stylized packs as the humans). *Killing* an animal is a petty deed — 2 points for a chicken, 5 for a sheep, 8 for a pig, party-favor feathers included — permitted and pointless, exactly like civilian kills. *Stealing* one is the real prize: a live pig is a squirming over-the-shoulder carry worth a fat 40 l…

## 3. Critic findings

### Round 1 — 2 finding(s)

- **LORE_BREAK** in `Barks_08_CorpseDiscoveredSecond` (by critic)
  - offending: "alarm_state": "Suspicious" … "Face-down, back opened, and not one goblin in the county for a hundred years — so it's a badger with ambition."
  - canon: A second soft signal of any kind (a corpse discovered counts) arriving while the hamlet is already SUSPICIOUS does not refresh the clock — it escalates straight to RAID. A second corpse therefore cannot sit in a sustained Suspicious state with the denial holding. (Secondary: militia are not posted in the hamlet during Quiet/Suspicious — the barracks only starts producing militia once RAID begins, so a Militia speaker in Suspicious does not exist in this slice.) — evidence `GDD§2.6#1`
  - correction: Speaker: Guard; alarm_state: Raid; line: "Second body. Badger with ambition, that's — no. No. BELL! RING THE BELL!" (Note should read: second soft signal while Suspicious escalates straight to RAID; the denial breaks mid-sentence, which is where the joke lands.)
- **LORE_BREAK** in `Barks_12_PatrolOverdueNoCheckIn` (by critic)
  - offending: three grown men lost on a road with signposts on it
  - canon: A forest patrol is three guards — two militia and an archer — and the archer who carries the patrol's signal horn is canonically female ("a committed, interruptible wind-up she sounds the moment the patrol is losing"). "Three grown men" miscounts the patrol's makeup. — evidence `GDD§2.1#1`
  - correction: That patrol's well past its waypoint — three grown soldiers lost on a road with signposts on it.

### Round 2 — 1 finding(s)

- **LORE_BREAK** in `Barks_15_BucketBrigadeFormUp` (by critic)
  - offending: Bucket line, move! Pass it — don't sniff it, just throw it!
  - canon: Fouling the well *permanently disables* the bucket brigade (+30 counterplay). There is no state where the brigade forms up and passes fouled water — either the well is clean and the brigade works, or it's fouled and the brigade is gone for the raid. The row's own note ("the fouled water lands as a gag inside the firefighting behaviour") describes a behaviour the slice does not have, and it collides with row 16, which correctly plays the brigade as dead once the well is fouled. — evidence `GDD§2.6#3`
  - correction: Bucket line, move! Well to the granary — pass it up, don't spill it!

### Round 3 — 0 finding(s)

