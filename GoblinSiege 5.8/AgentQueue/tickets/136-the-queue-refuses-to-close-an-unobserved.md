---
id: 136
title: The queue refuses to close an unobserved ticket: observed/scenario fields, evidence ladder, loud escape hatch
agent: claude-gate
status: done
claimed: 2026-08-12T03:42Z
build: none
waiting_on:
evaluated: 2026-08-12T03:48:15Z
files: 
  - GoblinSiege 5.8/AgentQueue/gsqueue.ps1
  - GoblinSiege 5.8/AgentQueue/QUEUE.md
  - GoblinSiege 5.8/CLAUDE.md
  - CLAUDE.md
observed: 2026-08-12T03:48:32Z | ran six gate tests: done refused a fully-written reviewed ticket that had no observation, the phrase list rejected an artifact-describing sentence, and -Unobserved closed with a permanent board marker
scenario: gsqueue.ps1 driven directly in PowerShell against throwaway tickets 137 and 138, both then deleted and the board re-rendered
---

## Goal

The queue refuses to close an unobserved ticket: observed/scenario fields, evidence ladder, loud escape hatch

Michael, 2026-08-11, after I handed him four broken passes at directional locomotion in one session:
*"Can you create a fix in your process to stop making the same mistakes? use the workflow to
analyze your failures so you can give me what I want."*

## Generate

### The finding that decided the design

`AGENT_STATE.md` already carries a FAILED entry titled *"THREE FIXES IN A ROW SHIPPED WITHOUT ANYONE
WATCHING THEM RUN, AND ALL THREE WERE WRONG"* (#113/#116/#118). **Ticket #120 exists solely to record
that lesson - and #120 was itself closed unwatched.** Its own Evaluate reads *"No PIE. Nobody has
watched a fight."* Every mechanical gate passed it.

