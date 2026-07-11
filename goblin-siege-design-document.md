# GOBLIN SIEGE — Game Design Document

*Major revision · "The Settlement Raid" — restructures the game around one core level for the 7-week UE5 prototype. Supersedes the arena-combat draft; combat/weapon content carries forward unchanged unless noted. All §13 blocking/important questions are now resolved (2026-07-10 review).*

---

## 1. Vision

**Goblin Siege is a third-person co-operative raid game in which you play the monsters. You and your goblin warband slip out of your Overlord's lair, creep through the forest, breach a fortified human settlement by wit or by force, burn it down, rob it blind, and scurry home before the humans get organized.**

It plays like a satire. You are the evil exposing the idiocy of the good: the humans are pompous, greedy, and a little bit stupid, and the game's biggest laughs come from watching their so-called civilization fail exactly the way you'd expect — the watchman asleep at his post, the guards abandoning a chase to scoop up dropped coins, the town fighting over buckets while its granary burns.

### Design pillars

- **Weighty, readable combat.** Attacks commit, dodges commit, enemies telegraph. Every hit has impact — hitstop, knockback, stagger. You win by reading the fight, not mashing. *(Unchanged from the combat prototype — this is proven.)*
- **Fire and destruction as a language.** Torches, flammable buildings, and a settlement that visibly tries to save itself. The town is made of wood and the goblins know it.
- **Many ways over the wall.** The palisade is a puzzle, not a corridor. Dig under it, batter through it, fly over it, or find the gap the humans never fixed. How you get *in* is as much the game as what you do inside.
- **A horde at your back.** One horn-blast and the treeline empties. The AI horde makes a lone goblin feel like an invasion — cheap to command, expendable by design, hilarious in motion.
- **Pressure, not safety.** The longer you linger, the harder the settlement fights back. Greed (more loot, more fire, more score) pulls against survival (make it back to the runic site — nothing counts until you're home).
- **Satire, always.** Every system should get a chance to be funny. Humans are greedy and dim; goblins are gleeful vermin. If a mechanic can express that (coin distractions, bucket brigades, a snoring watchman), lean in.

### Touchstones

- **Overlord** — the tone anchor. Gleeful evil, an expendable minion horde, the "good" people revealed as venal idiots.
- **Darktide** — horde combat, class identity, mixed-enemy waves demanding target priority.
- **Helldivers** — mission-and-extraction structure; nothing is yours until you extract.
- **Elden Ring** — deliberate stamina-gated melee, dodge i-frames, telegraphs you read rather than out-click.
- **Enshrouded** *(environmental)* — smashable, burnable structures that reshape the play space.

---

## 2. Core gameplay loop

1. **In the Overlord's lair**, pick your class (weapon-kit) and step into the runic circle.
2. **Materialize at the runic site** — a ring of glowing standing stones in a forest clearing. This is spawn, home base, and the extraction point, all in one.
3. **Move through the forest** along (or off) the paths toward the settlement, dodging or ambushing human patrols. Getting spotted and letting a runner escape warns the town.
4. **Scout the settlement.** Walk the palisade. Find the gap. Watch the gate change guards. Count the woodpiles.
5. **Breach** — dig under the wall, gather wood and build a ram or a catapult, squeeze through the hidden gap, or slip one goblin inside to open the gate for everyone else.
6. **Raid** — burn the three granaries (the core objective), loot the marketplace, fight the guards, sound the horn and let the horde loose. The settlement fights back: the barracks pumps out guards, civilians run bucket lines from the well to douse your fires.
7. **Get out.** Completing the objective **opens the portal** at the runic site. Back through the forest — the humans may pursue — step into the circle to teleport home, tossing loot into the portal as you go.
8. **Bank your score.** Points are tallied from objectives, loot, and mischief — **fully banked only if you make it back.** *(Prototype: score screen and personal-best; the spending economy comes later — see §10.)*

**Every raid is capped at 30 minutes** (decided). The runic circle only holds its charge so long — His Eternal Darkness does not wait on goblins. See §9 for the clock's rules.

---

## 3. Tone & satire — how the game stays funny

The comedy is systemic, not just written. Rules of thumb:

- **Humans are greedy first, brave second.** Greed is an exploitable AI stat, not just a joke. Guards will break off a chase to pick up dropped coins. Civilians will loot their *own* marketplace once chaos starts. The town's wood for your battering ram? Stacked neatly outside their own walls.
- **Humans are confident and wrong.** The watchtower guard dozes. The gate guards bicker. Bark lines drip with misplaced superiority ("A goblin? Here? Don't be daft, it's market day.").
- **Goblins are vermin, and proud.** The horde giggles, trips, cheers when things burn, and screams with joy when catapulted. Being flung over a wall as living ammunition is the mission statement.
- **His Eternal Darkness frames everything.** The Overlord is never seen and never shouts — he *whispers*. His voice is layered dark whispers with subtitles, seeping in to set objectives, comment on your failures, and deliver the post-raid verdict. The contrast — apocalyptic gravitas applied to burning a grain silo in a town called Pennybrook — is the joke. He is the tutorial, the narrator, and evil middle-management all at once. *(Decided: name and whisper+subtitle delivery are locked; no VO actor needed for the prototype.)*

### Violence style — cartoony gore *(decided)*

Everyone in Pennybrook is killable — guards *and* civilians — and the violence is **cartoony with a hint of gore**: exaggerated blood spatters, flying limbs, and comically clean cartoon bones. Think Saturday-morning-meets-slaughter: gibs that pop like party favors, never lingering viscera or realistic wound detail. Deaths are punchlines, not snuff.

- **Rating target: T at minimum.** Stylized/exaggerated cartoon gore is what keeps dismemberment on the T side of the line (à la Castle Crashers); the art direction (§11) does the heavy lifting. If a ratings pass ever pushes back, gib intensity is a single tuning scalar, not a redesign.
- **Civilian kills score** (see §10) — modest points, because the satire lands better when killing civilians is *permitted and petty* rather than either forbidden or lucrative. Fleeing, screaming, and dropping their own stolen loot is their real contribution.

**Satire mechanics (prototype-scoped where marked):**

| Mechanic | What it does | Prototype? |
|---|---|---|
| **Coin toss** | Throw looted coins to make greedy humans (guards included) break behavior and scramble for them. A distraction tool that *is* the theme. | Stretch — high value, small system (a "lure" stimulus) |
| **Sleeping watchman** | The watchtower guard periodically dozes; his vision cone visibly droops. Timing your approach around his naps is stealth-as-comedy. | Yes — it's a timer on his perception |
| **Self-looting civilians** | Once alarm passes a threshold, some civilians grab loot from market stalls and run for their houses — competing with *you* for score. | Stretch |
| **Bucket brigade** | Civilians queue at the well and douse fires — earnest, orderly, and completely outmatched. Foul the well to stop it. | Yes — core counter-system (§7) |
| **Bark system** | Short text barks on guards/civilians keyed to states (idle, suspicious, alarmed, greedy, fleeing). Cheapest comedy per byte in the game. | Yes — text only |

*(The former open question on civilian harm is resolved above: killable, scored, cartoony gore, T-rating target.)*

---

## 4. The goblins — classes & weapons

**Unchanged from the previous draft.** A goblin's identity is its weapon-kit; each kit has its own (future) progression tree. Full stats and kit details live in the prior combat sections and `race-design-goblins.md`; summary:

| Class | Kit | HP | Speed | Role |
|---|---|---|---|---|
| **Slasher** | Daggers ⇄ Bow (one weapon, live swap) | 110 | Fast | Mobile skirmisher, flex melee/ranged — the natural scout & inside-job goblin |
| **Brute** | Great-club | 170 | Slow | Frontline breaker — best vs the gate, cover, and formations; 1.5× body & reach |
| **Shaman (Shadow)** | Shadow Staff | 85 | Medium | Ranged armor-ignoring pressure + group root/stun (Shadow Grasp) |
| **Shaman (Blood)** | Blood Staff | 85 | Medium | Blood-orb economy caster — dagger/spear/lance conjured arsenal |

**Universal kit (every class):**

- **Torch toss (Q)** — sticks where it lands, ignites, spreads. The racial equalizer.
- **The Horn (G)** — summons the AI horde (§5). New; replaces the old fixed "2 AI companions."
- **Dodge roll** — 0.22s i-frames, committed recovery.
- **Interact (E/hold E)** — dig, chop, carry, build, open gate, loot. The new verbs of the raid layer.
- **5 lives** — losing all HP costs a life; respawn at the runic site (decided — see §9). Losing the last life ends your raid.

**Class texture in the new loop (design intent, not hard gating):** the Brute batters gates and hauls two wood bundles at once; the Slasher squeezes through gaps fastest and picks off the watchman at range; the Shamans keep the horde alive and control the bucket brigade. Every class can do every raid verb — classes change *how well*, never *whether*.

---

## 5. The Horn & the horde — NEW core system

Every goblin carries a war-horn. Blowing it (G) calls AI goblins scurrying out of the treeline to fight beside you.

### Rules

- **Cap:** 10 active horde goblins per player *(prototype: 1 player → 10; tunable after playtests)*.
- **Reserve:** each raid has a finite horde pool — starting at **2× the cap** (20 for solo). Dead horde goblins are gone; the horn refills you from the remaining pool. When the pool is dry, the treeline is silent. *(Decided: these numbers are freely tunable in playtest — how killable horde goblins turn out to be sets the pool. The pool is also one of the future point-spend upgrades, and couriers spend from it too — §9.)*
- **Summon flow:** blow the horn → 3–4 goblins per blast come sprinting from the nearest off-screen treeline/spawn point over a few seconds (they *run in*, never pop in — watching them arrive is the joke and the fantasy). Short cooldown between blasts.
- **Inside walls:** the horn still works but horde goblins must physically path in through whatever breach exists — a gate you opened, the gap, your tunnel. **The horde is a reason to make a big breach.** Only players can go over via catapult.

### Control model — *follow & frenzy, with a point*

Deliberately simple (decided): no squad-command layer.

- **Follow (default):** the horde trails its summoner in a loose scamper.
- **Frenzy (automatic):** any enemy that gets close, or anything the player attacks, gets swarmed. They disengage and re-follow when it dies or leaves the leash radius.
- **Point (MMB or T, aimed at reticle):** one context command — *"get 'em."* Pointing at an enemy: swarm it. Pointing at the gate, a door, a stall, a fence: smash it. Pointing at a granary while holding a torch out? They cheer (they don't carry torches — fire stays a *player* verb, so the player is always the arsonist).
- **Stretch (post-core):** point at a woodpile → nearest 2–3 horde goblins haul bundles to the active build site. If AI hauling slips, players haul and nothing else breaks.

### Horde goblin spec

One archetype for the prototype: **Horde Goblin** — ~40 HP, dagger swipe, fast, no dodge, comically fearless. No lives; they die for good (that's the point — and the pool limit is the resource). They take friendly fire like everyone else, so a badly-thrown torch into your own horde is both a tragedy and the funniest thing that will happen all raid.

---

## 6. The level — Pennybrook and the forest

One core level for the prototype. Everything below is the full scope of the playable space.

**Naming tone (decided):** "Pennybrook" is confirmed, and it sets the register for all human naming — cozy, prosperous, faintly smug English-village names with money and comfort baked in (Copperfield Lane, the Gilded Hog tavern, Mayor Goldbottom). On-the-nose enough to smile at, never so jokey it breaks the world.

**Time of day (decided):** the raid takes place at **dusk** — fixed golden-hour-into-twilight lighting for the prototype. Readable enough for market bustle and combat, dark enough that sneaking the treeline feels right, and firelight gets to be gorgeous against it. A full **day/night cycle is roadmap** (post-prototype); nothing in the lighting build should preclude it, but no time is spent on it in the slice.

### 6.1 The runic site (spawn / extraction)

A ring of goblin-carved standing stones in a forest clearing, faintly glowing, humming with the Overlord's magic. Teleports the warband to and from the lair.

- **Spawn:** raids begin here. Loadout is chosen back at the lair (a menu screen for the prototype; the lair as a walkable hub is post-prototype).
- **The portal opens on objective completion** (decided): the stones idle dark until all three granaries burn, then flare open. Once open, goblins can step through to extract (**deeds bank on exit**) and **loot can be tossed into the portal** to bank instantly.
- **Loot staging:** courier sacks delivered before the portal opens pile up *inside* the circle — safe, since humans won't enter — and auto-bank the moment it flares open.
- **At 0:00 the portal begins to collapse** — 90 seconds to get through before it dies (see §9, "The clock").
- **Respawn:** respawn point when a life is lost (§9) — the stones re-knit you whether or not the portal is open.
- Humans are superstitious about the stones and won't enter the circle — the one safe tile in the world. *(Also the satire: they filed a complaint about the stones instead of removing them.)*

### 6.2 The forest

Dense woodland ringing the settlement, with **worn paths** connecting the runic site to the settlement's approaches (main path to the gate; a fainter trail circling toward the rear). Off-path movement is allowed everywhere but slower going and darker.

- **Human patrols (cadence decided):** a fresh patrol (2 militia + 1 archer) enters the forest **every 5–7 minutes at random intervals** — roughly 4–6 patrols across a 30-minute raid — walking loops between the settlement and the forest edges. Players choose: slip around them, ambush them quietly, or brawl. **Patrols are fully eliminable** — wipe one and the forest is genuinely clear until the next cadence tick — *until the alarm goes off*, at which point the nearby castle starts sending real reinforcements (§8.1). **If a patroller breaks away and reaches the settlement, the alarm starts warm** — the raid begins with guards already suspicious. Patrols intercept loot couriers **opportunistically only** (decided) — a courier who crosses their path is in trouble, but no one hunts him.
- **Harvestable trees & deadfalls:** marked chop-able trees and log piles — the wood source for siege building (§7.2). Chopping is loud; a nearby patrol will investigate.
- **Landmarks for orientation:** the watchtower's silhouette over the treetops, chimney smoke, a crashed cart on the main path (free starter loot + tutorial-by-scenery), and — far beyond the settlement on the horizon — the silhouette of **Highpurse Keep**, the castle whose soldiers answer Pennybrook's bell (§8.1). A skybox promise of consequences, and the roadmap's next raid target.

### 6.3 The settlement

A small human farming settlement — one social class above a hamlet and insufferably proud of it. Enclosed by a **wooden palisade**.

| Structure | Function | Notes |
|---|---|---|
| **Palisade wall** | The enclosure. Wooden stakes, walkable perimeter outside. | Not climbable by hand. Diggable-under at its base. Burnable only slowly and loudly (fire on the wall itself spikes alarm hard — possible but rude). **Always has a hidden gap somewhere** (§7.1). |
| **Gatehouse** | Main (only) gate. **2 militia guards** posted. | Gate = structural HP object: batter it down (ram ≫ Brute club > other melee) **or open it from inside** via the crossbar (hold-E interact). Opening from inside is silent until someone notices it's open. |
| **Watchtower** | **1 archer guard** with a bell. | Elevated vision cone over the approaches. If he *confirms* a goblin sighting he rings the bell → immediate Raid alarm. He periodically dozes (§3). Killable from range; the tower is climbable via ladder. |
| **Granary ×3** | **The core objective.** Fat wooden silos of hoarded grain. | Burnable → Chaos-fracture collapse when fully burned (already designed/scaffolded). Spread across the settlement so torching all three forces you to cross town. |
| **Well** | Center of town. The humans' anti-fire resource. | Civilians (and idle guards) run bucket lines from it to douse burning structures. **Foulable:** a goblin interact ("dump something unspeakable in it") disables firefighting for the rest of the raid + score bonus + alarm spike. |
| **Barracks** | Guard spawner. **Players can't enter** (door too stout, windows barred). | Stone base, not flammable — the established human counter to fire. Spawns guards on a timer while the alarm is raised; beat it down (structural HP) to silence it. |
| **Marketplace** | Loot concentration. | Stalls of goods = smashable score pickups. Locked strongbox (hold-E) with the big score item. Self-looting civilians compete here (stretch). |
| **Houses ×6–8** | Civilian homes, set dressing + light loot. | 2–3 enterable single-room interiors; the rest are shells. Burnable — pure mischief (score, alarm, and moral ambiguity). |

**Population:** ~6 posted guards (2 gate, 1 tower, 3 wandering) + barracks reinforcements while alarmed, ~8–10 civilians on daily-routine loops (well, market, fields near the gate).

---

## 7. Getting in — the breach systems (NEW)

The palisade is the puzzle. Four solutions, deliberately overlapping in usefulness:

### 7.1 The hidden gap — free, if you find it
Every wooden palisade has a spot the humans never quite fixed. Each raid, **1 gap is active out of ~4 candidate locations** (randomized): loose planks, a hog-hole, a section patched with a cart wheel. Squeeze-through is single-file and slow (players and horde both fit; the Brute grumbles and takes longer). Finding it costs scouting time; using it is silent. *Satire dressing: one candidate spot is "fixed" with a NO GOBLINS sign.*

### 7.2 Wood-and-build — the ram and the catapult *(decided: gather wood, then build)*
A light resource loop, no inventory screen:

- **Wood bundles** come from chopping marked trees, forest deadfalls, or — funniest and fastest — **stealing the settlement's own woodpiles** stacked outside the wall.
- Carrying a bundle is a visible over-the-shoulder carry: **move slower, can't attack** (drop it to fight). Brute carries two.
- Deposit bundles at a **build site** (fixed, pre-placed footprints — near the gate for the ram; in a clearing in wall-range for the catapult). When the wood quota is met, any goblin channels **Build** (hold E) to raise it. More builders = faster.

| Engine | Wood cost | What it does |
|---|---|---|
| **Battering ram** | 4 bundles | 2+ goblins grab handles (horde goblins auto-man spare handles) and swing on a rhythm; each hit chunks the gate's structural HP. Loud — guards converge. The brute-force fantasy. |
| **Catapult** | 6 bundles | A goblin climbs in, aims an arc over the wall, launches (scream included), lands with a roll **inside**. Player-transport only — the horde can't use it. The comedy option and the express lane for an inside-job gate opening. |

### 7.3 Digging — slow, quiet, reusable
Channel **Dig** (hold E) at the palisade base to burrow under. Slow solo; each additional digger (players — horde-assist is stretch) speeds it up. Produces a **tunnel both teams' goblins can use all raid, both directions** — a permanent private door, including for extraction under pursuit. Quiet, but a guard walking past mid-dig will notice the flying dirt.

### 7.4 The inside job — open the gate
Any goblin already inside (gap, tunnel, or catapult) can lift the gate's crossbar (hold E, ~3s, interruptible). The gate swings wide for the whole horde. Big score bonus (**"Inside Job"**), and the single best expression of the game's fantasy: one sneaky goblin turning the humans' front door into a goblin superhighway.

**Design guarantee:** the gap and the gate always exist, so a breach is never resource-gated; wood/dig/catapult are *better, louder, or funnier* options layered on top.

---

## 8. The defenders & the alarm

### 8.1 Alarm phases *(rework of the heat meter for a stealth-shaped raid)*

The meter persists but now drives **phases** rather than only escalating waves:

1. **QUIET** — daily routine. Guards posted, civilians pottering, watchman half-asleep. Alarm ticks only from *witnessed* events.
2. **SUSPICIOUS** (alarm > low threshold) — a sighting, a noise, a patrol gone missing. Nearest guard investigates; the watchman actually watches; civilians mutter. Decays back to QUIET if nothing is confirmed.
3. **RAID** (bell rung / open combat / big fire seen) — the meter becomes the old escalation system: barracks starts spawning, guard mix toughens per tier (militia → +archers → +knights), horde waves at threshold crossings. **The bell also signals Highpurse Keep** *(the nearby castle — offscreen, a distant silhouette on the horizon; name placeholder in the Pennybrook register)*: from RAID onward, **castle reinforcement squads** march in from the far map edge on a timer. Before the alarm, the forest's only threat is the 5–7-minute patrol cadence — which you can keep clearing; after it, the castle's soldiers keep coming whether you clear them or not (decided).
4. **RAZED / RELIEF** — once the settlement is substantially burned, internal spawners go quiet and the castle reinforcements intensify into full **relief columns** — the "get out now" pressure on the trip home.

**Alarm sources:** confirmed sightings, the bell, combat, structure fires, the barracks falling, patrol runners reaching town. **Alarm reducers:** none. Goblins don't de-escalate; they leave.

### 8.2 The humans

Existing archetypes carry over untouched (see `race-design-humans.md`): **Militia** (30 HP filler), **Archer** (20 HP, aim-line telegraph), **Knight** (75 HP, armor 6, late tiers). New behavior roles, not new stat blocks:

- **Patrol** (forest): militia + archer on routes; a survivor of a botched ambush becomes a **runner** (sprints for the gate to warn the town).
- **Watchman** (tower): archer + bell + doze cycle.
- **Gate guards:** militia with a "defend the gate" post behavior; they investigate the crossbar being lifted if within earshot.
- **Firefighters:** the established bucket-brigade behavior, now sourced at the well; interruptible and killable mid-douse.
- **Civilians** *(new archetype)*: 10 HP, no attack. Routine loops → flee/panic when alarmed → some join bucket lines → (stretch) some loot their own market. Their harm-policy is an open tone question (§3, §13).

---

## 9. Lives, death & extraction

- **5 lives** per player-goblin, as established. Horde goblins have none.
- **Decided:** on death you respawn **at the runic site** after ~4s — the stones re-knit you. Locked for the prototype (no breach-point respawn A/B needed). This makes the forest run meaningful, makes deep raids feel risky, and gives the tunnel/opened-gate persistent value on the way back in.
- The horde does **not** die with you; leaderless horde goblins hold position and defend themselves until you return or re-horn.
- Losing the last life = raid over; **unbanked score is heavily docked** — you keep **25%** as a consolation ("His Eternal Darkness salvages something from your corpse"). Making it back through the stones banks 100% plus the return bonus. *(Decided — and softened further by the courier system below.)*

### Loot couriers — banking mid-raid *(new system, for review)*

Michael's ask: goblins should be able to help carry loot back to the portal. Proposed design:

**Score comes in two kinds:**

- **Deeds** — kills, arson, mischief bonuses (Inside Job, well-fouling…). Intangible. Deeds bank only when *you* exit through the stones; a wipe salvages 25%.
- **Loot** — physical objects: market goods, strongboxes, grain sacks, civilian valuables. Loot is *carried*, and anything that physically reaches the runic site is **banked immediately and permanently** — wipe-proof.

**How loot moves:**

- Players can carry loot sacks themselves (same over-the-shoulder carry state as wood bundles — slower, can't attack). Fine for a last armful on the way out.
- **The courier command:** point (the §5 horde command) at a loot pile or dropped sack → the nearest horde goblin shoulders it and **runs it all the way home to the runic site** — then stays there (returns to the reserve pool, if any remains, rather than trotting back alone through an alerted forest). If the portal is open, the sack goes straight in (banked); if not yet open, it **stages inside the circle** and auto-banks the moment the portal flares (§6.1).
- **The tradeoff that makes it a system:** every courier is a fighter who leaves the raid. Sending five sacks home mid-raid means five fewer blades when the knights arrive. Greed vs muscle, using the horde pool you already manage.
- **The risk that makes it a story:** couriers are squishy, alone, and waddling under a sack through a forest with patrols in it. **Patrols intercept couriers opportunistically only (decided)** — a courier who crosses a patrol's path is in trouble, but nothing hunts him. A dead courier drops the sack where he fell (recoverable, on your next trip out). Watching your loot toddle into the treeline and *hoping* is exactly the right emotion.
- **Comedy layer:** couriers grunt, drop things, pick them back up, and occasionally carry the sack on their head. The Overlord whispers approvingly when loot banks ("Yesss. The grain of the unworthy.").

**Prototype scope:** the courier is core-adjacent — it reuses the carry state (§7.2), the point command (§5), and forest pathing that already exists, so the new work is one BT behavior + the two-kind score split. Slotted into week 6 (§12.2), cut-order slot between civilians and coin toss if pressed. **Standing proposals (unflagged, so treated as accepted):** the ×1.5 return bonus applies to deeds only — couriered loot already got its safety; arrived couriers join the reserve pool rather than walking back alone.

### The clock — 30-minute raids *(decided)*

Every raid is **hard-capped at 30 minutes**. Diegetic frame: the runic circle holds its charge only so long, and His Eternal Darkness has other appointments.

- **HUD timer**, always visible; the final 5 minutes, the whispers turn impatient (bark trigger).
- The cap is what the patrol cadence (§6.2) and castle reinforcements (§8.1) are tuned against: 5–7-minute patrol ticks give roughly 4–6 patrols per raid, and the alarm decides how bad the back half gets.
- **The portal only exists to leave through once the objective is done** (§6.1) — burn the granaries or there is nothing to extract through.
- **At 0:00** *(decided)*: the portal begins to collapse. **A 90-second grace window** — screen-wide warning, the stones' glow guttering, the whispers going cold — and then it dies. Anyone through in time banks normally. **Anyone still outside is left behind**: raid over, unbanked deeds take the wipe penalty (keep 25% — His Eternal Darkness salvages what he can from whatever the humans leave of you). Staged and tossed-in loot is already safe.
- **If the objective was never completed by 0:00**, the portal never opened — the whole warband is left behind, same salvage rule. Burn the granaries; that's why you're here.
- The clock is the third pressure alongside lives and the alarm — and the strongest argument for sending loot home *early*.

## 10. Scoring — the prototype economy

**Decided: score only for the 7-week prototype.** No progression trees, no spending — an end-of-raid tally, a rating, and a personal best. The score architecture is built so the future economy (weapon trees, horde upgrades, raid loadouts — all deferred) can consume it later without rework.

**Proposed score table** *(all values placeholder for playtesting):*

| Event | Points |
|---|---|
| Granary burned | 100 each |
| All 3 granaries (objective complete) | +150 |
| Loot item (market goods, house valuables) | 5–25 each *(loot-kind: bankable by courier — §9)* |
| Marketplace strongbox | 75 *(loot-kind)* |
| Guard defeated | 10 (militia) / 15 (archer) / 25 (knight) |
| Civilian killed | 5 *(permitted and petty — see §3 violence style)* |
| Watchman silenced before he rings the bell | +25 |
| **"Inside Job"** — gate opened from within | +50 |
| Well fouled | +30 |
| Barracks demolished | +40 |
| Patrol wiped with no runner escaping | +15 |
| **Made it home** — exit via the runic site | **×1.5 on the whole tally** |
| Per unused life remaining at extraction | +20 |

The multiplier-on-return is the loop's spine: **deeds** are provisional until you're standing in the stones (×1.5 applies to deeds; wipe = keep 25%), while **couriered loot is already safe** (§9). End-of-raid screen: itemized tally split by deeds/loot, an Overlord whisper verdict ("Adequate. I have seen rats do better. …adequate."), letter grade, personal best.

---

## 11. Asset manifest — everything the prototype needs

Scoped to one level, one defender race, third-person camera, dusk lighting.

### 11.0 Art direction & the material language *(decided)*

**Stylized and hand-painted.** Chunky proportions, painterly textures, saturated dusk palette. This is locked — it hides kitbash seams, sells the satire, keeps the cartoony gore on the T side of the rating, and is achievable solo. **Kitbashing from Fab/Marketplace stylized-fantasy packs is green-lit** for environment and humans; custom-art time goes to goblins, the runic site, and hero props only.

**Every material telegraphs its hardness.** The player should read at a glance what burns, what breaks, and what shrugs — the art *is* the tutorial:

| Material | Look | Burns? | Breaks? | Gameplay meaning |
|---|---|---|---|---|
| **Thatch / hay** | Shaggy, golden, overhanging | Instantly, spreads fast | Yes (trivial) | Torch magnets — roofs and haycarts are your accelerant |
| **Raw wood** (palisade, gate, granaries, stalls) | Visible planks, grain, rope lashings | Yes, on a delay | Yes — club, ram, horde | The default goblin-interactive surface; the whole town is made of it |
| **Hardened/banded wood** (gate crossbar, strongbox) | Iron bands, big rivets | Slowly / no | Only via the right verb (ram, interact) | Signals "there's a mechanic here, not just HP" |
| **Stone** (barracks base, well, tower footing) | Rounded painted masonry | No | Structural HP only — slow, loud | The fire-immune counterweight; beat it down or leave it |
| **Metal** (knight armor, bell) | Painted specular, dented | No | No — mitigates (armor stat) | Pierce it (armor-ignoring damage) or stagger the wearer |
| **Runic stone** (the site) | Dark stone, glowing carvings | No | Indestructible | Sacred to goblins, untouchable by design |

Rule for every new asset: assign its material class *first*, then its look — never ship a surface whose appearance lies about its hardness.

### 11.1 Environment
- **Forest kit:** pine/oak trees (+ marked *choppable* variant), bushes, rocks, deadfall logs, dirt-path spline textures, forest floor materials, treeline "horde spawn" markers, crashed cart set piece.
- **Runic site:** standing stones ×6–8 (emissive runes), circle ground decal, portal activation FX.
- **Palisade kit (modular):** wall segment, corner, gate-hinge segment, **gap variants ×4** (loose planks / hog-hole / cart-wheel patch / NO GOBLINS sign), dig-spot base decal.
- **Gatehouse:** frame, double gate (destructible: intact / battered / breaking / down states or Chaos), crossbar (animated interact).
- **Watchtower:** tower, ladder, platform, **bell** (animated + interactable).
- **Buildings:** granary ×1 modeled (instanced ×3) with burn states + Geometry Collection; barracks (stone base, barred door/windows); well (bucket + winch, "fouled" state variant); market stalls ×3–4 variants (smashable); house shells ×3 variants + 1 enterable interior set; fences, crates, barrels (incl. explosive), woodpiles, hay carts, street props.
- **Siege engines:** battering ram (build-stages ×3 + handles), catapult (build-stages ×3 + arm animation + aiming arc indicator), build-site footprint decals, carryable wood bundle.

### 11.2 Characters
- **Player goblins ×4 kits:** shared goblin body (Brute at 1.5× scale) + kit dressing (daggers+bow / great-club / two staff variants). Custom art priority #1.
- **Horde goblin:** one cheap variant mesh (color/prop randomization for crowd variety).
- **Humans:** militia, archer, knight (armored), **civilian ×2 body variants** (mix-and-match heads/palettes stretch goal).

### 11.3 Animation *(the big third-person cost — budget honestly)*
- **Goblin shared set:** locomotion (idle/walk/run/strafe), dodge roll, hit reacts, death, torch throw, horn blow, interact-channel (covers dig/build/crossbar/foul-well), carry locomotion, catapult ride + landing roll, emotes (cheer/giggle — the tone carriers).
- **Per-kit attack sets:** daggers 3-hit combo + lunge; bow draw/fire/charged; club light/heavy/ground-slam; staff cast ×2 variants ×2 staves.
- **Human set:** locomotion, melee swing + windup (telegraph), bow draw/fire, hit/death, **bucket-douse loop, panic-flee, doze/wake, bell-ring, coin-scramble** (the comedy set), ram-the-alarm run.
- Marketplace animation packs + retarget cover most of this; hand-key only the comedy set and goblin signature moves.

### 11.4 VFX (Niagara)
Fire (torch flame, projectile trail, surface fire, structure-burn stages, smoke columns — visible over the treetops as your progress report), extinguish steam, telegraph rings & archer aim-lines (redesigned for 3D readability at third-person, likely ground decals), hitstop impact sparks, portal shimmer, dig dirt-spray, wood-splinter bursts, blood-orb & shadow-cast FX, coin glitter, alarm-state vignette, **gore set (cartoony):** exaggerated blood-spatter bursts + ground decals, gib/limb pops with clean white cartoon bones, comic "poof" on horde-goblin death — one shared gib component with an intensity scalar (the ratings knob, §3).

### 11.5 Audio
Horn call (hero sound #1), portal hum/whoosh, bell (hero sound #2), fire loop + collapse, chopping/digging/building, ram impact, catapult launch + goblin scream (hero sound #3), combat layer (per-weapon whooshes/impacts + wet cartoon splats for the gore), goblin chatter/giggles, human bark VO *(text-only for prototype — placeholder grunts fine)*, forest ambience dusk layer (crickets, evening birds), settlement ambience (market murmur → panic), score-tally stinger, **His Eternal Darkness: layered dark-whisper beds + subtitles** (decided — no VO actor needed; a whisper texture + subtitle system covers the prototype).

### 11.6 UI
HUD: health/lives, torch count, horn status (active horde / reserve pool), kit resource (orbs / combo pips / charge), objective tracker (granaries 0/3), alarm-phase indicator, **30-minute raid timer**, **subtitle strip for His Eternal Darkness's whispers**, interact prompts + channel bars, carry indicator. Screens: class select (lair menu), pause, end-of-raid score tally + grade, death/respawn. Reticle + soft lock-on marker (third-person aiming).

---

## 12. Systems list & 7-week build plan

### 12.1 Systems inventory *(★ = new since the combat prototype / tech scaffold)*

| # | System | Status |
|---|---|---|
| 1 | GAS combat pipeline, damage/armor/race matrix | Scaffolded (compiles, per build log) |
| 2 | 4 weapon kits + torch toss | Designed + browser-proven; UE port |
| 3 | Fire/flammable/spread + Chaos granary fracture | Scaffolded |
| 4 | Alarm meter + spawner + razed/relief patrol model | Scaffolded; ★ needs phase rework (QUIET/SUSPICIOUS/RAID) |
| 5 | ★ **Third-person camera & control** (was top-down) | New — reticle aim, soft lock, telegraph redesign for 3D |
| 6 | ★ **Horn & horde** — summon, pool, follow/frenzy/point | New — biggest new AI system |
| 7 | ★ **Stealth-lite perception** — vision cones, doze cycle, runners, investigate | New — keep minimal (seen/unseen + investigate), not a full stealth sim |
| 8 | ★ **Interact framework** — hold-E channels (dig/build/chop/carry/crossbar/foul/loot) | New — one system, many verbs |
| 9 | ★ **Breach set** — gap randomizer, gate HP + crossbar, tunnel, ram, catapult (+launch traversal) | New |
| 10 | ★ **Wood economy** — choppables, bundles, carry state, build sites | New |
| 11 | ★ **Civilians** — routines, panic, bucket brigade (firefighting BT task already scaffolded), well interaction | Partially scaffolded |
| 12 | ★ **Score system** — event bus → deeds/loot two-kind tally, banking multiplier, end screen | New (replaces progression for the slice) |
| 13 | ★ **Runic site** — spawn/respawn + objective-gated portal, loot toss-in/staging, 90s collapse | New |
| 14 | Lives/respawn on PlayerState | Scaffolded; retarget respawn to runic site (decided) |
| 15 | Barks (text) + His Eternal Darkness whispers/subtitles | New, cheap |
| 16 | ★ **Gore/gib system** — cartoony spatter + limb pops, shared component, intensity scalar | New — small; rides on existing death events |
| 17 | ★ **Loot couriers** — carryable loot sacks, point-to-courier BT behavior, mid-raid banking | New — reuses carry state + point command + forest pathing (§9) |
| 18 | ★ **Raid clock** — 30-min cap, HUD timer, 0:00 → 90s portal collapse & left-behind rule, impatient-whisper triggers | New — small (§9) |
| 19 | ★ **Patrol director** — 5–7-min random-cadence forest patrol spawns; castle-reinforcement squads post-alarm (an extension of system 4's spawn logic) | New (§6.2, §8.1) |

### 12.2 Week-by-week *(solo dev + AI-assist assumption; adjust when team size is confirmed)*

| Week | Goal | Deliverable |
|---|---|---|
| **1** | Third-person foundation | Camera/controls/reticle on the scaffold; one kit (club) attacking a militia in a blockout box; interact-channel framework |
| **2** | Fire + objective | Torch, fire spread, one granary burning → fracturing; alarm RAID phase + barracks spawning; blockout of full settlement footprint |
| **3** | The horde | Horn, pool, follow/frenzy/point; horde goblin archetype; 10 goblins + player vs guards runs at frame rate |
| **4** | The breach layer | Palisade + gate HP + crossbar open; hidden-gap randomizer; dig tunnel; forest blockout + runic site spawn/extract; **first full loop playable start-to-bank** |
| **5** | Wood, siege & stealth-lite | Chop/carry/build; ram; catapult + launch; patrols + runner behavior; watchman + bell + doze |
| **6** | The living town | Civilians, routines, bucket brigade, well fouling; loot sacks + courier behavior; remaining kits ported; score system (deeds/loot) + end screen; gore/gib pass; art pass 1 (kitbash dress-up) |
| **7** | Feel & funny | Telegraph/hitstop/shake tuning in 3D; barks; audio hero sounds; balance pass on score table & horde counts; bug triage; **playtest build** |

### 12.3 Honest scoping — the cut order

This slice is *bigger* than the old arena slice (stealth-lite, forest, siege building, horde AI, and a camera change, all at once). If weeks 5–6 slip, cut in this order, and the game still works because gap + gate always guarantee a breach:

1. **Catapult** (comedy, not load-bearing) → 2. **Self-looting civilians & coin toss** (stretch already) → 3. **Loot couriers** (players carry their own sacks; the two-kind score split stays) → 4. **Wood-gathering** (ram build sites become pre-stocked — the loop survives) → 5. **Digging** (gap + ram + inside-job remain) → 6. **Doze cycle** (watchman becomes simply killable). **Never cut:** the horn/horde, the three granaries, gate + gap, the runic-site banking loop, the score screen. That quintet *is* the game.

**Team reality (confirmed):** solo dev (Michael) + AI-assist. The week-by-week above is written for exactly that; the cut order is the safety valve, not a failure state.

**Post-slice roadmap (decided order):** ① **Points & upgrades** — the spending economy (weapon progression trees, horde upgrades, raid loadouts) built on the score system, worked toward a releasable build → ② **Co-op multiplayer** — turn on the networking everything was built ready for, then playtest it → ③ **The assassination mission** — Mayor Goldbottom, after multiplayer playtesting. Elves/Dwarves unchanged: designed, further out. Highpurse Keep (§6.2) is the natural future "settlement tier 2" raid target.

---

## 13. Decisions ledger & remaining open items

**Decided (2026-07-10 review — all former blocking/important questions resolved):**

- **Camera:** third-person, Overlord-style.
- **Horde control:** follow & frenzy + one point command.
- **Siege tools:** gather wood, then build.
- **Prototype economy:** score only; progression trees/spending deferred unless time remains.
- **Violence:** everyone killable, civilians included (civilian kill = small score). Cartoony gore — blood spatters, flying limbs, cartoon bones. Rating target: **T minimum**.
- **Art:** stylized, hand-painted, with an explicit material-hardness language (§11.0). Fab/Marketplace kitbashing green-lit.
- **Team:** solo (Michael) + AI-assist.
- **Time of day:** dusk, fixed, for the prototype; day/night cycle on the roadmap.
- **Respawn:** always the runic site (prototype).
- **The Overlord:** **His Eternal Darkness** — layered dark whispers with subtitles.
- **Naming:** Pennybrook confirmed; keep the cozy-smug register.
- **Horde numbers:** 10 active / 20 reserve as the starting point; tune freely in playtest against horde-goblin killability.
- **Multiplayer:** solo-only slice, built co-op-ready for post-prototype (or late-slice if time miraculously allows).

**Decided (2026-07-10, second review):**

- **Couriers:** patrols intercept opportunistically only; ×1.5-deeds-only and courier-to-reserve stand as accepted proposals.
- **Raid clock:** every mission capped at **30 minutes**.
- **Patrol cadence:** a patrol every **5–7 minutes at random intervals**; eliminable until the alarm summons castle reinforcements (Highpurse Keep — name placeholder).
- **Barks:** cryptic + dismissive His Eternal Darkness (everyone drops everything to listen; he never reveals the plan); humans are gross, unwashed medieval, smugly superior. Draft sheet: `claude/goblin-siege-bark-sheet.md`.
- **Post-slice order:** points/upgrades economy toward release → co-op multiplayer + playtest → Mayor Goldbottom assassination mission.
- **Extraction & timeout (third review):** the portal **opens on objective completion**; loot is tossed into the open portal (courier sacks stage in the circle before then); at **0:00 a 90-second grace window** runs before the portal collapses — anyone outside is **left behind** (wipe salvage on unbanked deeds).
- **Bark sheet:** approved as drafted (`claude/goblin-siege-bark-sheet.md`).

**Remaining open items:**

1. **Score table blessing** — §10 values are placeholders; sign off after the first playable tally (week 6).
2. **Gore intensity default** — the gib scalar ships tunable (§11.4); pick the default feel in the week-7 polish pass.

---

*Companion documents: `race-design-goblins.md`, `race-design-humans.md`, `race-design-elves.md`, `race-design-dwarves.md`, `claude/goblin-siege-bark-sheet.md`, `claude/goblin-siege-ue5.8-tech-design-doc.md` (needs a follow-up pass to add systems 5–19 of §12.1), `claude/goblin-siege-build-log-and-mcp-plan.md`. Playable combat reference: `goblin-siege-prototype_3.html`.*
