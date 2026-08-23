---
id: 238
title: AI reaction delay: a beat between noticing and swinging
agent: claude-acf
status: done
claimed: 2026-08-21T18:32Z
build: none
waiting_on:
evaluated: 2026-08-21T18:54:10Z
observed: UNOBSERVED 2026-08-21T18:54:11Z - Built and shipped but not watched, and the frame analysis since suggests it targets the wrong cause - see the Evaluate section and #239.
scenario: none - never run
files: 
  - Source/GoblinSiege/AI/GSAIControllerBase.h
  - Source/GoblinSiege/AI/GSAIControllerBase.cpp
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.cpp
---

## Goal

AI reaction delay: a beat between noticing and swinging

## Generate

`GS.AI.ReactionSeconds` (default 0.35) and `AGSAIControllerBase::HasReactedTo(AActor*)` - a beat
between an AI acquiring a target and being allowed to swing at it. Lazy: the first call for a target
stamps the clock, later calls report whether the beat has elapsed. No tick, no blackboard key.

Gates SWINGING only, in `BTTask_MeleeAttack`, and FAILS rather than waits so the tree keeps closing,
circling and facing during the beat - a task that blocked there would read as a freeze rather than a
reaction. A target SWITCH restarts the beat, so flicking between two enemies does not buy a free
instant swing.

## Evaluate

**BUILT, NOT WATCHED - and probably aimed at the wrong target.**

The complaint was *"there's no buffer with the AI decisions, they twitch... they do decisions right
away instead of having to take some time."* The finding that prompted this was real: target SWITCHING
was already damped (`TargetSwitchHysteresis`, `ReacquireIntervalSeconds`) and turning already
interpolates, but nothing put a delay between a decision becoming true and the AI acting.

**Then Michael added: the same twitch is in the ARCHERS' MOVEMENT, and "they don't keep track of who
they're running away from."** Movement is not gated by this ticket at all, so this cannot be the
whole answer and may not be any of it.

**Frame analysis of CombatTwitching.mp4 found a better candidate** (ffmpeg via `imageio_ffmpeg`;
1284 frames, per-frame motion measured over the viewport crop). Mean inter-frame motion change 3.96;
the burstiest window, frames 1080-1130, scores 13.85. Those frames show a tight pile of goblins
overlapping around the player, heavily motion-blurred - crowding, not fighting, since the player and
the horde are both `Teams.Goblin` and friendly since #229.

**And the cause is in `UGSHordeSubsystem::GetFollowSlotFor`** - see #239. A goblin's follow slot is
its index among currently-valid roster entries, so one death renumbers every goblin behind it and
the whole formation reshuffles, re-evaluated every 0.2s with no latch. That is the twitch, and it is
the same class of bug as the archers not remembering who they flee from: identity derived from a
volatile index instead of held.

**#239 is BLOCKED** - those files belong to claude-warren's #236, which has right of way.

## Refine

Keeping this rather than reverting it: a reaction beat is worth having on its own terms, and
`GS.AI.ReactionSeconds 0` disables it if it proves to be noise. But it should NOT be credited with
fixing the twitch, and #239 is where the actual fix lives.
