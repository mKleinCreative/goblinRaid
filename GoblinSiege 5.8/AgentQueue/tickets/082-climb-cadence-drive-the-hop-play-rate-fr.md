---
id: 082
title: "Climb cadence: drive the hop play rate from actual speed instead of a fixed constant"
agent: claude-climbrebuild
status: done
claimed: 2026-08-08T21:15Z
build: none
waiting_on:
evaluated: 2026-08-08T21:19:38Z
files: 
  - Content/Characters/ScoutV2/Animations/ThirdPerson_AnimBP_Gob.uasset
---

## Goal

The fixed `ClimbUp` play rate has now been wrong three times - 7.0 (implies 324 uu/s), then 6.33
(implies 293), against a session that actually climbed at **201 uu/s**. A constant cannot describe a
speed that varies per climb. Michael: *"go with your recommendation"* - drive it from real speed.

## Generate

**`ClimbUp`'s `PlayRate` is now an exposed pin, fed by a new `ClimbPlayRate` variable.**

Computed at the end of `BlueprintUpdateAnimation`:

```
curZ    = GetActorLocation(PawnOwner).Z
rate    = clamp( |curZ - ClimbLastZ| / DeltaTimeX / 46.3 , 0.60 , 9.00 )
ClimbLastZ = curZ
```

`46.3 uu/s` is the clip's own vertical speed, measured from its root track
(`A_MX_Braced_Hop_Up_Gob` rises **77.1uu over 1.667s**). So the rate is literally
*achieved climb speed / clip speed* - the definition of "no foot sliding", at any speed.

Exposing the pin needed the anim node's `ShowPinForProperties`: `PlayRate` is `overridable=True`,
so setting `show_pin=True` on that entry and then `refresh_node` materialises the pin. Setting the
property alone is not enough - the node reported `PlayRate` as shown while its pin list was still
just `['Pose']` until it was reconstructed.

`ShimmyL`/`ShimmyR` keep their measured constants (2.98 / 3.72) from #081.

## Evaluate

**I nearly shipped this reading the wrong quantity, and the log is what caught it.** The obvious
source is `GetVelocity()`, and it is wrong here: in the #080 log, `vel=Z=420.302` held steady across
frames 334-340 while `dZ` was **0.00** - the character was completely stationary against the eave.
`GetVelocity()` during the climb reports what `ClimbTick` *commanded*, not what happened. Feeding it
in would have pinned the rate at the 9.00 clamp, i.e. worse than the 7.0 this ticket exists to
replace. Actor-position delta over `DeltaTimeX` is the only source here that reports reality.

**This is the same class of bug as the fixed inset, and the third time today.** Inset, then play
rate, both "tuned" against one sample and both wrong on the next. The fix in both cases was to stop
choosing a number and derive it. That is the pattern worth carrying forward, not the specific values.

**NOT PLAYED.** Six wiring checks pass on readback, `UP_TO_DATE`, saved. Whether the cadence *reads*
as Moria - smooth but hectic - is Michael's call and cannot be measured from here.

**Bounds are a judgement, not a measurement.** Clamp `0.60..9.00`: below 0.6 a near-stationary
climber would freeze mid-hop (reads as a hitch); above 9.0 the hop is under 0.19s and blurs. If it
looks wrong at the extremes, those two numbers are the dial - not the 46.3, which is measured.

**Owed AGENT_STATE.md** - DECISION (2026-08-08): climb animation play rate is DERIVED per frame from
achieved displacement over the clip's own root-track speed. Do not replace it with a constant.

## Refine

- **Used actor displacement, not `GetVelocity()`**, for the reason above. This is the single decision
  that makes the ticket work rather than regress.
- **Divided by `DeltaTimeX` rather than assuming a tick rate.** The logs run at ~19fps in PIE and
  would differ in a packaged build; a per-frame delta without dividing by dt would make the cadence
  frame-rate dependent.
- **Took 46.3 from the root track** (`get_bone_pose_for_time` at t=0 and t=length) rather than eyeballing.
- **Updated `ClimbLastZ` after the rate is set**, so the delta is exactly one frame and cannot drift.
- **Left the shimmies on constants.** Lateral speed varies far less than vertical, and each shimmy
  clip needs its own divisor - three more variables for a motion that is briefly used. Revisit if it
  slides.

**Deliberately left undone:** per-goblin cadence jitter for a horde (needs a random offset seeded per
instance, and matters most once several goblins climb together); the same treatment for the shimmy
states; braced<->free-hang blending; the two-window motion warp. Stage 4 still awaits Michael's
observation, and roof continuation still owes `Small_8`/`Small_10`.
