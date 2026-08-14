---
id: 150
title: Register ACF's 40 author-written Claude skills so sessions can see them
agent: claude-acfskills
status: done
claimed: 2026-08-13T23:29Z
build: none
waiting_on:
evaluated: 2026-08-14T00:08:18Z
observed: 2026-08-14T00:07:47Z | A fresh session started in GoblinSiege 5.8 listed all 40 ACF skill packs by name; a fresh session started at D:\goblinRaid listed none of them - discovery is cwd-based, and the root-start failure is silent
scenario: Two new Claude Code sessions started back to back, one with cwd D:\goblinRaid\GoblinSiege 5.8 and one with cwd D:\goblinRaid, comparing the skill list each one loaded
files: 
  - .gitignore
  - Tools/Register-ACFSkills.ps1
  - CLAUDE.md
---

## Goal

Michael, after pasting ACF's Fab description: *"Go on... then go ahead and use the process to
Register the MCP skills"* — the changelog line that prompted it being **"45+ Claude MCP Skills"** in
ACF v4.4.

ACF ships `SKILL.md` packs at `Plugins/Marketplace/AscentCombatFramework/Resources/Skills/`. Claude
Code only discovers skills under `.claude/skills/`, and there was **no such directory anywhere** — not
in the repo, not in the project, not in the user profile. So every session that has ever run against
this repo has been blind to them.

That is not a hypothetical cost. Earlier in this same session I answered Michael's ACF questions by
fanning out three agents to read plugin headers, and produced a build-vs-reuse table — while the
author's own setup guides, wiring steps and **common-failures tables** sat unread on disk. The
`acf-core` pack alone answers "AI not attacking player" with "teams are not registered as hostile in
`UACFTeamManagerComponent`".

## Generate

**Count correction, carried into the title:** the claim said 43. There are **40 skill packs / 43
markdown files** — `chooser-actions` carries three sub-documents beside its `SKILL.md`. I had counted
a glob of `*.md`. The ticket title and CLAUDE.md both say 40.

### `Tools/Register-ACFSkills.ps1` (new)

Copies each `Resources/Skills/<name>/` directory that contains a `SKILL.md` into
`.claude/skills/<name>/`. Directory names are preserved rather than prefixed, because each pack's
frontmatter `name:` already matches its folder and renaming would desync the two.

Three properties that matter more than the copy itself:

- **It writes a manifest** (`.claude/skills/.acf-manifest.json`) recording plugin version, UTC
  timestamp, source root and the exact list of names installed. The next run removes *only* what that
  manifest names. A naive "wipe `.claude/skills` and re-copy" would delete hand-written project skills
  the day anyone adds one; this cannot.
- **It refuses to clobber.** After the manifest-driven removal, anything still occupying a target name
  belongs to somebody else, so it is skipped and reported rather than overwritten.
- **It fails loudly and usefully when the plugin is absent** — which is the normal state of a fresh
  clone, since `Plugins/Marketplace/` is gitignored. The error names the Fab listing.

`-Clean` removes without re-copying. `-Quiet` prints only the summary. ASCII-only source, per the
PowerShell-5.1 BOM trap in CLAUDE.md rule 8.

### `.gitignore` (+8 lines, at :62-69)

`.claude/skills/` is ignored, directly beneath and explicitly reasoning from the existing
`Plugins/Marketplace/` rule: it is marketplace content, it is regenerable, and **the generator is
versioned while its output is not**. Without this the untracked `GoblinSiege 5.8/.claude/` would have
gone into the next commit, putting ~40 directories of another vendor's licensed documentation into our
history.

### `CLAUDE.md` — new §3a

