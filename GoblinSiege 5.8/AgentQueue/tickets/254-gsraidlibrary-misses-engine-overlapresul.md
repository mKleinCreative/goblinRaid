---
id: 254
title: GSRaidLibrary misses Engine/OverlapResult.h so FOverlapResult is undefined
agent: claude-acf
status: done
claimed: 2026-08-22T22:05Z
build: none
waiting_on:
evaluated: 2026-08-22T22:07:05Z
observed: UNOBSERVED 2026-08-22T22:07:06Z - A missing include has no runtime behaviour to watch - the file did not compile at all before. Evidence is the three C2027 errors disappearing from the next build.
scenario: none - never run
files: 
  - Source/GoblinSiege/Raid/GSRaidLibrary.cpp
---

## Goal

`GSRaidLibrary.cpp` failed with `use of undefined type 'FOverlapResult'` at three sites, blocking the
whole module. Held by no open ticket, so fair to take.

## Generate

Added `#include "Engine/OverlapResult.h"`. In UE 5.8 `Engine/World.h` only FORWARD-declares
`FOverlapResult`, so a translation unit that ITERATES overlap results rather than merely issuing the
query must include it itself. Commented in place, because the failure reads like a missing module
dependency rather than a missing header.

## Evaluate

Verified: the three `C2027` errors and the consequent `C2737` are gone from the next build. Nothing
else changed and no behaviour was touched - this file did not compile before, so there is no
before/after behaviour to compare.

The build still fails, on `GSWarrenPlacementComponent.cpp:251` (ambiguous `AddIgnoredComponent`),
which belongs to ticket #245 and is deliberately NOT touched.

## Refine

Nothing. A missing include has one correct fix.
