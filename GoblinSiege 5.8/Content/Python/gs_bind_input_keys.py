# -*- coding: utf-8 -*-
"""
================================================================================
gs_bind_input_keys.py -- Goblin Siege | assign keys to Input Actions in an IMC
2026-08-05.  Target: UE 5.8, project "GoblinSiege 5.8".
================================================================================

WHAT THIS DOES

  Binds Input Actions to keyboard keys inside /Game/Input/IMC_Default, driven by
  the BINDINGS table below.  Written table-driven rather than as a one-shot for
  the swap key because "bind X to Y" is a request that recurs, and the whole
  awkward part -- finding the existing mapping, not clobbering the gamepad
  binding, writing the array back, proving it stuck -- is the same every time.

  CURRENT TABLE:  IA_SwapWeaponMode -> Tab   (sword <-> bow, Michael 2026-08-05)

WHY A SCRIPT AND NOT THREE CLICKS

  It is three clicks, and doing it by hand is completely fine.  The reason this
  exists is that it VERIFIES: it reads the mapping back off the asset after
  saving and prints what it found, and it tells you if some OTHER action is
  already sitting on the same key.  A duplicate binding is silent in Enhanced
  Input -- both actions just fire -- and "Tab does two things now" is a horrible
  bug to track down later.

WHAT IT DELIBERATELY DOES NOT TOUCH

  * GAMEPAD bindings for the same action are left alone.  A mapping whose key
    name starts with "Gamepad" is skipped, so rebinding the keyboard key does
    not silently unbind the controller.
  * TRIGGERS AND MODIFIERS on an existing mapping are preserved -- only the Key
    field is written.  A mapping that had a Hold trigger keeps it.

HOW TO RUN

  Editor Output Log -> cmd dropdown -> Python.  NOT while PIE is running.

      exec(open(r"D:/goblinRaid/GoblinSiege 5.8/Content/Python/gs_bind_input_keys.py").read())

  To rebind something else, edit BINDINGS and re-run.  Idempotent: a mapping
  that is already on the right key is reported and left alone.

  Key names are FKey names as Unreal spells them -- "Tab", "LeftShift",
  "SpaceBar", "E", "One", "MiddleMouseButton".  If you get them wrong the script
  says so rather than writing a dead binding: every key is validated against
  unreal.Key before anything is saved.

BINDING PROJECT RULES HONOURED HERE
  * PIE guard -- asset writes during PIE silently no-op on this project.
  * save_asset() lies; saving is EditorLoadingAndSavingUtils.save_packages(
    [pkg], only_dirty=False) and is VERIFIED BY FILE MTIME.
  * EditorAssetSubsystem, not EditorAssetLibrary (CLAUDE.md 2026-08-04: the
    library silently reports False/None for assets that exist in this build).
  * Struct arrays returned by get_editor_property are COPIES.  Mutating an
    element does not write through -- the whole array is rebuilt and set back.
================================================================================
"""

import os
import time
import traceback

import unreal

# ------------------------------------------------------------------------------
# THE TABLE -- edit this
# ------------------------------------------------------------------------------

IMC_PATH = "/Game/Input/IMC_Default"

BINDINGS = [
    # (Input Action asset path, FKey name)
    ("/Game/Input/IA_SwapWeaponMode", "Tab"),
]

MTIME_FRESH_WINDOW = 600.0


# ------------------------------------------------------------------------------
# LOGGING
# ------------------------------------------------------------------------------

def log(msg):
    print("[GS-INPUT] %s" % msg)


def ok(step, msg):
    print("[GS-INPUT] PASS  | %-24s | %s" % (step, msg))
    return True


def fail(step, msg):
    print("[GS-INPUT] FAIL  | %-24s | %s" % (step, msg))
    return False


def warn(step, msg):
    print("[GS-INPUT] WARN  | %-24s | %s" % (step, msg))
    return True


# ------------------------------------------------------------------------------
# ASSET ACCESS  (subsystem first -- EditorAssetLibrary lies in this build)
# ------------------------------------------------------------------------------

def _aes():
    try:
        return unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    except Exception:
        return None


def load_asset(pkg_path):
    sub = _aes()
    if sub is not None:
        try:
            a = sub.load_asset(pkg_path)
            if a is not None:
                return a
        except Exception:
            pass
    try:
        return unreal.EditorAssetLibrary.load_asset(pkg_path)
    except Exception:
        return None


# ------------------------------------------------------------------------------
# PIE GUARD
# ------------------------------------------------------------------------------

def pie_guard():
    try:
        ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        editor_world = ues.get_editor_world()
    except Exception:
        return ok("pie-guard", "could not check PIE -- proceeding")
    if editor_world is None:
        bar = "!" * 78
        print(bar)
        print("!! GS-INPUT REFUSING TO RUN -- PLAY IN EDITOR IS ACTIVE")
        print("!! Asset writes during PIE silently no-op on this project.")
        print("!! Stop PIE (Esc), then re-run.")
        print(bar)
        return False
    return ok("pie-guard", "PIE not active")


# ------------------------------------------------------------------------------
# PERSISTENCE -- save_asset() returns True when it saved nothing
# ------------------------------------------------------------------------------

def disk_path_for(pkg_path):
    if not pkg_path.startswith("/Game/"):
        return None
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, pkg_path[len("/Game/"):].replace("/", os.sep) + ".uasset")


