# GOBLIN SIEGE — Game Design Document

**v1.0 — LOCKED 2026-08-19.**

*This file is **canonical**. It supersedes the repo-root `goblin-siege-design-document.md`, which is
frozen as the artifact submitted for Assignment #02 and must not be edited to record a design change.
This document was previously a derived export of that file; as of 2026-08-19 the direction is
reversed. It lives at `GoblinSiege 5.8/docs/` because the Code Architect probes
`project_root/docs/goblin-siege-gdd.md` first (`Tools/CodeArchitect/ca/config.py:48-54`) — the move
cost no code change and took the design document out of a tool's private folder.*

> **The change rule.** A change to this document needs an **AgentQueue ticket** and a dated entry in
> `docs/decisions-ledger.md`. The ticket id goes in the revision row below. No silent rewrites — the
> project has twice paid for a document that drifted from the build with nobody able to say when or
> why.

> **Status honesty.** §12.1 grades what is *built*, not what is *designed*, on four rungs —
> **BUILT / WIRED / SKELETON / MISSING** — where BUILT means a human watched it work. A class that
> compiles is not a system that runs, and this project has repeatedly paid for conflating the two.
> The word "Scaffolded" was retired precisely because it let the two pass as one.

> **The parser contract.** `Tools/CodeArchitect/ca/gdd.py` reads this file structurally. The rules
> below were established **by testing the regex against mutations**, not by reading it — a first pass
> at this note asserted that a fourth column drops a row, and that turned out to be false.
>
> §12.1's table is `| id | System | Status |`, ids `1`–`20` plus `5b`; all 21 are pinned by
> `features.json` and a missing one is reported as drift. A row is **silently dropped** by: a
> non-numeric id (`14a` fails, `5b` is the one legal suffix), an id with a prefix (`#14`), a missing
> trailing pipe, only two columns, or **any leading whitespace before the first pipe**. A **fourth
> column is worse than a drop** — the row still parses and the extra cell is swallowed into the
> status string, so the table looks fine and the status quietly lies. `Tools/check_gdd.py` catches
> both cases.
>
> §12.2's block letters must be **bold and within A–H** — a block `I` is dropped without a word.
> §12.3's never-cut line must stay **one unwrapped line**: the regex captures to end-of-line only,
> and a wrapped line silently truncated that list from five items to three until 2026-08-19. Do not
> restate that heading's literal text anywhere above §12.3 either — the regex takes the *first* match
> in the file, so a mention in prose hijacks the parse (which it did, once, while this note was being
> written).
>
> The status *text* is free prose: `gdd.py` treats it as a claim, never as truth.

## Revision history

| Version | Date | Ticket | What changed |
|---|---|---|---|
| v1.0 | 2026-08-19 | #198 | This file becomes canonical and moves to `docs/`. Reconciled against the live tree: grapple written in as a core verb, audio un-cut for world SFX, climb re-graded as Blueprint-driven, the `L_CombatArena` / `L_Tutorial_Island` divergence stated, §12.1 re-graded, §12.4 scope freeze added, §13 pointed at a real ledger. 23 rulings taken — see `docs/decisions-ledger.md`. |
| v1.1 | 2026-08-21 | #296 | **World corruption added** — the land visibly turns as you raid. One global monotonic 0..1 scalar drives sky, fog, sun, grade, world materials, VFX and ambience. Added to the §12.4 IN column; §1 pillar reworded; the wayfinding consequence amended, since corruption is now what "the environment does the leading" actually means. Rulings 40–45; ruling 62 (2026-08-24) adds that **civilian kills corrupt more than soldiers'**. Re-filed under #296 — #252 was abandoned without its edits being reverted. |
| v1.2 | 2026-08-24 | #277 | **Finite arrows added** — the player carries a quiver that empties, refilled by walking over a bundle or a dead archer. Torches stay infinite and AI archers never run dry; weapons are not lootable. Added to the §12.4 IN column. Rulings 46-52. |
| v1.3 | 2026-08-24 | #285 | **Sneaking cut from the demo.** The crouch-and-confirm stealth core, noise, takedowns, corpse-suspicion and the coin toss all leave the slice; the demo is a straight raid. **Removes the stealth core from the §12.4 "Never cut" line** - the only entry ever taken off it - and notes why in place. The bucket brigade stays deferred. Rulings 56-58. |
| — | 2026-08-14 | #158 | Re-exported 168 → 272 lines; four-rung grading replaced "Scaffolded"; control map corrected; five live defects recorded |
| — | 2026-08-04 | — | First export, describing what was *designed* rather than what was built |

## 1. Vision

Goblin Siege is a third-person raid game in which **you play the monster — singular**. You are a
goblin. Your Overlord — His Eternal Darkness, an unseen voice of layered whispers and subtitles —
sends you through a runic portal into a dusk-lit forest outside a tiny unwalled human hamlet in the
county of Groatsworth. Burn it, rob it blind, and scurry home before the humans get organized. It
plays as satire: the humans are pompous, greedy, and a little bit stupid. *(Co-op was in the
2026-08-04 export; it is post-slice roadmap, not this slice.)*

**The slice is the game's tutorial, framed in fiction (decision 37).** The clan is auditioning for
His Eternal Darkness's crusade; the score screen is his verdict.

