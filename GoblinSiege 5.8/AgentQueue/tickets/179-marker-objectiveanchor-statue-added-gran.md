---
id: 179
title: Marker.ObjectiveAnchor.Statue added, .Granary retired (GDD 156 roster) - WRITTEN, staged for the next build window
agent: claude-anchortags
status: review
claimed: 2026-08-18T01:25Z
build: required
waiting_on:
evaluated: 2026-08-18T01:27:54Z
observed:
scenario:
files: 
  - Source/GoblinSiege/Combat/GSGameplayTags.h
  - Source/GoblinSiege/Combat/GSGameplayTags.cpp
---

## Goal

Marker.ObjectiveAnchor.Statue added, .Granary retired (GDD 156 roster) - WRITTEN, staged for the next build window

## Goal (context)

Follow-on to #170. #156 removed the granary from the GDD on 2026-08-14; the required trio is
Market / Statue / Windmill with the wheat field demoted to optional. The C++ marker-anchor tag
set never followed: it still declared `Marker.ObjectiveAnchor.Granary` and had **no Statue
anchor at all**, so a generated layout has no tag to name the position of one of the three
required objectives.

**Michael asked for this to be written now and left staged for the next build window** (the
gate is shut on six open tickets). It is therefore WRITTEN AND UNCOMPILED. Do not close this
ticket on the strength of the diff.

## Generate

Two files, both claimed, nothing else touched.

**`Source/GoblinSiege/Combat/GSGameplayTags.h`**
- Added `UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_ObjectiveAnchor_Statue)`.
- Removed `UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker_ObjectiveAnchor_Granary)`.
- Reordered the anchor block into the required trio (Market, Statue, Mill) then the optional
  Field, with a comment recording the 2.8 roster and why the granary went.

**`Source/GoblinSiege/Combat/GSGameplayTags.cpp`**
- Matching `UE_DEFINE_GAMEPLAY_TAG_COMMENT` for `Marker.ObjectiveAnchor.Statue`: "Where the
  king's statue belongs - the village square, where the guards are thickest. Toppled, never
  burned."
- Removed the `Marker.ObjectiveAnchor.Granary` definition.
- Same reorder and comment.
- **One extra one-word fix inside a claimed file:** `Marker.CoverProp`'s comment cited the
  cover-guarantee rule as "GDD 2.8". It is 2.4, restated at 3.1 — 2.8 carries neither
  sentence. Same mis-citation #170 corrected across `level-gen/`; this was the last copy of it
  in `Source/`.

## Evaluate

**Nothing here is verified. There has been no compile and no PIE.** The build gate was closed
on six tickets (#169, #172, #173, #174, #175, #176) for the whole of this work, so
`Build-GoblinSiege.ps1` was never run. This ticket is `review` with `build: required` and must
not be closed until someone compiles it and confirms the tag resolves.

What *is* checked, statically, and how:

- **Retiring the Granary anchor breaks no caller.** `grep -rn "Marker_ObjectiveAnchor_Granary"`
  over `Source/` returned exactly two hits before this change — its own declaration and its own
  definition. Nothing queried it. After the change the only hit repo-wide is the explanatory
  comment in the header.
- **No config to update.** `Config/` has no `ObjectiveAnchor` entries; these are native tags
  declared in C++, not `DefaultGameplayTags.ini` rows, so there is no second place holding a
  stale copy.
- **`AGSRaidMarker::GatherByType` needs no change.** It matches on the parent with `MatchesTag`
  (`GSRaidMarker.cpp:67`), so `Marker.ObjectiveAnchor` finds the new `.Statue` automatically.
- **Nothing in `level-gen/` referenced these tag names.** `apply_in_editor.py` has no
  `ObjectiveAnchor` or `Marker.` strings, and it has never been run.

**Risk if the compile fails:** low but real and named — `UE_DEFINE_GAMEPLAY_TAG_COMMENT` /
`UE_DECLARE_GAMEPLAY_TAG_EXTERN` pairs must match exactly, and a typo between the two is a link
error rather than a compile error, so it will surface at the *end* of the build.

## Refine

Nothing changed after writing it, because nothing could be run to change it in response to.
Recording what was deliberately left out instead:

**The bigger granary surface is NOT touched, on purpose.** The gameplay tag was the small,
consumer-free half. Four more files still carry the granary, and they are a different and
riskier job:

- `Destruction/GSDestructibleObjective.h/.cpp` — tags itself with the **actor tag**
  `FName("Objective.Granary")`, and per #156 this is the very class the Statue is supposed to
  land on (a GeometryCollection released on destruction — what toppling stone wants and what
  burning never did). So the class is right and its tag is wrong.
- `Missions/GSObjective_BurnGranaries.h/.cpp` — an entire `UCLASS` named for granaries, whose
  `GranaryActorTag = TEXT("Objective.Granary")` is what counts them.

Renaming a `UCLASS` and an actor tag breaks Blueprint and level references that only the editor
can enumerate, and `AGSObjective_BurnGranaries` may well be placed in `L_Tutorial_Island`. That
is an editor-open job with a walkthrough at the end of it, not something to smuggle into a
two-file tag ticket while the gate is shut. **Wants its own ticket.**

Also left undone: no `Marker.ObjectiveAnchor.House` was added. 2.8 lists houses as optional
objectives alongside fields, but nothing places house anchors today and a tag with no producer
and no consumer is the exact thing this ticket is removing.
