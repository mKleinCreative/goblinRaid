---
id: 406
title: Install ACFU FullSample as reference and reconcile the GS-vs-ACF gap notes
agent: claude-acfsample
status: review
claimed: 2026-09-09T23:14Z
build: none
waiting_on:
evaluated: 2026-09-09T23:19:39Z
observed:
scenario:
files: 
  - .gitignore
  - CLAUDE.md
  - .claude/skills/gs-character-data-asset/SKILL.md
  - .claude/skills/gs-behaviour-tree-wiring/SKILL.md
  - Content/FullSample
---

## Goal

Install ACFU FullSample as reference and reconcile the GS-vs-ACF gap notes

## Generate

- **`Content/FullSample/`** — copied 4,839 files / 5,088,662,306 bytes from
  `D:\ACFUSampleProject4.2\FullSample`, excluding its `.git`. Staged to `D:\_fullsample_stage`
  first and moved in as a single same-volume rename, so the *running* editor's directory watcher
  never saw half-written `.uasset` files.
- **`.gitignore`** — comment block after `!Content/Items/` recording that `Content/FullSample/` is
  deliberately NOT negated (marketplace content, re-downloadable), so the next reader does not
  "fix" it into the seventh instance of the silently-untracked bug.
- **`CLAUDE.md` §3a** — replaced the "two gaps in this install" paragraph. `/Game/FullSample/` is
  no longer absent; documents the gitignore decision, the real provenance, and the
  "never press Update Project Settings" trap (it would overwrite the #228 `HealthAttribute` fix).
- **`.claude/skills/gs-character-data-asset/SKILL.md` §1** — added the shipping-sample confirmation.
- **`.claude/skills/gs-behaviour-tree-wiring/SKILL.md`** — added the shipping-sample context.
- **21-item GS-vs-ACF gap list** for Michael to rule on:
  `C:\Users\Michael\.claude\plans\i-just-downloaded-finally-bright-bachman.md`.

## Evaluate

**Verified, with evidence:**
- Copy parity — source and destination both report `Count 4839, Sum 5088662306`; `.git` absent.
- Ignore rule works — `git check-ignore -v` returns `GoblinSiege 5.8/.gitignore:32:Content/*`;
  `git status --short` lists the four doc edits and **no** FullSample path.
- Sample provenance — `git config --get remote.origin.url` on the source clone returns
  `https://gitlab.com/pasqualemangano/FullSample.git`, `main` at `52fa4f22` (2026-07-31).
- The two skill-pack corrections rest on disk scrapes: exactly one character sets
  `CharacterInitDataAsset` (`ACFZombieBP`); `BT_EnemyBoat` is the only BehaviorTree and there are
  zero Blackboard assets in the sample.

**Written but never run — do not read this ticket as more than it is:**
- **The editor has not seen any of it.** Asset registry reports `REGISTERED_FULLSAMPLE: 0`
  (`SCANNING: False`). Not one sample asset has been opened, loaded or rendered. Every statement
  above about the sample's wiring comes from string-scraping `.uasset` headers on disk — good
  enough to locate things, not good enough to trust as configuration.
- The 4.4.2-plugin load-warning count is unmeasured.
- Montage root-motion counts (gap-list item 3, which gates item 1) are unmeasured. That number is
  the whole reason item 1 is not already in flight.
- Gap-list items 1-21 are unruled and nothing from them is implemented.

**Outside the goal:** none. `Content/Maps/L_Tutorial_Island.umap` is modified in the working tree
by the open editor / another session; not mine, not touched.

**DECISION owed to `AGENT_STATE.md`:** `/Game/FullSample/` exists as of 2026-09-09 at
`Content/FullSample/`, gitignored. The standing "the sample is not present, get it from the ACF
Discord" caveat is retired.

## Refine

- **Corrected an invention of my own.** I wrote a GitHub URL for the sample repo into `CLAUDE.md`
  from assumption, then checked the clone's actual git config and replaced it with the real GitLab
  remote. The same check also refuted my own "ACF 4.2-era content vs 4.4.2 plugin" version-skew
  claim — the folder name says 4.2, the contents are `main` of ACF's RC/develop line dated
  2026-07-31. The skew risk in the gap list is smaller than I first wrote, and the clone carries
  `feat/climbing-ik`, `feat/hop-climbing`, `fix/climbing-V2`, `feature/laddersystem-v2` and
  `feat/Directional-Combat` branches this project has direct use for.
- **Deliberately left undone: all editor-side verification.** Forcing a registry rescan of 4,839
  assets, or restarting the editor, would corrupt the frame captures #405 is actively taking — it
  committed `ed93318` at 16:17, three minutes after I claimed this ticket, so that session is live
  despite its STALE flag. Michael schedules that, not me.
