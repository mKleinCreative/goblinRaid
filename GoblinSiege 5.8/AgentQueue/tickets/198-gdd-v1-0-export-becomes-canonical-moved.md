---
id: 198
title: GDD v1.0: export becomes canonical, moved to docs/, reconciled to the live tree, and locked (freeze rule + drift check + ledger + scope table)
agent: claude-gddlock
status: done
claimed: 2026-08-19T21:12Z
build: none
waiting_on:
evaluated: 2026-08-19T23:01:21Z
observed: 2026-08-19T21:38:10Z | the gate printed FAIL on four banked text rows that still name the granary, and rejected four deliberately injected table mutations one at a time - a fourth column, a non-numeric id, a re-wrapped never-cut line and a reintroduced granary in the roster sentence - exiting 1 each time and 0 with none injected
scenario: python Tools/check_gdd.py -v against the real docs/goblin-siege-gdd.md in D:\goblinRaid, with the document md5-verified byte-identical after each injected break was reverted
files: 
  - Tools/CodeArchitect/docs/goblin-siege-gdd.md
  - docs/goblin-siege-gdd.md
  - docs/decisions-ledger.md
  - Tools/check_gdd.py
  - goblin-siege-design-document.md
---

## Goal

GDD v1.0: export becomes canonical, moved to docs/, reconciled to the live tree, and locked (freeze rule + drift check + ledger + scope table)

## Generate

**1. `Tools/CodeArchitect/docs/goblin-siege-gdd.md` → `docs/goblin-siege-gdd.md` (`git mv`).**
`ca/config.py:48-54` probes `project_root/docs/goblin-siege-gdd.md` *before* `tool_root/docs/...`, so
the canonical doc moved out of a tool's private folder for **zero code change**. Verified by running
`Config()` and reading back `gdd_path` — it resolves to the new location.

**2. The export became canonical (ruling 1).** Header rewritten to invert the direction: this file is
canonical, the repo-root doc is the frozen assignment artifact. Added `v1.0 — LOCKED 2026-08-19`, a
**Revision history** table, the **change rule** (a GDD change needs a ticket and a ledger entry), a
status-honesty note, and a tested **parser contract** (see Evaluate — my first version of it was
wrong).

**3. Reconciled against the live tree**, section by section:
- **§4** — grapple added to the control map and written up as a core Scout verb (ruling 12); traversal
  documented as Blueprint-driven with vault/mantle called out as non-existent (ruling 11); the
  "interact has never run" defect downgraded to *believed fixed, not observed fixed*.
- **§5** — the pool-size flag deleted; 20 is settled and derived, not a literal (ruling 23).
- **§6** — the timebox recorded as **won**; the **two-statue roster trap** written up in full; the
  `L_Tutorial_Island` / `L_CombatArena` divergence stated outright; the 4,000×4,000 navmesh vs the
  24,000×24,000 site recorded; the Warren given its mesh and both jobs (ruling 18); Ablaze (7) and the
  statue burn gate (8) rulings attached to their objectives.
- **§7** — the "lean five" split into a per-system table: two built, four absent. Noise IN (9).
- **§8** — scope banner; six live defects and scope calls including the finite 15-pool (19) and the
  watchtower/bell/well deferral (16).
- **§9** — the Warren's loot-not-deeds banking rule; the ×1.5 multiplier recorded as **absent**.
- **§10** — table settled: field 50 / house 10 (21), with the four now-unbackable lines struck.
- **§11** — the "zero audio assets" line corrected against ~2,000 assets in
  `Content/NaPH_RPG_Fantasy_Sounds_Bundle` (10); gore recorded as cut (15).
- **§12.1** — all 21 rows re-graded. Four changed grade on evidence: **4** SKELETON→WIRED-unreachable,
  **11** SKELETON→MISSING, **17** SKELETON→WIRED-no-cargo, **8** MISSING→believed-wired-unobserved.
- **§12.2** — blocks A–H rewritten with the rulings baked in and a critical path.
- **§12.3** — cut list updated; **the never-cut line un-wrapped** (see Evaluate).
- **§12.4** — **new**: the IN / CUT / DEFERRED scope-freeze table (ruling 2), plus the two
  consequences that follow from the deferrals rather than being chosen.
- **§13** — now points at a ledger that exists.

**4. `docs/decisions-ledger.md` — new.** The §13 both documents had been pointing at for weeks
without it existing. Today's 23 rulings, plus the 2026-08-14 roster revision, ACF adoption, the
climbing rulings, the horde rulings, the ranged rulings, the interact rulings, and the load-bearing
decisions 1–41 / Q-series — each with its source.

**5. `goblin-siege-design-document.md` — frozen banner only.** Content deliberately untouched.

