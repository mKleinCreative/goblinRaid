# GOBLIN SIEGE — Game Design Document (repo export)

*Exported 2026-08-04 from the canonical Claude-project doc `goblin-siege-design-document.md`
("The Tutorial Island" revision) for on-disk consumption by the Code Architect. The Claude
project holds the canonical copy; re-export when it revs. Sections outside the agent's parse
surface are preserved but may be summarized — §12 (systems & build plan) is verbatim.*

## 1. Vision

Goblin Siege is a third-person co-operative raid game in which you play the monsters. You and
your goblin warband slip out of your Overlord's lair, creep through the forest, descend on a
human settlement — in this slice a tiny unwalled village — burn it down, rob it blind, and
scurry home before the humans get organized. It plays like a satire: the humans are pompous,
greedy, and a little bit stupid.

**The slice is the game's tutorial, framed in fiction (decision 37).** The clan is auditioning
for His Eternal Darkness's crusade; the score screen is his verdict. Win condition unchanged
(decision 38): **burn all three objectives, then extract** — as of Q-32 (2026-07-31), "three
objectives" means three TYPES: at least one windmill, at least one field, and the market.

Design pillars: weighty readable combat · fire and destruction as a language · quiet in, loud
out · one place learned by heart (single hand-authored map, decision 39) · a horde at your back
(ground-bound, vault-only per 41-a; the player out-traverses his own horde by design) ·
pressure, not safety (30-minute raids, banking loop) · satire, always.

## 2. Core gameplay loop

Pick class in the lair → materialize at the runic site (three burn targets named on arrival) →
approach through patrols → case the village → work quietly (crouch, takedowns, coin toss, loot)
→ go loud on purpose (first torch, horn, horde) → burn the remaining objective types → portal
opens → get out, tossing loot into the portal → bank and hear the verdict. 30-minute cap; at
0:00 the portal collapses over 90 seconds; anyone outside is left behind.

## 4. The goblins — classes & weapons

The tutorial ships one class — the **Scout** (canonical name per Q-38, amending decision 36's
"Slasher"): sword ⇄ bow live-swap, 110 HP, fast. Universal kit: torch toss (Q, aimable — the
mill demands a window throw), the Horn (G), crouch toggle, dodge roll (0.22s i-frames),
traversal (vault/mantle/climb — player-only), **interact (E/hold-E): loot / takedown /
foul-well / extract + the carry state** (loot sacks, live animals), 5 lives.

## 5. The Horn & the horde

Cap 10 active per player; reserve 2× cap; pool debits on horn-spawn only (decision 40);
follow & frenzy + one point command (swarm / smash / courier). Horde goblins: ~40 HP, no lives,
no dodge; steer around ambient fire giggling (40); ground-bound but **can vault** (41-a) —
never climb or mantle; unreachable goblins idle → re-horn free → trudge home to the reserve.
`Stranded` BT state; Panic-Stranded when fire blocks every exit including vault points.

## 6. The level — the tutorial island

One map: `Tutorial_Island` (`/Game/Maps/L_Tutorial_Island`). Dusk, fixed. The runic site
(spawn/respawn/extraction; portal opens on objective completion; pouch auto-banks on circle
entry; 90s collapse at 0:00) sits in the mountains per Q-28; the island keeps a dense forest
frame ("sea of trees"); woodland is green and unburnable ("it's magic").

Objective inventory (T-09, confirmed): **4 windmills, 3 fields, 1 market** across four zones.
Win per Q-32: one of each TYPE; siblings demote Required → Optional on first same-type
completion (market exempt — only one exists). Burn types: mill = window-throw ignition →
interior build → grain-dust detonation → state-swap (sails spin while burning, stop at
Detonated — 2026-08-01 mandate); field = grid cells, ≥70% (placeholder), doused cells
re-ignitable, fire may jump to adjacent flammables; market = stall-based burn via
`BP_GS_TestStall`-class flammables. Granary deferred out of the tutorial set (Q-29).

Population (data rows): 4 posted guards (1 watchtower / 1 barracks door / 2 wandering),
patrols of 2 militia + 1 archer with torches drawn every 5–7 min, 6–10 civilians, penned
livestock (chickens/sheep/pigs — kill petty 2/5/8, steal big 10/25/40).

