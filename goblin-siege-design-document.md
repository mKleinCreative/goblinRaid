# GOBLIN SIEGE — Game Design Document

*Major revision · "The Generated Hamlet" — folds the 2026-07-20 hamlet pivot and the 2026-07-21 restructure rulings (ledger decisions 1–35, §13) into the canonical document, per the approved restructure proposal (Q-01). Supersedes the "Settlement Raid" revision's §6–§7: the slice raid target is now an unwalled, procedurally generated hamlet, and the palisade breach layer relocates intact to **Appendix A** as tier-2 content — deferred, not cut. Combat/weapon content carries forward unchanged unless noted.*

---

## 1. Vision

**Goblin Siege is a third-person co-operative raid game in which you play the monsters. You and your goblin warband slip out of your Overlord's lair, creep through the forest, descend on a human settlement — in this slice a tiny unwalled hamlet, up the ladder a walled town, one day a castle — burn it down, rob it blind, and scurry home before the humans get organized.**

It plays like a satire. You are the evil exposing the idiocy of the good: the humans are pompous, greedy, and a little bit stupid, and the game's biggest laughs come from watching their so-called civilization fail exactly the way you'd expect — the watchman asleep at his post, the guards abandoning a chase to scoop up dropped coins, the town fighting over buckets while its wheat fields burn.

### Design pillars