**Win condition (revised 2026-08-14):** destroy all three required objectives, then extract. The
three are **Market · Statue · Windmill** — one of each in the tutorial hamlet. The **wheat fields
and the houses are optional objectives**: worth points, not gating extraction. *This supersedes
"granary / field / mill" in the pre-2026-08-14 canonical doc. The granary was already deferred out
of the tutorial set by Q-29 in July and never existed in content; the Statue replaces it, and the
field moves to optional. The code was already built to a Field/Windmill/Market roster, so this
change is smaller in code than it looks in prose — it swaps one type, not three.*

Design pillars: weighty readable combat · fire and destruction as a language, written on the land itself (40) · quiet in, loud out ·
one place learned by heart · a horde at your back (ground-bound, vault-only per 41-a; the player
out-traverses his own horde by design) · pressure, not safety (30-minute raids, banking loop) ·
satire, always.

## 2. Core gameplay loop

Materialize at the runic site (three raid targets named on arrival) → approach through patrols →
case the hamlet → work quietly (crouch, takedowns, coin toss, loot) → go loud on purpose (first
torch, horn, horde) → destroy the remaining objective types → portal opens → get out, tossing loot
into the portal → bank and hear the verdict. 30-minute cap; at 0:00 the portal collapses over 90
seconds; anyone outside is left behind.

## 4. The goblins — controls & kit

The tutorial ships one class — the **Scout**: sword ⇄ bow live-swap, 110 HP, fast.

**Control map (corrected 2026-08-14 — the 2026-08-04 export predated three remaps).** Tickets
#039–#041 took `Q` for the weapon wheel and retired `IA_ThrowTorch`; #058/#061 moved interact to
`F` and gave `E` to traversal; #122 moved block to right mouse and retired `G`.

| Verb | Binding |
|---|---|
| Weapon wheel | `Q` (radial; slot selection drives what ATTACK does) |
| Attack | Left mouse, routed by equipped slot |
| Block | Right mouse (sword out) |
| Aim | Right mouse (bow / torch) |
| Interact — loot / takedown / foul-well / extract / **carry** | `F`, hold-to-channel |
| Traversal — vault / mantle / climb | `E` (hold) — player-only |
| Grapple — throw, anchor, walk away to haul | Weapon-wheel slot (`EGSWeaponSlot::Grapple`), fired by ATTACK |
| Horde orders — Attack / Hold / Loot / Follow | `R` (radial order wheel, `IA_HordeOrder`) |
| Horn (summon the horde) | Middle mouse |
| Jump | Space · **Dodge roll** LeftAlt (0.22s i-frames) · **Crouch** toggle |

5 lives. **Carry state** (loot sacks, live animals) rides on the interact channel.

**The grapple is a core Scout verb (ruling 12, 2026-08-19).** `GSGA_GrappleThrow` +
`UGSGrappleHaulComponent` implement throw → anchor → *walk away to build rope tension*, driving
`UGSTopplableComponent`. It is the fourth wheel slot beside torch, sword and bow, and it is **how the
statue comes down** (§6) — the mechanical answer to "the one target that doesn't burn". It shipped
across tickets #173–#177, #183 and #193 and had appeared in no design document until this revision.

**Traversal is Blueprint-driven, not dead (ruling 11).** `UGSClimbLibrary` has **zero C++ callers**,
which reads like a corpse and is not one: the climb lives in the player Blueprint calling that
library, and `GSPlayerCharacter.h:325`'s "STRICTLY a jump: never a vault, never a mantle, never a
climb" disclaims the *jump input*, not the feature. **Vault and mantle genuinely do not exist** —
which leaves decision 41-a (horde goblins are ground-bound but *can vault*) depending on a verb the
project has never had.

> **Defect status (updated 2026-08-19):** the 2026-08-14 entry here read *"`InteractAction` is unset
> on the CDO … interact has never run"*, which was true for eight days and is now **believed fixed** —
> tickets #161, #163, #172, #181 and #187 landed the assignment, the first two interactables, the
> channel ring and *"interact prompt `OnFocusChanged` finally binds"*. **It is believed fixed, not
> observed fixed**, and §12.2 Block A's first action is to watch it rather than trust this line. The
> original bug is the reason the "symbols exist ⇒ done" rule was banned; the inverse — "the doc says
> missing ⇒ rebuild it" — would cost exactly as much.

## 5. The Horn & the horde