## 7. The approach — stealth

One-number confirm model (~1.5s sighting confirm) plus the lean five: **noise** (audible radius
per action, crouched 0.45–0.6×; thrown chicken = deliberate noise lure), **crouch toggle**
(0.75× detection), **silent takedown** (hold-E, 120° behind-cone, 1.2s channel, quiet slump,
+5 deeds), **corpse-suspicion** (1.5s confirm; does not void First Spark), **coin toss**
(debits pouch; greedy humans scramble). Quiet in, loud out; "First Spark Unseen" +40.
Rejected for the slice: light/shadow detection, disguises, body carrying, ghost-run win.

## 8. The defenders & the alarm

Phases QUIET → SUSPICIOUS → RAID → RAZED/RELIEF on one persistent meter. Fire promotes to RAID
when a human sees it, ~10s unseen fuse as backstop. Bell signals Highpurse Keep: castle
reinforcement squads from the far edge on a timer, RAID onward. Humans: Militia 30 HP, Archer
20 HP (aim-line telegraph), Knight 75 HP/armor 6 (late tiers), civilians 10 HP with
disbelief → panic; bucket brigade effective at field edges only, responds past 25% burned;
foul the well to stop it. No alarm reducers — goblins don't de-escalate; they leave.

## 9. Lives, death & extraction

5 lives; respawn at the runic site (~4s). Two-kind score: **deeds** (bank on exit, ×1.5 return
bonus, wipe keeps 25%) and **loot** (physical; banks instantly at the circle — couriers, pouch
auto-bank, portal toss-in). Courier command sends the nearest horde goblin home with cargo
(then to reserve). Livestock are loot on the hoof; chickens weightless (carry two, fight
one-handed). Pouch drops on death as a recoverable sack.

## 10. Scoring

Score-only economy for the prototype. Objectives 100 each completion-only +150 all-three;
loot 5–25; strongbox 75; livestock steal 40/25/10, kill 8/5/2; guards 10/15/25; silent takedown
+5; civilian 5; watchman pre-bell +25; First Spark +40; well fouled +30; barracks +40; patrol
wiped no-runner +15; extraction ×1.5 deeds; +20 per unused life. End screen: itemized tally,
Overlord verdict whisper, letter grade, personal best.

## 11. Asset manifest

(Summarized — see canonical doc. Notables: material-hardness language table; granary is a
custom silo-bellied hero asset with Barn stand-in; windmill window sockets for the aimed throw;
gore is cartoony with an intensity scalar; barks text-only; His Eternal Darkness =
layered whispers + subtitles.)

## 12. Systems list & build plan

### 12.1 Systems inventory