- **Weighty, readable combat.** Attacks commit, dodges commit, enemies telegraph. Every hit has impact — hitstop, knockback, stagger. You win by reading the fight, not mashing. *(Unchanged from the combat prototype — this is proven.)*
- **Fire and destruction as a language.** Torches, flammable buildings, and a settlement that visibly tries to save itself. The hamlet is made of wood and wheat, and the goblins know it.
- **Quiet in, loud out.** No wall means no front door — the approach is a stealth game, but the objective is arson, and fire cannot be hidden. Stealth controls *when* the mayhem starts, not whether. *(The old pillar — "many ways over the wall" — isn't gone: the breach layer returns when the walls do, at tier 2. Appendix A.)*
- **No two raids the same map.** A seeded generator arranges the hamlet fresh every raid from hand-authored modules. Scouting never goes stale, because the town was never placed by a human.
- **A horde at your back.** One horn-blast and the treeline empties. The AI horde makes a lone goblin feel like an invasion — cheap to command, expendable by design, hilarious in motion.
- **Pressure, not safety.** The longer you linger, the harder the county fights back. Greed (more loot, more fire, more score) pulls against survival (make it back to the runic site — nothing counts until you're home).
- **Satire, always.** Every system should get a chance to be funny. Humans are greedy and dim; goblins are gleeful vermin. If a mechanic can express that (coin distractions, bucket brigades, a snoring watchman, a village that refuses to believe you exist), lean in.

### Touchstones

- **Overlord** — the tone anchor. Gleeful evil, an expendable minion horde, the "good" people revealed as venal idiots.
- **Darktide** — horde combat, class identity, mixed-enemy waves demanding target priority.
- **Helldivers** — mission-and-extraction structure; nothing is yours until you extract.
- **Elden Ring** — deliberate stamina-gated melee, dodge i-frames, telegraphs you read rather than out-click.
- **Enshrouded** *(environmental)* — smashable, burnable structures that reshape the play space.

---

## 2. Core gameplay loop

1. **In the Overlord's lair**, pick your class (weapon-kit) and step into the runic circle.
2. **Materialize at the runic site** — a ring of glowing standing stones in a forest clearing. Spawn, home base, and extraction point, all in one. **His Eternal Darkness names this raid's three burn targets** as you arrive (HUD + whisper — see §6.3, decision 11); the generator decided where they sit.
3. **Move through the forest** along (or off) the paths toward the hamlet, dodging or ambushing human patrols — you'll see their drawn torches flickering through the trees before they see you. Getting spotted and letting a runner escape warns the town.
4. **Case the hamlet.** No wall, no front door: circle it from the treeline, read the guard posts and the watchtower's cone, and *find* your targets — the environment guides you (converging roads, signposts, wheat thickening toward a field, the racket of a pen), never a HUD arrow.
5. **Work quietly.** Crouch between hedgerows and haycarts, time the watchman's naps, take down an unaware guard from behind, toss a coin to pull the greedy off their posts — and start looting before anyone believes you exist.
6. **Go loud — on purpose.** Torch the first objective and the quiet phase is over. Blow the horn, bring the horde sprinting from the treeline, burn the rest, and fight the town's response: guards, bucket lines, the bell, and eventually soldiers marching in from the distant castle.
7. **Get out.** Burning the third objective **opens the portal** at the runic site. Back through the forest — the humans may pursue — step into the circle to teleport home, tossing loot into the portal as you go.
8. **Bank your score.** Points tally from objectives, loot, and mischief — **fully banked only if you make it back.** *(Prototype: score screen and personal-best; the spending economy comes later — see §10.)*

**Every raid is capped at 30 minutes** (decided). The runic circle only holds its charge so long — His Eternal Darkness does not wait on goblins. See §9 for the clock's rules.

---

## 3. Tone & satire — how the game stays funny

The comedy is systemic, not just written. Rules of thumb:

- **Humans are greedy first, brave second.** Greed is an exploitable AI stat, not just a joke. Guards will break off a chase to pick up dropped coins. Civilians will loot their *own* marketplace once chaos starts.
- **Humans are confident and wrong.** The watchtower guard dozes. Bark lines drip with misplaced superiority ("A goblin? Here? Don't be daft, it's shearing day.").
- **The kingdom told them you're extinct** *(new tone pillar — decided, 2026-07-21)*. The hamlets are sparse but **saturated with royal propaganda**: the king slew the last dragon, vanquished the darklord, and wiped out his goblin minions — it says so on the statue. So when goblins appear, villagers **initially disbelieve their own eyes** — the double-take, the "must've been a badger," the guard polishing the WarriorStatue of the king who supposedly ended you. Feeds the disbelief bark family, the propaganda prop set (§11.1), and the First Spark comedy: the town's first response to arson is denial.
- **Goblins are vermin, and proud.** The horde giggles, trips, cheers when things burn, and screams with joy when catapulted *(tier 2)*. Being flung over a wall as living ammunition is the mission statement — one rung of the ladder away.
- **His Eternal Darkness frames everything.** Never seen, never shouts — he *whispers*, layered dark whispers with subtitles, setting objectives, commenting on failures, delivering the post-raid verdict. Apocalyptic gravitas applied to stealing pigs from a nameless hamlet in Groatsworth county — that contrast is the joke. *(Decided: name and whisper+subtitle delivery locked; no VO actor needed for the prototype.)*

### Violence style — cartoony gore *(decided)*

Everyone in the hamlet is killable — guards *and* civilians — and the violence is **cartoony with a hint of gore**: exaggerated blood spatters, flying limbs, comically clean cartoon bones. Gibs pop like party favors; deaths are punchlines, not snuff.

- **Rating target: T at minimum.** Stylized cartoon gore keeps dismemberment on the T side (à la Castle Crashers); gib intensity is a single tuning scalar, not a redesign.
- **Civilian kills score** (see §10) — modest points, because the satire lands better when killing civilians is *permitted and petty* rather than forbidden or lucrative. The same rule now covers **livestock** (§9): kill petty, steal big.

**Satire mechanics (prototype-scoped where marked):**

| Mechanic | What it does | Prototype? |
|---|---|---|
| **Coin toss** | Throw looted coins (debited from your pouch — §7) to make greedy humans, guards included, break behavior and scramble. A distraction tool that *is* the theme. | **Yes — promoted to core** (2026-07-20) |
| **Thrown chicken** | A **noise** lure — hurl a squawking chicken and the nearest guard investigates the racket. Greed stays the coin toss's joke; the chicken's is chaos. *(decided, 2026-07-21)* | Yes — rides the noise system |
| **Disbelief double-takes** | Propaganda-fed civilians refuse to believe the first sighting (see tone pillar above); barks carry it. | Yes — bark states |
| **Sleeping watchman** | The watchtower guard periodically dozes; his vision cone visibly droops. Timing your approach around his naps is stealth-as-comedy. | Yes — a timer on his perception |
| **Self-looting civilians** | Past an alarm threshold, some civilians grab loot from market stalls and run — competing with *you* for score. | Stretch |
| **Bucket brigade** | Civilians queue at the well and douse fires — earnest, orderly, and outmatched (see §8.2 for the field-fire rules). Foul the well to stop it. | Yes — core counter-system |
| **Bark system** | Short text barks keyed to states (idle, suspicious, disbelieving, alarmed, greedy, fleeing). Cheapest comedy per byte in the game. | Yes — text only |

---

## 4. The goblins — classes & weapons

**Unchanged from the previous draft.** A goblin's identity is its weapon-kit; each kit has its own (future) progression tree. Full stats live in the prior combat sections and `race-design-goblins.md`; summary:

| Class | Kit | HP | Speed | Role |
|---|---|---|---|---|
| **Slasher** | Daggers ⇄ Bow (one weapon, live swap) | 110 | Fast | Mobile skirmisher — fastest crouch-walk, cleanest takedowns, picks off the watchman at range |
| **Brute** | Great-club | 170 | Slow | Frontline breaker — worst at hiding, best when hiding stops; 1.5× body & reach; carries double |
| **Shaman (Shadow)** | Shadow Staff | 85 | Medium | Ranged armor-ignoring pressure + group root/stun (Shadow Grasp) — controls the bucket brigade |
| **Shaman (Blood)** | Blood Staff | 85 | Medium | Blood-orb economy caster — dagger/spear/lance conjured arsenal |

**Universal kit (every class):**

- **Torch toss (Q)** — sticks where it lands, ignites, spreads. The racial equalizer. Aimable — the mill demands a throw *through a window* (§6.3).
- **The Horn (G)** — summons the AI horde (§5), and formally ends the quiet half.
- **Crouch (toggle)** *(decided, 2026-07-21)* — the stealth stance (§7).
- **Dodge roll** — 0.22s i-frames, committed recovery.
- **Interact (E/hold E)** — the slice's verbs *(decided, 2026-07-21)*: **loot** (the gather-pouch channel), **takedown**, **foul the well**, **extract**, plus the **carry** state (loot sacks, live animals). Dig/chop/build/crossbar are tier-2 verbs (Appendix A).
- **5 lives** — losing all HP costs a life; respawn at the runic site (decided — §9). Losing the last life ends your raid.

**Class texture in the new loop (design intent, not hard gating):** the Slasher sneaks and silences; the Brute is the go-loud specialist who hauls two pigs at once; the Shamans keep the horde alive and shut down the bucket lines. Every class can do every raid verb — classes change *how well*, never *whether*.

---

## 5. The Horn & the horde — core system

Every goblin carries a war-horn. Blowing it (G) calls AI goblins scurrying out of the treeline to fight beside you.

### Rules

- **Cap:** 10 active horde goblins per player *(prototype: 1 player → 10; tunable after playtests)*.
- **Reserve:** each raid has a finite horde pool — starting at **2× the cap** (20 for solo). Dead horde goblins are gone; the horn refills you from the remaining pool. When the pool is dry, the treeline is silent. *(Decided: freely tunable in playtest; the pool is a future point-spend upgrade, and couriers spend from it too — §9.)*
- **Summon flow:** blow the horn → 3–4 goblins per blast come sprinting from the nearest off-screen treeline spawn over a few seconds (they *run in*, never pop in — watching them arrive is the joke and the fantasy). Short cooldown between blasts. The horn is *loud* — blowing it early is a choice you get to regret.
- **Tier 1 has no walls, so the horde flows freely through the hamlet** — which is exactly why higher tiers reintroduce them: the ladder is partly a story about the horde's access getting harder to arrange. *(The walled-access rules — horde paths in through gates, gaps, and tunnels; only players fly over by catapult — are tier-2 content, Appendix A.)*

### Control model — *follow & frenzy, with a point*

Deliberately simple (decided): no squad-command layer.

- **Follow (default):** the horde trails its summoner in a loose scamper.
- **Frenzy (automatic):** any enemy that gets close, or anything the player attacks, gets swarmed. They disengage and re-follow when it dies or leaves the leash radius.
- **Point (MMB or T, aimed at reticle):** one context command — *"get 'em."* Pointing at an enemy: swarm it. Pointing at a stall, a fence, a pen gate: smash it. Pointing at a dropped loot sack **or a pig**: the nearest goblin shoulders it and couriers it home (§9). Pointing at an objective while holding a torch out? They cheer — they don't carry torches. Fire stays a *player* verb, so the player is always the arsonist.

### Horde goblin spec

One archetype for the prototype: **Horde Goblin** — ~40 HP, dagger swipe, fast, no dodge, comically fearless. No lives; they die for good (that's the point — the pool limit is the resource). They take friendly fire like everyone else, so a badly-thrown torch into your own horde is both a tragedy and the funniest thing that will happen all raid.

---

## 6. The level — the hamlet and the forest

One core raid *environment* for the prototype — but no longer one map. An authored forest ring and runic site enclose a **procedurally generated hamlet**: every raid, a seeded generator arranges the settlement fresh from hand-authored modules (§6.3).

**Naming tone (decided — revised 2026-07-21, decision 10):** the cozy-smug, money-soaked register stands, stretched into an **ascending-currency register of county names** across the settlement ladder (§6.4): groat → penny → silver → highpurse. **Groatsworth, Pennybrook, and Silverford are *counties*, not towns — the raided settlements themselves stay nameless**, small parts of a larger countryside (there are many hamlets in Groatsworth; you are burning one of them). **Highpurse Keep** stays a named place: the county seat at the top of the ladder.

**Time of day (decided, unchanged):** the raid takes place at **dusk** — fixed golden-hour-into-twilight lighting for the prototype. Firelight gets to be gorgeous against it. Day/night cycle is roadmap; nothing in the lighting build should preclude it.

### 6.1 The runic site (spawn / extraction)

A ring of goblin-carved standing stones in a forest clearing, faintly glowing, humming with the Overlord's magic. Teleports the warband to and from the lair. **Authored once; persists across seeds.**

- **Spawn:** raids begin here. Loadout is chosen back at the lair (a menu screen for the prototype; the lair as a walkable hub is post-prototype).
- **The portal opens on objective completion** (decided): the stones idle dark until all three of the raid's burn objectives are done, then flare open. Once open, goblins can step through to extract (**deeds bank on exit**) and **loot can be tossed into the portal** to bank instantly.
- **Loot staging:** courier sacks (and delivered livestock) arriving before the portal opens pile up *inside* the circle — safe, since humans won't enter — and auto-bank the moment it flares.
- **Pouch auto-bank** *(decided, 2026-07-21)*: stepping into the circle **banks your pouch automatically, mid-raid included** — a greedy goblin can jog home, empty its pockets into safety, and head back out.
- **At 0:00 the portal begins to collapse** — 90 seconds to get through before it dies (§9, "The clock").
- **Respawn:** respawn point when a life is lost (§9) — the stones re-knit you whether or not the portal is open.
- Humans are superstitious about the stones and won't enter the circle — the one safe tile in the world. *(Also the satire: they filed a complaint about the stones instead of removing them.)*

### 6.2 The forest

Dense woodland ringing the hamlet, with **worn paths** connecting the runic site to the settlement's open approaches — with no wall there is no front door, and the fainter circling trail matters more, not less. Off-path movement is allowed everywhere but slower going and darker. **The forest ring is authored once and persists across seeds** — the fixed, learnable frame around the generated picture. It is also **green and unburnable** *(decided, 2026-07-21 — "it's magic")*: your fire language stops at the treeline, so the run home never burns down behind you.

- **Human patrols (cadence decided):** a fresh patrol — **3 guards: 2 militia + 1 archer, torches drawn** *(decided, 2026-07-21)* — enters the forest **every 5–7 minutes at random intervals** (~4–6 patrols per raid), walking loops between the hamlet and the forest edges. Their **firelight is visible through the trees before they can see you** — the patrol announces itself, and reading the bobbing glow is the forest's stealth tell. Players choose: slip around, ambush quietly, or brawl. **Patrols are fully eliminable** — wipe one and the forest is genuinely clear until the next cadence tick — *until the alarm goes off*, when the castle starts sending real reinforcements (§8.1). **A patroller who breaks away and reaches the hamlet starts the raid warm.** Patrols intercept loot couriers **opportunistically only** (decided).
- **Landmarks for orientation:** the watchtower's silhouette over the treetops, chimney smoke, a crashed cart on the main path (free starter loot + tutorial-by-scenery), **the windmill's turning sails when this seed rolled one** — and, far on the horizon, the silhouette of **Highpurse Keep**, the castle whose soldiers answer the bell (§8.1): a skybox promise of consequences, and the ladder's top rung.
- *(The old harvestable-trees/deadfalls wood loop is tier-2 content with the rest of the wood economy — Appendix A. Chopping is not a slice verb — decided, 2026-07-21.)*

### 6.3 The generated hamlet

A tiny, unwalled human farming hamlet doing suspiciously well for itself. No palisade, open approaches; defense by sightlines and a handful of guards. At raid start, a **constrained seeded generator** assembles it from three hand-authored **module types** on a **zone-ring template** *(decided, 2026-07-21 — modules occupy zones on a ring; roads are wayfinding dressing, not structure; the road-and-plots model becomes the tier-2+ generator evolution, where streets are real)*, stitched together with trees and greenery:

| Module | Contents | Burn objective |
|---|---|---|
| **Core village** | Houses (6–8 in a ring, **shells-only interiors for the slice** — decided), the **well** (bucket-brigade source, foulable), the **watchtower** (watchman + bell + doze cycle), the small **barracks** (stone-based, unburnable, beatable), street lamps, propaganda props (the WarriorStatue et al., §3) — and, **only when the village carries an objective: market stalls with lootable gold and the strongbox** *(decided, 2026-07-21 — market wealth spawns only where the granary objective rolls; a non-objective village still generates its garrison, well, houses, and civilians, but keeps no wealth worth the trip)* | **The village granary** — a fat, silo-bellied hoard of grain sitting where the guards are thickest. Chaos-fracture collapse when fully burned. *(Custom asset — decided, 2026-07-21: the granary is a custom silo-bellied build, and the custom-vs-kitbash style seam is a feature: the things you burn read as yours to burn. The Barn stands in until it lands.)* |
| **Farmstead** | Barn, **livestock pens** (chickens, sheep, pigs — §9: kill petty, steal big; a startled flock is a noise stimulus, so pens are stealth terrain), troughs, coop, haystacks, scarecrow | **The ripe wheat field itself** — fire spreads row to row across the crop grid; the most visible arson in the game. Doused cells are **re-ignitable**, and field fire **may jump to adjacent flammables** — fences, haystacks, a granary built too close *(decided, 2026-07-21 — more fire the better)*. Completion is measured in **grid cells: ≥70% burned (placeholder)**; Combat & Feel owns the metric. |
| **Windmill** | The mill and its grain logistics (sack and crate clutter) — and the **landmark**: its sails turn above the treetops, an orientation aid from anywhere in the forest | **The mill** — and it has the game's wow moment *(decided, 2026-07-21)*: exterior fire alone won't take it. You ignite it by **tossing torches through its windows** (an aimed throw); the interior fire builds until the **grain dust detonates** — a one-off explosion (VFX + light spike budget-exempted for the moment itself) — and the aftermath **state-swaps to a smashed version**: silhouette kept, collapsed inward, still burning. |

- **The objective roll (decided):** each generated map carries **three burn objectives randomized across the module types, max two of the same kind** — granary + two fields one raid, mill + granary + field the next. The portal opens when all three burn. **Two-village rolls are allowed** *(decided, 2026-07-21)*, with a standing generator rule: **two villages always generate far apart**. The roll changes where the loot, livestock, guards, and fire risk sit relative to each other; this is the tier-1 difficulty configuration — higher tiers re-weight module mixes, objective counts, and rewards.
- **Objective discovery (decided, 2026-07-21):** the roll is **named at raid start** — HUD objective list plus the Overlord's whisper — but the environment, not the UI, guides you to *where*: **converging roads, signposts, wheat-density gradients thickening toward a field, the audible racket of a pen**. This is a **standing generator placement requirement**, not set dressing. **No HUD arrows, ever.** Finding the targets is the scout.
- **The objective manifest (decided, 2026-07-21 — hybrid approved):** the generator emits a **manifest that is the source of truth for the roll**; per-entry actor resolution stays tag-search. Test plan: benchmark raid-start cost of manifest+validation vs pure tag-search — honestly noted: the slice is offline solo/co-op, no shared many-player resource exists, so the measurable risk is **raid-start hitching**; any future online backend load-tests separately.
- **Trees & greenery seams:** hedgerows, copses, garden plots, and meadow grass fill the gaps between modules — and double as the stealth layer's cover language. **Density goes up substantially from the prototype's first pass** *(decided, 2026-07-21 — "a lot more trees")*. **Cover rule (decided):** the generator guarantees broken sightlines between the treeline and every objective. Fair by construction, on a map no human ever reviewed.
- **Persistence & cost:** modules and arrangement change per seed; the forest ring and runic site do not (§6.1–6.2). Runtime navmesh builds at raid start — a loading-moment cost, not gameplay.
- **Golden-seed fallback (decided):** if variance tuning slips, ship the single best seed as *the* hamlet. Human gate: Michael plays 5 seeds; all must read fair — **on foot**, which is why the walkable base character controller is week 1's top priority (§12.2, decision 23).
- **Population (decided, 2026-07-21 — supersedes the old settlement table):** **4 posted guards — 1 in the watchtower, 1 at the barracks door, 2 wandering** (a data row, not code) — plus barracks reinforcements while alarmed, **6–10 civilians** on routine loops, and penned livestock.

### 6.4 The settlement ladder

The generator is the first rung of the game's long-term structure. Each tier scales defenses, enemy quality, mechanics, and rewards; all tiers procedurally generated. **Tier names are county names** (decision 10) — you raid ever-richer counties' settlements, and the settlements stay nameless until the one that isn't.

| Tier | County / target | New defenses | New play |
|---|---|---|---|
| 1 · *the slice* | **Groatsworth** — an unwalled hamlet | Open ground, 1 watchtower, small barracks | Pure stealth-approach raiding — this document |
| 2 | **Pennybrook** — a palisade town | Wooden wall, gatehouse, more guards | The breach layer — hidden gap, dig, ram, catapult, the gate-opening inside job (Appendix A; fully designed, deferred not cut) |
| 3 | **Silverford** — a stone-wall town | Unburnable perimeter, knights standard | Fire stops working on the walls; digging and siege engines become load-bearing |
| 4 | **Highpurse Keep** — the castle itself | The county seat — where the reinforcements come *from* | Assassination missions (Mayor Goldbottom) — the roadmap's endgame |

The ladder reframes rather than discards: tier 1 exists so the stealth-and-raid core is proven on open ground before walls complicate it — and it is partly a story about the horde's access getting harder to arrange (§5).

---

## 7. The approach — stealth

With no wall, getting in is trivial; getting in **unnoticed** is the game. Stealth in the slice is deliberately *lite* — one readable foundation plus exactly five systems.

**The foundation (existing, unchanged): the one-number confirm model.** A guard's sighting of you must **hold ~1.5 seconds to confirm**. Break line of sight before the confirm lands and you were never there; get confirmed and the town moves to SUSPICIOUS (§8.1). Every stealth system below tunes this one number's inputs — nothing replaces it.

**The lean five (decided):**

1. **Noise.** Every action gets a formalized **audible radius** (extends the existing noise events): walking quiet, running less so, smashing loud, fire a town-wide announcement. Crouched actions scale to **0.45–0.6× radius** *(placeholder, signed 2026-07-21)*. Guards investigate noise. Squawking chickens investigate *you* — and a **thrown chicken is a deliberate noise lure** (§3, decision 35).
2. **Crouch — a toggle** *(decided, 2026-07-21)*. The stealth stance: multiplies every guard's effective detection range by **0.75× *(placeholder, signed)*** and slows you to a creep. Hedgerows, fences, haycarts, and pen walls are the cover language; the generator guarantees broken sightlines between treeline and every objective (§6.3).
3. **Silent takedown.** Hold-E on an *unaware* human from behind — eligibility is a **120° behind-cone**, the channel runs **1.2s** *(placeholders, signed)* — one silent, committed animation and they're gone: **the victim slumps quietly, no scream** *(decided)*, and **the goblin stays silent too before the horn — no gloat lines in the quiet half** *(decided, 2026-07-21)*. Interruptible, position-demanding, deeply goblin. Any class; the Slasher fastest. **+5 deed bonus** on the kill.
4. **Corpse-suspicion.** No body dragging in this slice — but a guard who *sees* a corpse (same **1.5s confirm-hold** as a live sighting — decided, 2026-07-21) goes SUSPICIOUS and investigates. Sloppy stealth leaves a trail of alarms. **A found corpse does *not* void "First Spark Unseen"** *(decided)* — the bonus tracks confirmed *goblin* sightings, not suspicions; the town finding bodies and refusing to draw the obvious conclusion is the propaganda pillar (§3) doing its job.
5. **The coin toss — promoted from stretch to core** *(decided, 2026-07-20)*. Throw looted coins — **each toss debits real coins from your pouch** *(decided, 2026-07-21)* — and greedy humans (guards included) break behavior and scramble. The distraction tool that *is* the thesis: greed as an exploitable AI stat, and a distraction that literally costs you score.

**Loop shape (decided): quiet in, loud out.** The objective is arson, and fire cannot be hidden — stealth is not about ghosting the mission, it is about **controlling when the loud half starts**. The reward for staying hidden is *position*: full pockets, thinned guards, a fouled well, and the horde one horn-blast away. The horn is the formal go-loud button. Score bonus: **"First Spark Unseen" +40** — no confirmed goblin sighting before the first objective ignites.

**Explicitly rejected for the slice (decided):** light/shadow detection, disguises, body carrying, a ghost-run win condition.

> **Tier-2 appendix note:** the former §7 — the breach systems (hidden gap, gate + crossbar/Inside Job, digging, the wood economy, ram, catapult) — is relocated **verbatim and in full to Appendix A** as tier-2 content: designed, architected (tech doc §17, §20), and **deferred to the ladder's second rung, not cut**.

---

## 8. The defenders & the alarm

### 8.1 Alarm phases

The meter persists and drives **phases** rather than only escalating waves:

1. **QUIET** — daily routine. Guards posted, civilians pottering, watchman half-asleep, pens clucking. Alarm ticks only from *witnessed* events.
2. **SUSPICIOUS** (alarm > low threshold) — a half-confirmed sighting, a noise, a corpse, a patrol gone missing. Nearest guard investigates; the watchman actually watches; civilians mutter (and disbelieve — §3). Decays back to QUIET if nothing is confirmed.
3. **RAID** (bell rung / open combat / **fire seen by a human** — *decided, 2026-07-21: fire promotes the alarm when a human sees it, with a **~10-second unseen-fire fuse** as backstop; even an unwitnessed blaze announces itself shortly*). The meter becomes the escalation system: barracks spawns, guard mix toughens per tier (militia → +archers → +knights), horde waves at threshold crossings. **The bell also signals Highpurse Keep**: from RAID onward, **castle reinforcement squads** march in from the far map edge on a timer. Before the alarm, the forest's only threat is the 5–7-minute patrol cadence — which you can keep clearing; after it, the castle's soldiers keep coming whether you clear them or not (decided).
4. **RAZED / RELIEF** — once the hamlet is substantially burned, internal spawners go quiet and the castle reinforcements intensify into full **relief columns** — the "get out now" pressure on the trip home.

**Alarm sources:** confirmed sightings, the bell, combat, seen fires (or the fuse), the barracks falling, patrol runners reaching town. **Alarm reducers:** none. Goblins don't de-escalate; they leave.

### 8.2 The humans (and their animals)

Existing archetypes carry over untouched (see `race-design-humans.md`): **Militia** (30 HP filler), **Archer** (20 HP, aim-line telegraph), **Knight** (75 HP, armor 6, late tiers). Behavior roles, not new stat blocks:

- **Patrol** (forest): 2 militia + 1 archer, **torches drawn** — the firelight-through-trees tell (§6.2). A survivor of a botched ambush becomes a **runner** (sprints to warn the hamlet).
- **Watchman** (tower): archer + bell + doze cycle.
- **Garrison** *(decided posting, a data row)*: 1 watchtower / 1 barracks door / 2 wandering.
- **Firefighters:** bucket-brigade behavior sourced at the well; interruptible and killable mid-douse. **Field fires (decided, 2026-07-21 — Town Option C):** the brigade fights a burning crop **effectively at the field's edges only** — they can hold a perimeter, never the heart — and only responds once **the field is >25% burned** (the smoke draw): before that, nobody believes a field is really going. Earnest, orderly, and structurally too late.
- **Civilians** (10 HP, no attack): routine loops → **disbelief** (§3) → flee/panic when it stops being deniable → some join bucket lines → (stretch) some loot their own market.
- **Livestock** *(new — see §9)*: chickens, sheep, pigs on the civilian panic branch — flee behavior, startled-flock noise stimuli, catchable and carryable. **No named animals** *(decided — they are score, not pets)*.

---

## 9. Lives, death & extraction

- **5 lives** per player-goblin. Horde goblins have none.
- **Decided:** on death you respawn **at the runic site** after ~4s — the stones re-knit you. This makes the forest run meaningful and deep raids risky.
- The horde does **not** die with you; leaderless horde goblins hold position and defend themselves until you return or re-horn.
- Losing the last life = raid over; **unbanked score is heavily docked** — you keep **25%** as a consolation ("His Eternal Darkness salvages something from your corpse"). Making it back through the stones banks 100% plus the return bonus.

### Loot couriers — banking mid-raid *(decided)*

**Score comes in two kinds:**

- **Deeds** — kills, takedowns, arson, mischief bonuses (well-fouling, First Spark…). Intangible. Deeds bank only when *you* exit through the stones; a wipe salvages 25%. The **×1.5 return bonus applies to deeds only**.
- **Loot** — physical objects: market gold, strongboxes, grain sacks, valuables, **and livestock**. Loot is *carried*, and anything that physically reaches the runic site is **banked immediately and permanently** — wipe-proof.

**How loot moves:**

- Players carry loot sacks themselves (over-the-shoulder carry — slower, can't attack). Fine for a last armful on the way out.
- **The courier command:** point (§5) at a loot pile, dropped sack, **or animal** → the nearest horde goblin shoulders it (or chases, catches, and shoulders it) and **runs it all the way home** — then joins the reserve pool rather than trotting back alone. If the portal is open, the cargo banks; if not, it stages inside the circle and auto-banks when the portal flares (§6.1).
- **The tradeoff:** every courier is a fighter who leaves the raid. Greed vs muscle, from the horde pool you already manage.
- **The risk:** couriers are squishy, alone, and waddling under a sack through a patrolled forest. Patrols intercept **opportunistically only** (decided). A dead courier drops its cargo where it fell (recoverable). Watching your loot toddle into the treeline and *hoping* is exactly the right emotion.
- **Comedy layer:** grunts, drops, head-carries. The Overlord whispers approvingly when loot banks ("Yesss. The grain of the unworthy.").

### Livestock — loot that runs away *(decided, 2026-07-20/21)*

The pens hold **chickens, sheep, and pigs** (kitbash packs). The civilian satire rule applies on the hoof: **kill petty, steal big.**

- **Kill** = petty deed points (chicken 2 / sheep 5 / pig 8). Chicken death = feather-poof gib variant. Permitted and pointless.
- **Steal** = loot-kind (chicken 10 / sheep 25 / pig 40 — placeholders): live animals use the existing carry state and are valid courier targets. A pig is a squirming over-the-shoulder carry; the Brute carries two. **Chickens are weightless** *(decided, 2026-07-21)*: carry **two, one under each arm**, and **fighting one-handed while holding a chicken is possible** — encouraged, even.
- **Startled flocks are noise stimuli** — blundering into a pen pulls the nearest guard; a **thrown chicken** is the deliberate version (§3, §7). Pens are stealth terrain: hazard on one route, tool on another, payday on the way out.
- Implementation: archetype data rows + flee behavior on the civilian panic branch (§8.2); week 6 alongside civilians.

### Gathering loot — the pouch *(decided 2026-07-11/12; auto-bank added 2026-07-21)*

- **Opening a chest is a destruction, not a hinge:** the strongbox lid is a destructible (Chaos/Geometry Collection) smashed open by force; the coins are non-interactable until the lid is down.
- **Channeled gathering, not instant grab:** stash-type loot is worked with hold-E — several seconds packing coin bit by bit, interruptible like every channel. You keep what you'd packed; the rest stays.
- **The pouch** is your running unbanked-loot tally — passive, no slowdown, no fight penalty. **The coin toss (§7) spends from it.**
- **On death, the pouch drops** — spills as a lootable sack at your corpse, recoverable by you or a teammate. Don't die holding more than you're willing to lose.
- **Auto-bank (decided, 2026-07-21):** entering the runic circle banks the pouch automatically, mid-raid included (§6.1).

### The clock — 30-minute raids *(decided, unchanged)*

Hard cap at **30 minutes**; HUD timer always visible; the whispers turn impatient in the final five. **At 0:00** the portal begins to collapse — a **90-second grace window** (screen-wide warning, guttering stones, cold whispers) and then it dies. Anyone through in time banks normally; **anyone outside is left behind** — raid over, unbanked deeds take the wipe penalty (keep 25%). Staged and tossed-in loot is already safe. If the objectives were never completed by 0:00, the portal never opened — same salvage rule. Burn the targets; that's why you're here. The clock is the third pressure alongside lives and the alarm — and the strongest argument for sending loot home *early*.

---

## 10. Scoring — the prototype economy

**Decided: score only for the 7-week prototype.** No progression trees, no spending — an end-of-raid tally, a rating, and a personal best, architected so the future economy can consume it later without rework.

**Score table** *(all values placeholder for playtesting):*

| Event | Points |
|---|---|
| Objective burned (granary / field / mill) | 100 each — **completion-only** *(decided, 2026-07-21: awarded when the objective completes — collapse, ≥70% of field cells, the mill blast — never partial credit)* |
| All 3 objectives (raid complete) | +150 |
| Loot item (market goods, house valuables) | 5–25 each *(loot-kind: bankable by courier — §9)* |
| **Market gold** | **5–15 per stall** *(loot-kind; spawns only in objective-rolled villages — §6.3)* |
| Marketplace strongbox | 75 *(loot-kind)* |
| **Livestock stolen** | **pig 40 / sheep 25 / chicken 10** *(loot-kind — delivered live to the stones)* |
| **Livestock killed** | **pig 8 / sheep 5 / chicken 2** *(deed — permitted and petty, §3)* |
| Guard defeated | 10 (militia) / 15 (archer) / 25 (knight) |
| **Silent takedown** | **+5** *(deed, on top of the kill's value — §7)* |
| Civilian killed | 5 *(permitted and petty — §3)* |
| Watchman silenced before he rings the bell | +25 |
| **"First Spark Unseen"** — no confirmed sighting before the first ignition | **+40** *(deed; corpse discoveries don't void it — §7)* |
| Well fouled | +30 |
| Barracks demolished | +40 |
| Patrol wiped with no runner escaping | +15 |
| **Made it home** — exit via the runic site | **×1.5 on deeds** |
| Per unused life remaining at extraction | +20 |

*(The "Inside Job" +50 relocates to Appendix A with the breach layer — decided, 2026-07-21: a far-future achievement, appendix only.)*

The multiplier-on-return is the loop's spine: **deeds** are provisional until you're standing in the stones, while **couriered and banked loot is already safe** (§9). Kill-petty/steal-big, the pouch, the couriers, and the multiplier are one tension — greed vs safety — expressed four ways, by design. End-of-raid screen: itemized deeds/loot tally, an Overlord whisper verdict ("Adequate. I have seen rats do better. …adequate."), letter grade, personal best.

---

## 11. Asset manifest — everything the prototype needs

Scoped to one generated environment, one defender race, third-person camera, dusk lighting.

### 11.0 Art direction & the material language *(decided)*

**Stylized and hand-painted.** Chunky proportions, painterly textures, saturated dusk palette. Locked — it hides kitbash seams, sells the satire, keeps the cartoony gore on the T side, and is achievable solo. **Kitbashing from Fab/Marketplace stylized-fantasy packs is green-lit** for environment, humans, and livestock (the slice builds on the Dreamscape Farmlands set); custom-art time goes to goblins, the runic site, and hero props — **and the custom-vs-kitbash seam is a feature** *(decided, 2026-07-21)*: the silo-bellied granary and the other things you burn read as *yours to burn* against the kitbashed everyday.

**Every material telegraphs its hardness.** The art *is* the tutorial:

| Material | Look | Burns? | Breaks? | Gameplay meaning |
|---|---|---|---|---|
| **Thatch / hay / ripe wheat** | Shaggy, golden, overhanging | Instantly, spreads fast | Yes (trivial) | Torch magnets — roofs, haycarts, and crop rows are your accelerant |
| **Raw wood** (granary, barn, mill, stalls, fences) | Visible planks, grain, rope lashings | Yes, on a delay | Yes — club, horde | The default goblin-interactive surface; the hamlet is made of it |
| **Hardened/banded wood** (strongbox) | Iron bands, big rivets | Slowly / no | Only via the right verb | Signals "there's a mechanic here, not just HP" |
| **Stone** (barracks base, well, tower footing) | Rounded painted masonry | No | Structural HP only — slow, loud | The fire-immune counterweight; beat it down or leave it |
| **Metal** (knight armor, bell) | Painted specular, dented | No | No — mitigates (armor stat) | Pierce it or stagger the wearer |
| **Runic stone** (the site) | Dark stone, glowing carvings | No | Indestructible | Sacred to goblins, untouchable by design |
| **Forest-ring green** | Deep, lush, faintly too-alive | **No** *("it's magic" — decided 2026-07-21)* | No | The frame doesn't burn; your fire language stops at the treeline |

Rule for every new asset: assign its material class *first*, then its look — never ship a surface whose appearance lies about its hardness. **Generator corollary:** every prefab-kit piece must read correctly from every angle, because no human places it.

### 11.1 Environment
- **Forest kit:** pines/oaks/birches, bushes, rocks, dirt-path spline textures, treeline "horde spawn" markers, crashed cart set piece. *(Choppable-tree variants: tier 2, Appendix A.)*
- **Runic site:** standing stones ×6–8 (emissive runes), circle ground decal, portal activation FX.
- **Hamlet prefab kit (generator modules, Dreamscape kitbash + conformance pass):** house shells ×small/medium variants (**shells-only interiors for the slice** — decided), well + roof, market stalls + tables + covers (**cover recolor to burgundy/gold approved** — decided 2026-07-21), barn, fence/gate segments, pens, troughs, coop, haystacks, scarecrow, street lamps, clutter (barrels, crates, sacks, firewood), **wayfinding dressing** (signposts, converging road decals — a placement requirement, §6.3), **propaganda props** (WarriorStatue and kin, recast as royal propaganda — §3).
- **Objectives:** **granary — custom silo-bellied build** with burn states + Geometry Collection (Barn stands in until it lands — decided); **wheat-field grid cells** with per-cell burn/doused/re-ignited states (§6.3); **windmill** — base + animated sails, window sockets for the aimed torch throw, interior-fire glow stages, **grain-dust explosion one-off**, and the **smashed state-swap variant** (silhouette kept, collapsed inward, still burning). **Baseline light budget: ≤3 shadowless dynamic lights (field) / ≤2 (mill)**, explosion spike exempted *(decided, 2026-07-21)*.
- **Defense:** watchtower (ladder, platform, **bell** — animated + interactable), barracks (stone base, barred door/windows), well "fouled" state variant.
- **Loot props:** strongbox (destructible lid, fill states, padlock+hasp — as built), **dropped loot pouch/sack** (death-drop and courier-drop, shared prop).
- *(Palisade kit, gatehouse, siege engines, build-site decals, wood bundles: tier 2 — Appendix A.)*

### 11.2 Characters
- **Player goblins ×4 kits:** shared body (Brute at 1.5×) + kit dressing. Custom art priority #1.
- **Horde goblin:** one cheap variant mesh (color/prop randomization).
- **Humans:** militia, archer, knight, civilian ×2 body variants.
- **Livestock:** chicken, sheep, pig (kitbash skeletal meshes; no named individuals — decided).

### 11.3 Animation
- **Goblin shared set:** locomotion, **crouch locomotion (toggle stance)**, dodge roll, hit reacts, death, torch throw (**incl. aimed window throw**), horn blow, **takedown (silent — no bark pre-horn)**, interact-channel (covers loot/foul-well/extract), carry locomotion (sack / pig / **two chickens underarm + one-handed fighting variant** — decided 2026-07-21), emotes (cheer/giggle).
- **Per-kit attack sets:** unchanged (daggers combo + lunge; bow; club light/heavy/slam; staff casts ×2 ×2).
- **Human set:** locomotion, melee windup/swing, bow, hit/death, **quiet takedown slump** *(decided — the victim's half of §7's takedown)*, bucket-douse loop, panic-flee, **disbelief double-take** (§3), doze/wake, bell-ring, coin-scramble, run-the-alarm.
- **Livestock set:** idle/graze, flee/flap, carried squirm.
- Marketplace packs + retarget cover most; hand-key the comedy set and goblin signature moves.

### 11.4 VFX (Niagara)
Fire (torch flame, surface fire, structure-burn stages, **row-to-row field-fire spread + smoke draw**, smoke columns — your progress report over the treetops), extinguish steam, **the grain-dust explosion** (one-off, budget-exempt light spike — §6.3), telegraph rings & aim-lines (ground decals for 3D), hitstop sparks, portal shimmer, wood-splinter bursts, blood-orb & shadow FX, coin glitter, alarm vignette, **gore set (cartoony):** spatter bursts, gib/limb pops with clean cartoon bones, **feather-poof** (chicken), comic "poof" on horde death — one shared gib component with an intensity scalar (the ratings knob).

### 11.5 Audio
Horn call (hero sound #1), portal hum/whoosh, bell (hero #2), fire loop + collapse, **the mill detonation (hero #3)**, pen ambience → **startled-flock squawk burst** (a *stimulus* as much as a sound), combat layer + wet cartoon splats, goblin chatter/giggles (suppressed pre-horn — §7), human bark VO *(text-only for prototype)*, forest dusk ambience, hamlet ambience (pottering → panic), score stinger, **His Eternal Darkness: layered dark-whisper beds + subtitles** (decided).

### 11.6 UI
HUD: health/lives, torch count, horn status (active/reserve), kit resource, **objective tracker naming this raid's roll** (granary/field/mill ×3 — names only, **no arrows**, §6.3), alarm-phase indicator, 30-minute timer, subtitle strip, interact prompts + channel bars, carry indicator, **pouch tally**. Screens: class select, pause, end-of-raid deeds/loot tally + grade, death/respawn. Reticle + soft lock-on.

---

## 12. Systems list & 7-week build plan

### 12.1 Systems inventory *(★ = new since the combat prototype / tech scaffold; ► = changed or added in the 2026-07-20/21 restructure)*

| # | System | Status |
|---|---|---|
| 1 | GAS combat pipeline, damage/armor/race matrix | Scaffolded (compiles, per build log) |
| 2 | 4 weapon kits + torch toss | Designed + browser-proven; UE port. ► Torch throw gains the aimed window-throw (mill) |
| 3 | ► Fire/flammable/spread + Chaos fracture | Scaffolded; now **three objective burn types**: granary collapse, **spreading field fire** (grid cells, ≥70% completion, re-ignitable, jumps to adjacent flammables — Combat & Feel owns), **mill chain** (window ignition → dust explosion → state-swap) |
| 4 | ► Alarm meter + phases (QUIET/SUSPICIOUS/RAID/RAZED) | Scaffolded; phase rework + **fire-seen promotion w/ ~10s unseen fuse** + startled-flock stimuli feed it |
| 5 | ★ Third-person camera & control | New — reticle aim, soft lock, telegraph redesign for 3D. ► **Walkable base controller is W1's top deliverable** (decision 23) |
| 6 | ★ Horn & horde — summon, pool, follow/frenzy/point | New — biggest new AI system |
| 7 | ★ ► **The stealth five** on the one-number confirm model | New — noise radii (data), crouch toggle (0.75× detection, 0.45–0.6× noise — placeholders), takedown awareness/behind-cone, corpse-suspicion (confirm-hold, doesn't void First Spark), coin-toss lure (pouch-debited) — plus the existing doze/runner/investigate set |
| 8 | ★ ► Interact framework — hold-E channels | New — slice verbs: **loot (gather-pouch) / takedown / foul-well / extract** + carry state. Dig/build/chop/crossbar → tier 2 (Appendix A) |
| 9 | ★ Breach set — gap, gate HP + crossbar, tunnel, ram, catapult | ► **DEFERRED — tier-2 backlog (Appendix A).** Design + architecture stand and wait |
| 10 | ★ Wood economy — choppables, bundles, build sites | ► **DEFERRED — tier-2 backlog (Appendix A)** |
| 11 | ★ ► Civilians **+ livestock** | Partially scaffolded (firefighting BT task exists) — routines, disbelief→panic, bucket brigade (**field-edge rules + >25% smoke gate**), well interaction; **livestock**: archetype data rows, flee on the panic branch, flock stimuli, carryable/courier-able, chickens weightless ×2 underarm |
| 12 | ★ Score system — event bus → deeds/loot two-kind tally, banking multiplier, end screen | New — ► + livestock/market-gold/takedown/First-Spark rows; objective points completion-only |
| 13 | ★ Runic site — spawn/respawn + objective-gated portal, toss-in/staging, 90s collapse | New — ► + **pouch auto-bank on circle entry, mid-raid included** |
| 14 | Lives/respawn on PlayerState | Scaffolded; respawn at runic site (decided) |
| 15 | Barks (text) + His Eternal Darkness whispers/subtitles | New, cheap — ► + disbelief/livestock/stealth bark families (Writer extension pass) |
| 16 | ★ Gore/gib system — cartoony, shared component, intensity scalar | New — small; ► + feather-poof variant |
| 17 | ★ ► Loot couriers — sacks **+ livestock** as cargo, point-to-courier BT, mid-raid banking | New — reuses carry state + point command + forest pathing; chase-catch-carry for animals |
| 18 | ★ Raid clock — 30-min cap, 0:00 → 90s collapse & left-behind rule, impatient whispers | New — small |
| 19 | ★ Patrol director — 5–7-min cadence (► 2 militia + 1 archer, torches drawn); castle reinforcements post-alarm | New (extends system 4's spawn logic) |
| 20 | ★ ► **Settlement generator** | New — **zone-ring template** (roads = wayfinding dressing; road-and-plots is the tier-2+ evolution), three module types, module placement (**two-village rolls far apart**; non-objective villages carry no market), **objective roll (max 2 of a kind) emitting the manifest** (source of truth; per-entry resolution stays tag-search; benchmark raid-start cost — §6.3), guard posts/routes/routines by rule, **cover + wayfinding placement rules**, **substantially increased tree/cover density**, runtime navmesh at raid start, golden-seed fallback. The slice's biggest new technical bet |

### 12.2 Week-by-week *(solo dev + AI-assist — confirmed)*

| Week | Goal | Deliverable |
|---|---|---|
| **1** | Third-person foundation + quiet verbs | **Walkable base character controller first — top priority (decision 23): Michael must be able to review generator seeds on foot from day one.** Then camera/reticle on the scaffold; one kit (club) attacking a militia in a blockout; **interact-channel framework** (five systems ride on it — built first); **crouch toggle + noise foundations** |
| **2** | Fire + the three burn types | Torch + spread; **granary collapse, spreading field fire, mill chain (window ignition → explosion → state-swap)**; alarm-phase skeleton (incl. fire-seen/fuse promotion); raid-clock skeleton |
| **3** | The horde | Horn, pool, follow/frenzy/point; horde archetype; 10 goblins + player vs guards at frame rate |
| **4** | The generated hamlet | **Generator v1** (zone-ring template, module placement, objective roll + **manifest + validation; run the manifest-vs-tag-search raid-start benchmark**) + runic-site portal — **first full loop playable on a generated hamlet, start-to-bank** |
| **5** | Stealth complete | **The stealth five complete** (takedowns, corpse-suspicion, coin toss); patrols (torches drawn) + runner; watchman + bell + doze; **generator seed tuning + the 5-seed on-foot review gate** |
| **6** | The living hamlet | Civilians (disbelief → panic), routines, bucket brigade (field-edge rules), well fouling; **livestock**; loot sacks + couriers + gathering pouch (+ auto-bank); remaining kits ported; score system + end screen; gore pass; art pass 1 (kitbash dress-up + stall recolor) |
| **7** | Feel & funny | Telegraph/hitstop/shake tuning in 3D; barks (incl. disbelief family); hero audio (horn, bell, **mill detonation**); balance pass on score table & horde counts; bug triage; **playtest build** |

### 12.3 Honest scoping — the cut order

If weeks 5–6 slip, cut in this order *(revised per the pivot — decision 7)*:

1. **Self-looting civilians** (stretch already) → 2. **Generator variance** — the **golden-seed fallback**: ship the single best seed as *the* hamlet; the pipeline is still proven and the slice still plays → 3. **Sheep & chickens** (pigs alone carry the livestock design) → 4. **Coin toss** (back to stretch) → 5. **Corpse-suspicion** → 6. **Takedowns**.

**Never cut:** the horn/horde, **the three-objective burn structure** (floor if the roll must simplify: three fixed objectives, one of each type), **the crouch-and-confirm stealth core**, the runic-site banking loop, the score screen. That quintet *is* the game on any map.

**The breach systems are not in this cut order because they are not in the slice** — they are tier-2 content (Appendix A). Deferred and cut are different words on purpose.

**Team reality (confirmed):** solo dev (Michael) + AI-assist. The week-by-week is written for exactly that; the cut order is the safety valve, not a failure state.

**Post-slice roadmap (decided order):** ① **Points & upgrades** — the spending economy built on the score system, worked toward a releasable build → ② **Co-op multiplayer** — turn on the networking everything was built ready for, then playtest → ③ **The assassination mission** — Mayor Goldbottom, after multiplayer playtesting — via the ladder's upper tiers (Pennybrook's breach layer, Silverford, Highpurse Keep). Elves/Dwarves unchanged: designed, further out.

---

## 13. Decisions ledger & remaining open items

**Decided (2026-07-10, three review rounds — summarized):** camera third-person, Overlord-style · horde control = follow & frenzy + one point command · siege tools = gather wood, then build *(stands as designed; tier-2 content since 2026-07-20 — Appendix A)* · prototype economy = score only · everyone killable incl. civilians, cartoony gore, T-minimum target · art stylized/hand-painted + material-hardness language + kitbash green-lit · team = solo + AI-assist · dusk, fixed · respawn = runic site · the Overlord = His Eternal Darkness, whispers + subtitles · naming register cozy-smug, Pennybrook confirmed *(2026-07-21: Pennybrook is now the tier-2 county's name — decision 10; the slice's raided settlements are nameless)* · horde 10 active / 20 reserve · solo slice, co-op-ready · couriers: opportunistic intercept only, ×1.5 deeds-only, courier-to-reserve · 30-minute raid clock · patrol cadence 5–7 min, eliminable until the alarm · bark rules + sheet approved · post-slice order: economy → co-op → assassination · portal opens on objective completion; 0:00 → 90s collapse; left-behind rule.

**Decided (2026-07-11/12):** chest looting = channeled hold-E gathering (the pouch); on-death pouch drop shares the courier sack prop · chest lid = destructible (Chaos), smashed open; coins gated on lid-broken.

**Decided (2026-07-20 — the hamlet pivot, decisions 1–9):**
1. Slice pivots to an unwalled hamlet (no palisade, open approaches, small garrison); forest/patrols/runic site/clock/lives/alarm/horde/couriers/scoring unchanged.
2. The settlement ladder is the long-term structure (hamlet → palisade → stone wall → castle keep); the breach layer is **tier-2 content, not cut**; ascending-currency naming register.
3. Constrained procedural generation: hand-authored prefabs, seeded arrangement, rule-placed posts/routes/cover; forest ring + runic site persist; runtime navmesh; golden-seed fallback; 5-seed human gate.
4. Stealth = the lean five on the one-number confirm model (noise radii, crouch, takedown, corpse-suspicion, coin toss promoted to core); light/shadow, disguises, body-carrying, ghost-runs rejected; quiet in, loud out; "First Spark Unseen" +40.
5. Livestock: kill petty (2/5/8), steal big (10/25/40); carryable + courier-able; startled flocks are noise stimuli.
6. Agent architecture: six game-system agents + four studio agents (ten total).
7. Revised cut order (above); revised never-cut quintet; breach systems outside the cut order.
8. Revised week plan (above).
9. Hamlet map composition: three module types (core village / farmstead / windmill), three-objective roll, max two of a kind; village=granary, farmstead=field, windmill=mill; portal on all three.

**Decided (2026-07-21 — restructure merge approved (Q-01) + rulings, decisions 10–35):**
10. Ladder names are **county** names (Groatsworth/Pennybrook/Silverford); raided settlements stay nameless; Highpurse Keep stays a named place.
11. Objective discovery: roll **named at raid start** (HUD + Overlord); environment guides to *where* (converging roads, signposts, wheat-density gradients, audible pens — a standing generator placement requirement); **no HUD arrows**.
12. Chop deferred to tier 2 entirely; slice interact verbs = loot, takedown, foul-well, extract (+ carry).
13. "Inside Job" = far-future achievement; lives in Appendix A only.
14. House interiors shells-only for the slice; revisit post-prototype.
15. Garrison: 4 posted = 1 watchtower / 1 barracks door / 2 wandering (data row); forest patrols = 2 militia + 1 archer with **torches drawn** (firelight visible before they are).
16. Field-fire brigade: **effective at the edges** (Town Option C), gated on the smoke draw — responds only once the field is **>25% burned**.
17. Non-objective core villages still generate (garrison/well/houses/civilians) but carry **no market or strongbox** — market wealth only where the granary objective rolls.
18. Mill chain: ignition by torches **through the windows** (aimed throw; exterior fire insufficient) → interior build-up → **grain-dust explosion** (one-off VFX/light spike allowed) → **state-swap** to a smashed, inward-collapsed, still-burning silhouette. Baseline light budget ≤3 shadowless (field) / ≤2 (mill).
19. Fire → RAID requires the fire to be **seen by a human**; **~10s unseen fuse** as backstop.
20. Crouch = **toggle** · coin toss **debits pouch coins** · takedown kill = **quiet slump** · placeholders signed: **1.2s** takedown channel / **0.75×** crouch detection / **120°** behind-cone / **0.45–0.6×** crouched noise scale.
21. Granary = **custom silo-bellied** build; the custom-vs-kitbash style seam is a feature; Barn stand-in approved meanwhile.
22. **Two-village rolls allowed**; new generator rule: two villages always generate **far apart**.
23. Tree/cover density up **substantially**; **walkable base character controller = top W1 priority** so Michael reviews seeds on foot.
24. Corpses: the 1.5s confirm-hold applies to corpse recognition; a found corpse does **not** void First Spark Unseen — guards go Suspicious only.
25. **Pouch auto-banks** on entering the runic circle, mid-raid included.
26. Doused field cells **re-ignitable**; field fire **may jump** to adjacent flammables (fences, haystacks, a close granary) — more fire the better.
27. Stall-cover recolor to **burgundy/gold** approved.
28. Chickens: **weightless**, carry **two (one under each arm)**; fighting one-handed while holding a chicken is possible.
29. No named livestock · player-goblin **silent on takedowns pre-horn** · objective points are **completion-only**.
30. **Propaganda tone pillar** (§3): hamlets saturated with royal propaganda (the king slew the last dragon, vanquished the darklord and his minions) → villagers **initially disbelieve their eyes**; feeds disbelief barks, propaganda props (WarriorStatue kept as such), First Spark comedy.
31. Forest ring is green and **unburnable** ("it's magic").
32. Field-fire completion: **Combat & Feel owns it**; unit = grid cells; **≥70% (placeholder)**.
33. **Objective manifest approved** (hybrid: manifest = source of truth for the roll; per-entry actor resolution stays tag-search). Test plan addition: benchmark raid-start cost of manifest+validation vs tag-search — noting honestly that the slice is offline solo/co-op (no shared many-player resource exists); the measurable risk is raid-start hitching; any future online backend load-tests separately.
34. **Zone-ring template** for the slice (roads are wayfinding dressing); road-and-plots becomes the tier-2+ generator evolution where streets are real (towns/cities).
35. Thrown chicken = **noise** lure (greed stays the coin toss's joke).

**Remaining open items:**

1. **Score table blessing** — §10 values (now incl. livestock, market gold, takedown, First Spark) are placeholders; sign off after the first playable tally (week 6).
2. **Gore intensity default** — the gib scalar ships tunable; pick the default in the week-7 polish pass.
3. **Pouch gather-rate & chest fill states** — channel speed, and whether fill states are visual or gating; pass once system 8 is implemented.
4. **Chest lid fracture setup** — piece count, break conditions, debris behavior; a Blender-side call once the destructible pipeline is wired for props generally.
5. **Generator module specifics** beyond decisions 9/17/22/34 — instance counts beyond the three objective carriers, zone-ring radii, the signpost/road wayfinding grammar; Settlement Generator agent to spec before W4.
6. **Seed-review gate** — Michael plays 5 seeds on foot (W5); all must read fair.
7. **Manifest benchmark result** (W4) — adopt or fall back per the raid-start numbers (decision 33's test).
8. **Strongbox under a two-village roll** — one strongbox seeded into one of the two objective villages, or one each? *(placeholder: one, seeded)*.
9. **Bark-sheet extension** — livestock, disbelief family, stealth states, pig-delivery whispers; Writer to draft, Michael to approve.

---

## 14. Modeling best practices (Blender/asset pipeline)

A running log of production-technical rules learned the hard way during asset builds — separate from the art-direction rules in the style guide, which cover how things should *look*. This section covers how they should be *built* so they don't cause downstream engine problems. Check this before modeling any new asset; add to it whenever a build reveals a new one.

1. **Never fan a face's fill to a single center vertex. *(added 2026-07-12, from the treasure-chest end caps.)*** Capping a hole (a disc, a fan-shaped panel, any n-gon boundary) by connecting every boundary vertex to one shared center point creates a pole with as many edges as the boundary has vertices — the chest's end caps had a 41-edge pole from a 41-point boundary. A pole that severe skews normal interpolation badly enough to cause visible faceted shading and lighting artifacts under smooth shading, even though the topology "closes the hole" correctly on paper. **Fix:** fill n-gon boundaries with a proper triangulation that distributes edges across the surface (Blender: fill the boundary as an n-gon, then triangulate with the beauty method) rather than a manual fan from one point. Target keeping any single vertex's edge count in the single digits (under ~10) unless there's a specific structural reason (an actual wheel-spoke shape) for a higher-valence pole. This applies to any cap/fill face on any asset, not just chest lids — check for it on every hollow object's end caps, domes, and disc-shaped fills before calling the mesh done.

---

## Appendix A — Tier-2 content: the breach layer (Pennybrook)

*Everything in this appendix is designed, architected (tech doc §17, §20), and **deferred to the settlement ladder's second rung — not cut**. It re-enters at tier 2 (Pennybrook county's palisade towns), where the wall makes it load-bearing. No text below has been revised since the 2026-07-20 pivot; treat all numbers as pre-pivot placeholders when tier 2 is scoped. Related relocations: the horde's walled-access rules (§5), the "Inside Job" +50 score row (now a far-future achievement — decision 13), and the harvestable-trees wood loop (old §6.2).*

### A.0 The palisade settlement *(the old §6.3 — tier-2 target reference)*

A small human farming settlement — one social class above a hamlet and insufferably proud of it. Enclosed by a **wooden palisade**.

| Structure | Function | Notes |
|---|---|---|
| **Palisade wall** | The enclosure. Wooden stakes, walkable perimeter outside. | Not climbable by hand. Diggable-under at its base. Burnable only slowly and loudly (fire on the wall itself spikes alarm hard — possible but rude). **Always has a hidden gap somewhere** (A.1). |
| **Gatehouse** | Main (only) gate. **2 militia guards** posted. | Gate = structural HP object: batter it down (ram ≫ Brute club > other melee) **or open it from inside** via the crossbar (hold-E interact). Opening from inside is silent until someone notices it's open. |
| **Watchtower / Granaries ×3 / Well / Barracks / Marketplace / Houses** | As per the tier-1 designs, scaled up. | Population: ~6 posted guards (2 gate, 1 tower, 3 wandering) + barracks reinforcements, ~8–10 civilians. |

*(Forest wood sources — marked choppable trees and deadfalls, chopping loud enough to draw a patrol — return with this tier.)*

### A.1 The hidden gap — free, if you find it

Every wooden palisade has a spot the humans never quite fixed. Each raid, **1 gap is active out of ~4 candidate locations** (randomized): loose planks, a hog-hole, a section patched with a cart wheel. Squeeze-through is single-file and slow (players and horde both fit; the Brute grumbles and takes longer). Finding it costs scouting time; using it is silent. *Satire dressing: one candidate spot is "fixed" with a NO GOBLINS sign.*

### A.2 Wood-and-build — the ram and the catapult *(decided: gather wood, then build)*

A light resource loop, no inventory screen:

- **Wood bundles** come from chopping marked trees, forest deadfalls, or — funniest and fastest — **stealing the settlement's own woodpiles** stacked outside the wall.
- Carrying a bundle is a visible over-the-shoulder carry: **move slower, can't attack** (drop it to fight). Brute carries two.
- Deposit bundles at a **build site** (fixed, pre-placed footprints — near the gate for the ram; in a clearing in wall-range for the catapult). When the wood quota is met, any goblin channels **Build** (hold E) to raise it. More builders = faster.

| Engine | Wood cost | What it does |
|---|---|---|
| **Battering ram** | 4 bundles | 2+ goblins grab handles (horde goblins auto-man spare handles) and swing on a rhythm; each hit chunks the gate's structural HP. Loud — guards converge. The brute-force fantasy. |
| **Catapult** | 6 bundles | A goblin climbs in, aims an arc over the wall, launches (scream included), lands with a roll **inside**. Player-transport only — the horde can't use it. The comedy option and the express lane for an inside-job gate opening. |

### A.3 Digging — slow, quiet, reusable

Channel **Dig** (hold E) at the palisade base to burrow under. Slow solo; each additional digger (players — horde-assist is stretch) speeds it up. Produces a **tunnel both teams' goblins can use all raid, both directions** — a permanent private door, including for extraction under pursuit. Quiet, but a guard walking past mid-dig will notice the flying dirt.

### A.4 The inside job — open the gate

Any goblin already inside (gap, tunnel, or catapult) can lift the gate's crossbar (hold E, ~3s, interruptible). The gate swings wide for the whole horde. Big score bonus (**"Inside Job" +50** — *now a far-future achievement, decision 13*), and the single best expression of the game's fantasy: one sneaky goblin turning the humans' front door into a goblin superhighway.

**Design guarantee:** the gap and the gate always exist, so a breach is never resource-gated; wood/dig/catapult are *better, louder, or funnier* options layered on top.

---

*Companion documents: `race-design-goblins.md`, `race-design-humans.md`, `race-design-elves.md`, `race-design-dwarves.md`, `claude/goblin-siege-hamlet-pivot-decisions.md` (the 2026-07-20 pivot record — now merged; this document again governs), `claude/goblin-siege-gdd-assignment1-draft.md` (Assignment #01 GDD), `claude/goblin-siege-bark-sheet.md` (approved; extension pass pending — open item 9), `claude/goblin-siege-ue5.8-tech-design-doc.md` (needs a follow-up pass to add systems 5–20 of §12.1), `claude/goblin-siege-build-log-and-mcp-plan.md`, `claude/goblin-siege-art-style-guide.md`, `claude/goblin-siege-asset-review-playbook.md`, `claude/goblin-siege-hamlet-generator.py.md` (working generator prototype), plus the studio's standing **decision queue**, **agent cycle log**, and **agent role specifications** (agent pack). Playable combat reference: `goblin-siege-prototype_3.html`.*
