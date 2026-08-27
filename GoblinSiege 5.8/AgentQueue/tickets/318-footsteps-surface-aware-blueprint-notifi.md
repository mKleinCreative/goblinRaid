---
id: 318
title: Footsteps: surface-aware Blueprint notifies on the goblin locomotion set
agent: claude-audio
status: done
claimed: 2026-08-26T16:54Z
build: none
waiting_on:
evaluated: 2026-08-26T17:05:13Z
observed: 2026-08-26T17:05:34Z | Michael walked the goblin across open ground and heard footfalls keeping time with the legs, then sprinted and heard a different, faster footstep - walk and run read as two distinct sounds, not one sound sped up. Ground type made no difference underfoot anywhere he walked, which is the dirt fallback doing its job while no surfaces are painted.
scenario: PIE on L_Tutorial_Island, player goblin on ThirdPerson_AnimBP_Gob, walking then sprinting across open ground, on headphones.
files: 
  - Content/Audio/Notifies
  - Content/Characters/ScoutV2
---

## Goal

Footsteps: surface-aware Blueprint notifies on the goblin locomotion set

## Generate

**The goblin has footsteps.** Until this ticket the game had none at all — the four `SC_GS_Foot_*`
cues were orphans, no notify existed anywhere in the project, and nothing queried a physical material.

**Two Blueprint `UAnimNotify` subclasses** at `Content/Audio/Notifies/`:
`AN_GS_Footstep_Walk` and `AN_GS_Footstep_Run`. Each overrides `Received_Notify` (a *function*, not an
event — it returns bool) with:

```
Entry → GetOwner → IsValid → Branch
      → GetComponentByClass(ACFEffectsManagerComponent) → IsValid → Branch
      → GetCurrentTerrain()  → Equal(byte, SurfaceType2) → Branch
            true  → PlaySoundAtLocation(GrassCue, GetActorLocation)
            false → PlaySoundAtLocation(DirtCue,  GetActorLocation)
      (no ACF component → falls through to DirtCue)
```

Cues live in **class-default Blueprint variables** (`DirtCue`, `GrassCue`), so the placement script only
ever chooses *which class* to place and never writes a per-instance object property — sidestepping the
"object pin defaults cannot be verified" trap that bit #301. Defaults read back off the compiled CDO.

**`NoiseLoudness` (walk 0.2 / run 1.0) is present and read by nothing** — the deliberate stub per GDD
ruling 56, which cut noise from the demo. If stealth returns, one node in two Blueprints turns every
footstep in the game into a noise event with correct relative loudness, without re-placing 34 notifies.

**34 notifies placed across the 17 animations that actually play**, at 25% and 75% of each cycle.

## Evaluate

**The moveset finding is the one that would have wasted the ticket.** `Content/Characters/ScoutV2`
holds *three* competing locomotion sets — `A_MX_Std_*_Gob`, `A_MX_standing_*_Gob` and
`A_GOB_DK2_*_IP`. Following the plan's "~40 core locomotion sequences" by name would have hit the
wrong ones and produced **silence with no error**. Walking the dependency chain
`ThirdPerson_AnimBP_Gob → BS_GS_Loco_Pack → 16 A_GOB_DK2_*_IP` (plus `BS_GS_CrouchMove →
A_MX_Sneak_Walk_Gob`) settled it: **17 sequences are live, and the `A_MX_Std_*` set is referenced by
nothing.** Ask the blendspace, not the file names.

**Verified:** both Blueprints compile `BS_UP_TO_DATE`; all four cue defaults read back off the compiled
CDOs; all 34 notifies re-read from disk after saving (17/17 assets saved); in PIE the pawn is confirmed
on `ThirdPerson_AnimBP_Gob_C` with the ACF effects component present. **Heard by Michael: footsteps
audible, and walk and run are distinct cues.**

**NOT verified, and the list matters:**
- **The grass branch has never executed.** `GetCurrentTerrain()` returns `SURFACE_TYPE_DEFAULT`
  everywhere because the seven `PM_GS_*` materials are painted on nothing (T1 is blocked by #313's
  World Partition conversion of `L_Tutorial_Island`). Every step so far has taken the Dirt fallback.
  The switch is built and compiles; **whether it selects grass correctly is untested.**
- **The notify times are a first guess** — 25%/75% of each cycle, not measured foot plants. They sound
  right to Michael, which is the only test available, but a limp or double-tap on some directions
  would not surprise me. `set_notify_trigger_time` fixes it with no re-authoring.
- **Goblins only.** `ABP_Human`'s moveset is untouched, so every defender is still silent-footed.
- **The sneak walk uses the ordinary walk cue** at full volume, which is wrong for a crouch — though
  with stealth cut (ruling 56) it matters less than it would have.
- **Sound plays at the actor location, not the foot.** The two rigs disagree on bone names (goblin
  `l_foot`/`r_foot`, humans `leftfoot`/`rightfoot`, neither is UE's `foot_l`), so a class-default bone
  name would work on one rig and silently fail on the other. Actor location is ~90uu from the foot
  against a 150uu attenuation inner radius — inaudible, and it dodges the trap entirely.

**Owed to AGENT_STATE.md — a DECISION line:** *the live goblin moveset is `BS_GS_Loco_Pack` → the 16
`A_GOB_DK2_*_IP` sequences; the `A_MX_Std_*_Gob` set is referenced by nothing and edits there are
invisible.*

## Refine

**Changed in response to my own evaluation:**
- **Duplicated `AN_GS_Footstep_Walk` into `Run` rather than rebuilding the graph.** The hand-built Run
  stub was deleted. A 16-node graph reproduced by hand is a graph that differs by hand; duplicating and
  changing three defaults guarantees they are identical.
- **Chased the blendspace instead of trusting names**, after noticing three plausible walk sets.
- Fixed a wrong assumed asset path (`Animations/` vs the real `Anims_Pack/`) by reading package names
  off the asset registry rather than composing them.

**A real failure worth recording:** the first wiring pass reported 22/23 connections and *did not say
which one failed*. It was `GetComponentByClass → GetCurrentTerrain` — `GetComponentByClass` returns a
base `UActorComponent*`, so it will not connect to a typed pin until `ComponentClass` is set. Note that
the class pin, like object pins, **reads back empty** from `get_node_pins`, so the only proof it took is
that the connection then succeeded and the Blueprint compiled. Inspecting per-pin connection state is
the way to find a failed connection; the summary count only tells you one is missing.

**Deliberately left undone:**
- `ABP_Human` footsteps — a mechanical repeat once the goblin's timing is settled by ear.
- Per-foot socket placement and a quieter sneak cue — both wait on evidence that they are audible
  problems rather than theoretical ones.
- The grass path stays untested until #313 releases the map and T1 paints the surfaces.