| # | System | Status |
|---|---|---|
| 1 | GAS combat pipeline, damage/armor/race matrix | Scaffolded (compiles) |
| 2 | 4 weapon kits + torch toss | Designed + browser-proven; UE port. Torch throw gains the aimed window-throw (mill) |
| 3 | Fire/flammable/spread + Chaos fracture | Scaffolded; **three objective burn types**: granary collapse, **spreading field fire**, **mill chain** |
| 4 | Alarm meter + phases (QUIET/SUSPICIOUS/RAID/RAZED) | Scaffolded; phase rework + **fire-seen promotion w/ ~10s unseen fuse** + startled-flock stimuli |
| 5 | Third-person camera & control | **Built** — third-person rig, soft-lock targeting, crouch, sprint/stamina, dodge |
| 5b | **Traversal — vault / mantle / climb** | **New to this inventory (2026-07-28)** — was never on the systems list despite being built. Vault and mantle done; free climb in flight. Player-only (decision 41) |
| 6 | Horn & horde — summon, pool, follow/frenzy/point | Biggest remaining AI system. + `Stranded` state, fire avoidance, spawn-only pool debit (decisions 40–41) |
| 7 | **The stealth five** on the one-number confirm model | Noise radii (data), crouch toggle, takedown behind-cone, corpse-suspicion, coin-toss lure — plus the existing doze/runner/investigate set |
| 8 | Interact framework — hold-E channels | Slice verbs: **loot / takedown / foul-well / extract** + carry state. **Blocking five other systems; nothing of it exists yet** |
| 9 | Breach set | **DEFERRED — tier-2 (Appendix A)** |
| 10 | Wood economy | **DEFERRED — tier-2 (Appendix A)** |
| 11 | Civilians **+ livestock** | Partially scaffolded (firefighting BT task exists) — routines, disbelief→panic, bucket brigade, well interaction; livestock archetypes + flee branch |
| 12 | Score system — deeds/loot two-kind tally, banking multiplier, end screen | + livestock/market-gold/takedown/First-Spark rows; objective points completion-only |
| 13 | Runic site — spawn/respawn + objective-gated portal, toss-in/staging, 90s collapse | + **pouch auto-bank on circle entry** |
| 14 | Lives/respawn on PlayerState | Scaffolded; respawn at runic site |
| 15 | Barks + His Eternal Darkness whispers/subtitles | Cheap — + disbelief/livestock/stealth/horde bark families |
| 16 | Gore/gib system | Small; + feather-poof variant |
| 17 | Loot couriers — sacks **+ livestock** as cargo, point-to-courier BT | Reuses carry state + point command; chase-catch-carry for animals |
| 18 | Raid clock — 30-min cap, 0:00 → 90s collapse, left-behind rule | Small |
| 19 | Patrol director — 5–7-min cadence; castle reinforcements post-alarm | Extends system 4's spawn logic |
| 20 | ~~Settlement generator~~ → **The authored tutorial map** | **Generator SHELVED 2026-07-28 (decision 39).** Replaced by standing up `Tutorial_Island`: move into `Content/Maps`, dress for gameplay, place the three fixed objectives + runic site + patrol routes + guard posts, bake navmesh once, walk-and-fix pass. Generator spec, doctrine and prototype scripts preserved for tier 2+ |

### 12.2 Build plan — core-first blocks *(revised 2026-07-28)*

| Block | Contents | Exit test |
|---|---|---|
| **A** · Unblock the verbs | Weapon data asset + GEs, GameMode swapped into the level, **the interact framework**, health bound to GAS | Hold E on an interactable as the real Scout; the channel fills and aborts correctly |
| **B** · Someone to fight | Human race/archetype data, `BT_Militia`, Scout sword ability, attack-clip trimming, **death clips retargeted**, noise foundations | Kill a militia with the sword and watch it die properly |
| **C** · Fire and the burn structure | Torch/fire content, the three burn types, alarm phases, clock skeleton | Torch a granary; it burns, fractures, and ticks the alarm |
| **D** · The horde | Subsystem, horde goblin, spawn markers, BT (incl. `Stranded`), horn + point command, **the tuning table**, horde perf pass | 10 goblins + player brawling guards at frame rate |
| **E** · The loop closes | Map stood up, runic site + portal states, respawn retarget, 30-min clock, walk-and-fix pass | **First full loop playable, start to bank** |
| **F** · Stealth complete | The stealth five, patrol director, watchman + bell + doze, runners | The quiet half plays end to end |
| **G** · The living village + score | Civilians, livestock, bucket brigade, well fouling, couriers + pouch, score + end screen, gore, art pass | A raid scores itself correctly |
| **H** · Feel & funny | Tuning, barks, hero audio, balance, bug triage | **Playtest build** |

### 12.3 Honest scoping — the cut order

Cut order if late blocks slip: self-looting civilians → sheep & chickens → coin toss →
corpse-suspicion → takedowns.

**Never cut:** the horn/horde, the three-objective burn structure, the crouch-and-confirm
stealth core, the runic-site banking loop, the score screen.

Team reality: solo dev (Michael) + AI-assist. Post-slice roadmap: economy → co-op → the ladder
(generator un-shelved) → the assassination mission.

## 13. Decisions ledger (pointers)

Canonical ledger lives in the Claude-project doc §13 (decisions 1–41 plus Q-28…Q-38 rulings in
`claude/goblin-siege-decision-queue.md`). Load-bearing for code: Scout naming (Q-38) · win by
type-flags `Objective.Burn.Mill/.Field/.Market` with Required→Optional demotion (Q-32/Q-37) ·
horde vault-only (41-a) · pool debit on spawn only (40) · burn-visual mandate (2026-08-01) ·
firebreak Unburnt→Doused allowed (Q-35) · replicate cheap root state only (Q-36).
