---
id: 167
title: Supplement to #165 - GSCombatDebugEnabled had no declaration and only linked via unity build
agent: claude-smash
status: done
claimed: 2026-08-17T01:48Z
build: none
waiting_on:
evaluated: 2026-08-17T01:51:18Z
observed: UNOBSERVED 2026-08-17T01:51:19Z - A missing declaration has no runtime behaviour. Proven by the build: C3861 gone. The debug drawing it gates was already working.
scenario: none - never run
files: 
  - Source/GoblinSiege/Weapons/GSArrowProjectile.cpp
---

## Goal

Supplement to #165 - GSCombatDebugEnabled had no declaration and only linked via unity build

## Generate

Declared `GSCombatDebugEnabled()` in `Weapons/Abilities/GSGA_SwordLight.h` (where the `GS.Combat.Debug`
cvar it reads already lives) and included that header from `Weapons/GSArrowProjectile.cpp`.

## Evaluate

**The function was defined in `GSGA_SwordLight.cpp` and declared in NO header.**
`GSArrowProjectile.cpp:315` called it and compiled only because Unreal's unity build happened to
merge both files into one translation unit. Adding an unrelated `#include` to GSGA_SwordLight.cpp
re-split the blob and the call became `error C3861: identifier not found`.

So this was a live latent bug with a fuse in it: ANY future include change, in either file, would
have detonated it, and the error would have pointed at the arrow rather than at the missing
declaration. It surfaced during #165 only by luck.

Build succeeded after the fix.

## Refine

**Considered and rejected: a forward declaration in GSArrowProjectile.cpp.** One line, no include,
and it would have left the next caller to rediscover the same thing. A free function used across
files needs a real declaration in a header, not each caller privately promising the linker it exists.

**Left undone:** no sweep for other functions in this module that are cross-file-called without a
declaration. Unity builds hide these by construction, so a grep would not find them reliably - they
only appear when the blob re-splits. Worth knowing the failure mode rather than hunting it.
