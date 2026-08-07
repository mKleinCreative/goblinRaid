---
id: 045
title: Clamp view pitch so the aim camera cannot swing below the wheat; build WBP_WeaponWheel
agent: claude-cam
status: done
claimed: 2026-08-06T20:54Z
build: none
waiting_on:
evaluated: 2026-08-06T20:58:45Z
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Content/UI/WBP_WeaponWheel.uasset
---

## Goal

Clamp view pitch so the aim camera cannot swing below the wheat; build WBP_WeaponWheel

## Generate

Michael after testing #044: *"the arc looks right now and the camera stays clear, but we need a
maximum distance it'll pitch down. the camera still gets stuck in the grass."*

**1. View pitch clamp (C++, needs a build).** `ViewPitchMaxDegrees = 55` and
`ViewPitchMinDegrees = -70`, pushed onto the `PlayerCameraManager` by a new `ApplyViewPitchLimits()`
called from **both** `BeginPlay` and a new `PossessedBy` override - either can run first, and the
raid director respawns the pawn.

Engine default is +/-89.9. The correction from #042-#044 stops growing at `AimHighPitchDegrees` (45)
while `sin(pitch)` keeps climbing, so clearance decays past that: 193uu at 60 degrees, 161uu at 75 -
against a 158uu wheat canopy. The lift handles the useful range; the clamp bounds the tail. 55
degrees costs only the near-vertical lob, which is past the 45-degree max-range angle anyway.

**This reverses #042's explicit refusal to clamp.** That refusal was right in isolation - "the arc IS
the weapon" - and wrong as a permanent rule: it assumed the correction could cover every angle, and
it cannot without putting the camera in orbit.

**2. `WBP_WeaponWheel` built (no rebuild needed).** Created via VibeUE's `WidgetService`: a
`CanvasPanel` root with `Label_Torch`, `Label_Bow`, `Label_Sword` and a `Centre` image, all anchored
to screen centre with `set_anchors`/`set_alignment`/`set_size`/`set_position`.

Laid out to **match `SlotForDirection` exactly** - torch top (0,-200), bow bottom-right (150,120),
sword bottom-left (-150,120). Verified with `capture_preview`: the rendered PNG shows TORCH top,
SWORD bottom-left, BOW bottom-right. The sector maths and the picture now agree, which is the whole
reason `SlotForDirection` was made BlueprintPure.

**Also answered:** there is no `UnrealUI` plugin in this project (plugins are VibeUE,
AscentCombatFramework, MetaRoad). `WidgetService` is the tool - 41 actions including `add_component`,
`bind_event`, `add_view_model_binding`, `spawn_widget_in_pie` and `capture_preview`.

## Evaluate

**The widget is verified by an actual rendered image** - `capture_preview` PNG, read and looked at,
positions correct. That is stronger evidence than anything in #042/#043/#044, all of which were
trigonometry.

**The clamp is NOT COMPILED** - editor is open - and is unverified in every other sense too. 55
degrees is chosen from the clearance curve, not from throwing anything. If it turns out you *want* a
steeper lob (over a wall, onto a high roof), 55 is the number that will feel wrong, and it is
EditDefaultsOnly on `BP_GSPlayerCharacter` so it retunes without a rebuild.

**The widget does nothing yet.** It has no binding to `OnWheelOpenChanged` or
`OnWheelHighlightChanged`, nothing spawns it into the viewport, and no label highlights. It is a
static picture in the correct arrangement. Making it live needs either a Blueprint graph (Event
Construct -> bind to the weapon component's delegates) or an MVVM ViewModel - `WidgetService` supports
both, and I have done neither.

**Preview caveat:** the capture renders on a transparent/white background, so white text on white is
barely legible in the PNG. I read the layout, not the styling. In game it sits over the world, but
there is no dark backing and no font styling - it will be hard to read against bright terrain, which
is most of this level.

**The clamp is global, not aim-only.** It applies to the hip camera too. That is deliberate - the hip
arm is 450 and dives harder for the same pitch - but it is a change to how the whole game looks
around, made to fix an aiming problem, and nobody has walked around with it.

**Touched outside the goal:** none; all three files claimed before editing.

**Owed AGENT_STATE.md** - a DECISIONS line once confirmed: *the aim camera is bounded by BOTH a
pitch-driven lift (useful range) and a hard `ViewPitchMax` (the tail); neither alone is sufficient,
and #042's blanket refusal to clamp was wrong.* Holding it until it has been played, per the pattern
that caught #043.

## Refine

- **Reversed #042's refusal to clamp, and said so at the property rather than only here.** The
  original reasoning is quoted in the header comment along with why it does not survive: it assumed
  a correction curve could cover every angle. Someone reading `ViewPitchMaxDegrees = 55` needs to
  know a clamp was rejected once and why that changed, or they will remove it.
- **Applied the limits from PossessedBy as well as BeginPlay.** Only doing it in BeginPlay works in
  PIE and fails on respawn, which this game does through the raid director - a bug that would have
  appeared as "the camera is fine until you die once".
- **Rendered the widget and looked at it** instead of trusting four `set_position` calls. Three
  labels in a triangle is exactly the kind of layout where a sign error reads as plausible, and the
  wheel's whole job is to agree with `SlotForDirection`.
- **Reused `SlotForDirection`'s geometry as the layout spec** rather than eyeballing a triangle.
  Torch at (0,-200) is the same "up is torch" the input maths uses; if one changes the other is
  visibly wrong.
- **Did not invent a `UnrealUI`.** Checked the plugin list and the toolsets rather than assuming what
  Michael meant - the honest answer is that no such plugin is installed and `WidgetService` is what
  exists.

**Deliberately left undone:** binding the widget to the wheel's delegates, spawning it into the
viewport, and styling it (backing panel, font, highlight state) - all real work, and the difference
between a picture and a menu. Also: tuning 55 degrees, and confirming the clamp does not make general
traversal feel constrained.
