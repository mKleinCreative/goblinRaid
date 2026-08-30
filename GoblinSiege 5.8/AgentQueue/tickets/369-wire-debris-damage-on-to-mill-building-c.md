---
id: 369
title: Wire debris damage on to mill/building crumble
agent: claude-fire
status: done
claimed: 2026-08-30T05:06Z
build: none
waiting_on:
evaluated: 2026-08-30T05:11:51Z
observed: 2026-08-30T05:11:52Z | Player killed outright by falling debris on first live test
scenario: Live PIE, L_Tutorial_Island, standing near falling mill/building debris
files: 
  - Source/GoblinSiege/Destruction/GSMillObjective.cpp
  - Source/GoblinSiege/Destruction/GSBuildingObjective.cpp
---

## Goal

Wire debris damage on to mill/building crumble

## Generate

#368 built the mechanism (`UGSCrumbleComponent::bEnableDamageFromCollision`), off by default. This
ticket turns it on at the three places this project already crumbles something with real force
behind it: `AGSMillObjective::SinkTower`'s falling top cap (`TopDropImpulse` 400,000),
`AGSMillObjective::DropSails`'s loose parts (sails/roof, flung sideways), and
`AGSBuildingObjective::CrumblePieces`'s collapse ring (`CollapseShoveMagnitude` default 5,000,000).
One line each, set alongside the existing `KeepAnchoredBelowFraction`/`CharAmountOnRelease` tuning
at each call site - no new plumbing needed since #368 already did that work.

Rebuilt together with #368, same pass.

## Evaluate

**Verified live.** Michael stood near falling debris and was killed by it - see #368's Evaluate for
the exact quote. Not isolated which specific piece (cap vs. sail vs. building debris) landed the
kill, since the mechanism is identical at all three sites and the point was "does flying geometry
kill," not "which flavor of flying geometry."

## Refine

Closing `done`. Worth a follow-up, not urgent: statue/monument crumbles (`UGSTopplableComponent`'s
callers) were deliberately left OFF - a toppled idol was never part of "flying geometry" in
Michael's framing, and turning it on everywhere by default would be a bigger behavior change than
asked for. Easy to flip on later if wanted.

## Generate

<!-- REPLACE: what you produced. Files touched, what each change does, the calls
you made. Delete this comment when you write the section. -->

## Evaluate

<!-- REPLACE: judge your own output against the goal, adversarially. What is
verified and by what evidence (a log line, a PIE observation, a compile result -
not "should work"); what is written but has never run; what you touched outside
the goal; the DECISION or FAILED line this owes AGENT_STATE.md. -->

## Refine

<!-- REPLACE: what you changed in response to your own evaluation, and what you
are deliberately leaving undone. "Nothing changed, and here is why the first pass
survives scrutiny" is a valid answer; silence is not. -->
