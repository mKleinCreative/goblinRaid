---
id: 035
title: Refresh AGENT_STATE.md NEXT against what is actually built, then regenerate the decision-queue board
agent: claude-orchestrator
status: done
claimed: 2026-08-06T17:54Z
build: none
waiting_on:
evaluated: 2026-08-06T18:01Z
files: 
  - GoblinSiege 5.8/AGENT_STATE.md
  - decision-queue-kanban.html
---

## Goal

Refresh AGENT_STATE.md NEXT against what is actually built, then regenerate the decision-queue board

## Generate

**`AGENT_STATE.md` ## NEXT.** Every `missing:` symbol on the old list was re-checked against the
tree with `grep -rl "class GOBLINSIEGE_API <S>"` and `find Content -iname "<A>.uasset"`. Three
entries had all their symbols present and were removed: **interact framework** (u=10.0, the top item
for two days), **lives / respawn** (`AGSPlayerState` + `EGSRaidResult::OutOfLives`), **runic site**
(`AGSRunicSite` + `BP_GS_RunicSite`). Four more had partial completions now recorded on the line:
`AGSHordeGoblin`, `Takedown`, `DA_Race_Human`/`BT_Militia`, and `UGSCarryComponent` - the last of
which was the *only* thing blocking loot couriers, so that moved BLOCKED -> ELIGIBLE.

Added an **unranked** section for the eight items raised since the live-003 ranking run: the nine
`HANDOFF.md` code-review findings, the radial weapon wheel, the missing torch-throw animation, the
arrow/RaceTag friendly-fire gap, Michael's burn-duration note, the dead melee
`AttackCooldownSeconds`, the two never-exercised lose paths, and `BP_GS_Arrow`. They carry no `u`
because no ranking run has scored them and inventing numbers would launder a guess as a measurement.

**`decision-queue-kanban.html`.** Reseeded to 27 cards. Four code changes beyond the data:
- `byPriority` comparator: unranked sorts FIRST. The old sort was `(+b.utility || 0) - ...`, which
  cannot tell "unscored" from "scored zero" and would have filed a crash below a u=1.75 nice-to-have.
- `localStorage` key bumped `v1` -> `v2`. `load()` prefers storage over `SEED`, so without this the
  refresh would have been invisible in every browser that had already opened the old board.
- An `unranked` badge, so a card with no meter cannot be misread as u=0.
- The stat tile relabelled "highest-utility eligible" -> "top of Eligible", since after the sort
  change the highest-utility card is no longer the one at the top.

## Evaluate

**Verified, with evidence:**
- Every existence claim is a command result, not memory. `UGSScoreSubsystem`, `UGSHordeSubsystem`,
  `UGSBarkSubsystem`, `UGSGibComponent`, `UGSPatrolDirector`, `ReportGSNoise`, `CoinToss`,
  `AM_GS_Death`, `DA_Weapon_Greatclub`, `DT_Barks`, `DT_NoiseEvents`, `BT_Civilian`,
  `DA_Race_Livestock`, `BT_HordeGoblin` absent; the seven listed above present.
- Seed parses: `JSON OK. cards=27 lanes=5`, no duplicate ids, no bad lane refs.
- Whole inline script parses: `node --check` on the extracted 409-line block, clean.
- The comparator was **executed** against the real seed, not eyeballed: the six unranked eligible
  cards print above u=5.75, and the "top of Eligible" tile resolves to the code-review findings.

**NOT verified: the board has never been rendered.** The Chrome extension reported "Browser
extension is not connected", so no screenshot exists. JSON validity, JS syntax and sort order are all
proven; **layout, the new badge's appearance, and light/dark are not.** The previous revision of this
file was checked visually in both modes and this one has not been.

**The ranking is two days stale and I did not fix that.** Ten items still carry `u` from run
live-003 while the project has moved through ~25 tickets. Re-ranking is a scoring run against the
design docs, not something to fake from inside a refresh - so the header now says plainly that the
order is two days old rather than presenting it as current.

**Unranked-first is a judgement, and it has a cost.** It puts six cards above `Someone to fight`
(u=5.75), which no ranking run endorsed. The alternative - numeric sort - actively misrepresents a
crash as the lowest-priority item on the board, which is worse. The badge and `meta.utilityNote` both
say the cards are unscored so the reader can discount the order.

**Two cards state their own unreliability**, and that is deliberate: `tkt-debug` carries "BuildingStatus
lies" and `tkt-queue` carries "advisory only". A Done lane that only says Done is how #010's breach
went unnoticed.

**Owed AGENT_STATE.md** - nothing new. This ticket's finding *is* the NEXT rewrite.

**Touched outside the goal:** nothing. Both claimed files, no source, no compile.

## Refine

- **Bumped the storage key only after re-reading `load()`.** The regeneration was complete and
  correct and would have displayed nothing on Michael's machine, because the board he already has
  open is served from `localStorage`, not from the seed I just rewrote. A regeneration nobody can see
  is not a regeneration.
- **Executed the comparator instead of reasoning about it.** The sort touches three call sites and I
  had already changed the tile that depends on it; running it against the real data is what confirmed
  the tile follows the board rather than contradicting it.
- **Gave the unranked items no score rather than a plausible one.** Assigning them u values would
  have made the meters render and the board look complete, and every one of those numbers would have
  been fiction sitting next to ten that came from a real run.
- **Recorded partial completions on the surviving lines** (`AGSHordeGoblin already exists`,
  `Takedown arrived with the interact framework`) rather than silently shortening the `missing:`
  lists. The next agent should be able to see that the item shrank and why.

**Deliberately left undone:** a fresh ranking run; a visual check of the board (blocked on the
extension, and worth doing before anyone trusts the layout); folding the nine review findings into
individual tickets - they are one card and one NEXT line until someone claims them.