**6. `Tools/check_gdd.py` — new, the drift gate (ruling 2).** Nine checks in four groups: the parser
contract, `features.json` coverage, the **required roster vs the generator's `REQUIRED_KINDS`**, and
banked player-facing CSV vocabulary. Rules are **imported, never restated** — `BANNED_TERMS` from
`gsstyle.py`, `REQUIRED_KINDS` from `gslevelgen.generate`. Exit 0 clean / 1 drift / **2 incomplete**,
because a check that could not run must not exit 0.

## Evaluate

**What is verified, and how.**

- **The parser survives the move and every edit.** `ca/gdd.py` parses 21 ids (`1`–`20` + `5b`), all 8
  blocks, 5 never-cut items, zero warnings, zero `coverage_check` drift. This also closes the
  verification **#158 left open** — its own Evaluate said nobody had ever run the parser against the
  reshaped §12.1.
- **The gate is proven to fail**, not just to pass. Four mutation classes were injected into the real
  doc and each was caught: a 4th column (`status_column_clean`), a non-numeric id
  (`systems_inventory` + `feature_coverage`), a re-wrapped never-cut line (`never_cut_unwrapped`), and
  a reintroduced granary in the roster sentence (`roster_matches_generator`). The doc was restored and
  **md5-verified byte-identical** after every run.
- **The gate caught real, pre-existing drift on its first run**: `barks.csv:16`, `barks.csv:17`,
  `prompts.csv:2` and `whispers.csv:5` still name the granary, five days after the GDD removed it.
  That is Block F work and is **not fixed here** — the gate reporting it is the deliverable.

**Two things I got wrong, and how they were caught.**

1. **I wrote a false claim into the locked document.** The first parser contract asserted "a fourth
   column silently drops every row." The break test disproved it: `| 14 | … | **BUILT** | extra |`
   still matches, and the extra cell is **absorbed into the status string** — worse than a drop,
   because a drop is loud in the id list. I then tested eight mutations against the regex directly and
   rewrote the contract from measured behaviour. **The lesson is the project's own:** I derived a rule
   by reading code and shipped it as fact into the one document everything else now trusts.
2. **My own prose hijacked the parser.** The contract note contained the literal `**Never cut:**`, and
   `gdd.py` uses `re.search` — first match wins — so the never-cut list parsed as a fragment of my
   sentence. Caught immediately because I re-parsed after the edit rather than after finishing.

**What is written but has never run:** the full Code Architect (`architect.main`) has not been run
end-to-end against the moved file — only `gdd.py`'s parse and `coverage_check`, which is the surface
the file's shape actually touches. Worth one scan-only run when the editor is next up.

**Touched outside the goal:** nothing. `gsstyle.py` needs its audio-rule *rationale* narrowed per
ruling 10 (the comment says "a slice with zero audio assets", which is now false; the banned terms
themselves still stand, because barks remain text-only). It is **not claimed by this ticket** and was
left alone.

**Owed to `AGENT_STATE.md`:** a DECISIONS line pointing at the new ledger, and the two findings that
should not have to be rediscovered — the map divergence and the missing ×1.5 multiplier. Taken as a
separate claim rather than edited unclaimed.

## Refine

- **Rewrote the parser contract from tests instead of from reading**, after the break test disproved
  my first version. The note now records that it was established by mutation testing, and says so
  explicitly — including that the first pass was wrong — so the next reader knows which kind of claim
  it is.
- **Added two checks the break test showed were missing**: `status_column_clean` (a stray `|` in a
  status cell, the corruption case) and `status_not_blank`. Without the first, the 4-column mutation
  passed the whole gate.
- **Fixed the roster regex for wrapped lines** (`The\s+three\s+are`) — it failed on the real document
  because the sentence wraps between "The" and "three". Found by running the gate, not by review.

**Deliberately left undone:**

- **`check_gdd.py` is NOT wired into `Build-GoblinSiege.ps1`.** Two reasons, and the second is the
  real one: that file is not claimed by this ticket, and **the gate currently exits 1**, so wiring it
  today would block the very build window ruling 5 just authorised. Wiring it is correct *after* the
  four banked rows are regenerated. Michael's call — see the handback note.
- The banked CSV regeneration itself (Block F): needs a claim on `content-pipeline/` and API calls.
- The `gsstyle.py` audio-rationale narrowing (ruling 10), as above.

---

# Addendum — the control map was missing the order wheel

Found while auditing the prompt rows' control bindings (#200). §4's table listed the weapon wheel,
attack, block, aim, interact, traversal, grapple, horn, jump, dodge and crouch — but **not the horde
order wheel**, which is `IA_HordeOrder` on **R** and is how three of the four horde orders are
issued. Added as a row. The gate did not catch this and could not: it checks the objective roster
against the generator, not the control map against the input bindings.