Placed immediately after §3 (VibeUE's `AgentSkillToolset` packs) because the single most likely
failure is an agent conflating the two systems. §3a states plainly that ACF's are **Claude Code
project skills invoked by name**, not `call_tool` skills; that the script must be re-run after an ACF
update; and — the actual point — **read the ACF skill before reading ACF headers.**

It also records the two gaps found while verifying, so nobody trusts a skill's paths blindly:
`/Game/FullSample/` is not present in this install (the sample items, pawns and FX every pack
references live there; free from the ACF Discord to verified Fab customers), and the `Docs/*_Wiki.md`
files the `ai-framework` pack cites do not exist here.

## Evaluate

**Verified by running it, not by reading it back.** A full lifecycle was exercised end to end:

| Check | Evidence |
|---|---|
| First run installs | 40 named `+` lines, `Registered 40 ACF skill(s) from plugin v4.4` |
| Idempotent | Second run: still exactly 40 directories, no duplication, no skips |
| `-Clean` works | `Removed 40 previously registered ACF skill(s)` then `dirs now: 0` |
| Non-destructive to source | Plugin's own `Resources/Skills` still 40 directories after the clean |
| Re-register after clean | Back to 40 |
| On disk | 40 directories, 43 `.md` files, manifest reads `"pluginVersion": "4.4"` |
| Frontmatter intact | All 40 `SKILL.md` open with `---` and carry `name:` + `description:` |
| Names usable as skill ids | 0 mismatches between frontmatter `name:` and directory name, all 40 |
| Actually ignored by git | `git check-ignore -v` -> `.gitignore:68`; `GoblinSiege 5.8/.claude/` no longer appears as untracked |

**NOW OBSERVED, and it found the one thing the install could not see.** A fresh session started in
`GoblinSiege 5.8` lists all 40 packs by name. A fresh session started at `D:\goblinRaid` lists
**none**. Discovery is plainly cwd-based — no reparse-point or loader subtlety involved, which also
retires the junction-vs-copy doubt recorded under Refine: the copy decision stands on its own merits,
not on luck.

That asymmetry is a **silent** failure. A session at the repo root gets no warning; it simply goes on
answering ACF questions out of plugin headers, which is precisely the waste this ticket exists to
stop. Fixed in `D:\goblinRaid\CLAUDE.md` (the repo-root guide, added under the "one level ABOVE the
Unreal project" note): start sessions inside `GoblinSiege 5.8`, and `cd` mid-session repairs the shell
but **not** the already-loaded skill list.

**A self-inflicted error worth recording:** my first frontmatter validation combined `-Raw` and
`-TotalCount` on `Get-Content`, which is illegal. The variable was never assigned, every file failed
an empty test, and it printed `malformed: 40` — a clean, plausible, entirely fabricated failure
report. I caught it because the *other* half of the same command contradicted it. An instrument that
reports total failure deserves the same suspicion as one that reports total success.

**Owed to AGENT_STATE:** ACF ships 40 Claude skill packs and this project ignored them for the
plugin's entire tenure; they are now registered and CLAUDE.md §3a points at them. And the wider
finding behind the ticket — the ACF overlap is not confined to combat. Climbing
(`UACFClimbingComponent`, `UACFLedgeClimbingComponent`, `UACFVaultComponent`, `UACFLadderComponent`,
`UACFGripPointComponent`) and the moveset/overlay animation layering that is the structural answer to
the human-T-pose stopgap in `GSCharacterBase::PlayAnimMontage` were both absent from the migration
plan.

**Touched outside the claim:** `D:\goblinRaid\CLAUDE.md` — the **repo-root** guide, whereas this
ticket's `CLAUDE.md` claim resolved to the project one. Two different files with the same basename,
and the claim did not distinguish them. Michael authorised this edit explicitly ("either append it
before closing or hand me a fresh ticket after") and nobody else was working, so no conflict occurred
— but a claim line that cannot tell two files apart is a real hole in the protocol, not a technicality.
**Claim repo-root files by an unambiguous path.**

`.claude/skills/README.md` was claimed and deliberately **not** written — it would have sat inside the
ignored directory, so the explanation went into the script's header block and CLAUDE.md §3a, where it
survives a clean.

The ticket **file** was renamed `...-43-...` -> `...-40-...` to match the corrected count; `gsqueue
check -Id 150` still resolves it, so the id lookup does not depend on the slug.

## Refine

**Changed after self-review:** the count, everywhere (43 -> 40 packs / 43 files). Left uncorrected it
would have been quoted onward as the number of skills.

**Reconsidered and rejected — directory junctions.** `mklink /J` was the first design: no duplication,
auto-fresh on plugin update. Dropped for two reasons. Git follows junctions and would have tracked the
contents anyway, so the `.gitignore` entry was needed regardless and the main advantage evaporated;
and it stakes the whole feature on the skill loader following reparse points, which is unverified and
would fail *silently* — precisely the failure mode this ticket exists to end. Copying is dumber and
cannot fail that way. The cost is staleness after an ACF update, which the manifest's `pluginVersion`
makes visible and one re-run fixes.

**Deliberately left undone:**

- **No prefixing.** Some names are generic (`teams`, `save-system`, `ui-tools`, `state-machine`). There
  are zero project-authored skills today so nothing can collide, and prefixing would break the
  frontmatter/directory agreement verified above. The script already refuses to overwrite a name it
  does not own, so the collision surfaces as a report rather than a silent overwrite.
- **No auto-run hook.** Wiring this into build or session start was tempting and is a separate call
  about where such hooks belong.
- **Not read.** Registering the packs is not reading them. The migration ordering I gave Michael
  earlier this session was derived from headers and should be re-derived through `ai-framework`,
  `actions-system`, `collisions-manager`, `teams` and `acf-core` before any of it is acted on.
