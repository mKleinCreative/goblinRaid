---
id: 166
title: Supplement to #165 - AscentCombatFramework must be named explicitly to LINK, not just include
agent: claude-smash
status: done
claimed: 2026-08-17T01:46Z
build: none
waiting_on:
evaluated: 2026-08-17T01:51:17Z
observed: UNOBSERVED 2026-08-17T01:51:18Z - A build-config line has no runtime behaviour. Proven by the build: same source, one module added, LNK2019 gone.
scenario: none - never run
files: 
  - Source/GoblinSiege/GoblinSiege.Build.cs
---

## Goal

Supplement to #165 - AscentCombatFramework must be named explicitly to LINK, not just include

## Generate

Added `"AscentCombatFramework"` to `PublicDependencyModuleNames` in
`Source/GoblinSiege/GoblinSiege.Build.cs`, directly under `AIFramework`, with a comment recording why
the paragraph above it is wrong.

## Evaluate

**The existing comment claims listing ACF modules explicitly "would be noise that hides which one we
actually reach for", because AIFramework's own dependencies carry them transitively.** That is true
for INCLUDE PATHS and false for LINKING. `#include "Components/ACFDestructableComponent.h"` compiled
without complaint; the link then failed with `LNK2019` on `Z_Construct_UClass_UACFDestructableComponent`
and `UACFDestructableComponent::ForceDestruction`.

The distinction is invisible until the first time the project CALLS into ACF rather than merely
deriving from a type it re-exports - which is exactly what #142/#143 did and why nobody hit this
before. Build succeeded immediately after.

Verified by the build, not by reasoning: same source, one line added, link error gone.

## Refine

**Left the original paragraph in place rather than deleting it**, with the correction beneath. It
explains a real decision about transitive dependencies that is still mostly right; silently deleting
it would lose the reasoning and invite someone to "tidy up" the explicit entry later.

**Left undone:** no audit of which OTHER ACF modules the project calls into versus merely includes.
The rule is now written down - add a module the moment you call into it - which is cheaper than an
audit that goes stale.
