# Goblin Siege — decisions ledger

**Created 2026-08-19 (#198).** Until today, both design documents pointed at a "§13 decisions ledger"
that **did not exist in either of them**. The rulings were real but scattered — through
`AGENT_STATE.md`'s DECISIONS section (~470 lines), ticket bodies, and chat — which is precisely how
settled questions kept getting re-opened by the next session. This file is the single numbered,
dated, citable list.

## How to use this file

- **A "decided" entry is frozen.** Do not re-litigate it, re-design around it, or "fix" it back. If
  it is genuinely wrong, that is a new ticket and a new dated entry that says which one it supersedes
  — never a silent edit.
- **Every entry carries its source**: a ticket id, an `AGENT_STATE.md` section, or a dated ruling from
  Michael. An entry with no source is not a ruling, it is a recollection.
- **Adding an entry needs a ticket.** Same rule as the GDD (`docs/goblin-siege-gdd.md`, change rule).
- Entries are grouped by date, newest first. Numbering within a group is stable once written.

---

## 2026-08-19 — the reconcile-and-lock rulings (#198)

Taken while reconciling the GDD against the live tree. Every one is Michael's, given in session.

| # | Ruling | Why it was asked |
|---|---|---|
| 1 | **`docs/goblin-siege-gdd.md` is canonical**; the repo-root `goblin-siege-design-document.md` freezes as the submitted Assignment #02 artifact | Two documents disagreed and neither was authoritative. The export was the status-honest one, and #158 had already established that when an export and a canon disagree you check which is *older*, not which is derived |
| 2 | **Lock with all four mechanisms**: versioned freeze + change rule · machine drift check · consolidated ledger · scope-freeze table | A doc that drifts silently is what produced the granary the evaluator kept enforcing after the design had dropped it |
| 3 | **The tutorial map is `L_Groatsworth`, built from the generator plan** | The generator won its week-2 timebox on 2026-08-18 — 8 seeds, 8 passes, on the measured kit |
| 4 | **"Finished" means the full §12.2 A–H block plan** | Sets the done bar before work starts, so scope cannot quietly shrink to "a raid ran once" |
| 5 | **Take a build window now and clear all seven stale tickets** (178, 179, 184, 185, 186, 189, 190) | The build gate was closed and four of the seven were waiting on the same build. QUEUE.md rule 6 makes a STALE ticket Michael's call, never an agent's |
| 6 | **Teach the evaluator the tutorial roster rule** — a `--tutorial` fixed roster plus a one-of-each check — rather than hand-picking a lucky seed | The passing seed-1 plan was *statue / windmill / statue*: legal under §1's generic "never more than two of a kind", wrong for a tutorial that must teach all three kinds of destruction. A hand-picked seed leaves the rule unenforced and free to break on the next regeneration |
| 7 | **Build the windmill's Stage 1 "Ablaze"** — the design is right, the code is behind | The shipped enum is `Intact/Smouldering/Detonated` and exterior fire is a deliberate no-op. The two-stage revision exists so a torch is never *silently* wasted: the player must see what they bought and what is still owed |
| 8 | **Remove the statue's burn gate** — topple is the only path | `AGSDestructibleObjective` gated the fracture behind burn-complete, so the statue had to be set on fire before it could be pulled down — contradicting the one line the objective exists to deliver ("the one target that doesn't burn") |
| 9 | **Noise is IN**, and is flagged as a major next item | A sound pack is plugged in, removing the excuse the 2026-08-14 audio cut rested on. Noise is what makes crouch a decision rather than a speed penalty |
| 10 | **Audio is IN for world/SFX; barks stay text-only** | `Content/NaPH_RPG_Fantasy_Sounds_Bundle` holds ~2,000 assets, so "the project contains zero audio assets" was dead. But no VO pipeline exists, so the Overlord and the guards stay subtitled. `gsstyle.py`'s banned-audio-word rule **narrows rather than lifts** |
| 11 | **Traversal §5b: climb ships in Blueprint** — re-grade the row, do not rebuild it | `UGSClimbLibrary` has zero C++ callers, which reads like a corpse; the climb actually lives in the player Blueprint calling it. Vault and mantle genuinely do not exist |
| 12 | **The grapple is a core Scout verb** — into §4's kit and §6 as how the statue falls | It shipped across #173–#177, #183 and #193 and appeared in no design document at all |
| 13 | **Civilians: IN** | §2.6 wants 6–10 on routines, and the bucket brigade already depends on them. ACF's `UACFAIRoutineComponent` covers most of it |
| 14 | **Patrols: IN, configured not built** | ACF ships `UACFAIPatrolComponent` / `ACFUpdatePatrolBTService`, which **retires the planned `UGSPatrolDirector`**. The overdue-patrol check-in soft signal stays ours — ACF patrols walk waypoints, they do not report to a stealth director |
| 15 | **Gore / gibs: CUT** | The existing ragdoll death covers it |
| 16 | **Watchtower, bell and well: DEFERRED to the full game** | None of the three exists in any form. **Consequence:** removes *watchman silenced +25*, *well fouled +30* and the bucket-brigade counterplay from §10 |
| 17 | **Wayfinding signposts: DEFERRED** | **Consequence:** with decision 11 banning HUD arrows and waypoints, objective *names* become the player's only guidance. The generator already emits roads and signposts per plan, so reversing this is placement and a mesh, not a system |
| 18 | **The Warren: IN, on `N_ChaosRune2`** — it is both the goblin arrival mouth and the turn-in point for livestock and looted crate/chest/barrel goods | Satisfies the settled no-pop-in mandate (goblins emerge from a hole, they do not fade in) and gives loot a second permanent banking point. **Deeds still bank only through the real portal** |
| 19 | **The finite 15-defender raid-response pool and Highpurse Keep: IN** | "Highpurse" returns zero hits in Source; what ships is an escalating spawn-rate tier with no floor. The design promises a garrison you can actually exhaust — *the hamlet can't stop you; the county can, exactly once* |
| 20 | **ACF Phase 2 takes Option A** — `AGSCharacterBase : AACFCharacter`, moving horde, defenders **and the player** together | ACF's player is an `AACFCharacter`; Option B would leave the player on a different damage path from everything he fights, and would force the combat verbs to be duplicated or hoisted |
| 21 | **Optional objective scores: wheat field 50, house 10** | Fills the last TBD. Burning every house tops out near 300 against the required trio's 450, honouring §1's "deliberately below 100" clause. Deferred lines are struck from the table rather than left unwired |
| 22 | **Retire `L_Hamlet_T1`** (159 MB, unreferenced) after a reference check; keep the small scratch maps | Zero tickets is not zero references — verify before deleting |
| 23 | **Horde pool is 20.** Delete the disagreement flag | Not a ruling so much as a resolution: `ActiveCap = 10 × ReserveMultiplier = 2` derives 20 and never carries a literal, and §2.8 said 20 all along. Only the stale 2026-08-04 export text dissented |

### Two findings recorded the same day (not rulings, but load-bearing)

- **The two maps have diverged.** `L_Tutorial_Island` holds the 2026-08-05 world and contains **zero**
  raid markers, interactables, loot, breakables, grapple anchors or the topplable statue. **Every
  system shipped since ~2026-08-07 exists only in `L_CombatArena`.** Any claim that a system "works
  in the tutorial" is false by default until re-checked.
- **The ×1.5 extraction multiplier does not exist anywhere in Source.** Not stubbed, not unwired —
  absent. The rule that makes deeds provisional, and the reason the run home is a decision, has never
  been implemented.

---

## 2026-08-14 (Michael) — the roster revision

Recorded in tickets #156, #157, #158.

1. **The required roster is Market / Statue / Windmill.** The granary is removed entirely — *"when
   initially written I had meant a different building, but had never created it."* It never had a
   mesh, a Blueprint or a placed instance. **The wheat fields and the houses become optional** — worth
   points, not gating extraction.
2. **Bind / capture is cut** for this slice; civilians are retained on the strength of the
   bucket-brigade logic. The civilian pair now offers the takedown choice only.
3. **The hamlet map is a week-2 generator timebox with a hand-authored fallback**, superseding the
   2026-07-23 hand-authored-only ruling. The framing that made this cheap: *the generator authors ONE
   hamlet* (in scope) is a different question from *the generator varies the layout raid to raid*
   (still post-slice, funding-gated). Superseded by ruling 3 above — the timebox was won.
4. **Barks are text-only** this slice. *(Amended by ruling 10: world SFX is now IN; bark VO stays out.)*
5. **The tutorial-prompt toggle ships as a config bool.**
6. **A player holding cargo cannot blow the horn** — drop it first. `State.Carrying` blocks it,
   deliberately.

---

## 2026-08-12 (Michael) — ACF adoption

Source: `ACF_HORDE_MIGRATION.md`.

1. **Full adoption.** The horde reparents onto the ACF hierarchy and orders go through ACF's command
   system. #141's hand-rolled order board is parked.
2. **The defenders come too** — *"defenders are eventually going to need to do patrols, I believe
   it's worth having them follow."* One AI stack, not two. This is what makes patrols and civilian
   routines a configuration job instead of a build.
3. **What stays ours, and must not be swallowed by the migration:** the radial order wheel, the
   world-space order marker, the courier verb, formations and ring slots, and **the horn plus the
   finite pool of 20** — ACF spawns groups and has no concept of a depleting shared pool.
4. Phase 2's fork was left open here and is settled by ruling 20 above.

---

## 2026-08-08 — the climbing rulings

Source: `AGENT_STATE.md` DECISIONS. These took the climb from "stalls at the same lip every time,
four sessions running" to working. **Settled — do not re-open.**

1. **Ledge detection is a SEARCH, not a tuned constant.** `FindClimbLedge` sweeps insets 80–340 step
   20 and takes the first surface that is walkable *and* has open sky above. Measured on 56 roof lips
   across 14 houses: the best fixed inset scores 88%, the search scores 96%. *"Just tune it" is the
   trap that cost a day.*
2. **The sky check is load-bearing.** A candidate deck with a roof above it is an interior floor;
   deleting the test puts the player inside the house.
3. **Goblins have claws: `WalkableFloorAngle` is 65°**, and that angle is simultaneously the steepest
   walkable surface and the boundary above which a surface must be climbed. Do not add a looser gate
   inside the ledge search.
4. **Climb animation play rate is DERIVED per frame**, not set. `GetVelocity()` is the wrong input —
   during a climb it reports what `ClimbTick` commanded, not what moved.
5. **Braced hops stay.** The target feel is the Moria scene — *smooth and a little hectic*. Smooth
   means no sliding; it does not mean a continuous climb cycle.
6. **Hold-E climbs.** Do not repurpose E as a release verb.
7. **Stamina freezes on the wall** — no drain, no regen. That is correct behaviour, not a stuck tick.
8. **Roof continuation is CLOSED, not deferred** — *"the roofs are fine, consider it closed."* 98% of
   roof lips resolved across 20 houses is done, not 98% of the way to done.

---

## 2026-08-07 — the horde rulings

Source: `AGENT_STATE.md` DECISIONS, taken while #069 was built. **Settled.** The first four are the
ones a later agent is most likely to "fix" back.

1. **The war-horn is on MIDDLE MOUSE**, not G. G is `IA_Block`. F is `IA_Interact`. Both GDDs' "G" is
   an erratum.
2. **The horde arrives from a portal / Warren mouth, not the treeline.** Emerging from a hole
   satisfies the same no-pop-in mandate. **Do not "restore" the treeline.** (See ruling 18 — the
   placeholder is now `N_ChaosRune2` and is built as the Warren-to-be.)
3. **Combat verbs live on `AGSCharacterBase`.** Reparenting `AGSHordeGoblin` to `AGSEnemyCharacter` is
   the tempting one-liner and silently flips five class-identity checks, none of which fail loudly.
4. **`AGSHordeSpawnMarker` will NEVER be built** — the `Marker.HordeArrival` tag and
   `AGSRaidMarker::GatherByType` already do the whole job.
5. **Active cap is per player (10 each); the raid pool of 20 is shared.**
6. **Fire kills your own horde and that is intended.** Do not "fix" it in `AGSFireVolume` — that would
   make the player fire-immune too.
7. **Corpses are never destroyed** — *"we want to see where things died."* Accepted knowingly as a
   perf cost.
8. **The horn raises the alarm straight to RAID**, not Suspicious. §2.6's Suspicious-tier horn is a
   *patrol's* horn — a different event sharing a noun.
9. **`SummonsPerBlast` is a fixed 4**, not a random 3–4 — a player counting his pool should not have
   to guess.
10. **A dead horde goblin is a stat, not a score** — no deed, no loot, no penalty.
11. **The navmesh gets WIDENED, not switched to invokers.** Invokers give no navmesh where no invoker
    stands, which would break patrols.
12. **AI vault is deferred** until the climb rebuild settles. Decision 41-a still stands, but nothing
    in the project can vault from code.

---

## 2026-08-06 (Michael) — the ranged rulings

1. **An arrow STICKS in an ally; it does not pass through.** It is stopped by an allied body, deals
   nothing, and the shot is wasted. Positioning is the player's problem. Asked with the horde case
   explicitly on the table and answered anyway — so an agent finding "every shot eaten by a friendly"
   is looking at intended behaviour, not a bug.
2. **The radial weapon wheel is built C++-first**: the enum and selection maths in code with
   BlueprintReadOnly state; the UMG widget comes after, against a working backend.

---

## 2026-08-04 — the interact framework rulings

1. **E with full hands: the focused interactable wins.** Carry-to-extract has to work, so
   "hands full = drop" is wrong.
2. **Carry stays one slot, attacks blocked, for the slice.** The "chickens are weightless, carry two,
   fight one-handed" rule needs weight classes and waits for livestock.
3. **`MoveSpeedMultiplier` gets wired into CharacterMovement once**, retiring the cache-and-restore
   pattern that `OnStartCrouch` silently defeats.
4. **The co-op server path goes in now** — cheaper before five systems hook the completion delegate
   than after.
5. **No interacting or blocking while staggered.**
6. **Full hands cannot throw a torch** — drop the sack first.
7. **Extraction is an auto-bank circle, not a hold-E verb.** `Interact.Extract` stays declared but
   unused.

---

## Earlier — decisions 1–41 and the Q-series

These predate the single-file agent memory and are cited throughout both design documents. The ones
**load-bearing for code**:

| Ref | Ruling |
|---|---|
| Q-38 | The canonical class name is **SCOUT** (amends decision 36); sword ⇄ bow |
| Q-32 / Q-37 | **Win by type-flags** — burn one of each required TYPE, then extract; siblings demote Required → Optional on first same-type completion |
| 40 | **Pool debits on spawn only** — a delivered courier rejoins the reserve; only death spends a goblin for good |
| 41-a | Horde goblins are **ground-bound but can vault**; never climb or mantle. *(See §12.1 row 5b — no vault exists in the project, so this rule currently has nothing to stand on)* |
| Q-35 | Firebreak **Unburnt → Doused** is allowed |
| Q-36 | **Replicate cheap root state only** — single-player slice, co-op-ready |
| Q-28 | The runic site sits in the mountains |
| Q-29 | The granary was deferred out of the tutorial set in July — which is why the 2026-08-14 removal was smaller in code than it looked in prose |
| 11 | **No HUD arrows, no waypoints.** Objectives are named, not pointed at — the environment does the leading. *(See ruling 17: with wayfinding deferred, names are currently all there is)* |
| 37 | The slice **is the game's tutorial, framed in fiction** — the clan auditions for His Eternal Darkness, and the score screen is his verdict |
| 2026-08-01 | **Burn-visual mandate:** anything burnable chars black and smoulders; mill sails spin while burning and stop at Detonated |
