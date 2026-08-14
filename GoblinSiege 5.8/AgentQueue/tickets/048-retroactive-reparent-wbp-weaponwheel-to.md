---
id: 048
title: RETROACTIVE: reparent WBP_WeaponWheel to GSWeaponWheelWidget, assign WeaponWheelWidgetClass, PIE-verify
agent: claude-wheel
status: done
claimed: 2026-08-06T21:41Z
build: none
waiting_on:
evaluated: 2026-08-06T21:42:04Z
files: 
  - Content/UI/WBP_WeaponWheel.uasset
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

RETROACTIVE: reparent WBP_WeaponWheel to GSWeaponWheelWidget, assign WeaponWheelWidgetClass, PIE-verify

## Generate

The two editor steps #046 identified and could not do without a DLL. **This claim is retroactive** -
both files were claimed under #046, I closed that ticket before the build, then edited them anyway.
Same breach as #010, recorded rather than tidied away.

- **Reparented `WBP_WeaponWheel`** from `UserWidget` to `GSWeaponWheelWidget`
  (`BlueprintEditorLibrary.reparent_blueprint`, compile, save). #046 flagged that a parent change can
  silently drop widget-tree state; captured the tree before and after and it survived intact -
  `['Root','Label_Torch','Label_Bow','Label_Sword','Centre']` both sides.
- **Assigned `WeaponWheelWidgetClass = WBP_WeaponWheel_C`** on `BP_GSPlayerCharacter`, re-fetching the
  CDO after compile per AGENT_STATE's note that the old pointer goes stale.

Both re-read off a fresh `collect_garbage()` + `load_asset`, not off the objects I had just written.

## Evaluate

**PIE-verified, with the component driven directly:**

| step | observed |
|---|---|
| widget created | 1 live `WBP_WeaponWheel_C_0`, `is_in_viewport = True` |
| at rest | `COLLAPSED` |
| `open_weapon_wheel()` | `HIT_TEST_INVISIBLE` - the delegate fired |
| drag up | `highlight = TORCH`, `committed = True` |
| `close_weapon_wheel(False)` | `COLLAPSED`, slot still `SWORD` - cancel path holds |

That covers the whole contract #046 wrote: creation, viewport, both delegates, and cancel. No
"found no UGSWeaponComponent" or "missing Label_*" warnings in the log, so the component lookup and
all three `BindWidgetOptional` bindings resolved.

**What is NOT verified: what it looks like.** I could not read the label colours - they are
`protected` on the C++ class, so Python cannot see them - and two attempts at a screenshot failed:
`CaptureViewport` grabbed the editor viewport (camera at 0,0,0), and `HighResShot` in PIE returned a
fully black frame. That second one is the **unfocused-editor trap already in AGENT_STATE** from the
perf work: a background editor does not render, so a capture taken while the window is behind gets
nothing. Stopped after two attempts rather than looping.

So: **the wheel provably opens, tracks and cancels. Whether the highlight reads at a glance is
unverified**, and that is the part Michael has to judge anyway. Still no backing panel and no font
styling, so white-ish text sits over bright terrain.

**Q itself remains untested.** Everything here drove the component API directly; a keypress is the
one input MCP cannot fake.

## Refine

- **Captured the widget tree before reparenting**, because #046 had named tree loss as the risk. It
  cost one line and turned "reparenting probably preserved it" into a before/after comparison.
- **Re-read every write off a fresh load.** Three separate times this session a save returned True and
  changed nothing; that is now reflex rather than diligence.
- **Stopped after two failed screenshots.** The second failure matched a trap already recorded in
  AGENT_STATE, which is the point at which more attempts stop being investigation.
- **Cancelled rather than committed when closing PIE**, so the pawn was left on SWORD - the slot it
  started in - instead of leaving the test's torch selected in the saved state.
- **Claimed retroactively and said so.** The alternative was quietly editing two files under a closed
  ticket, which makes every other ticket's file list less trustworthy.

**Deliberately left undone:** styling (backing panel, font, icons) - worth seeing before guessing;
and a real keypress test, which is Michael pressing Q.
