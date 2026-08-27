---
id: 316
title: The corrupted sky loses its blue and the sun goes red: ozone absorption and AtmosphereSunDiskColorScale
agent: claude-corruption
status: done
claimed: 2026-08-26T03:23Z
build: required
waiting_on: Needs editor-closed build with 315. Then PIE GS_BurnTest, GS.Corruption.Set 1, and LOOK UP: the blue rim at the extremes should be gone and the sun disc should be red.
evaluated: 2026-08-26T03:25:20Z
observed: 2026-08-26T18:50:55Z | Michael raised corruption in PIE and looked up: the sun disc now reads red instead of yellow, and the blue that survived at the extremes of the sky dome is gone - the two things that prompted this ticket both resolved on screen
scenario: PIE after the 11:05 build, corruption raised, judged by eye looking at the sky
files: 
  - Source/GoblinSiege/World/GSCorruptionDirector.h
  - Source/GoblinSiege/World/GSCorruptionDirector.cpp
---

## Goal

The corrupted sky loses its blue and the sun goes red: ozone absorption and AtmosphereSunDiskColorScale

## Generate

Two defects Michael saw in PIE, 2026-08-26: *"the sky went dark orange except for the extreme of the
skybox which was blue, also the sun was the same yellow color"*. Neither was a fault in the existing
code - both were parameters the director never touched, and in both cases the parameter that mattered
is a different one from the parameter that looks like it should matter.

**1. The blue extreme was OZONE.** `RayleighScattering` recolours the bulk of the dome, and the
director drove it correctly - but `OtherAbsorption` (ozone) defaults to a blue-cyan absorption that
dominates at the zenith and at grazing angles, so blue survives however red the Rayleigh goes. Now
driven, with `OtherAbsorptionScale`, toward warm absorption (`0.45, 0.26, 0.14`, scale x2.0).
`MieScattering` joins them so the haze reads as smoke rather than clean white.

**2. The yellow sun was a SECOND sun parameter.**
`UDirectionalLightComponent::AtmosphereSunDiskColorScale` is what the disc looks like in the sky;
`SetLightColor` - which the director already drove - is only what the sun does to the scene. Driving
one without the other lights the world warm and leaves the disc yellow, which is exactly what was
observed. Now lerped to `(1.0, 0.15, 0.04)`.

All four new values are captured as baselines, lerped from them, and **restored** on teardown, so a
PIE stop does not leave the editor sky red. Both ends are `EditAnywhere` for tuning by eye.

`DescribeOutputs()` now prints live rayleigh and ozone values on the SkyAtmosphere line.

## Evaluate

**NOT COMPILED, NOT WATCHED.** The editor is open and #315/#316 hold the gate.

**The real finding here is about the instrument, not the sky.** Both defects were invisible to
every check this feature has. The build passed, the drivers worked, `GS.Corruption.Dump` reported
the SkyAtmosphere as `found`, and the world genuinely changed colour - and yet two of the most
visible things in frame were wrong. **Nothing but a human looking up would have caught either.**
That is the third time in this feature that the diagnostic said "working" about something partially
broken (the Stationary sun, the type-weighting aggregate in #315, now this), and each time the fix
was to make the instrument report a *value* rather than a *status*. Hence ozone now being printed.

**Adversarially:**

- **The chosen colours are guesses at a look**, and the ozone values in particular are the ones I
  would expect to need adjusting - too warm and the zenith may read brown-grey and muddy rather than
  bruised. That is why both ends are EditAnywhere; per the project's standing rule I should hand
  over the numbers and let Michael judge, not iterate on it myself.
- **`MieScattering` was added without being asked for.** Michael raised the ozone and the sun; the
  haze colour is my addition on the theory that clean white haze under a red sky would look wrong.
  It is a scope increase, it is small, and it is easy to revert by setting it equal to the base.
- **The `Cast<UDirectionalLightComponent>` could fail** on a light whose component is some other
  directional-light subclass, in which case the disc silently stays yellow again - the same class of
  silent failure this ticket exists to fix. It is not currently reported.
- **The Stationary-mobility warning still stands** and interacts with this: on a Stationary light the
  disc scale should still apply, but the baked indirect will not follow the new sun colour.

**Owes `AGENT_STATE.md`:** a FAILED line - *"the sun has two colours (light colour and
AtmosphereSunDiskColorScale) and the sky has two blues (rayleigh and ozone); driving one of each pair
looks like the feature half-works"*.

## Refine

**Changed from my own review:** the disc scale was initially written against `ULightComponent`, which
does not carry it - `AtmosphereSunDiskColorScale` is declared on `UDirectionalLightComponent`
specifically, because only a directional light can be an atmosphere sun. Both the capture and the
apply now cast.

**Deliberately left undone:**

- **Replacing the sun with VFX.** Offered and not chosen for now; the parameter route is correct,
  cheap and leaves the VFX option open to layer on top later. A Niagara sun needs an actor tracking
  the light direction at effectively infinite distance, which is a real ticket, not a drop-in.
- **Reporting a failed disc cast**, per the third bullet above.
