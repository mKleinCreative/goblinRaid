---
id: 157
title: GDD 2.8: reverse the hand-authored-only ruling - generator gets a timeboxed attempt with the blockout as fallback
agent: claude-gdd2
status: done
claimed: 2026-08-14T22:55Z
build: none
waiting_on:
evaluated: 2026-08-14T22:58:11Z
observed: UNOBSERVED 2026-08-14T22:58:11Z - A design document has no runtime behaviour to watch. Verified by grep that no hand-authored-only assertion remains. The week-2 bar written into it has NOT been tested against what the composer actually produces - that is the real unproven claim.
scenario: none - never run
files: 
  - ../goblin-siege-design-document.md
---

## Goal

#156 left the GDD self-contradictory and said so: §2.8 recorded a dated, reasoned
hand-authored-only ruling from 2026-07-23, while Michael had that same day decided to *"pick up the
map generation for the tutorial level"*. Asked how to write the reversal, he chose the timebox
framing. This ticket reconciles the document.

Reversing a dated ruling is not the same as deleting it. The July reasoning was correct and is not
refuted by anything that has happened since; what changed is the starting position, and the document
has to say which.

## Generate

**The framing that made this cheap: separate "the generator authors ONE hamlet" from "the generator
varies the layout raid to raid."** Only the first is now in scope. The second stays post-slice and
funding-gated exactly as before. That distinction keeps most of the document true as written -
lines 17 and 49 both say "the post-slice generator varies the layout raid to raid", which needed no
edit at all - and confines the reversal to the passages that actually asserted hand-authored-only.

**§2.8, the ruling itself (line 137)** - rewritten from one paragraph into four:

1. The new ruling, dated, explicitly superseding 2026-07-23.
2. The old reasoning restated and **conceded** - "Nothing about that argument has been refuted."
3. What actually changed: the composer, the measured roof tiler and the learned style guide were
   built after July, and the abandoned stamping work left a dissection of 44 hand-authored buildings
   as a reusable kit reference. "The generator is no longer a blank page with a seven-week clock in
   front of it - it is a partly-built system with a known failure rate. That is a different bet than
   the one refused in July."
4. The timebox as a **rule, not a preference**: a walkable Groatsworth - three module types placed,
   roads connecting them, no objective sited somewhere unreachable - by end of week 2, or the
   blockout wins and the generator returns to post-slice. Plus why the switch is cheap (same kit,
   same deliverable, different order) and why the bet is worth taking at all (every rung above tier 1
   needs the generator, and this is the only rung where a hand-authored answer exists to compare a
   bad seed against).

**Five consistency edits elsewhere**, all of which asserted the old ruling:

| Line | Change |
|---|---|
| 9 (header scope) | "one hand-authored tutorial hamlet raid" -> the timebox, with per-raid generation still post-slice |
| 21 (overview) | "One hamlet, **hand-built** to teach" -> "One hamlet, **built** to teach"; the descoping sentence now descopes raid-to-raid variation specifically, not the generator wholesale |
| 88 (crouch/cover) | The cover-placement rule was "hand-placed to guarantee broken sightlines... the same rule the post-slice generator will enforce once it exists". Now: hand-placed = a review pass, generated = enforced algorithmically from day one - "which makes this the sharpest test of whether the generator is ready, and a fair reason to fail the week-2 timebox" |
| 151 | "the same prefab kit is what the post-slice generator will eventually arrange" -> the kit is hand-made either way; only its arrangement is in question |
| 157 (Avery Shelly failure mode) | Was "genuinely moot for *this* slice, since no generator runs at raid start". Now **half live**: still no per-raid seed gate, but if the generator authors Groatsworth that layout **is** a seed and gets reviewed like one. Names the distinction between the two levers - "patching one bad seed keeps the generator; missing the timebox retires it for the slice" |
| 233 (roadmap status) | "Status: descoped from procedural to hand-authored this revision" -> "one hamlet, generated if the timebox clears and hand-authored if it doesn't (revised 2026-08-14)" |

Also carried the bind cut into line 233, which #156 missed - it still said "both the takedown and the
bind choice".

## Evaluate

**Verified by grep.** `descoped from procedural|hand-authored this revision|deliberately descoped`
returns nothing. "timebox" appears 5 times across the sections that need it (header, overview,
crouch/cover, the ruling, the roadmap status). One file modified.

**The strongest thing about this edit, and the reason to keep it:** the document now records a
*falsifiable* condition with a date and an owner. "Walkable Groatsworth by end of week 2" can be
checked; "we prefer procedural" cannot. The July ruling was similarly concrete, which is why it was
possible to reverse it honestly rather than quietly drift away from it.

**Unverified, and it is the whole risk:** nothing here has been tested against the actual generator.
The week-2 bar was written by me from the GDD's own wayfinding and cover requirements, not measured
against what the composer currently produces. #068 was abandoned at a 1/8 pass rate; whether the
post-#066 composer clears "walkable, roads connecting, no unreachable objective" in two weeks is
genuinely unknown. **If that bar turns out to be either trivially met or absurdly out of reach, it
is the wrong bar and should be re-cut before week 2 rather than argued about during it.**

**Owed to AGENT_STATE:** the 2026-07-23 hand-authored-only ruling is superseded; the tutorial hamlet
now has a week-2 generator timebox with a hand-authored fallback, and the cover-placement rule is the
named test.

## Refine

**Changed after self-review:** the first pass wrote the timebox as "if the generator isn't working by
week 2 we'll hand-author it", which is a preference dressed as a rule - it invites relitigating in
week 2 by whoever is most invested. Rewritten so the fallback is stated as not-arguable ("not a defeat
condition to be argued with; it is the thing that keeps a failed generator from costing the slice its
map") and the bar is three checkable properties rather than a feeling.

**Deliberately left undone:**

- **The stale duplicate GDD** at `Tools/CodeArchitect/docs/goblin-siege-gdd.md` still carries the
  granary AND the old ruling. It was not in this claim. It should be deleted or turned into a pointer
  before someone quotes it - two copies of a design document is the failure CLAUDE.md already
  complains about in another context.
- **The banked prompt content** (`content-pipeline/out/prompts.trace.md` and siblings) still
  references the granary and will need regenerating against the new roster. Flagged in #156.
- **The settlement-generator spec** is referenced by §2.8 twice and may carry its own hand-authored
  assumptions. Not read, not claimed.