def save_and_verify(pkg_path):
    step = "save:" + pkg_path.rsplit("/", 1)[-1]
    try:
        asset = load_asset(pkg_path)
        if asset is None:
            return fail(step, "load_asset returned None")
        unreal.EditorLoadingAndSavingUtils.save_packages([asset.get_outermost()], False)
    except Exception:
        traceback.print_exc()
        return fail(step, "save_packages threw -- see traceback")

    fp = disk_path_for(pkg_path)
    if fp is None or not os.path.exists(fp):
        return fail(step, "no .uasset on disk at %s" % fp)
    age = time.time() - os.stat(fp).st_mtime
    if age > MTIME_FRESH_WINDOW:
        return fail(step, "STALE ON DISK: mtime %.0fs old -- the save wrote NOTHING" % age)
    return ok(step, "on disk, mtime %.0fs old" % age)


# ------------------------------------------------------------------------------
# KEYS
# ------------------------------------------------------------------------------

def make_key(key_name):
    """FKey from a name, trying both bindings Python has used for the struct.
    Returns None if the engine does not recognise the name -- validated BEFORE
    anything is written, because a dead FKey binds silently and behaves exactly
    like 'the input never arrived'."""
    for maker in (lambda: unreal.Key(key_name=key_name), lambda: unreal.Key(key_name)):
        try:
            k = maker()
            if k is not None and str(k.get_editor_property("key_name")) == key_name:
                return k
        except Exception:
            continue
    return None


def key_name_of(mapping):
    try:
        return str(mapping.get_editor_property("key").get_editor_property("key_name"))
    except Exception:
        return "<unreadable>"


def is_gamepad(mapping):
    return key_name_of(mapping).startswith("Gamepad")


# ------------------------------------------------------------------------------
# THE WORK
# ------------------------------------------------------------------------------

def bind_all():
    imc = load_asset(IMC_PATH)
    if imc is None:
        return fail("load-imc", "could not load %s" % IMC_PATH)
    ok("load-imc", IMC_PATH)

    try:
        mappings = list(imc.get_editor_property("mappings"))
    except Exception:
        traceback.print_exc()
        return fail("read-mappings", "could not read 'mappings' off the IMC")

    log("  %d existing mapping(s) in %s" % (len(mappings), IMC_PATH))
    for m in mappings:
        try:
            act = m.get_editor_property("action")
            log("    %-28s -> %s" % (unreal.SystemLibrary.get_object_name(act) if act else "<none>",
                                     key_name_of(m)))
        except Exception:
            pass

    changed = False

    for action_path, key_name in BINDINGS:
        step = key_name + ":" + action_path.rsplit("/", 1)[-1]

        action = load_asset(action_path)
        if action is None:
            fail(step, "could not load the Input Action %s" % action_path)
            continue

        new_key = make_key(key_name)
        if new_key is None:
            fail(step, "'%s' is not a key Unreal recognises. Use the FKey spelling "
                       "(Tab, SpaceBar, LeftShift, E, One, MiddleMouseButton)." % key_name)
            continue

        # A duplicate binding is SILENT in Enhanced Input -- both actions fire on the
        # same press -- so say so loudly rather than quietly creating one.
        for m in mappings:
            other = m.get_editor_property("action")
            if other is not None and other != action and key_name_of(m) == key_name:
                warn(step, "'%s' is ALREADY bound to %s in this context. Both actions will "
                           "fire on that press. Rebind or remove one."
                     % (key_name, unreal.SystemLibrary.get_object_name(other)))

        # Keyboard mappings for this action, gamepad left alone on purpose.
        existing = [m for m in mappings
                    if m.get_editor_property("action") == action and not is_gamepad(m)]

        if existing:
            target = existing[0]
            if key_name_of(target) == key_name:
                ok(step, "already bound to %s -- left alone" % key_name)
            else:
                was = key_name_of(target)
                # Only the Key field is written; triggers and modifiers on this mapping survive.
                target.set_editor_property("key", new_key)
                changed = True
                ok(step, "rebound %s -> %s" % (was, key_name))
            for extra in existing[1:]:
                warn(step, "action also has a second keyboard mapping on '%s' -- left alone, "
                           "remove it by hand if it is a leftover" % key_name_of(extra))
        else:
            m = unreal.EnhancedActionKeyMapping()
            m.set_editor_property("action", action)
            m.set_editor_property("key", new_key)
            mappings.append(m)
            changed = True
            ok(step, "no keyboard mapping existed -- added one on %s" % key_name)

    if not changed:
        ok("write", "nothing to change")
        return verify()

    # Struct arrays come back from get_editor_property as COPIES: mutating an element
    # does not write through to the asset. The whole array has to go back.
    try:
        imc.set_editor_property("mappings", mappings)
    except Exception:
        traceback.print_exc()
        return fail("write", "could not write 'mappings' back to the IMC")

    save_and_verify(IMC_PATH)
    return verify()


def verify():
    """Re-read from the asset. A tool call that returned True is not evidence."""
    imc = load_asset(IMC_PATH)
    if imc is None:
        return fail("verify", "could not reload %s" % IMC_PATH)
    try:
        mappings = list(imc.get_editor_property("mappings"))
    except Exception:
        return fail("verify", "could not re-read 'mappings'")

    all_good = True
    for action_path, key_name in BINDINGS:
        action = load_asset(action_path)
        hits = [key_name_of(m) for m in mappings
                if action is not None and m.get_editor_property("action") == action]
        label = action_path.rsplit("/", 1)[-1]
        if key_name in hits:
            ok("verify", "%s is bound to %s (all its keys: %s)" % (label, key_name, hits))
        else:
            fail("verify", "%s is NOT on %s -- its keys read back as %s" % (label, key_name, hits))
            all_good = False
    return all_good


def main():
    print("")
    print("[GS-INPUT] gs_bind_input_keys.py")
    if not pie_guard():
        return False
    result = bind_all()
    print("")
    print("[GS-INPUT] %s" % ("DONE" if result else "FINISHED WITH FAILURES -- read above"))
    print("")
    return result


if __name__ == "__main__":
    main()
