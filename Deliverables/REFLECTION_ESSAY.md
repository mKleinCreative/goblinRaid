# Pipeline Audit & Cost Analysis

## Pipeline Production & Functionality

**What the pipeline produced (present in the playable build):** the ACF combat-framework
migration (weapons, damage, teams), horde AI (summon/follow/orders via the Warren), fire and
Chaos-fracture destruction across 42+ buildings and field terrain, an audio mixer, a
world-corruption state system, the front-end menu stack, and — as of the final packaging pass —
the first complete Windows Shipping build in the project's history, produced by 395 completed
agent tickets over 143 commits.

**Manual steps still in the loop:** (1) *Verification* — every ticket requires a human or a
runtime log watching the actual behavior before it's trusted; three tickets that skipped this
(#113/#116/#118) shipped wrong. (2) *Design judgment on ambiguous fixes* — `BP_GrappleHook`'s
corrupted rope logic was left for a human to re-author rather than guessed at blind, since no
agent had the missing design intent. (3) *Credentialed actions* — the Drive upload for this
project's playable link is a human action by construction.

**What 100% automation would require:** not better engine access — VibeUE already drives Unreal
directly, no manual reformatting. It needs an agent that can watch its own PIE session
(`CaptureViewport`, on-screen reads) and adversarially self-verify with the same discipline the
queue enforces on humans (`observed`/`-Unobserved`, rule 3a) — without that, autonomous closing
just reproduces the #113/#116/#118 failure at scale.

## Architectural Reflection

**Decision to change:** the observed-evidence gate (rule 3a, ticket #136) and the single-file
shared memory (`AGENT_STATE.md`) were both built *reactively*, after real incidents cost real
time — a same-file collision between two agents (`GSPlayerCharacter.cpp`) forced the whole
claim-based queue into existence; three unwatched "done" tickets shipping wrong forced the
observed-evidence gate; a stale handoff banner re-inviting already-fixed work forced the
memory-file consolidation.

**Specific alternative:** build both from session zero as non-negotiable defaults — "no ticket
closes without a described, scenario-specific observation" as rule zero, and one distilled memory
file instead of scattered docs from the start — rather than discovering each the expensive way
after a specific failure exposed it.

## Cost Analysis

**Total actual run cost:** this pipeline runs on a flat-rate Claude subscription, not
per-token billing, so there is no per-project dollar total to report. A measured, representative
session (Claude Code's own `/cost` readout): **$6.99** in API-equivalent cost for an 18m52s
session, 98% of input tokens served from prompt cache.

**Most expensive step:** a single diagnostic workflow — root-causing a fatal engine `SavePackage`
crash via 5 parallel/sequential agents (2 engine-source research agents, project-history, fix,
verify) — cost **398,087 tokens** in one run. Pure investigation overhead: it shipped no content
directly, and existed only because 3 cheaper manual fix attempts had already failed.

**Sustainability:** financially solo/small-team viable at this scale — even that difficult session
used ~4% of a session budget and the day's total work used **11%** of a full week's subscription
allowance. The real cost lever is spawning subagents by default rather than deliberately: 100% of
recent usage came from subagent-heavy sessions, 79% above 150k tokens of context.

## Mid-Project Cost-Reduction Change

**Strategy — Before:** long-running jobs (engine cooks, headless regeneration scripts) were
watched via `ScheduleWakeup` polling — a timer fires, re-enters full conversation context, checks
progress, reschedules if not done.

**Strategy — After:** switched to launching them as background processes with a completion
**task notification** — zero cost while running, one re-entry exactly when there's real news.

**Token cost — Before:** two polling loops from one debugging session cost **1.4m and 1.2m
tokens** (~2.6m combined) for routine progress-checking.

**Token cost — After:** the background-notification jobs that replaced the pattern for the rest
of the session cost only tokens spent once real results existed — no polling tax.