`gsqueue.ps1` had not been modified since 2026-08-06. **Nothing mechanical was ever added in response
to that pattern; the entire response was prose.** The pattern then recurred twice within 24 hours
(#133's blendspace, #135's horde tick).

An audit of the four process documents found "verify with evidence, not assumption" stated in **5
normative locations and ~12 case-law restatements**, plus ~15 per-asset verification recipes. The
message is not under-stated. **The prose layer is saturated and the enforcement layer never looks at
content**, so this ticket adds no new exhortation - it adds a gate.

### 1. Two new frontmatter fields and a verb

`observed:` (what was seen) and `scenario:` (what it was run in), added exactly as `evaluated:` was:
a local in `Read-Ticket`, a `switch` arm, a property on the returned object, and a line in the
`Invoke-Claim` heredoc. No backfill needed - `Set-TicketField` inserts before the closing fence, the
same reason 23 tickets with no `evaluated:` line still parse.

New verb: `observed -Id <n> -What "..." -Scenario "..."`, both required.

**Two fields rather than one, because two different things go wrong.** `-What` catches "I never
looked". `-Scenario` catches the more expensive one: #132 and #133 were both tested with
`GS.Combat.Duel`, which spawns **defenders**, so neither ever ran on a horn-summoned goblin; #133's
blendspace was signed off from the player pawn, which exercises **one of its five direction
columns**. The audit called wrong-scenario evidence *"arguably the project's most repeated failure"*
and noted it *"has no representation at all in the rules docs"*. Now it has a field.

### 2. The refusal, and the ladder printed inside it

A new gate in `Invoke-Done`, modelled on the review gate it sits beside. It is the **first check in
the file that asks whether the work ran** - every other one inspects text and timestamps.

The refusal prints an evidence ladder (human watched it > runtime log line in the right scenario >
screenshot you opened > static re-read / compile / tool return value, marked NOT evidence). That
ranking previously existed only as scattered prose across ~800 lines of `AGENT_STATE.md`. **Putting
it in the refusal is the point: it is now shown at the moment the decision is actually made**, which
is the one place it has never been.

### 3. The non-evidence phrase list, and where it is deliberately NOT applied

`compil`, `read back`, `reads back`, `read-back`, `should work`, `by inspection`, `builds clean`,
`build succeeded`, `no errors`, `no warnings`, `samples match`, `looks correct`, `looks right`,
`verified the asset`, `up to date`.

**Scanned only against the one-line `observed:` field - never against Generate/Evaluate/Refine.**
This follows the precedent already set in this file: an italics-based placeholder heuristic was
rejected because real writeups are full of underscores. A phrase scan over prose has the same defect
in a worse form - **a good Evaluate discusses these phrases in order to disclaim them**, so scanning
prose would punish precisely the honest writeups it exists to reward.

### 4. The escape hatch, loud by construction

`done -Id <n> -Unobserved "<reason>"` closes the ticket, writes `observed: UNOBSERVED <stamp> -
<reason>`, prints a warning, and marks the row `**UNOBSERVED**` on the board permanently. It takes a
reason string rather than being a bare switch so it cannot be typed reflexively.

Shipping unwatched stays possible on purpose - a broken editor must never deadlock the queue - but
becomes loud, attributable and permanently visible instead of silent and routine.

### 5. Documentation: corrections, not additions

- `QUEUE.md` gains rule **3a** and the evidence ladder as a table, with the #120 story as its
  justification.
- **Two live contradictions found by the audit, both fixed.** `GoblinSiege 5.8/CLAUDE.md` said stop
  after **2** failed attempts while `QUEUE.md` rule 3 grants **3**; the queue rules existed in three
  copies claiming **four**, **five** and **six** rules. Both `CLAUDE.md` copies now defer to
  `QUEUE.md` as the single source instead of restating it.

## Evaluate

**The gate was exercised, not reasoned about.** Six tests against two scratch tickets (#137, #138,
both deleted afterwards and the board re-rendered):

| test | expected | result |
|---|---|---|
| `done` with placeholders | REFUSE | REFUSED (pre-existing gate, still works) |
| `done` with G/E/R written, reviewed, never observed | **REFUSE** | **REFUSED** - the new gate |
| `observed -What "compiles clean and the samples match"` | **REFUSE** | **REFUSED** on `compil` |
| `observed` with a real observation | accept | accepted, both fields stamped |
| `done` after observing | close | closed |
| `done -Unobserved "..."` | close + mark | closed, warned, board shows `**UNOBSERVED**` |

**The pass/fail criterion set in the plan was #120, and it passes:** a ticket whose Evaluate says
*"No PIE. Nobody has watched a fight"* can no longer reach `done` silently. Test 2 is exactly that
situation - full G/E/R, stamped review, no observation - and it is refused.

`gsqueue.ps1` still parses clean (`[Parser]::ParseFile`, **0 errors**) and is still **pure ASCII, no
BOM** (41,677 bytes, 0 bytes > 0x7F, first bytes `#Re`) - both verified by byte scan, because a
single em-dash in this file breaks the *parse* with a misleading error, and PowerShell 5.1 reads a
BOM-less UTF-8 `.ps1` as ANSI.

**What is NOT verified, stated plainly:**

- **No other agent has used the new verb.** Everything above is me driving my own tool. The friction
  cost on a real ticket - whether `-Scenario` gets filled honestly or becomes a ritual phrase - is
  unknown and is the most likely way this degrades.
- **The phrase list is a heuristic and will produce false positives.** "the compilation of goblins
  dispersed correctly" contains `compil`. The mitigation is that the failure is loud, instant and
  trivially worked around by rephrasing - unlike the failure it replaces, which was silent.
- **A determined agent can still defeat this** by writing a plausible sentence into `-What` without
  looking at anything. No gate can prevent lying; this one removes *drifting* into it by making the
  claim explicit, separate, and impossible to satisfy with a compile result.

**Owed to `AGENT_STATE.md`:** a DECISION line - *closing a ticket now requires an observation;
`-Unobserved` is the honest escape and marks the board* - and the correction that the 2-vs-3 attempt
contradiction is resolved in favour of QUEUE.md's three.

## Refine

**Changed in response to my own review:** the phrase scan was originally going to run over the whole
Evaluate section. Reading this script's own history stopped me - the placeholder check carries a
comment recording that an italics heuristic was rejected for eating genuine prose. My own Evaluates
routinely quote "should work" in order to disown it, so the first design would have refused the best
writeups in the repo. Moving the scan to a dedicated one-line field keeps the check sharp and
removes the false-positive surface entirely.

**Deliberately left undone:**

- **No claim-time requirement to declare the intended observation.** It was tempting - naming the
  test before doing the work is what would have made me notice that no animation instrument existed
  - but `-Files` is currently optional, and adding a required parameter to `claim` would break every
  documented call site including the ones in both `CLAUDE.md` copies. The gate at close time is
  where the leverage is.
- **The ~15 orphaned per-asset verification recipes are not yet indexed** into one table. That is
  real and worth doing; it is documentation reorganisation and does not belong in the same ticket as
  the gate.
- **Nothing enforces the scenario is a GOOD one.** "PIE, as the player" satisfies `-Scenario`
  perfectly and would not have caught the blendspace. The field makes the choice visible and
  reviewable, which is the most a text field can do; the structural answer is the instrument in the
  companion ticket, which reports every pawn at once so coverage stops depending on memory.
