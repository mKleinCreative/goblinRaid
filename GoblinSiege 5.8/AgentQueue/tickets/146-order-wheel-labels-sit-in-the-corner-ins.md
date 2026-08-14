---
id: 146
title: Order wheel labels sit in the corner instead of around the wheel; beacon material reverted to DefaultMaterial
agent: claude-wheelui
status: done
claimed: 2026-08-13T05:56Z
build: none
waiting_on:
evaluated: 2026-08-13T06:00:00Z
observed: UNOBSERVED 2026-08-13T06:00:01Z - The labels are read back at the right anchors and offsets but nobody has seen the wheel drawn since the change - radius and font size are guesses until it is looked at.
scenario: none - never run
files: 
  - Content/UI/WBP_HordeOrderWheel.uasset
  - Content/Blueprints/BP_HordeOrderMarker.uasset
---

## Goal

Michael, using the wheel for the first time: *"the wheels come up into the top left corner of the PIE
screen, but doesn't do anything meaningful"* and *"the labels are spawning in the top right and I
don't see anything in the center"*.

`WBP_HordeOrderWheel` was created in #141 with all five `TextBlock`s present and their text set, but
their canvas slots were never positioned - #141 recorded that the slot layout could not be reached
from Python and left them at the canvas default.

## Generate

**Found the route that #141 missed.** `WidgetTree` is not exposed as an editor property on either the
`WidgetBlueprint` or its CDO - which is where #141 gave up - but the `CanvasPanelSlot` objects are
individually addressable by object path:

```
/Game/UI/WBP_HordeOrderWheel.WBP_HordeOrderWheel:WidgetTree.RootCanvas.CanvasPanelSlot_0
```

`unreal.load_object` on that path returns the slot, and `layout_data` / `auto_size` set normally.
`WidgetService.get_widget_snapshot` is what surfaced the names - it reports each component's
`slot_info` including the live anchors, offsets and alignment, and its `Slots` property lists the
slot object paths verbatim.

**All five labels re-anchored to screen centre** (`anchor_min = anchor_max = 0.5,0.5`, alignment
`0.5,0.5`, auto-size on) and offset into the wheel geometry that
`UGSHordeCommandComponent::OrderForDirection` actually implements:

| Label | Offset | Drag direction |
|---|---|---|
| `Label_Attack` | (0, -150) | up |
| `Label_Hold` | (+170, 0) | right |
| `Label_Follow` | (0, +150) | down |
| `Label_Loot` | (-170, 0) | left |
| `Label_Subject` | (0, +70) | centre, under the hub |

Text justification centred on all five so a long subject name grows both ways from the hub rather
than shunting sideways.

Anchoring to centre rather than positioning in absolute pixels is the part that fixes Michael's
actual complaint: the labels were at the canvas default anchor, so they sat in a screen corner and
moved with resolution. Centre-anchored, the wheel is where the crosshair is on any screen.

## Evaluate

**Verified by read-back**, not by claim: `get_widget_snapshot` after saving reports all five labels at
`anchors(0.5,0.5) align(0.5,0.5) autosize=True` with the offsets above, and `RootCanvas` unchanged.

**NOT verified: what it looks like.** Nobody has seen this wheel drawn since the change. The offsets
are chosen from the sector maths, not from a screenshot, and 150/170uu is a guess at a readable
radius. It may want to be bigger, and `Label_Subject` at +70 may collide with `Label_Follow` at +150
once a long name is in it.

**The beacon material could NOT be fixed here, and the reason is worth recording.** `BeaconMesh` is a
native component created in `AGSHordeOrderMarker`'s constructor, not an SCS node - `Blueprint`'s
`simple_construction_script` property is not exposed and there is no SCS node for it anyway. Setting
the material on the Blueprint CDO's component (what #141 did) reads back correctly and then reverts
on the next compile, because a native component's override lives in a component-override record the
Python write never creates. It needs a C++ default instead, which is #147.

**A correction #141 owes:** its Handoff section says the wheel labels "could not be positioned from
Python". That was wrong - they could, via the slot object paths above. Recorded so the next agent does
not repeat the dead end.

## Refine

**Changed on self-review:** the first attempt reached for `WidgetTree` twice (asset, then CDO) and
failed both times before `get_widget_snapshot` showed the slot paths. The lesson is that this service
reports more structure than the reflection surface exposes - read the snapshot before concluding
something is unreachable.

**Deliberately left undone:**

- **The beacon material** - moved to #147 as a C++ change, for the reason in Evaluate. This ticket
  claimed `BP_HordeOrderMarker.uasset` and made no change to it.
- **Any judgement on how the wheel reads.** Radius, font size and the subject label's position are
  guesses until somebody looks at it.
