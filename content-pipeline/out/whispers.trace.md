# Retrieval trace — His Eternal Darkness — whisper and verdict lines

**Gap this fills.** GDD 3.2 (The Writer): 'new capture/purge-triggered Overlord verdict lines (2.10)' are outstanding. 2.10 asks for a raid that comes home with a rope chain of prisoners to earn a different flavour of verdict than one that comes home merely rich, and the clock nudges at 10/5/2 minutes have no lines either.

## 1. Queries and what they retrieved

**Query:** `His Eternal Darkness whispers verdict adequate rats layered subtitles register`

- `GDD§1#0` (BM25 21.226) — 1. Executive Summary
- `GDD§2.10#1` (BM25 17.01) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic
- `GDD§2.10#2` (BM25 16.407) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic
- `GDD§1#1` (BM25 16.254) — 1. Executive Summary

**Query:** `capture bind prisoners rope chain None Left Behind tribute proof they remember us`

- `GDD§2.10#1` (BM25 18.168) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic
- `GDD§2.10#2` (BM25 16.41) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic
- `GDD§2.7#3` (BM25 13.576) — 2. Game Mechanics > 2.7 Loot, livestock, and couriers — greed as a system
- `GDD§2.6#3` (BM25 12.408) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees

**Query:** `raid clock 30 minutes ten five two minute nudges portal collapse grace window`

- `GDD§2.2#0` (BM25 19.161) — 2. Game Mechanics > 2.2 Win, lose, and the three pressures
- `GDD§1#2` (BM25 17.128) — 1. Executive Summary
- `GDD§1#1` (BM25 15.844) — 1. Executive Summary
- `GDD§3.1#5` (BM25 15.612) — 3. AI Architecture > 3.1 The game-system agents

**Query:** `score deeds loot banking extraction multiplier end screen letter grade personal best`

- `GDD§2.9#0` (BM25 27.146) — 2. Game Mechanics > 2.9 Scoring (placeholder values, tuned in week 6–7 playtests)
- `GDD§3.2#0` (BM25 13.767) — 3. AI Architecture > 3.2 The studio agents
- `GDD§3.2#1` (BM25 12.659) — 3. AI Architecture > 3.2 The studio agents
- `GDD§2.1#1` (BM25 12.556) — 2. Game Mechanics > 2.1 The core loop

**Query:** `windmill ablaze detonation window torch two stage set piece`

- `GDD§2.8#1` (BM25 23.659) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§2.8#2` (BM25 23.207) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§2.8#3` (BM25 7.221) — 2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of
- `GDD§1#1` (BM25 5.974) — 1. Executive Summary

**Query:** `now is the time of the goblins propaganda vengeance theme tone escalates`

- `GDD§2.10#2` (BM25 23.587) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic
- `GDD§2.10#3` (BM25 19.14) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic
- `GDD§2.10#1` (BM25 16.013) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic
- `GDD§2.10#0` (BM25 8.822) — 2. Game Mechanics > 2.10 Tone — the comedy is systemic

**Query:** `runic site standing stones portal extraction courier banking`

- `GDD§1#1` (BM25 16.788) — 1. Executive Summary
- `GDD§3.1#5` (BM25 12.685) — 3. AI Architecture > 3.1 The game-system agents
- `GDD§2.6#3` (BM25 12.269) — 2. Game Mechanics > 2.6 The town that fights back — what the player sees
- `GDD§1#0` (BM25 11.566) — 1. Executive Summary

## 2. Retrieved chunk -> generated line

### Whispers_01_ClockTenMinutes

**Output:** Ten minutes, goblin. I am checking on you. Something should be on fire by now.

**Retrieved `GDD§2.2#0`** (2. Game Mechanics > 2.2 Win, lose, and the three pressures):

> - **Win:** all three assigned objectives burned *and* you exit through the portal. Deeds (kills, arson, mischief bonuses) multiply ×1.5 on a successful exit, plus +20 per unused life. - **Lose:** all 5 lives spent, or still outside the circle when the portal dies (0:00 plus a 90-second collapse window — screen-wide warning, guttering stone-glow, the whispers going cold). Either way you keep only 25% of unbanked deeds…

**Retrieved `GDD§1#2`** (1. Executive Summary):

