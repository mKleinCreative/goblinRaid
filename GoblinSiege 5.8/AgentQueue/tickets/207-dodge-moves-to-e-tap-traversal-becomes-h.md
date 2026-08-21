---
id: 207
title: Dodge moves to E (tap), traversal becomes hold-E, LeftAlt retired - and the traversal roll stops stealing the dodge's animation slot
agent: claude-dodgekey
status: done
claimed: 2026-08-20T20:16Z
build: none
waiting_on:
evaluated: 2026-08-20T20:37:26Z
observed: UNOBSERVED 2026-08-20T20:37:27Z - The remap itself was verified against the reloaded asset, but on its own it did NOT fix the bug - a Hold trigger does not stop the Blueprint's Started pin firing on press, so a tap still ran both systems. What Michael watched working is 208, which this ticket only set up. Closed 2026-08-20.
scenario: none - never run
files: 
  - Content/Input/IMC_Default.uasset
---

## Goal

Dodge moves to E (tap), traversal becomes hold-E, LeftAlt retired - and the traversal roll stops stealing the dodge's animation slot

## Generate

`IMC_Default`, three edits, verified by reloading the asset:

| Key | Action | Trigger |
|---|---|---|
| `E` | `IA_Dodge` | **Tap** (< 0.20s) — new row |
| `E` | `IA_Traverse` | **Hold** (>= 0.20s) — was no trigger at all |
| ~~`LeftAlt`~~ | ~~`IA_Dodge`~~ | **removed** |

Michael's ruling after discovering he had been testing dodge on `E` for days, believing it was the
dodge key. It was `IA_Traverse`.

**The `LeftAlt` row was removed rather than left as a second dodge key.** Two keys for one verb is
how this confusion survived as long as it did.

**This makes the binding match a ruling it had been silently contradicting.** The 2026-08-08 climbing
decisions say *"Hold-E climbs. Michael likes it; do not repurpose E as a release verb."* Until now
`E` carried **no trigger**, so it fired on press - the document described a behaviour the input asset
never implemented. It does now.

The key struct for the new row is **copied off the existing `E` row** rather than constructed:
`unreal.Key` is not constructible from Python and guessing cost a round trip.

## Evaluate

**Verified against a reloaded asset**, not a read-back of the in-memory object: 18 mappings, `E` on
both actions with Tap and Hold respectively, no `LeftAlt` row.

**It did NOT fix the bug on its own, and that is the useful finding.** Michael tapped `E` and still
got a wrong roll intermittently. The reason is that a Hold trigger does **not** stop the Blueprint's
`Started` pin firing on the initial press - `Started` fires on press regardless of whether the hold
ever completes. So a tap still ran the traversal chain alongside the dodge. That is #208.

**Unresolved by this ticket and worth stating:** `IA_Dodge` on a Tap means a **held** `E` no longer
dodges at all. That is correct for a tap/hold split, but if dodge-on-hold is ever wanted, Tap is the
wrong trigger and this is where to change it.

## Refine

Chose Tap/Hold over moving traversal to a different key, because the climbing ruling names `E`
specifically and moving it would have contradicted settled, signed-off work to fix an unrelated bug.
The 0.20s threshold is shared by both triggers so there is no dead band between them.
