# Pre-Build Declaration — Assignment #6

**Goblin Siege** · Michael Klein · written 2026-08-06, **before any pipeline code**

---

**1. What content type does my game currently generate manually, inconsistently, or not at all?**

Hamlet layouts, and the buildings inside them. The tutorial hamlet was hand-placed once
(GDD §2.8) and the procedural generator was descoped. Every additional settlement is manual
work that does not exist.

**2. What specific rule from my GDD must every piece of that content satisfy?**

§2.8's **cover-guarantee rule**: broken sightlines between the treeline and every burn
objective. Secondary, same section: exactly three objectives, never more than two of a kind.

**3. What does a failure look like — concretely, in my game's terms?**

A granary standing in unbroken line-of-sight from the treeline. The player is confirmed
before the first spark, "First Spark Unseen" (+40, §2.9) becomes unwinnable, and §2.4's
"quiet half of the raid" is dead on that map — the level still loads and looks fine. Week 1
shipped the physical version twice: a road through a house, a windmill on an unbuildable
rock face.

---

*116 words of answers (excluding the three question prompts). Limit: 150.*

---

## Addendum — 2026-08-17

**The text above is unchanged and stays that way.** It was written on 2026-08-06, before any
pipeline code, which is the only thing it is evidence of. It is not being edited to match
what the GDD says today.

What changed: on **2026-08-14** (queue #156) Michael ruled the **granary out of the GDD
entirely** — *"when initially written I had meant a different building, but had never created
it. Windmill, Market and Statue works fine."* The required trio became **Market / Statue /
Windmill**, and the wheat field was demoted from a required objective to an optional one.
The granary had no mesh, no Blueprint and no placed instance; it was the emptiest thing in
the tutorial slice.

Answers 1 and 2 survive that ruling intact. The content type is still hamlet layouts and
their buildings, and the rule is still the cover guarantee — *broken sightlines between the
treeline and every objective*.

**One correction to answer 2, found by checking its own citation.** The declaration attributes
the cover guarantee to §2.8. It is not there. The rule is stated at **§2.4** — "if the
generator authors it, the cover-placement rule has to be enforced algorithmically from day
one" — and restated at **§3.1** — "it is the rule a generated layout has to enforce
algorithmically rather than by review." The "never more than two of a kind" clause is **§1**;
§2.8 specifies the fixed one-of-each tutorial layout and names the randomized roll as the
thing it replaces. §2.8 governs the hamlet this generator targets and carries the roster, but
it carries neither sentence about sightlines.

The rule the pipeline enforces has not changed. Only the section numbers have, and they now
point at the sections that contain the words.

Answer 3's example changes actor and nothing else: **the statue** now stands in the village
square where the granary used to, at the same central radius and for the same stated reason
(it sits where the guards are thickest). An objective standing in unbroken line of sight from
the treeline still kills "First Spark Unseen" (+40, §2.9) and still guts §2.4's quiet half of
the raid, on a level that still loads and still looks fine.

The pipeline did not notice any of this on its own, and that is recorded as a finding in
`README.md` — an evaluator can be silently invalidated by an edit to the document it cites.