> **Win condition:** burn all three of the raid's assigned objectives — this slice's single tutorial hamlet carries one of each: the village core's granary, a farmstead's field, and the windmill (a randomized mix across many hamlets, never more than two of a kind, is the post-slice generator's job, §2.8) — which opens the extraction portal at the runic site; then step through it before the 30-minute raid clock (plus a …

### Whispers_02_ClockFiveMinutes

**Output:** Five minutes. What still stands lies {dir} — follow the smoke and stop admiring the pigs.

**Retrieved `GDD§2.2#0`** (2. Game Mechanics > 2.2 Win, lose, and the three pressures):

> - **Win:** all three assigned objectives burned *and* you exit through the portal. Deeds (kills, arson, mischief bonuses) multiply ×1.5 on a successful exit, plus +20 per unused life. - **Lose:** all 5 lives spent, or still outside the circle when the portal dies (0:00 plus a 90-second collapse window — screen-wide warning, guttering stone-glow, the whispers going cold). Either way you keep only 25% of unbanked deeds…

**Retrieved `GDD§2.8#3`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> This hamlet carries **all three burn objectives in a single fixed layout — one granary, one field, one windmill** — rather than the randomized two-of-three mix a procedural version will eventually roll; a new player sees every objective type, and how each one burns, in their very first raid instead of maybe two of them. **Trees and greenery** (hedgerows, copses, garden plots) fill the seams between modules and double…

### Whispers_03_ClockTwoMinutes

**Output:** Two minutes. The stones are {dir}. Run, thief — I close the portal, not the door.

**Retrieved `GDD§2.2#0`** (2. Game Mechanics > 2.2 Win, lose, and the three pressures):

> - **Win:** all three assigned objectives burned *and* you exit through the portal. Deeds (kills, arson, mischief bonuses) multiply ×1.5 on a successful exit, plus +20 per unused life. - **Lose:** all 5 lives spent, or still outside the circle when the portal dies (0:00 plus a 90-second collapse window — screen-wide warning, guttering stone-glow, the whispers going cold). Either way you keep only 25% of unbanked deeds…

**Retrieved `GDD§2.1#1`** (2. Game Mechanics > 2.1 The core loop):

> One raid, capped at 30 minutes: 1. **Step through as the Scout.** This slice's one playable kit — daggers and bow, live-swapped — is where every raid starts: no class-select screen this slice, just a straight drop into the **runic site** — a ring of glowing standing stones in a forest clearing. This is spawn, respawn, and the extraction point, all in one. Humans are too superstitious to enter the circle; it is the on…

### Whispers_04_ObjectiveGranaryStage1FirstT

**Output:** Their whole winter, stacked in one shed — light it and go. The guards are thickest here; that is the tax.

**Retrieved `GDD§2.8#1`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> **One hand-authored hamlet, not a generator (decided, 2026-07-23).** The slice ships a single, hand-composed hamlet assembled once from three hand-authored **module types**, stitched together with trees and greenery on an authored road-and-plots layout — not a constrained generator assembling a fresh one every raid. That's a deliberate scope call, not a fallback: procedurally generating an entire map free of placemen…

**Retrieved `GDD§2.8#3`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> This hamlet carries **all three burn objectives in a single fixed layout — one granary, one field, one windmill** — rather than the randomized two-of-three mix a procedural version will eventually roll; a new player sees every objective type, and how each one burns, in their very first raid instead of maybe two of them. **Trees and greenery** (hedgerows, copses, garden plots) fill the seams between modules and double…

### Whispers_05_ObjectiveFieldStage1FirstTim

**Output:** One torch in the wheat, goblin. The fire walks the rows itself — let their buckets try to keep up.

**Retrieved `GDD§2.8#1`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> **One hand-authored hamlet, not a generator (decided, 2026-07-23).** The slice ships a single, hand-composed hamlet assembled once from three hand-authored **module types**, stitched together with trees and greenery on an authored road-and-plots layout — not a constrained generator assembling a fresh one every raid. That's a deliberate scope call, not a fallback: procedurally generating an entire map free of placemen…

**Retrieved `GDD§2.8#3`** (2. Game Mechanics > 2.8 The tutorial hamlet — and the ladder it's the first rung of):

> This hamlet carries **all three burn objectives in a single fixed layout — one granary, one field, one windmill** — rather than the randomized two-of-three mix a procedural version will eventually roll; a new player sees every objective type, and how each one burns, in their very first raid instead of maybe two of them. **Trees and greenery** (hedgerows, copses, garden plots) fill the seams between modules and double…

### Whispers_06_LootFirstSackBanked

**Output:** The first sack crosses the stones. It is mine now. No bell un-takes it.

**Retrieved `GDD§1#1`** (1. Executive Summary):

> **Goblin Siege** is a third-person raid game in which the player is the monster. You are a goblin. Your Overlord — an unseen voice called **His Eternal Darkness**, who communicates only in layered whispers and subtitles — sends you through a runic portal into a dusk-lit forest in the county of **Groatsworth** *(name placeholder in the game's cozy-smug naming register)*, outside a tiny, unwalled human farming hamlet —…

**Retrieved `GDD§2.2#0`** (2. Game Mechanics > 2.2 Win, lose, and the three pressures):

> - **Win:** all three assigned objectives burned *and* you exit through the portal. Deeds (kills, arson, mischief bonuses) multiply ×1.5 on a successful exit, plus +20 per unused life. - **Lose:** all 5 lives spent, or still outside the circle when the portal dies (0:00 plus a 90-second collapse window — screen-wide warning, guttering stone-glow, the whispers going cold). Either way you keep only 25% of unbanked deeds…

## 3. Critic findings

### Round 1 — 1 finding(s)

- **TONE_DRIFT** in `Whispers_13_VerdictWinRichNoCaptives` (by critic)
  - offending: and no one alive to say who took them
  - canon: Tier 1's Overlord mocks rather than menaces, and the running joke depends on surviving villagers doing the denying ("must've been a badger"); a body count is explicitly *not* the tribute His Eternal Darkness wants — the contrast for a "merely rich" raid is the absence of prisoners/proof, not a silent massacre. — evidence `GDD§2.10#1`
  - correction: Gold, a pig, three fires — and not one of them roped to admit we were ever here. Adequate. I have seen rats do better. …adequate.

### Round 2 — 1 finding(s)

- **REGISTER_DRIFT** in `Whispers_13_VerdictWinRichNoCaptives` (by critic)
  - offending: Gold, a pig, three fires — and not one of them roped to admit we were ever here. Adequate. I have seen rats do better. …adequate.
  - canon: The canonical verdict stinger is a single compact whisper — "Adequate. I have seen rats do better. …adequate." Wrapping it in an extra lead-in clause and then saying "Adequate" twice inside the same line makes it too long and stuttering to deliver as one spoken bark; the stinger's shape depends on "adequate" landing once at the front-load and once as the trailing sigh, not three times. — evidence `GDD§2.1#1`
  - correction: Gold, a pig, three fires — and not one of them roped to say we were ever here. I have seen rats do better. …adequate.

### Round 3 — 0 finding(s)

