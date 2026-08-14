---
id: 055
title: BP safe edits: ClimbDrainIdle to zero, kill the red climb trace draw
agent: claude-stamina
status: done
claimed: 2026-08-07T02:01Z
build: none
waiting_on:
evaluated: 2026-08-07T02:27:09Z
files: 
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

BP safe edits: ClimbDrainIdle to zero, kill the red climb trace draw

## Generate

One file touched: `Content/Blueprints/BP_GSPlayerCharacter.uasset`. Two changes.

**1. ClimbDrainIdle 2.5 -> 0.0.** Written to the CDO, not to the FBPVariableDescription:

    cdo = unreal.get_default_object(unreal.load_asset(p).generated_class())
    cdo.set_editor_property('ClimbDrainIdle', 0.0)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)

`set_variable_default_value` was deliberately NOT called. This variable's VarDesc
default string is empty (`get_variable_info` returns ""), so 2.5 lived only in the
serialized CDO; the VarDesc write would have returned True and changed nothing.
The variable is Blueprint-only (no C++ UPROPERTY) and is read by exactly one node —
pin B of a Select Float inside custom event `ClimbStaminaExits`, chosen when both
|PlayerClimbX| and |PlayerClimbY| are inside the 0.15 deadzone. Result is
`Clamp(SprintStamina - drain*dt, 0, MaxStamina)`. It is a subtrahend, never a
divisor, so 0 cannot produce NaN or a divide-by-zero. Hanging still is now free;
moving still drains ClimbDrainMove (11.0), untouched.

**2. Both `ForOneFrame` debug draws -> `None`.**

    unreal.BlueprintService.set_node_pin_value(p, "EventGraph",
        "C1B1E60B49A25C3BE2D7CAA1F199D8D5", "DrawDebugType", "None")   # Sphere Trace By Channel (5690, 163)
    unreal.BlueprintService.set_node_pin_value(p, "EventGraph",
        "BC0EC0C34C7C04899DAF9D82ADDBB8A1", "DrawDebugType", "None")   # Line Trace By Channel   (7370, 176)

Both sit on the Event Tick sprint/stamina exec chain, NOT in DoTraverse/ClimbTick
(this BP has only two real UEdGraphs: EventGraph and UserConstructionScript; the
"events" are K2Node_CustomEvents in y-bands). They were ungated literals — bClimbDebug
never reached them — so the red sphere/line drew every frame regardless of the flag.
The bare string `"None"` was used, matching what the other 8 trace nodes in this same
asset already store.

Then `compile_blueprint` + `EditorAssetSubsystem.save_loaded_asset(bp)` -> True.

## Evaluate

Verified, with evidence:

- BEFORE, read live: `ClimbDrainIdle = 2.5`; both pins `default_value: "ForOneFrame"`.
- AFTER, in a SEPARATE call following `collect_garbage()` and a re-`load_asset`:
  `ClimbDrainIdle = 0.0`; both pins `default_value: "None"`.
- The CDO value was re-read AFTER `compile_blueprint` specifically to catch the
  `CopyPropertiesForUnrelatedObjects` trap that can re-assert the old CDO over a
  fresh write. It read 0.0 post-compile and again post-save.
- Exhaustive sweep of all 556 EventGraph nodes: 10 nodes carry a DrawDebugType pin,
  0 remain `ForOneFrame`.
- Save reached disk, not just memory: `save_loaded_asset` -> True;
  `get_dirty_content_packages()` -> 0 with the target absent from the list; the
  on-disk file went 1,577,701 -> 1,578,326 bytes with a fresh mtime; git now reports
  the uasset as modified. Pre-edit copy taken at
  `D:\goblinRaid\BP_Backup_20260806\BP_GSPlayerCharacter.uasset` (1,577,701 bytes) —
  no backup of this asset existed before, only git.
- No `True` return was accepted as evidence on its own; every claim above is a re-read.

Honest limits:

- The post-edit read-back comes from the editor's resident package after
  `collect_garbage()`, not from a forced `reload_packages` off disk. GC will not evict
  a referenced package, so this proves in-memory state plus a successful save, not a
  byte-level disk parse. The disk size/mtime/git delta is the corroborating evidence.
- NOT run in PIE. Behaviour is inferred from the graph topology, not observed on a wall.
  Deliberate: `load_asset` returns None under PIE and the edit needed a clean save.
- Nothing outside the goal was touched. No C++, no other asset, no VarDesc string.

Known consequence, not a defect: normal stamina regen is gated behind
`NOT PlayerIsClimbing`, so with idle drain at 0 stamina now FREEZES on the wall rather
than recovering. "Hang forever" is satisfied; "recover while hanging" is not, and would
require changing that regen gate — a separate decision, not taken here.

AGENT_STATE.md owes: DECISION — climb idle stamina drain is now zero (hang indefinitely),
and the two ungated per-tick debug traces on the Event Tick sprint chain are off.

## Refine

Nothing changed on a second pass, and here is why the first survives scrutiny.

Three things I went looking for and did not find:
1. A VarDesc/CDO split that would let 2.5 come back on the next full compile — the
   VarDesc string is empty, and I re-read the CDO after compile AND after save.
2. A third `ForOneFrame` pin hiding outside the two GUIDs — swept all 556 nodes; there
   is none, and fixing only one of the pair would have left a red line drawing forever.
3. A divide/duration use of ClimbDrainIdle that would make 0 unsafe — it has exactly one
   reader and it is a subtrahend.

Deliberately left undone:
- Did NOT call `set_variable_default_value` to "keep the VarDesc consistent". It is not
  needed for behaviour and it would change which store is authoritative for this one
  variable while every other variable on the BP still relies on the CDO. Minimal blast
  radius was the better trade.
- Did NOT wire bClimbDebug into the DrawDebugType pins. That is a larger change and it
  would not match the surrounding style — the other 8 traces are hard literals.
- Did NOT touch the regen gate, and did NOT enter PIE to watch a climb. Both are
  follow-ups for whoever owns the stamina feel.
