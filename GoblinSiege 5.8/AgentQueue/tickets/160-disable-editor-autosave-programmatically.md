---
id: 160
title: Disable editor autosave programmatically (Editor Preferences UI will not open)
agent: claude-autosave
status: done
claimed: 2026-08-15T05:15Z
build: none
waiting_on:
evaluated: 2026-08-15T06:32:15Z
observed: UNOBSERVED 2026-08-15T06:32:16Z - An editor preference has no in-game behaviour to watch. The live value reads False and the ini is written, but no editor restart has happened, so persistence is unproven.
scenario: none - never run
files: 
  - Config/DefaultEditorPerProjectUserSettings.ini
---

## Goal

Disable editor autosave programmatically (Editor Preferences UI will not open)

## Generate

Michael cannot open Editor Preferences at all - clicking it makes the level viewport blink and no
panel appears. Anything that is normally a checkbox in there has to be set another way.

- **Live:** `unreal.find_object(None, '/Script/UnrealEd.Default__EditorLoadingSavingSettings')` then
  `set_editor_property('bAutoSaveEnable', False)`. Confirmed `True -> False` by reading it back.
  Note the Python-style name `auto_save_enable` is NOT exposed; the raw `bAutoSaveEnable` is.
- **Persistent:** new `Config/DefaultEditorPerProjectUserSettings.ini` with
  `[/Script/UnrealEd.EditorLoadingSavingSettings] bAutoSaveEnable=False`, plus the previous values
  commented out so turning it back on restores the old behaviour exactly. The file header records
  *why* it exists, since a config file nobody remembers creating is worse than no config file.

The per-user `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` was deliberately NOT
edited - the editor rewrites it on exit and would have clobbered the change.

## Evaluate

**Verified live:** `bAutoSaveEnable` reads `False` in the running editor.

**I was wrong about why this mattered, and the ticket exists partly because of that.** I told
Michael autosave was "the thing that's been destroying the tree". It was not. Autosave writes to
`Saved/Autosaves/` - the `BT_HordeGoblin_Auto1/2/3` files found earlier are exactly its output - and
never touches `Content/`. The real cause was a blackboard parent link plus four missing keys (#154).
Turning autosave off is still worth having (one fewer background writer during asset surgery) but it
fixed nothing, and saying so at the time would have saved a detour.

**NOT verified:** that the setting survives an editor restart. The ini is written and the live value
is set, but no restart has happened, and if the editor writes its per-user ini on exit with the new
value the two will agree - unverified either way.

## Refine

**Changed after self-review:** first pass only set the live value, which would have been lost on
restart - exactly the failure mode of everything else this session. Added the persistent ini.

**Deliberately left undone:** no attempt to diagnose *why* Editor Preferences will not open. That is
a real and worsening problem - it blocks every editor setting, not just this one - and it deserves
its own ticket rather than being worked around one property at a time.
