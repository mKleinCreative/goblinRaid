---
id: 205
title: Two ACF installs collide and no build can run: retire the project 4.4 copy, adopt the engine 4.4.2, repoint the skills registration
agent: claude-acfswap
status: done
claimed: 2026-08-20T19:14Z
build: required
waiting_on:
evaluated: 2026-08-20T19:18:06Z
observed: UNOBSERVED 2026-08-20T19:18:07Z - The swap is applied and the skills re-registered from v4.4.2, but the thing this ticket exists to fix - that a build can run at all - cannot be tested while the ticket is open, because an open ticket shuts the build gate. The build is the proof and it runs next. Nothing about ACF behaviour in game is claimed. Closed on Michael's instruction 2026-08-20.
scenario: none - never run
files: 
  - Tools/Register-ACFSkills.ps1
  - CLAUDE.md
---

## Goal

Two ACF installs collide and no build can run: retire the project 4.4 copy, adopt the engine 4.4.2, repoint the skills registration

## Generate

**The blocker.** No build of any kind could run. UBT died in 13.7s during plugin resolution, before
computing an action list: *"Plugin 'AscentCombatFramework' (referenced via MyProject.uproject) does
not contain the 'AscentCombatFramework' module, but lists it."* Two complete installs claimed the
same plugin name:

| | Version | Where | Dated |
|---|---|---|---|
| Engine | **4.4.2** | `UE_5.8/Engine/Plugins/Marketplace/ACFUAsce5ab7c1439afbV5/` | 2026-08-19 21:03 |
| Project | 4.4 | `GoblinSiege 5.8/Plugins/Marketplace/AscentCombatFramework/` | 2026-07-24 |

Both complete - 48 modules each, both genuinely containing the module - so this was a **name
collision**, not a missing module. The timing was conclusive: the project built clean 2026-08-18
20:49, the engine copy landed 08-19 21:03, the next build failed.

**Michael's ruling: adopt the engine 4.4.2.** Three changes:

1. **The project copy was MOVED, not deleted** - to `D:\ACF_4.4_replaced_20260820`, a same-volume
   rename so 7 GB moved instantly. It is gitignored and therefore unrecoverable from git; a move is
   reversible and a delete is not. Verified afterwards that only `MetaRoad` and `VibeUE` `.uplugin`
   files remain under the project's `Plugins/`.
2. **`Tools/Register-ACFSkills.ps1` now DISCOVERS the plugin** instead of hardcoding one path -
   project-local first (that is the copy a build would use if it ever exists again), then the engine.
   The engine folder carries an **install-specific hash** in its name, which is exactly the kind of
   string that must never be pasted into a script: it changes on reinstall.
3. **`CLAUDE.md` 3a** records where ACF now lives, that both copies must never coexist, and the exact
   UBT error to recognise it by.

## Evaluate

**Verified:** the script parses, runs, and re-registered **40 skills from plugin v4.4.2** (previously
40 from 4.4) into `.claude/skills`, with `.acf-manifest.json` updated. Both copies ship the same 40
packs, so nothing was gained or lost there.

**A correction Michael should have:** the release notes quoted at the time describe **4.4**, and the
project copy was *already* 4.4. This is the **4.4.2 patch**, not the 4.3 to 4.4 feature jump - no
Designer Mode, dialogue tools or Fog Of War arrived here that were not already present.

**NOT verified: that the build now succeeds.** That is this ticket's whole point and it cannot be
checked while the ticket is open, because an open ticket shuts the build gate. Same shape as #204,
#176, #179 and #189.

**Risk deliberately accepted:** ACF was upgraded mid-migration, with Phase 0/1a done and Phase 2
(Option A, `AGSCharacterBase : AACFCharacter`) still ahead. A patch bump is low risk, but if Phase 2
behaves oddly, this is a variable that changed underneath it.

## Refine

Chose discovery over a hardcoded engine path after noticing the hash in the folder name - a hardcoded
`ACFUAsce5ab7c1439afbV5` would break silently on the next reinstall, and silently is how this whole
problem arrived.

**Left undone:** `D:\ACF_4.4_replaced_20260820` is 7 GB sitting outside the repo. Delete it once a
build and a PIE session have both come back clean - not before.