Follow & frenzy plus a point-command wheel (Attack / Hold / Loot / Follow, #141). Horde goblins:
~40 HP, no lives, no dodge; steer around ambient fire giggling (40); ground-bound but **can vault**
(41-a) — never climb or mantle; unreachable goblins idle → re-horn free → trudge home to the
reserve. `Stranded` BT state; Panic-Stranded when fire blocks every exit including vault points.

**The horn is tap-or-hold — settled 2026-08-21 (ruling 37).** A tap summons **exactly one** goblin;
holding middle mouse streams them out of the Warren **one at a time** until the squad is full, the
button comes up, or the reserve is dry. This **supersedes decision 9** (`SummonsPerBlast` a fixed 4):
the "a player counting his pool should not have to guess" objection is answered better by the new
shape than by the old number, because you get one per press and holding shows you each arrival.
`SummonsPerBlast` survives only as the batch size for `GS.Horde.SpawnTest`.

**Pool size is 20 — settled 2026-08-19 (ruling 23).** The flag this section used to carry is deleted:
there was never a real disagreement. `UGSHordeSubsystem` has `ActiveCap = 10` and
`ReserveMultiplier = 2`, so `GetRaidPoolSize()` derives **20** and never carries a literal; §2.8 said
20 all along. Only the 2026-08-04 export text dissented. **20 goblins in the raid pool, 10 active at
once, per player** — one notch above the Groatsworth garrison of 15, which is the point.

**Order-wheel status:** Attack and Follow work — `GSHordeSubsystem` routes an Attack order's subject
into `TargetActor` and clears Follow onto the follow branch. **Hold, Loot and Smash are inert:**
`BTTask_PickUpCargo`, `BTTask_DeliverCargo` and `BTTask_SmashOrderTarget` compile but appear in none
of the project's behaviour trees, and `BB_HordeGoblin` lacks the four `Order*` keys the controller
writes (#154). Both halves are needed; the keys alone will not do it.

## 6. The level — the hamlet

**The timebox was won — the map is `L_Groatsworth` (ruling 3, 2026-08-19).** On 2026-08-18 the
generator ran eight seeds through the evaluator and **all eight passed**, on the measured kit
(`synthetic_kit: false`). The bounded attempt succeeded, so the generator authors Groatsworth and the
hand-authored blockout is retired unused. *Per-raid* generation remains post-slice and funding-gated.
`apply_in_editor.py` has **never been run**, so placement is untested and the first attempt is a
debugging session, not a delivery.

> **The roster trap, found 2026-08-19.** The passing plan for seed 1 is **statue / windmill / statue**
> — two statues and **no market**. That is legal: `check_objective_mix` enforces §1's *generic* rule
> (exactly three required, never more than two of a kind) and `generate.py:493` rolls a duplicate 35%
> of the time on purpose. But the **tutorial** hamlet has a stricter rule — one of each, so a first-time
> player meets all three kinds of destruction — and the evaluator had never been told it. Downstream it
> is worse: `UGSRaidDirector` seeds `RequiredTypes` from what is *placed* and demotes siblings, so a
> two-statue hamlet ships a **two-type win condition** and the player never meets the market, which is
> also the loot concentration and the strongbox spawn. **Fix (ruling 6): a `--tutorial` fixed roster
> plus a tutorial rule in the evaluator** — not a hand-picked lucky seed, which would leave the rule
> unenforced and free to break on the next regeneration. This is the same shape as the granary the
> evaluator kept checking after the GDD dropped it.

*The superseded 2026-08-14 ruling, kept for the trail:* The canonical doc's
2026-07-23 hand-authored-only ruling is superseded by a **timebox**: the generator gets a bounded
attempt at authoring one Groatsworth, and a hand-authored blockout is the fallback. The bar is a
walkable hamlet — three module types placed, roads connecting them, no objective sited somewhere
unreachable — **by end of week 2**, or the blockout wins and the generator returns to post-slice
work. *Per-raid* generation remains post-slice and funding-gated either way. The cover-placement
rule (broken sightlines guaranteed between treeline and every objective) is the sharpest test of
whether the generator is ready, because a generated layout must enforce it algorithmically where a
hand-placed one can be reviewed.

**What exists today is not that map.** `Content/Maps/L_Tutorial_Island` is the Dreamscape Farmlands
marketplace demo island — ~9,100 actors, ~500m × 880m, 67 harvested house objectives, and ~176 MB of
LFS traffic on every save, which is the recorded reason arrival markers were never placed in it. It
is the only level where the raid loop has ever completed end to end, so it stays as the **systems
regression map**. `L_Groatsworth` is the shipping tutorial map and does not exist yet.

> **The two maps have diverged, and this is the single most under-reported fact in the project
> (2026-08-19).** A scan of the two packages: `L_Tutorial_Island` holds the **2026-08-05 world** — 67
> `GSBuildingObjective`, 2 `GSFieldFireObjective`, 1 market, 1 mill, 1 runic site, 3 defenders, a
> `PlayerStart` and `GEN_NavBounds_Village`. It contains **zero** raid markers, horde arrival points,
> interactables, loot, breakables, grapple anchors or the topplable statue. **Every system shipped
> since ~2026-08-07 exists only in `Content/Maps/Test/L_CombatArena`**, whose actors are literally
> labelled `BP_*_TEST`. Its objective roster is also one revision behind — Mill / Market / **Field**,
> not the required trio. So "the tutorial map" and "the game" are currently two different codebases'
> worth of content, and any claim that a system works "in the tutorial" is false by default.

> **The navmesh is the recorded trap.** `GEN_NavBounds_Village` is **4,000 × 4,000 uu** — an 80 m box
> for a whole village, and the reason defenders freeze the moment they chase the player off it. The
> generator's site is **24,000 × 24,000 uu** with a treeline radius of 9,000. Bake `L_Groatsworth`'s
> navmesh to the site. Settled: **widen, never switch to invokers** — invokers give no navmesh where
> no invoker stands, which breaks patrols, and patrols are now IN.

The runic site (spawn/respawn/extraction; portal opens on objective completion; pouch auto-banks on
circle entry; 90s collapse at 0:00) sits in the mountains per Q-28. Dense forest frame; woodland is
green and unburnable ("it's magic").

**The Warren — BUILT (ruling 18, 2026-08-19; placed-not-planted, ruling 36, 2026-08-21).**
`N_ChaosRune2` is the Warren mouth, and it does **three** jobs: it is where summoned goblins
**arrive from** (satisfying the settled no-pop-in mandate — they emerge from a hole, they do not fade
in, and the treeline arrival of the old canonical §2.5 stays superseded), it is the **respawn point**
once a level has one, and it is the **turn-in point** for cargo — livestock and whatever comes out of
crates, chests and barrels. `AGSWarren` + `BP_GS_Warren` implement all three.

**There is no planting channel and no digger (ruling 36).** The Warren is placed by a designer and
open from `BeginPlay` — Michael: *"just a magic spot where they can summon goblins and drop off
loot"*. `UGSHordeSubsystem::NotifyGoblinSpentOnWarren()` survives that cut **uncalled and must not be
deleted**: it is the fourth pool exit (spent, but not dead — no death path, no credit back), and the
accounting is correct the day a digger ever lands. Banking rules in §9.

**Objective structure.** Required: one Market, one Statue, one Windmill. Optional: wheat fields,
houses. Siblings demote Required → Optional on first same-type completion. Burn/destroy behaviour by
type:

- **Windmill** — two stages, both visible. Stage 1 **Ablaze**: any torch on the frame catches the
  sails, thickens the smoke, posts a HUD line and an Overlord bark. Stage 2 **Detonation**: a torch
  thrown **through a window** catches the grain dust; it detonates and state-swaps to a
  smashed-but-recognizable silhouette. *Neither stage exists in code today — the shipped enum is
  `Intact/Smouldering/Detonated` and exterior fire is a deliberate no-op that broadcasts a refusal,
  which is the pre-revision design.* **Ruling 7 (2026-08-19): build Ablaze; the design is right.**
  Exterior fire must catch the sails, thicken the smoke, post the HUD line and fire the bark hook that
  already sits unused at `GSMillObjective.h:62`. The point of the two-stage revision is that a torch
  is **never silently wasted** — the player sees what they bought and what is still owed.
- **Market** — stall-based burn via flammable stall actors. The map's loot concentration; the
  strongbox spawns here.
- **Statue** — the propaganda monument to the king who supposedly wiped the goblins out (§2.10).
  **It does not burn: it is brought down** — grapple it, anchor it, and walk away until the rope pulls
  it over (§4). Lands on the existing geometry-collection destructible objective, which is what
  toppling stone wants and what burning never did. Largely real as of 2026-08-18:
  `AGSObjective_ToppleStatue`, `UGSTopplableComponent`, a 44-chunk `GC_Statue_Warrior` fracture,
  `UGSGrappleHaulComponent`, and `OnToppled` feeding the deed tally (#192/#193/#195/#196).
  **Ruling 8 (2026-08-19): remove the burn gate.** `AGSDestructibleObjective` still gates the fracture
  behind burn-complete — flagged in its own header — which means the statue currently has to be set on
  fire before it can be pulled down, contradicting the one line the whole objective exists to deliver.
  Topple is the only path.
- **Fields (optional)** — grid cells, spread row to row, doused cells re-ignitable, fire may jump to
  adjacent flammables.

Population: 4 posted guards (1 watchtower / 1 barracks door / 2 wandering), patrols of 2 militia +
1 archer every 5–7 min, 6–10 civilians, penned livestock (chickens/sheep/pigs). Garrison /
raid-response pool 15.

## 7. The approach — stealth

One-number confirm model (~1.5s sighting confirm) plus the lean five: **noise** (audible radius per
action, crouched 0.45–0.6×; thrown chicken = deliberate lure), **crouch toggle** (0.75× detection),
**silent takedown** (hold-interact, 120° behind-cone, 1.2s channel, quiet slump, +5 deeds),
**corpse-suspicion** (1.5s confirm; does not void First Spark), **coin toss** (debits pouch; greedy
humans scramble). Quiet in, loud out; "First Spark Unseen" +40. **Bind/capture is CUT from this
slice (2026-08-14)** — the civilian pair now offers the takedown choice only. Also rejected for the
slice: light/shadow detection, disguises, body carrying, ghost-run win.

> **Live defect:** `UGSSightPerceptionComponent` — the only caller of `ReportSightingConfirmed` — is
> on **zero actors**. No guard in this project can currently see anything for stealth purposes, so
> SUSPICIOUS is unreachable. The detection *model* is built and correct
> (`ConfirmHoldSeconds = 1.5`, `CrouchRangeMultiplier = 0.55`, `FullCoverRangeMultiplier = 0.3`, the
> 60° peripheral half-angle and the watchman's cone droop all live in `Stealth/GSStealthTypes.h`);
> what is missing is a **consumer**, not a design.

**The five, graded honestly (2026-08-19).** "MISSING" was too blunt — two of the five are built:

| System | Status |
|---|---|
| ~1.5 s sighting confirm | **Built** — `FGSDetectionTuning::ConfirmHoldSeconds`, de-duped into one soft signal by `UGSStealthSubsystem` |
| Crouch detection | **Built** — stance multipliers, cone, cover and doze maths in `GSSightPerceptionComponent.cpp` |
| **Noise** | **Absent** — no `MakeNoise`, no `ReportNoise`, no radius anywhere in Source; only the `EGSAlarmSource::CombatNoise` enum label |
| Silent takedown | **Absent** — the `Interact.Takedown` tag exists and nothing implements it |
| Corpse-suspicion | **Absent** — corpses are handled only as *non-targets* by the acquire service; a body raises no signal |
| Coin toss | **Absent** — named in a comment as a thing that *should* call `SnapAwake`, and nowhere else |

**Noise is IN (ruling 9, 2026-08-19), and it is a major next item.** A sound pack is plugged in
(`Content/NaPH_RPG_Fantasy_Sounds_Bundle`), which removes the excuse the 2026-08-14 audio cut rested
on. Noise is what makes crouch a decision rather than a speed penalty, and the horn, the digging
Warren and a startled flock all cash out as noise events. A guard investigating a sound the player
cannot hear is a worse tutorial than one they can.

## 8. The defenders & the alarm

**Scope, set 2026-08-19:** civilians **IN** (13) · patrols **IN** (14) · the finite raid-response pool
and Highpurse Keep **IN** (19) · the watchtower, the bell and the well **DEFERRED to the full game**
(16). See §12.4.

Phases QUIET → SUSPICIOUS → RAID → RAZED/RELIEF on one persistent meter. Fire promotes to RAID when
a human sees it, ~10s unseen fuse as backstop. Bell signals Highpurse Keep: castle reinforcement
squads from the far edge on a timer, RAID onward. Humans: Militia 30 HP, Archer 20 HP (aim-line
telegraph), Knight 75 HP / armor 6, civilians 10 HP with disbelief → panic; bucket brigade effective
at field edges only, responds past 25% burned; foul the well to stop it. No alarm reducers — goblins
don't de-escalate; they leave.

> **Live defects and scope, 2026-08-19.**
>
> - **The alarm machine is built; the reporter is not.** `RequestAlarmPhase`, `SuspicionDecaySeconds`,
>   the unseen-fire fuse and the Razed transition all live in `Core/GSGameState.cpp`, and the horn
>   promotes straight to RAID. What is missing is anything that *files a signal* — see §7.
> - **The escalation rule is wrong.** A second soft signal arriving during SUSPICIOUS should promote
>   to RAID; it currently refreshes the Suspicious decay instead, so the town can never corroborate.
> - **The finite pool does not exist (IN, ruling 19).** "Highpurse" returns **zero hits** in Source.
>   What ships instead is `EGSReinforcementTier` Tier0–3 making spawners ~10% faster per tier — an
>   escalating tap with no floor, where the design promises a garrison of **15** you can actually
>   exhaust: *the hamlet can't stop you; the county can, exactly once, at this size.* Building the
>   pool is what makes killing your way through it a strategy rather than a treadmill.
> - **Watchtower, bell and well are DEFERRED (ruling 16).** No watchtower actor, no bell (one comment,
>   zero code), no well actor — `Interact.FoulWell` is a tag with nothing behind it. The dozing-watchman
>   *perception* model survives in `FGSDozeTuning`. **This removes three lines from §10** — watchman
>   silenced +25, well fouled +30, and the bucket-brigade counterplay — which come out of the table
>   rather than sitting there unwired.
> - **Civilians are MISSING, not skeletal (IN, ruling 13).** There is no civilian actor, class or
>   component — only the `Marker.CivilianAnchor` placement tag. ACF's `UACFAIRoutineComponent` covers
>   most of the routine layer once the migration lands.
> - **Patrols: configure, don't build (IN, ruling 14).** No patrol director, no route runner;
>   `GS.Combat.SpawnPatrol` is a clump-spawner. ACF ships `UACFAIPatrolComponent`,
>   `ACFUpdatePatrolBTService` and `BTTask_GoToNextWayPoint`, which retires the planned
>   `UGSPatrolDirector` rather than building it. The **check-in / overdue-patrol soft signal stays
>   ours** — ACF patrols walk waypoints, they do not report to a stealth director.

## 9. Lives, death & extraction

5 lives; respawn at the runic site (~4s). Two-kind score: **deeds** (bank on exit, ×1.5 return bonus,
wipe keeps 25%) and **loot** (physical; banks instantly at the circle — couriers, pouch auto-bank,
portal toss-in). Courier command sends the nearest horde goblin home with cargo. Livestock are loot
on the hoof; chickens weightless (carry two, fight one-handed). Pouch drops on death as a
recoverable sack.

**The Warren banks loot, never deeds (ruling 18).** Once placed it is the respawn point and
a second permanent banking point for loot and cargo — so a raid that ends in a wipe still keeps what
reached it. **Deeds bank only through the real portal**, on purpose: loot gets forgiving because a
struggling raid should walk away with what it actually gathered, but a second place to bank deeds
would erase the reason to ever risk the run home. That tension is the loop.

> **Live defect:** `UGSScoreSubsystem::AddLoot` has **zero callers project-wide** — its own header
> says *"What you carried out. Always 0 today."* The runic site never asks the pawn what it is
> carrying, and `TryExtract` ends the raid on contact, so mid-raid banking has no path. The loot
> half of the two-kind score has never run. **The Warren needs the same seam**, so build it once and
> give it both consumers.
>
> **Second defect, found 2026-08-19: the ×1.5 extraction multiplier does not exist anywhere in
> Source.** Not stubbed, not unwired — absent. The single rule that makes deeds provisional, and the
> reason the run home is a decision at all, has never been implemented.

## 10. Scoring

Score-only economy for the prototype. **Table settled 2026-08-19** — the two TBDs are filled and the
deferred lines are struck rather than left unwired.

Objectives **destroyed** 100 each, completion-only, +150 for all three · **wheat field 50** ·
**house 10** (ruling 21) · loot 5–25 · market gold 5–15 per stall · strongbox 75 · livestock stolen
40/25/10 (pig/sheep/chicken, loot-kind) · livestock killed 8/5/2 (deed, petty) · militia/archer/
knight 10/15/25 · silent takedown +5 · civilian 5 · First Spark Unseen +40 · barracks demolished +40 ·
clean patrol wipe +15 · extraction ×1.5 on deeds · +20 per unused life. End screen: itemized
deeds/loot split, letter grade, Overlord verdict, personal best.

**Why those two numbers.** A wheat field is a real set piece — the most visible arson in the game —
so it earns half a required objective. A house is a torch and a shrug at 10, so burning every one of
them tops out near 300, comfortably under the required trio's 450. That keeps the §1 promise that
optionals never rival the trio, and it is what stops the tally being decided by whoever had spare
torches.

**Struck from the table this revision**, because the systems behind them are deferred or cut:
*watchman silenced +25* and *well fouled +30* (ruling 16 — no watchtower, bell or well) · *civilian
captured 15* and *None Left Behind +40* (bind/capture was cut 2026-08-14). A scoring line with no
event behind it is a promise the end screen cannot keep.

> **Live defect:** exactly one scoring line is wired. Any burn objective awards deeds, so ~30 houses
> pay ~1620 points — dominating a tally whose lines mostly have no event behind them. **Deeds must be
> gated to the required types**; houses pay their 10 and nothing more.

## 11. Asset manifest

(Summarized — see canonical doc. Notables: material-hardness language table; the **statue** is the
village-centre hero asset and replaces the granary's custom silo-bellied mesh, which was never
built; windmill window sockets for the aimed throw; His Eternal Darkness = layered whispers +
subtitles.)

**Audio: corrected 2026-08-19 (ruling 10).** This section said *"the project contains zero audio
assets of any kind"*, and the 2026-08-14 cut list cut audio on that basis. Both are dead:
`Content/NaPH_RPG_Fantasy_Sounds_Bundle` holds **~2,000 assets**. **World and SFX audio is IN** —
footsteps, fire, smashing, the horn — which is what §7's noise system needs to be legible.
**Barks stay text-only**: no VO pipeline, no voiced Overlord, subtitles as designed. So
`content-pipeline/gsstyle.py`'s banned-audio-word rule **narrows rather than lifts** — it must keep
stopping a bark from promising voice, while allowing world SFX to be specified.

**Gore is CUT (ruling 15).** §2.10's gib system with an intensity scalar is not built and is not
being built for this slice — the existing ragdoll death covers it. The T-rating knob goes with it.

**Livestock art is on hand and is better than expected:** complete Chicken / Pig / Sheep rigs under
`Content/DreamscapeSeries/DreamscapeFarmlands/Meshes/Chars/`, with skeletons, physics assets and 34
animation clips including Hurt and Death for all three — which is exactly what the petty-kill line
needs. Pen props too: fence, hinged gate, posts, coop, trough. What is missing is *assembly*, not
sourcing: no AnimBP, no montages, no gameplay actor, and no `CarrySocket` on any skeleton in the
project.

## 12. Systems list & build status

### 12.1 Systems inventory

**Grading: BUILT** = a ticket records a human watching it work · **WIRED** = connected, unobserved ·
**SKELETON** = exists, connected to nothing · **MISSING**.

*Re-graded 2026-08-19 against a fresh read of `Source/GoblinSiege` (179 files, ~38,650 lines). The
2026-08-14 grades were transcribed from a survey and not one had been re-checked since. Ids and the
three-column shape are pinned by `features.json` — see the parser contract at the top.*

| # | System | Status |
|---|---|---|
| 1 | GAS combat pipeline, damage/armor/race matrix | **WIRED** — damage, directional plate, headshots, minimum floor all land; race matchup matrix rows are empty |
| 2 | Weapon kits + torch toss + grapple | **WIRED** — wheel, sword combo, bow, torch and the grapple haul all exist and have been played; the grapple pulled the statue over on 2026-08-18 (#193) |
| 3 | Fire / flammable / spread + Chaos fracture | **WIRED** — the deepest system in the repo (`GSFieldFireObjective.cpp` alone is 1,838 lines: 8-neighbour accumulated-fuel spread, wind, firebreaks, douse and dry-out, plus a 4096px world burn mask). Every recorded ignition was console-driven; nobody has watched a thrown torch light a field |
| 4 | Alarm meter + phases | **WIRED, unreachable** — upgraded from SKELETON: `RequestAlarmPhase`, `SuspicionDecaySeconds`, the unseen-fire fuse and Razed are all real in `Core/GSGameState.cpp`. What is missing is a **reporter**, not the machine — no guard can file a sighting (§7), so SUSPICIOUS is unreachable, and the escalation rule refreshes the decay where it should promote to RAID |
| 5 | Third-person camera & control | **BUILT** |
| 5b | Traversal — climb (Blueprint-driven) | **BUILT** — climb rebuilt over #070–#084 and signed off. `UGSClimbLibrary` has **zero C++ callers by design**: the climb lives in the player Blueprint calling it, so "no callers" is not "dead code". **Vault and mantle do not exist**, which leaves decision 41-a's vault-only horde depending on a verb the project has never had |
| 6 | Horn & horde — summon, pool, follow/frenzy/orders | **WIRED** — summon, follow, frenzy and Attack orders work; Hold/Loot/Smash inert (§5). Pool is 20 (10 active × reserve 2), settled. The behaviour tree was found gutted on 2026-08-14 and repaired (#153) |
| 7 | The stealth five | **SPLIT — two built, three absent.** The ~1.5s confirm and crouch detection are real and correct in `Stealth/`; **noise, takedown, corpse-suspicion and the coin toss have no code at all**. The perception component sits on **zero actors** - which became the argument for ruling 56 (#285), that being the only "Never cut" item never to have run. **The whole sneaking portion is CUT from the demo as of 2026-08-24**; noise is no longer IN and ruling 9 is superseded. The `Stealth/` code stays in the tree |
| 8 | Interact framework — hold-channel | **BELIEVED WIRED, UNOBSERVED** — was "MISSING and blocking" for eight days on the unset `InteractAction`. #161/#163/#172/#181/#187 landed the assignment, the first interactables, the channel ring and the focus prompt — **in `L_CombatArena` only**. Watch it before trusting this row in either direction (Block A) |
| 9 | Breach set | **DEFERRED — tier-2** |
| 10 | Wood economy | **DEFERRED — tier-2** |
| 11 | Civilians + livestock | **MISSING** — downgraded from SKELETON on evidence: there is **no civilian actor, class or component**, only the `Marker.CivilianAnchor` tag. Livestock art is complete (Chicken/Pig/Sheep rigs, 34 clips incl. Hurt and Death) with no gameplay actor. Civilians and patrols are **IN** (rulings 13/14) via ACF's routine and patrol components; **bind stays cut** |
| 12 | Score system — deeds/loot tally, banking, end screen | **WIRED, two holes** — end panel watched (#050/#051). `AddLoot` has **zero callers**, and the **×1.5 extraction multiplier does not exist anywhere in Source**. One line is wired and it is the wrong one: any burn objective pays deeds, so ~30 houses swamp the tally |
| 13 | Runic site — portal, toss-in, 90s collapse | **BUILT** — a full raid completed to `Extracted (3/3 objective types burned)`. The Warren (`AGSWarren`) now ships beside it as arrival mouth, respawn point and the loot bank that gave `AddLoot` its first caller |
| 14 | Lives / respawn | **BUILT** — 5 lives and the OutOfLives loss both driven and watched |
| 15 | Barks + Overlord whispers | **MISSING** — 9 validated prompt rows in `content-pipeline/out/prompts.csv`; the project contains **zero DataTable assets** and no first-encounter tracker. **The banked text still names the granary and must be regenerated** — `gsstyle.py` already does this and was proven on those rows (#191) |
| 16 | Gore / gib system | **CUT (ruling 15)** — ragdoll death covers it; not built, not scheduled |
| 17 | Loot couriers — sacks + livestock as cargo | **WIRED, no cargo** — upgraded: `BTTask_PickUpCargo`, `BTTask_DeliverCargo` and `NotifyCourierDelivered` are all real. What is missing is livestock actors and a `CarrySocket`, which exists on **none of the project's 25 skeletons**, so cargo attaches at the carrier's feet |
| 18 | Raid clock — 30-min cap, 90s collapse, left-behind | **BUILT** |
| 19 | Patrol director | **MISSING, and will not be built** — ACF's `UACFAIPatrolComponent` + `ACFUpdatePatrolBTService` retire `UGSPatrolDirector` (ruling 14). The overdue-patrol check-in soft signal stays ours. Today `GS.Combat.SpawnPatrol` is a clump-spawner |
| 20 | The hamlet map | **MISSING — unblocked.** The generator **won its timebox**: 8 seeds, 8 passes, measured kit, 2026-08-18. `L_Groatsworth` does not exist yet and `apply_in_editor.py` has never been run. See §6 for the two-statue roster trap |

### 12.2 Build plan — remaining blocks

| Block | Contents | Exit test |
|---|---|---|
| **A** · Unblock the verbs | **Re-grade before rebuilding** — watch the channel in `L_CombatArena` first. Residual only if it fails: `CarrySocket` on `GOB_Scout_v2_Skeleton` + an existence check that warns; carry-doesn't-consume fix | Hold interact on a carryable as the Scout; the channel fills, the thing rides on the shoulder, and it can be put down and picked up again |
| **B** · The map | Teach the generator the tutorial roster (`--tutorial`, one of each) and re-run the gate; create an empty `L_Groatsworth`; first-ever `apply_in_editor.py` run; bake the navmesh to the 24,000×24,000 site | Walk from treeline to the village core without falling through anything |
| **C** · The objectives | Blueprint subclasses of every objective class (**none exist today** — the single highest-leverage artifact); statue on the destructible with the **burn gate removed**; windmill **Ablaze stage** + window throw | Burn the market, topple the statue, take the mill through both stages — watched |
| **D** · The loop on the new map | Objectives + runic site + player start + the Warren mouth (`N_ChaosRune2`) + the plan's marker set placed; repoint `GameDefaultMap` off the ThirdPerson template | **"Extracted" on Groatsworth, watched** |
| **E** · Score honesty | Build the ×1.5 multiplier, give `AddLoot` callers at both banking points, unused-life and all-three bonuses, gate deeds to required types, `GS.Score.Status` readout | A raid scores itself per §10 and the number can be read at runtime |
| **F** · Teaching layer | Regenerate the banked text off the new roster; row struct → DataTable import → first-encounter tracker → HUD note + dismiss → config toggle | The 9 prompts fire once each, and the toggle silences them |
| **G** · Stealth, civilians & patrols | ACF Phase 2 **Option A** first; then perception onto guards, the escalation fix, ACF patrols and routines, the finite 15-pool, takedown, and **noise** | The quiet half plays end to end |
| **H** · Feel & funny | Close the four animation tickets, bark subsystem + DataTable, world SFX pass, balance, triage | **Playtest build** |

**Critical path:** A (re-grade) → B → C → D is the shortest route to a watchable raid on the real map.
E is largely data, F depends on the content regen, G is the biggest build and is gated on the ACF
move, H is last.

### 12.3 Honest scoping — the cut order

**Cut for this slice:** **the whole sneaking portion** (2026-08-24, ruling 56 — crouch-and-confirm,
noise, takedowns, corpse-suspicion, the coin toss) · bind/capture (2026-08-14) · **gore/gibs** (2026-08-19, ruling 15 — ragdoll
covers it) · **bark VO** (text-only; world SFX is *not* cut, see §11) · per-raid map generation ·
livestock beyond a single-species MVP.

Cut order if late blocks slip: self-looting civilians → sheep & chickens → coin toss →
corpse-suspicion → takedowns. **Ruling 56 took this list one step past its end**, cutting the
stealth core the order stopped short of. What remains to shed under pressure is the first two.

**Never cut:** the horn/horde · the three-objective destruction structure · the runic-site banking loop · the score screen.

> **The crouch-and-confirm stealth core was removed from this line on 2026-08-24 (ruling 56, #285).**
> It is the only item that was ever taken off it. Recorded here rather than deleted silently, because
> a "Never cut" list that quietly loses entries is worth nothing. The argument was that it is also the
> only item on the list that had **never run on a single actor** - §12.1 row 7, the perception component
> sits on zero actors - so the sunk cost was two systems that had never executed and the saving was
> three that did not exist. Cut from the DEMO; the `Stealth/` code is not deleted.

Team reality: solo dev (Michael) + AI agents. Post-slice roadmap: economy → co-op → the ladder
(generator un-shelved) → the assassination mission.

### 12.4 Scope freeze (2026-08-19)

*The lock this revision adds. Everything here was ruled on explicitly; **nothing joins the tutorial
without a new ruling and a ledger row.** The middle and right columns exist because the prose above
used to describe all of them as though they were built.*

| IN — build it for the tutorial | CUT — not this slice | DEFERRED — the full game |
|---|---|---|
| Civilians (13) | Gore / gibs — ragdoll covers it (15) | Watchtower, bell and the well (16) |
| Patrols, via ACF (14) | Bind / capture / rope chain (2026-08-14) | Wayfinding signposts (17) |
| Noise + world SFX (9, 10) | Bark VO — text and subtitles only (10) | Vault and mantle |
| The Warren, on `N_ChaosRune2` (18) | Per-raid map generation | Breach set · wood economy (tier-2) |
| The finite 15-defender pool + Highpurse Keep (19) | Livestock beyond one species | The Brute and both Shamans |
| The grapple as a core verb (12) | | |
| World corruption — the land turns as you raid (40) | | |
| Windmill Stage 1 Ablaze (7) | | |
| Finite arrows — a player-only quiver (46) | | |

**Two consequences that follow from the deferrals rather than being chosen.** Deferring the
watchtower, bell and well **removes three scoring lines** from §10 and the bucket-brigade counterplay
with them. Deferring wayfinding leaves **objective names as the player's only guidance**, because
decision 11 bans HUD arrows and waypoints — the generator already emits roads and signposts per plan,
so reversing this is placement and a mesh, not a system. **Ruling 40 is the answer to this**: world
corruption is the first thing built that makes decision 11's *"the environment does the leading"*
mean something, by giving the player a readout of their own progress that is the world itself.

## 13. Decisions ledger

**The ledger is `docs/decisions-ledger.md`.** Until 2026-08-19 both documents pointed at a
"repo-root §13" that **did not exist** — the rulings actually lived scattered through
`AGENT_STATE.md`'s DECISIONS section and ticket bodies, which is why the same questions kept being
re-opened. That file is now the single numbered, dated, citable list.

Load-bearing for code: Scout naming (Q-38) · win by type-flags with Required→Optional demotion
(Q-32/Q-37) · horde vault-only (41-a, and see §12.1 row 5b — no vault exists) · pool debit on spawn
only (40) · burn-visual mandate (2026-08-01) · firebreak Unburnt→Doused allowed (Q-35) · replicate
cheap root state only (Q-36).

**2026-08-14 (Michael):** required roster is **Market / Statue / Windmill**, granary removed, field
and houses optional · **bind cut**, civilians retained · the hamlet map is a **week-2 generator
timebox with a hand-authored fallback** · barks text-only · the prompt toggle ships as a config bool ·
a player holding cargo cannot blow the horn (drop first — `State.Carrying` blocks it, deliberately).

**2026-08-19 (Michael), 23 rulings** — full text and rationale in the ledger. In brief: this file
becomes canonical · the tutorial map is `L_Groatsworth` from the generator · the done bar is the full
§12.2 A–H plan · build Ablaze · remove the statue burn gate · noise and world SFX IN, bark VO out ·
climb re-graded as Blueprint-driven · the grapple is a core verb · civilians, patrols, the Warren and
the 15-pool IN · gore cut · watchtower/bell/well and wayfinding deferred · ACF Phase 2 takes
**Option A** · field 50 / house 10 · horde pool is 20.
