# -*- coding: utf-8 -*-
"""
================================================================================
gs_build_burn_material.py -- Goblin Siege | Burn/char material + smolder VFX
Asset Artist agent, 2026-07-31.  Target: UE 5.8, project "GoblinSiege 5.8".
================================================================================

WHAT THIS BUILDS

  1. /Game/VFX/Burn/MF_GS_BurnDissolve   MaterialFunction -- "the blueprint we
     add to the material of items that darkens them based off their interaction
     with fire".  Lerps BaseColor toward the palette Char value as the burn
     amount rises, weighted by an incoming char mask (vertex color R on
     burnables).  A second output, EmberEmissive, gives an optional ember glow
     while the object is mid-burn.
     DARKENING ONLY in this pass -- no opacity / no dissolve.  Michael asked for
     the prototype darkening first and said refine later; the graph is kept
     small enough to read at a glance.

     TWO WAYS TO DRIVE IT (2026-07-31 -- this is the whole point of the revision):

         burn = Max( GS_BurnAmount parameter , BurnAmountIn input )

       * PROPS drive the PARAMETER.  UGSBurnFXComponent makes a MID per burning
         actor and sets GS_BurnAmount on it.  Per-object, per-actor, exactly
         what a parameter is for.
       * CROP AND GROUND drive the INPUT.  The crop is ~275,000 painted foliage
         instances sharing ONE material instance, so there is no per-instance
         MID to set a parameter on; per-location burn can only come from
         SAMPLING the world burn mask (UGSBurnMaskSubsystem's render target) by
         world position.  A texture sample cannot feed a ScalarParameter, so
         before this revision the function simply could not drive foliage at
         all -- the mask existed, the material existed, and there was no pin
         between them.

     Max, not a switch or a branch: props leave the input at 0 and drive the
     parameter, foliage leaves the parameter at 0 and drives the input, and if
     both are non-zero the STRONGER WINS -- which is the correct read, because a
     crate standing in a burnt field should look at least as charred as the
     ground under it.  A branch would have needed a third "which one am I"
     parameter that every caller could set wrong.

  2. /Game/VFX/Burn/M_GS_BurnDemo        Trivial proof material using the above.
     Drop it on a test cube, open it, scrub GS_BurnAmount 0 -> 1, watch it char.

  3. /Game/VFX/NS_GS_SmokeColumn         Tall, slow, drifting smoke column.

  4. /Game/VFX/NS_GS_Smolder             The persistent "this place is burnt"
     system: sparse slow embers + thin smoke, low spawn rates, loops forever,
     cheap enough to leave running on many actors until raid end.

THE C++ CONTRACT -- DO NOT RENAME

  UGSBurnFXComponent drives the scalar parameter named exactly

      GS_BurnAmount

  on the MIDs it creates for a burning actor's meshes.  Renaming it silently
  breaks every burn visual in the game: SetScalarParameterValue against a name
  that does not exist is a no-op with no warning.  The same applies to the asset
  path /Game/VFX/NS_GS_Smolder, which that component soft-references.

HOW TO RUN

  Editor Output Log -> cmd dropdown -> Python.  NOT while PIE is running.

      exec(open(r"D:/goblinRaid/GoblinSiege 5.8/Content/Python/gs_build_burn_material.py").read())

  or, with Content/Python on the editor python path:

      import gs_build_burn_material as g
      g.main()

  Individual steps are re-runnable:
      g.step1_material_function()   g.step2_demo_material()
      g.step3_smoke_column()        g.step4_smolder()
      g.finish_niagara("/Game/VFX/NS_GS_Smolder")     # manual compile+save
      g.print_summary()

  Idempotent: existing assets are detected and retuned, not clobbered.
  Set FORCE_RECREATE = True below to delete and rebuild from scratch instead.

  ONE EXCEPTION, and it is deliberate (2026-07-31): MF_GS_BurnDissolve is
  REBUILT IN PLACE if the copy on disk predates the BurnAmountIn input, whatever
  FORCE_RECREATE says.  A function without that input cannot drive foliage, and
  "skipped, already exists" would leave the crop permanently un-burnable with a
  PASS line next to it.  The graph is generated here in full, so rebuilding it
  loses nothing that this script did not write.  M_GS_BurnDemo is rebuilt with
  it, because it holds a hard reference to the function object.

BINDING PROJECT RULES HONOURED HERE

  * PIE guard.  Asset writes made while PIE is active have silently no-op'd on
    this project before.  The script refuses to run and says so loudly.
  * Saving is EditorLoadingAndSavingUtils.save_packages([pkg], only_dirty=False)
    and is VERIFIED BY FILE MTIME.  save_asset() returns True when it saved
    nothing (claude/goblin-siege-asset-persistence-rules.md).
  * Niagara duplicates MUST BE COMPILED.  An uncompiled duplicate looks totally
    healthy -- loads, params read back, component reports the asset -- and then
    every instance dies at runtime with "Error initializing data interfaces"
    and nothing renders (fire-vfx-asset-inventory.md rule 3).  Compile is forced
    by opening the asset editor, and VERIFIED by grepping the editor log for
    "Compiling System".
  * NO Niagara user parameters, ever.  AddUserVariables hard-crashes the editor
    (VibeUE #464) and corrupts the system so later reads crash too.
  * Vector rapid-iteration params silently write (0,0,0) under UE struct
    notation "(X=..,Y=..,Z=..)".  Only the "0.0,0.0,130.0" form is used here,
    and every set is read back (rule 8).
  * M_Smoke_02 is BROKEN (references a texture pack we do not own).  Smoke uses
    M_Smoke_01 only.
  * The agent has no screenshot route.  Nothing here claims an effect "looks
    right" -- only that it exists, compiled, and read back correct.  Final
    visual judgement is Michael's.
================================================================================
"""

import os
import glob
import time
import traceback

import unreal

# ------------------------------------------------------------------------------
# CONFIG
# ------------------------------------------------------------------------------

# True = delete existing assets and rebuild from scratch.  False (default) =
# keep what exists, retune / re-verify it.  Deliberate choice, not a guess:
# NS_GS_SmokeColumn already exists on disk from an earlier session.
FORCE_RECREATE = False

BURN_DIR      = "/Game/VFX/Burn"
MF_PATH       = "/Game/VFX/Burn/MF_GS_BurnDissolve"
DEMO_PATH     = "/Game/VFX/Burn/M_GS_BurnDemo"
SMOKE_PATH    = "/Game/VFX/NS_GS_SmokeColumn"   # AGSFireVolume.SmokeSystem candidate
SMOLDER_PATH  = "/Game/VFX/NS_GS_Smolder"       # UGSBurnFXComponent soft-refs this EXACT path

# Duplicate sources, tried in order.  All three are known to exist on disk.
# NS_FlameSmoke is a marketplace smoke system (VolcanoEnvironmentVFX) and is what
# AGSFireVolume::AGSFireVolume currently defaults SmokeSystem to; it already ships a
# smoke sprite material, so duplicating it avoids the renderer-material swap that
# plain editor Python cannot do (see MANUAL ACTION notes at the end of a run).
SMOKE_SOURCES = [
    "/Game/VolcanoEnvironmentVFX/VFX/Niagara/NS_FlameSmoke",
    "/Game/VFX/NS_GS_TorchFlame",
]
SMOLDER_SOURCES = [
    SMOKE_PATH,                                        # inherit the smoke look
    "/Game/VolcanoEnvironmentVFX/VFX/Niagara/NS_FlameSmoke",
    "/Game/VFX/NS_GS_TorchFlame",
]

# The smoke material the inventory doc mandates.  Referenced in the manual-action
# report only -- renderer material assignment is not reachable from editor Python.
M_SMOKE_01 = "/Game/DreamscapeSeries/SharedResources/Materials/Effects/M_Smoke_01"

# Seconds to leave the Niagara asset editor open so the compile can finish.
# NS_GS_SurfaceFire took 2.4s; 30 is slack, not a measurement.
NIAGARA_COMPILE_WAIT = 30.0

# Anything saved within this many seconds of "now" counts as freshly written.
MTIME_FRESH_WINDOW = 600.0

# ------------------------------------------------------------------------------
# PALETTE  (claude/goblin-siege-art-style-guide.md sec.4)
# Style guide hex values are sRGB albedo targets.  Material parameter defaults in
# UE are LINEAR, so each one is converted below.  The sRGB source is kept in the
# comment so the values stay auditable against the guide.
# ------------------------------------------------------------------------------

CHAR_SRGB          = "#26211C"   # Char -- burned wood/thatch end state, near-black warm
CHAR_LINEAR        = (0.01940, 0.01523, 0.01161, 1.0)

TORCHFIRE_SRGB     = "#FF8C1A"   # Torchfire -- flame body / fire-lit emissive
TORCHFIRE_LINEAR   = (1.00000, 0.26225, 0.01033, 1.0)

TORCHCORE_SRGB     = "#FFD166"   # Torchfire Core -- hotter ember highlight, unused default
TORCHCORE_LINEAR   = (1.00000, 0.63760, 0.13290, 1.0)

WARMPLANK_SRGB     = "#8A6A42"   # Warm Plank -- sensible BaseColor preview default
WARMPLANK_LINEAR   = (0.25419, 0.14416, 0.05445, 1.0)


# ------------------------------------------------------------------------------
# RESULT LEDGER + LOGGING
# ------------------------------------------------------------------------------

RESULTS = {}          # asset path -> "CREATED" / "SKIPPED" / "UPDATED" / "FAILED"
NOTES = []            # free-form lines echoed in the final summary
MANUAL_ACTIONS = []   # things a human has to finish in the editor UI


def log(msg):
    print("[GS-BURN] %s" % msg)


def ok(step, msg):
    print("[GS-BURN] PASS  | %-28s | %s" % (step, msg))
    return True


def fail(step, msg):
    print("[GS-BURN] FAIL  | %-28s | %s" % (step, msg))
    return False


def warn(step, msg):
    print("[GS-BURN] WARN  | %-28s | %s" % (step, msg))
    return True


def note(msg):
    NOTES.append(msg)
    log("NOTE  | %s" % msg)


def manual(msg):
    MANUAL_ACTIONS.append(msg)
    log("MANUAL| %s" % msg)


# ------------------------------------------------------------------------------
# PIE GUARD
# This project has been bitten repeatedly by asset writes made while PIE is
# active: the call reports success and nothing reaches disk.  Refuse to run.
# Detection follows fire-vfx-asset-inventory.md rule 11: in PIE,
# UnrealEditorSubsystem.get_editor_world() returns null.
# ------------------------------------------------------------------------------

def is_pie_active():
    try:
        ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    except Exception:
        return False, "UnrealEditorSubsystem unavailable -- could not check PIE"

    editor_world = None
    game_world = None
    try:
        editor_world = ues.get_editor_world()
    except Exception:
        pass
    try:
        game_world = ues.get_game_world()
    except Exception:
        pass

    if editor_world is None:
        return True, "get_editor_world() returned None (classic PIE signature)"
    if game_world is not None and game_world != editor_world:
        return True, "get_game_world() differs from get_editor_world()"
    return False, "editor world '%s', no separate game world" % editor_world.get_name()


def pie_guard():
    active, why = is_pie_active()
    if active:
        bar = "!" * 78
        print(bar)
        print("!! GS-BURN REFUSING TO RUN -- PLAY IN EDITOR IS ACTIVE")
        print("!! %s" % why)
        print("!! Asset writes during PIE silently no-op on this project.")
        print("!! Stop PIE (Esc), then re-run this script.")
        print(bar)
        return False
    return ok("pie-guard", "PIE not active (%s)" % why)


# ------------------------------------------------------------------------------
# PERSISTENCE  (claude/goblin-siege-asset-persistence-rules.md)
# save_asset() returns True when it saved nothing.  Always save_packages with
# only_dirty=False, and always verify by file mtime -- never by return value.
# ------------------------------------------------------------------------------

def content_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())


def disk_path_for(pkg_path):
    """/Game/VFX/Burn/MF_GS_BurnDissolve -> D:/.../Content/VFX/Burn/MF_GS_BurnDissolve.uasset"""
    if not pkg_path.startswith("/Game/"):
        return None
    rel = pkg_path[len("/Game/"):].replace("/", os.sep)
    return os.path.join(content_dir(), rel + ".uasset")


def save_and_verify(pkg_path, step_label=None):
    step_label = step_label or ("save:" + pkg_path.rsplit("/", 1)[-1])
    try:
        asset = unreal.EditorAssetLibrary.load_asset(pkg_path)
        if asset is None:
            return fail(step_label, "load_asset returned None for %s" % pkg_path)
        pkg = asset.get_outermost()
        unreal.EditorLoadingAndSavingUtils.save_packages([pkg], False)  # only_dirty=False
    except Exception:
        traceback.print_exc()
        return fail(step_label, "save_packages threw -- see traceback above")

    fp = disk_path_for(pkg_path)
    if fp is None or not os.path.exists(fp):
        return fail(step_label, "no .uasset on disk at %s" % fp)
    age = time.time() - os.stat(fp).st_mtime
    if age > MTIME_FRESH_WINDOW:
        return fail(step_label,
                    "STALE ON DISK: mtime is %.0fs old -- the save did NOT write bytes" % age)
    return ok(step_label, "on disk, mtime %.0fs old (%s)" % (age, os.path.basename(fp)))


# ------------------------------------------------------------------------------
# SMALL ASSET / GRAPH HELPERS
# ------------------------------------------------------------------------------

def asset_exists(pkg_path):
    try:
        return unreal.EditorAssetLibrary.does_asset_exist(pkg_path)
    except Exception:
        return False


def ensure_dir(pkg_dir):
    try:
        if not unreal.EditorAssetLibrary.does_directory_exist(pkg_dir):
            unreal.EditorAssetLibrary.make_directory(pkg_dir)
        return True
    except Exception:
        traceback.print_exc()
        return False


def lcolor(t):
    return unreal.LinearColor(t[0], t[1], t[2], t[3])


def _conn(a, a_out, b, b_in_candidates):
    """connect_material_expressions with fallback input names.

    Single-input nodes (Abs, OneMinus, Saturate, FunctionOutput) expose an
    unnamed input; '' is the usual spelling but is not guaranteed, so each call
    site passes a candidate list and we take the first that returns True.
    """
    if isinstance(b_in_candidates, str):
        b_in_candidates = [b_in_candidates]
    for name in b_in_candidates:
        try:
            if unreal.MaterialEditingLibrary.connect_material_expressions(a, a_out, b, name):
                return True
        except Exception:
            pass
    log("  connect FAILED: %s.%s -> %s.%s" % (
        a.get_name(), a_out or "<default>", b.get_name(), b_in_candidates))
    return False


def _function_input_names(mf):
    """Best-effort list of the MaterialExpressionFunctionInput names on a
    material function.

    Returns None for UNKNOWN -- "the graph could not be read" and "the graph has
    no inputs" are different answers and the caller acts differently on each.
    Every route is probed rather than assumed, for the same reason the Niagara
    helpers probe: the exact Python binding differs between engine builds and a
    wrong guess here would silently mean "rebuild the function every single run".
    """
    if mf is None:
        return None

    # 1. The purpose-built API, when this build exposes it.
    try:
        ins = unreal.MaterialEditingLibrary.get_inputs_for_material_function(mf)
        if ins:
            names = []
            for e in ins:
                try:
                    names.append(str(e.get_editor_property("input_name")))
                except Exception:
                    pass
            if names:
                return names
    except Exception:
        pass

    # 2. Walk the expression list.  UE5 moved these onto ExpressionCollection and
    #    left FunctionExpressions as a deprecated alias; try both spellings.
    for prop in ("expression_collection", "function_expressions"):
        try:
            coll = mf.get_editor_property(prop)
        except Exception:
            continue
        if coll is None:
            continue
        exprs = coll
        try:
            exprs = coll.get_editor_property("expressions")
        except Exception:
            pass
        try:
            names = []
            for e in exprs:
                if isinstance(e, unreal.MaterialExpressionFunctionInput):
                    names.append(str(e.get_editor_property("input_name")))
            if names:
                return names
        except Exception:
            continue

    return None


# Set by step1 when it rebuilds the function, read by step2.  M_GS_BurnDemo holds
# a HARD reference to the function object, so a rebuilt function leaves the demo
# material pointing at a deleted one -- the demo has to be rebuilt in the same
# run or the "proof" material silently proves nothing.
_MF_REBUILT = [False]


def _set_preview_value(node, rgba):
    """MaterialExpressionFunctionInput.PreviewValue is FVector4f in C++.  Python
    may surface it as unreal.Vector4 or unreal.LinearColor depending on binding
    version -- try both rather than guess."""
    for maker in (lambda: unreal.Vector4(rgba[0], rgba[1], rgba[2], rgba[3]),
                  lambda: lcolor(rgba)):
        try:
            node.set_editor_property("preview_value", maker())
            return True
        except Exception:
            continue
    return False


# ==============================================================================
# STEP 1 -- MF_GS_BurnDissolve
#
#   Burn           = Max( GS_BurnAmount, BurnAmountIn )     <-- 2026-07-31
#
#   BurntBaseColor = Lerp( BaseColor, GS_CharColor,
#                          saturate(Burn * CharMask * GS_CharStrength) )
#
#   EmberEmissive  = GS_EmberColor * GS_EmberGlow * CharMask * BurningBand
#   BurningBand    = saturate( 1 - abs(Burn - 0.5) * 2.5 )
#                    -> 0 at Burn 0.1 and 0.9, 1 at 0.5.  Embers glow while
#                       the object is actively burning and fade out once charred.
#
# THE Max NODE IS THE POINT OF THIS REVISION.  GS_BurnAmount is a
# ScalarParameter, and a parameter can only be set per material instance -- fine
# for props, which get one MID each from UGSBurnFXComponent, and useless for the
# crop, which is ~275,000 painted foliage instances sharing ONE MID.  Per-blade
# burn for foliage can only come from SAMPLING the world burn mask by world
# position, and a texture sample cannot be wired into a parameter node.  So the
# function grew a real INPUT pin, and the two sources are combined with Max:
#
#     props    -> leave BurnAmountIn at 0 (its default), drive the parameter
#     crop     -> leave the parameter at 0 (its default), drive the input from
#                 TextureSample(GS_BurnMask, worldUV).R
#     both     -> the stronger wins, which is the right answer rather than a
#                 compromise: a crate standing in a burnt field should read at
#                 least as charred as the ground under it.
#
# Max rather than a branch/switch deliberately: a branch needs a third "which
# source am I on" parameter that every caller can set wrong, and its failure mode
# is an object that never chars with no error anywhere.  Max has no wrong setting
# -- an unwired input is 0 and 0 is the identity for it.
# ==============================================================================

def _mfx(mf, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression_in_function(mf, cls, x, y)


def step1_material_function():
    step = "1:MF_GS_BurnDissolve"
    try:
        if not ensure_dir(BURN_DIR):
            RESULTS[MF_PATH] = "FAILED"
            return fail(step, "could not create %s" % BURN_DIR)

        _MF_REBUILT[0] = False
        existed = asset_exists(MF_PATH)

        # WHY THE EXISTING ASSET IS NOT AUTOMATICALLY GOOD ENOUGH (2026-07-31).
        # The function on disk predates BurnAmountIn, and a function without that
        # input cannot be driven by the world burn mask at all -- so "already
        # exists, skipped" would print PASS over crop that can never char. The
        # graph is generated in full by this script, so a rebuild costs nothing
        # a human authored.
        stale = False
        if existed and not FORCE_RECREATE:
            names = _function_input_names(unreal.EditorAssetLibrary.load_asset(MF_PATH))
            if names is None:
                # UNKNOWN, not "no inputs". Rebuild rather than risk shipping a
                # function that silently cannot drive foliage -- a rebuild of a
                # generated asset is recoverable, a mask with no pin into the
                # material is a hunt through the whole burn stack.
                warn(step, "could not read the function's inputs on this engine build -- "
                           "rebuilding to be certain BurnAmountIn is present")
                stale = True
            elif "BurnAmountIn" not in names:
                warn(step, "existing function has inputs %s and no BurnAmountIn -- "
                           "rebuilding (it cannot be driven by the world burn mask "
                           "without it)" % names)
                stale = True

        if existed and not FORCE_RECREATE and not stale:
            mf = unreal.EditorAssetLibrary.load_asset(MF_PATH)
            RESULTS[MF_PATH] = "SKIPPED"
            ok(step, "already exists with BurnAmountIn -- left untouched (FORCE_RECREATE=False)")
            _verify_material_function(mf)
            return True

        if existed:
            # The demo material goes FIRST. It holds a hard reference to the
            # function object, so deleting the function under it leaves a
            # dangling pointer and can make the delete itself contentious;
            # dropping the referencer first keeps the delete clean. step2 sees
            # _MF_REBUILT and rebuilds the demo against the new function.
            _MF_REBUILT[0] = True
            if asset_exists(DEMO_PATH):
                unreal.EditorAssetLibrary.delete_asset(DEMO_PATH)
                log("  deleted %s first -- it hard-references the function being rebuilt"
                    % DEMO_PATH)
            unreal.EditorAssetLibrary.delete_asset(MF_PATH)
            log("  deleted existing %s (%s)" % (
                MF_PATH, "FORCE_RECREATE" if FORCE_RECREATE else "missing BurnAmountIn"))

        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mf = tools.create_asset("MF_GS_BurnDissolve", BURN_DIR,
                                unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
        if mf is None:
            RESULTS[MF_PATH] = "FAILED"
            return fail(step, "create_asset returned None")

        # ---- inputs -------------------------------------------------------
        in_base = _mfx(mf, unreal.MaterialExpressionFunctionInput, -900, -160)
        in_base.set_editor_property("input_name", "BaseColor")
        in_base.set_editor_property("input_type", unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3)
        in_base.set_editor_property("description", "Albedo before burning. Wire your atlas/base color here.")
        _set_preview_value(in_base, WARMPLANK_LINEAR)      # Warm Plank #8A6A42
        in_base.set_editor_property("use_preview_value_as_default", True)
        in_base.set_editor_property("sort_priority", 0)

        in_mask = _mfx(mf, unreal.MaterialExpressionFunctionInput, -900, 40)
        in_mask.set_editor_property("input_name", "CharMask")
        in_mask.set_editor_property("input_type", unreal.FunctionInputType.FUNCTION_INPUT_SCALAR)
        in_mask.set_editor_property("description",
                                    "Where burning shows first. Wire vertex color R on burnables. 1.0 = chars first.")
        _set_preview_value(in_mask, (1.0, 1.0, 1.0, 1.0))
        in_mask.set_editor_property("use_preview_value_as_default", True)
        in_mask.set_editor_property("sort_priority", 1)

        # THE MASK-DRIVABLE INPUT (2026-07-31). Everything the crop needs and the
        # one thing the function could not do before: a pin a TEXTURE SAMPLE can
        # reach. ~275,000 wheat instances share one material instance, so there
        # is no per-instance MID to set GS_BurnAmount on; the only per-location
        # burn signal available to foliage is the world burn mask sampled by
        # world position, and a sample cannot feed a ScalarParameter.
        #
        # OPTIONAL: use_preview_value_as_default = True means an unwired caller
        # gets the preview value rather than a compile error, and the preview
        # value is 0.0 -- the identity for the Max below. So every existing
        # caller (M_GS_BurnDemo, any prop material) keeps working untouched and
        # behaves exactly as it did.
        in_burn = _mfx(mf, unreal.MaterialExpressionFunctionInput, -900, 120)
        in_burn.set_editor_property("input_name", "BurnAmountIn")
        in_burn.set_editor_property("input_type", unreal.FunctionInputType.FUNCTION_INPUT_SCALAR)
        in_burn.set_editor_property("description",
                                    "OPTIONAL per-location burn, 0..1. For crop/ground: sample the "
                                    "world burn mask GS_BurnMask by world position and wire its R "
                                    "here. Leave unwired on props, which drive GS_BurnAmount "
                                    "instead. Combined with Max, so the stronger of the two wins.")
        _set_preview_value(in_burn, (0.0, 0.0, 0.0, 0.0))
        in_burn.set_editor_property("use_preview_value_as_default", True)
        in_burn.set_editor_property("sort_priority", 2)

        # ---- parameters ---------------------------------------------------
        # GS_BurnAmount is the C++ contract name. DO NOT RENAME.
        p_burn = _mfx(mf, unreal.MaterialExpressionScalarParameter, -900, 220)
        p_burn.set_editor_property("parameter_name", "GS_BurnAmount")
        p_burn.set_editor_property("default_value", 0.0)
        p_burn.set_editor_property("group", "GS Burn")

        p_charcol = _mfx(mf, unreal.MaterialExpressionVectorParameter, -560, -60)
        p_charcol.set_editor_property("parameter_name", "GS_CharColor")
        p_charcol.set_editor_property("default_value", lcolor(CHAR_LINEAR))   # Char #26211C
        p_charcol.set_editor_property("group", "GS Burn")

        p_charstr = _mfx(mf, unreal.MaterialExpressionScalarParameter, -900, 320)
        p_charstr.set_editor_property("parameter_name", "GS_CharStrength")
        p_charstr.set_editor_property("default_value", 1.0)
        p_charstr.set_editor_property("group", "GS Burn")

        p_glow = _mfx(mf, unreal.MaterialExpressionScalarParameter, -900, 560)
        p_glow.set_editor_property("parameter_name", "GS_EmberGlow")
        p_glow.set_editor_property("default_value", 0.0)
        p_glow.set_editor_property("group", "GS Burn")

        p_embercol = _mfx(mf, unreal.MaterialExpressionVectorParameter, -900, 660)
        p_embercol.set_editor_property("parameter_name", "GS_EmberColor")
        p_embercol.set_editor_property("default_value", lcolor(TORCHFIRE_LINEAR))  # Torchfire #FF8C1A
        p_embercol.set_editor_property("group", "GS Burn")

        # ---- the two burn sources, combined -------------------------------
        # Max, not Add and not a branch. Add would double-char a prop standing in
        # a burnt field and could push past 1 where the saturate hides real
        # signal; a branch needs a "which source" switch every caller can set
        # wrong. Max means each driver simply leaves the other at its 0 default
        # and the stronger reading wins -- correct rather than merely convenient,
        # since a crate in a razed field really should be at least as charred as
        # the ground under it.
        max_burn = _mfx(mf, unreal.MaterialExpressionMax, -760, 170)

        m = 0
        m += _conn(p_burn,  "", max_burn, ["A"])
        m += _conn(in_burn, "", max_burn, ["B"])
        if m != 2:
            warn(step, "burn Max: only %d/2 connections made -- if this is wrong the crop "
                       "cannot be driven by the mask at all" % m)
        else:
            ok(step, "burn sources combined: Max(GS_BurnAmount, BurnAmountIn) (2/2)")

        # ---- char lerp ----------------------------------------------------
        mul_bm = _mfx(mf, unreal.MaterialExpressionMultiply, -640, 220)
        mul_bs = _mfx(mf, unreal.MaterialExpressionMultiply, -480, 220)
        sat_c  = _mfx(mf, unreal.MaterialExpressionSaturate, -340, 220)
        lerp   = _mfx(mf, unreal.MaterialExpressionLinearInterpolate, -180, 0)
        out_bc = _mfx(mf, unreal.MaterialExpressionFunctionOutput, 60, 0)
        out_bc.set_editor_property("output_name", "BurntBaseColor")
        out_bc.set_editor_property("description",
                                   "Base color darkened toward Char by max(GS_BurnAmount, BurnAmountIn).")
        out_bc.set_editor_property("sort_priority", 0)

        # NOTE the A pin takes max_burn, NOT p_burn. Every consumer of the burn
        # amount reads the Max now; leaving one on the raw parameter would give a
        # crop material that chars but never glows (or the reverse), which is a
        # far worse bug to find than a function that plainly does nothing.
        c = 0
        c += _conn(max_burn, "", mul_bm, ["A"])
        c += _conn(in_mask, "", mul_bm, ["B"])
        c += _conn(mul_bm,  "", mul_bs, ["A"])
        c += _conn(p_charstr, "", mul_bs, ["B"])
        c += _conn(mul_bs,  "", sat_c, ["", "Input"])
        c += _conn(in_base, "", lerp, ["A"])
        c += _conn(p_charcol, "", lerp, ["B"])
        c += _conn(sat_c,   "", lerp, ["Alpha"])
        c += _conn(lerp,    "", out_bc, ["", "A"])
        if c != 9:
            warn(step, "char-lerp: only %d/9 connections made -- open the graph and check" % c)
        else:
            ok(step, "char-lerp graph wired (9/9 connections)")

        # ---- ember band ---------------------------------------------------
        k_half = _mfx(mf, unreal.MaterialExpressionConstant, -760, 440)
        k_half.set_editor_property("r", 0.5)
        k_wid = _mfx(mf, unreal.MaterialExpressionConstant, -600, 500)
        k_wid.set_editor_property("r", 2.5)

        sub_b = _mfx(mf, unreal.MaterialExpressionSubtract, -620, 420)
        abs_b = _mfx(mf, unreal.MaterialExpressionAbs, -500, 420)
        mul_w = _mfx(mf, unreal.MaterialExpressionMultiply, -400, 440)
        one_m = _mfx(mf, unreal.MaterialExpressionOneMinus, -300, 440)
        sat_b = _mfx(mf, unreal.MaterialExpressionSaturate, -200, 440)

        mul_e1 = _mfx(mf, unreal.MaterialExpressionMultiply, -100, 600)
        mul_e2 = _mfx(mf, unreal.MaterialExpressionMultiply, 0, 640)
        mul_e3 = _mfx(mf, unreal.MaterialExpressionMultiply, 100, 680)
        out_em = _mfx(mf, unreal.MaterialExpressionFunctionOutput, 260, 640)
        out_em.set_editor_property("output_name", "EmberEmissive")
        out_em.set_editor_property("description",
                                   "Optional ember glow while mid-burn. Wire to Emissive Color if wanted.")
        out_em.set_editor_property("sort_priority", 1)

        # Same rewire as the char lerp: the band is measured off the COMBINED
        # burn, so mask-driven crop gets the ember arc as well as the char.
        e = 0
        e += _conn(max_burn, "", sub_b, ["A"])
        e += _conn(k_half, "", sub_b, ["B"])
        e += _conn(sub_b,  "", abs_b, ["", "Input"])
        e += _conn(abs_b,  "", mul_w, ["A"])
        e += _conn(k_wid,  "", mul_w, ["B"])
        e += _conn(mul_w,  "", one_m, ["", "Input"])
        e += _conn(one_m,  "", sat_b, ["", "Input"])
        e += _conn(p_embercol, "", mul_e1, ["A"])
        e += _conn(sat_b,  "", mul_e1, ["B"])
        e += _conn(mul_e1, "", mul_e2, ["A"])
        e += _conn(p_glow, "", mul_e2, ["B"])
        e += _conn(mul_e2, "", mul_e3, ["A"])
        e += _conn(in_mask, "", mul_e3, ["B"])
        e += _conn(mul_e3, "", out_em, ["", "A"])
        if e != 14:
            warn(step, "ember band: only %d/14 connections made -- open the graph and check" % e)
        else:
            ok(step, "ember-glow graph wired (14/14 connections)")

        # ---- metadata + compile -------------------------------------------
        mf.set_editor_property(
            "description",
            "Goblin Siege burn/char. Darkens BaseColor toward the palette Char value "
            "(#26211C) as the burn amount goes 0->1, weighted by CharMask (vertex color R). "
            "TWO DRIVERS, combined with Max: the GS_BurnAmount scalar parameter, set per-MID "
            "from C++ by UGSBurnFXComponent for PROPS (do not rename), and the BurnAmountIn "
            "input, for CROP AND GROUND - wire TextureSample(GS_BurnMask, world UV).R into it, "
            "because ~275k foliage instances share one MID and cannot have a per-instance "
            "parameter. Leave whichever one you are not using at 0; the stronger wins. "
            "Second output EmberEmissive is an optional Torchfire (#FF8C1A) glow that "
            "peaks mid-burn. Darkening only: no opacity/dissolve in this pass.")
        mf.set_editor_property("expose_to_library", True)
        unreal.MaterialEditingLibrary.update_material_function(mf)

        RESULTS[MF_PATH] = "CREATED"
        save_and_verify(MF_PATH, "1:save-MF")
        _verify_material_function(mf)
        return ok(step, "built at %s" % MF_PATH)

    except Exception:
        traceback.print_exc()
        RESULTS[MF_PATH] = "FAILED"
        return fail(step, "exception -- see traceback above")


def _verify_material_function(mf):
    """Read back what we can cheaply. The authoritative parameter check is done
    in step 2 against the demo material that actually calls this function."""
    try:
        exposed = mf.get_editor_property("expose_to_library")
        desc = mf.get_editor_property("description") or ""
        ok("1:verify-MF", "expose_to_library=%s, description=%d chars" % (exposed, len(desc)))
    except Exception:
        warn("1:verify-MF", "could not read back expose_to_library/description")

    # THE INPUT IS A CONTRACT TOO (2026-07-31). Without BurnAmountIn the crop has
    # no way in at all, so this is checked with the same weight as the C++
    # parameter names -- a PASS on the rest of the function while this is missing
    # would be a lie about the one thing the revision exists for.
    names = _function_input_names(mf)
    if names is None:
        warn("1:verify-inputs", "could not read the function's inputs on this engine build -- "
                                "confirm BaseColor / CharMask / BurnAmountIn by hand")
    elif "BurnAmountIn" not in names:
        fail("1:verify-inputs", "BurnAmountIn MISSING -- inputs are %s. The world burn mask "
                                "cannot drive this function; crop will never char." % names)
    else:
        ok("1:verify-inputs", "inputs %s (BurnAmountIn present)" % names)


# ==============================================================================
# STEP 2 -- M_GS_BurnDemo
# Trivially simple proof material: Warm Plank constant -> MF -> BaseColor,
# EmberEmissive -> Emissive.  Drop on a cube, scrub GS_BurnAmount.
# It doubles as the READ-BACK VERIFICATION for step 1: if GS_BurnAmount shows up
# in this material's scalar parameter list, the function's parameters really did
# propagate to callers, which is the only thing that matters to the C++ side.
# ==============================================================================

def step2_demo_material():
    step = "2:M_GS_BurnDemo"
    try:
        mf = unreal.EditorAssetLibrary.load_asset(MF_PATH)
        if mf is None:
            RESULTS[DEMO_PATH] = "FAILED"
            return fail(step, "%s does not exist -- run step1_material_function() first" % MF_PATH)

        # Rebuild whenever the FUNCTION was rebuilt, not only on FORCE_RECREATE
        # (2026-07-31): the demo holds a hard reference to the function OBJECT,
        # so a function that step1 deleted and recreated leaves this material
        # calling a null function - it would compile to nothing and "prove" the
        # burn works by showing an unlit grey cube. step1 deletes it before the
        # function precisely so this branch rebuilds it cleanly.
        rebuild = FORCE_RECREATE or _MF_REBUILT[0]

        existed = asset_exists(DEMO_PATH)
        if existed and not rebuild:
            RESULTS[DEMO_PATH] = "SKIPPED"
            ok(step, "already exists -- left untouched (FORCE_RECREATE=False)")
            _verify_params_on_material(unreal.EditorAssetLibrary.load_asset(DEMO_PATH))
            return True
        if existed and rebuild:
            unreal.EditorAssetLibrary.delete_asset(DEMO_PATH)
            log("  deleted existing %s (%s)" % (
                DEMO_PATH, "FORCE_RECREATE" if FORCE_RECREATE else "function was rebuilt"))

        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = tools.create_asset("M_GS_BurnDemo", BURN_DIR,
                                 unreal.Material, unreal.MaterialFactoryNew())
        if mat is None:
            RESULTS[DEMO_PATH] = "FAILED"
            return fail(step, "create_asset returned None")

        mel = unreal.MaterialEditingLibrary
        call = mel.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -400, 0)
        call.set_editor_property("material_function", mf)

        base = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -760, -80)
        base.set_editor_property("constant", lcolor(WARMPLANK_LINEAR))
        mask = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -760, 100)
        mask.set_editor_property("r", 1.0)

        # BurnAmountIn is deliberately LEFT UNWIRED here: the demo is the PROP
        # path, driven by scrubbing the GS_BurnAmount parameter, and an unwired
        # optional input falls back to its 0.0 preview value which is the
        # identity for the Max. That it compiles and chars with the input dangling
        # is itself the proof that the new pin did not break existing callers.
        n = 0
        n += _conn(base, "", call, ["BaseColor"])
        n += _conn(mask, "", call, ["CharMask"])
        if n != 2:
            warn(step, "only %d/2 inputs wired into the function call node -- the "
                       "MaterialFunctionCall may not have rebuilt its pins yet" % n)

        p = 0
        try:
            if mel.connect_material_property(call, "BurntBaseColor", unreal.MaterialProperty.MP_BASE_COLOR):
                p += 1
        except Exception:
            traceback.print_exc()
        try:
            if mel.connect_material_property(call, "EmberEmissive", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
                p += 1
        except Exception:
            traceback.print_exc()
        if p != 2:
            warn(step, "only %d/2 function outputs reached material properties" % p)
        else:
            ok(step, "BurntBaseColor -> BaseColor, EmberEmissive -> Emissive")

        mel.recompile_material(mat)
        RESULTS[DEMO_PATH] = "CREATED"
        save_and_verify(DEMO_PATH, "2:save-demo")
        _verify_params_on_material(mat)
        note("Test the function: drop %s on a cube, open it, drag GS_BurnAmount 0 -> 1." % DEMO_PATH)
        return ok(step, "built at %s" % DEMO_PATH)

    except Exception:
        traceback.print_exc()
        RESULTS[DEMO_PATH] = "FAILED"
        return fail(step, "exception -- see traceback above")


def _verify_params_on_material(mat):
    """THE contract check: does GS_BurnAmount exist as a scalar parameter on a
    material that CALLS the function?  That is exactly what a MID created by
    UGSBurnFXComponent looks for at runtime."""
    step = "2:verify-params"
    if mat is None:
        return fail(step, "no material to verify")
    mel = unreal.MaterialEditingLibrary
    try:
        scalars = [str(x) for x in mel.get_scalar_parameter_names(mat)]
        vectors = [str(x) for x in mel.get_vector_parameter_names(mat)]
    except Exception:
        traceback.print_exc()
        return fail(step, "get_*_parameter_names threw -- verify by hand in the material editor")

    want_s = ["GS_BurnAmount", "GS_CharStrength", "GS_EmberGlow"]
    want_v = ["GS_CharColor", "GS_EmberColor"]
    missing = [w for w in want_s if w not in scalars] + [w for w in want_v if w not in vectors]
    log("  scalars found: %s" % scalars)
    log("  vectors found: %s" % vectors)
    if missing:
        return fail(step, "MISSING parameters %s -- the C++ contract is NOT satisfied" % missing)
    return ok(step, "all 5 GS_* parameters present and reachable from a calling material")


# ==============================================================================
# NIAGARA PLUMBING
#
# Rapid-iteration params are tuned through unreal.NiagaraService (VibeUE plugin),
# which is python-exposed and works from the editor console.  The exact method
# names differ between VibeUE builds, so every entry point is probed by getattr
# and reported rather than assumed.
#
# Param names look like:  Constants.<Emitter>.<Module>.<Input>
# They are DISCOVERED and fuzzy-matched here rather than hardcoded -- hardcoding
# them against the wrong duplicate source is the easiest way to silently tune
# nothing at all.
# ==============================================================================

def _svc():
    return getattr(unreal, "NiagaraService", None)


def _svc_call(names, *args):
    """Call the first NiagaraService method that exists from `names`."""
    svc = _svc()
    if svc is None:
        raise RuntimeError("unreal.NiagaraService not available (VibeUE plugin not loaded?)")
    for n in names:
        fn = getattr(svc, n, None)
        if fn is None:
            continue
        return n, fn(*args)
    raise RuntimeError("NiagaraService has none of %s" % (names,))


def _as_param_entries(raw):
    """Normalise whatever list_rapid_iteration_params returned into
    [(name, value_or_None), ...].  Handles list-of-str, list-of-dict,
    list-of-struct and a single JSON/text blob."""
    out = []
    if raw is None:
        return out
    if isinstance(raw, str):
        try:
            import json
            raw = json.loads(raw)
        except Exception:
            raw = [ln.strip() for ln in raw.splitlines() if "Constants." in ln]
    if isinstance(raw, dict):
        for k in ("params", "parameters", "results", "data"):
            if k in raw:
                raw = raw[k]
                break
    if isinstance(raw, dict):
        for k, v in raw.items():
            if "Constants." in str(k):
                out.append((str(k), v))
        return out
    try:
        items = list(raw)
    except Exception:
        return out
    for it in items:
        if isinstance(it, str):
            if "Constants." in it:
                out.append((it.strip(), None))
        elif isinstance(it, dict):
            nm = it.get("name") or it.get("Name") or it.get("param") or it.get("parameter")
            if nm:
                out.append((str(nm), it.get("value", it.get("Value"))))
        else:
            nm = getattr(it, "name", None) or getattr(it, "parameter_name", None)
            if nm:
                out.append((str(nm), getattr(it, "value", None)))
    return out


def list_params(system_path):
    try:
        used, raw = _svc_call(["list_rapid_iteration_params",
                               "get_rapid_iteration_params",
                               "list_rapid_iteration_parameters"], system_path)
        entries = _as_param_entries(raw)
        log("  %s -> %d rapid-iteration params (via %s)" % (system_path, len(entries), used))
        return entries
    except Exception:
        traceback.print_exc()
        return []


def emitters_in_order(entries):
    """Constants.<Emitter>.<Module>.<Input> -> ordered unique <Emitter> list."""
    seen, order = set(), []
    for name, _ in entries:
        parts = name.split(".")
        if len(parts) >= 2 and parts[1] not in seen:
            seen.add(parts[1])
            order.append(parts[1])
    return order


def find_param(entries, emitter, needles):
    """All needles must appear (case-insensitive) in a param on this emitter."""
    lo = [n.lower() for n in needles]
    hits = [nm for nm, _ in entries
            if nm.split(".")[1:2] == [emitter] and all(x in nm.lower() for x in lo)]
    hits.sort(key=len)
    return hits


def read_param(system_path, emitter, full_name):
    try:
        _, val = _svc_call(["get_rapid_iteration_param",
                            "get_rapid_iteration_param_value",
                            "read_rapid_iteration_param"], system_path, emitter, full_name)
        return val
    except Exception:
        return None


def set_param(system_path, emitter, full_name, value, is_vector=False):
    """Set + READ BACK.  Vectors are written in the '0.0,0.0,130.0' form only --
    UE struct notation reports success and writes (0,0,0) (runbook rule 8)."""
    label = full_name.split(".", 1)[-1]
    try:
        _svc_call(["set_rapid_iteration_param"], system_path, emitter, full_name, str(value))
    except Exception:
        traceback.print_exc()
        return fail("param", "SET THREW  %s = %s" % (label, value))

    back = read_param(system_path, emitter, full_name)
    if back is None:
        # No getter exposed on this VibeUE build: fall back to re-listing.
        for nm, v in list_params(system_path):
            if nm == full_name and v is not None:
                back = v
                break
    if back is None:
        return warn("param", "set %s = %s  (READ-BACK UNAVAILABLE -- verify by hand)"
                    % (label, value))

    bs = str(back)
    if is_vector:
        zeroish = all(abs(float(x)) < 1e-6 for x in _floats(bs)) if _floats(bs) else False
        want_zero = all(abs(float(x)) < 1e-6 for x in _floats(str(value)))
        if zeroish and not want_zero:
            return fail("param", "VECTOR ZEROED: %s wanted %s, read back %s "
                                 "(wrong string format -- runbook rule 8)" % (label, value, bs))
        return ok("param", "%s = %s (read back %s)" % (label, value, bs))
    return ok("param", "%s = %s (read back %s)" % (label, value, bs))


def _floats(s):
    out = []
    tok = ""
    for ch in str(s) + " ":
        if ch.isdigit() or ch in "-.eE+":
            tok += ch
        else:
            if tok:
                try:
                    out.append(float(tok))
                except ValueError:
                    pass
                tok = ""
    return out


def apply_tuning(system_path, tuning_by_role):
    """tuning_by_role: {role_index: [(needles_tuple, value, is_vector), ...]}
    Role 0 = the first emitter in the system (the smoke/body emitter),
    role 1 = the second (embers), if it exists.  Emitter names are discovered,
    printed, and matched by position -- duplicate sources differ."""
    entries = list_params(system_path)
    if not entries:
        return fail("tune", "no rapid-iteration params discovered for %s" % system_path)
    ems = emitters_in_order(entries)
    log("  emitters discovered (in order): %s" % ems)

    applied = 0
    skipped = 0
    for role, rows in sorted(tuning_by_role.items()):
        if role >= len(ems):
            warn("tune", "no emitter at role index %d -- skipping %d params" % (role, len(rows)))
            skipped += len(rows)
            continue
        em = ems[role]
        log("  --- role %d -> emitter '%s' ---" % (role, em))
        for needles, value, is_vec in rows:
            hits = find_param(entries, em, needles)
            if not hits:
                warn("tune", "no param on '%s' matching %s -- skipped" % (em, list(needles)))
                skipped += 1
                continue
            if len(hits) > 1:
                warn("tune", "%s matched %d params, using shortest: %s" % (list(needles), len(hits), hits))
            if set_param(system_path, em, hits[0], value, is_vec):
                applied += 1
            else:
                skipped += 1
    if skipped:
        warn("tune", "%s: %d params applied, %d skipped/failed" % (system_path, applied, skipped))
    else:
        ok("tune", "%s: %d params applied, all read back" % (system_path, applied))
    return applied > 0


# ------------------------------------------------------------------------------
# COMPILE THE DUPLICATE  (fire-vfx-asset-inventory.md rule 3 -- NOT OPTIONAL)
#
# duplicate_asset copies the system WITHOUT compiling it.  An uncompiled
# duplicate loads, lists params, sets params, reads them back, and reports itself
# assigned on a NiagaraComponent -- and then dies at runtime with
#   LogNiagara: Error: Error initializing data interfaces. Completing system.
# and renders nothing.  Opening the asset editor forces the compile.
#
# time.sleep() would block the game thread and the compile would never progress,
# so the wait is done on a slate post-tick callback and the finalise step prints
# its own PASS/FAIL later.  If the async route is unavailable, call
#   finish_niagara("/Game/VFX/NS_GS_Smolder")
# by hand after the editor has been open ~30s.
# ------------------------------------------------------------------------------

def newest_log_file():
    try:
        d = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_log_dir())
        files = glob.glob(os.path.join(d, "*.log"))
        return max(files, key=os.path.getmtime) if files else None
    except Exception:
        return None


def log_shows_compile(system_name):
    """The runbook's substitute for eyes: grep for 'Compiling System <name>'."""
    lf = newest_log_file()
    if not lf:
        return None, "no log file found"
    try:
        with open(lf, "r", errors="ignore") as f:
            text = f.read()
    except Exception:
        return None, "could not read %s" % lf
    hits = [ln for ln in text.splitlines()
            if "Compiling System" in ln and system_name in ln]
    errs = [ln for ln in text.splitlines()
            if "Error initializing data interfaces" in ln and system_name in ln]
    return (len(hits) > 0 and len(errs) == 0), \
           "%d 'Compiling System %s' line(s), %d data-interface error(s) in %s" % (
               len(hits), system_name, len(errs), os.path.basename(lf))


def open_for_compile(system_path):
    step = "compile-open:" + system_path.rsplit("/", 1)[-1]
    try:
        asset = unreal.EditorAssetLibrary.load_asset(system_path)
        if asset is None:
            return fail(step, "load_asset returned None")
        aes = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        aes.open_editor_for_assets([asset])
        return ok(step, "asset editor opened -- compile in flight, ~%.0fs" % NIAGARA_COMPILE_WAIT)
    except Exception:
        traceback.print_exc()
        return fail(step, "could not open the asset editor -- open %s by hand, wait for the "
                          "compile, then call finish_niagara('%s')" % (system_path, system_path))


def finish_niagara(system_path):
    """Save, verify by mtime, grep the log for the compile line, close the editor.
    Safe to call by hand at any time after the asset editor has been open a while."""
    name = system_path.rsplit("/", 1)[-1]
    step = "compile-verify:" + name
    okc, why = log_shows_compile(name)
    if okc is True:
        ok(step, why)
    elif okc is False:
        fail(step, "NOT COMPILED -- %s. Leave the asset editor open longer, then re-run "
                   "finish_niagara('%s'). Do not ship an uncompiled duplicate." % (why, system_path))
    else:
        warn(step, "could not check the log (%s) -- verify by hand" % why)

    saved = save_and_verify(system_path, "save:" + name)
    try:
        asset = unreal.EditorAssetLibrary.load_asset(system_path)
        aes = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        aes.close_all_editors_for_asset(asset)
        log("  closed asset editor for %s" % name)
    except Exception:
        warn(step, "could not close the asset editor for %s (harmless)" % name)
    return bool(okc) and saved


_COMPILE_QUEUE = []
_TICK_HANDLE = [None]
_TICK_T0 = [0.0]


def _compile_tick(delta_seconds):
    try:
        if time.time() - _TICK_T0[0] < NIAGARA_COMPILE_WAIT:
            return
        if _TICK_HANDLE[0] is not None:
            unreal.unregister_slate_post_tick_callback(_TICK_HANDLE[0])
            _TICK_HANDLE[0] = None
        for p in list(_COMPILE_QUEUE):
            finish_niagara(p)
        del _COMPILE_QUEUE[:]
        print("")
        print("[GS-BURN] ==== NIAGARA COMPILE PHASE COMPLETE ====")
        print_summary()
    except Exception:
        traceback.print_exc()


def start_compile_phase():
    """Open every queued system's editor, then finalise them all on a slate tick."""
    if not _COMPILE_QUEUE:
        return warn("compile", "nothing queued for compile")
    for p in _COMPILE_QUEUE:
        open_for_compile(p)
    try:
        _TICK_T0[0] = time.time()
        _TICK_HANDLE[0] = unreal.register_slate_post_tick_callback(_compile_tick)
        return ok("compile", "waiting %.0fs for %d system(s) to compile; the final summary "
                             "prints when they are done" % (NIAGARA_COMPILE_WAIT, len(_COMPILE_QUEUE)))
    except Exception:
        traceback.print_exc()
        manual("register_slate_post_tick_callback unavailable. Wait ~30s, then run: " +
               "; ".join("finish_niagara('%s')" % p for p in _COMPILE_QUEUE))
        return fail("compile", "async wait unavailable -- see MANUAL action above")


def duplicate_from(sources, dest_path, label):
    """Duplicate the first source that exists.  Returns (ok, source_used)."""
    for src in sources:
        if not asset_exists(src):
            log("  source not found, trying next: %s" % src)
            continue
        try:
            dup = unreal.EditorAssetLibrary.duplicate_asset(src, dest_path)
        except Exception:
            traceback.print_exc()
            dup = None
        if dup is not None:
            ok(label, "duplicated from %s" % src)
            return True, src
        warn(label, "duplicate_asset returned None from %s" % src)
    fail(label, "no usable duplicate source in %s" % sources)
    return False, None


# ==============================================================================
# STEP 3 -- NS_GS_SmokeColumn
# Tall, slow, drifting column.  AGSFireVolume already HAS a SmokeFX component and
# a SmokeSystem soft pointer, so once this asset is right the smoke appears with
# no C++ change (see the SmokeSystem note in the final summary).
# ==============================================================================

SMOKE_TUNING = {
    0: [  # body / smoke emitter
        (("SpawnRate",),                        22.0,             False),
        (("Lifetime", "Min"),                   3.5,              False),
        (("Lifetime", "Max"),                   6.0,              False),
        (("SpriteSize", "Min"),                 90.0,             False),
        (("SpriteSize", "Max"),                 180.0,            False),
        (("Sphere", "Radius"),                  70.0,             False),
        (("AddVelocity", "Velocity"),           "0.0,0.0,110.0",  True),
        (("Gravity",),                          "0.0,0.0,25.0",   True),   # gentle buoyancy
        (("Drag",),                             1.1,              False),
        (("Cone", "Angle"),                     14.0,             False),
    ],
    1: [  # embers / secondary emitter -- almost off on a pure smoke column
        (("SpawnRate",),                        4.0,              False),
        (("SpriteSize", "Min"),                 1.5,              False),
        (("SpriteSize", "Max"),                 3.0,              False),
        (("AddVelocity", "Velocity"),           "0.0,0.0,60.0",   True),
        (("Gravity",),                          "0.0,0.0,-40.0",  True),
    ],
}


def step3_smoke_column():
    step = "3:NS_GS_SmokeColumn"
    try:
        existed = asset_exists(SMOKE_PATH)
        if existed and FORCE_RECREATE:
            unreal.EditorAssetLibrary.delete_asset(SMOKE_PATH)
            log("  FORCE_RECREATE: deleted existing %s" % SMOKE_PATH)
            existed = False

        if existed:
            RESULTS[SMOKE_PATH] = "SKIPPED"
            ok(step, "already exists -- retuning and re-compiling rather than clobbering")
        else:
            done, src = duplicate_from(SMOKE_SOURCES, SMOKE_PATH, step)
            if not done:
                RESULTS[SMOKE_PATH] = "FAILED"
                return False
            RESULTS[SMOKE_PATH] = "CREATED"
            note("NS_GS_SmokeColumn duplicated from %s" % src)

        apply_tuning(SMOKE_PATH, SMOKE_TUNING)
        _COMPILE_QUEUE.append(SMOKE_PATH)
        manual("NS_GS_SmokeColumn: confirm the sprite renderer material is M_Smoke_01 "
               "(%s). M_Smoke_02 is BROKEN -- never use it. Renderer material assignment "
               "is not reachable from editor Python." % M_SMOKE_01)
        return ok(step, "tuned; queued for compile")
    except Exception:
        traceback.print_exc()
        RESULTS[SMOKE_PATH] = "FAILED"
        return fail(step, "exception -- see traceback above")


# ==============================================================================
# STEP 4 -- NS_GS_Smolder
# "This place is burnt": thin smoke + sparse slow embers, low spawn rates, loops
# forever, cheap enough to leave running on many actors until raid end.
# PATH IS A CONTRACT: UGSBurnFXComponent soft-references /Game/VFX/NS_GS_Smolder.
# ==============================================================================

SMOLDER_TUNING = {
    0: [  # thin smoke
        (("SpawnRate",),                        6.0,              False),
        (("Lifetime", "Min"),                   4.0,              False),
        (("Lifetime", "Max"),                   7.0,              False),
        (("SpriteSize", "Min"),                 40.0,             False),
        (("SpriteSize", "Max"),                 85.0,             False),
        (("Sphere", "Radius"),                  45.0,             False),
        (("AddVelocity", "Velocity"),           "0.0,0.0,45.0",   True),
        (("Gravity",),                          "0.0,0.0,12.0",   True),
        (("Drag",),                             1.6,              False),
        (("Cone", "Angle"),                     10.0,             False),
    ],
    1: [  # sparse slow embers
        (("SpawnRate",),                        1.5,              False),
        (("Lifetime", "Min"),                   1.2,              False),
        (("Lifetime", "Max"),                   2.4,              False),
        (("SpriteSize", "Min"),                 1.0,              False),
        (("SpriteSize", "Max"),                 2.2,              False),
        (("AddVelocity", "Velocity"),           "0.0,0.0,55.0",   True),
        (("Gravity",),                          "0.0,0.0,-90.0",  True),
    ],
}


def step4_smolder():
    step = "4:NS_GS_Smolder"
    try:
        existed = asset_exists(SMOLDER_PATH)
        if existed and FORCE_RECREATE:
            unreal.EditorAssetLibrary.delete_asset(SMOLDER_PATH)
            log("  FORCE_RECREATE: deleted existing %s" % SMOLDER_PATH)
            existed = False

        if existed:
            RESULTS[SMOLDER_PATH] = "SKIPPED"
            ok(step, "already exists -- retuning and re-compiling rather than clobbering")
        else:
            done, src = duplicate_from(SMOLDER_SOURCES, SMOLDER_PATH, step)
            if not done:
                RESULTS[SMOLDER_PATH] = "FAILED"
                return False
            RESULTS[SMOLDER_PATH] = "CREATED"
            note("NS_GS_Smolder duplicated from %s" % src)

        apply_tuning(SMOLDER_PATH, SMOLDER_TUNING)
        _COMPILE_QUEUE.append(SMOLDER_PATH)
        manual("NS_GS_Smolder: set Emitter State -> Loop Behavior = Infinite on BOTH "
               "emitters. Loop Behavior is an enum, invisible to the rapid-iteration "
               "param API (runbook rule 4) and not settable from editor Python. If the "
               "duplicate source was NS_GS_TorchFlame it is already Infinite.")
        manual("NS_GS_Smolder: this is the persistent system -- confirm spawn rates stay "
               "low enough to leave running on many actors until raid end.")
        return ok(step, "tuned; queued for compile")
    except Exception:
        traceback.print_exc()
        RESULTS[SMOLDER_PATH] = "FAILED"
        return fail(step, "exception -- see traceback above")


# ==============================================================================
# STEP 5 -- SUMMARY
# ==============================================================================

ALL_ASSETS = [MF_PATH, DEMO_PATH, SMOKE_PATH, SMOLDER_PATH]


def print_summary():
    print("")
    print("=" * 78)
    print("GS BURN BUILD -- SUMMARY")
    print("=" * 78)
    for p in ALL_ASSETS:
        verdict = RESULTS.get(p, "NOT RUN")
        on_disk = "?"
        fp = disk_path_for(p)
        if fp and os.path.exists(fp):
            age = time.time() - os.stat(fp).st_mtime
            on_disk = "on disk, mtime %.0fs old" % age
        elif fp:
            on_disk = "NO FILE ON DISK"
        print("  %-9s %-38s %s" % (verdict, p, on_disk))

    print("-" * 78)
    print("  PARAMETERS EXPOSED BY MF_GS_BurnDissolve (contract with C++):")
    print("    GS_BurnAmount    scalar  0.0   <- DRIVEN BY UGSBurnFXComponent. DO NOT RENAME.")
    print("    GS_CharStrength  scalar  1.0")
    print("    GS_EmberGlow     scalar  0.0")
    print("    GS_CharColor     vector  Char      #26211C")
    print("    GS_EmberColor    vector  Torchfire #FF8C1A")
    print("  FUNCTION INPUTS : BaseColor    (Vector3, default Warm Plank)")
    print("                    CharMask     (Scalar,  default 1.0)")
    print("                    BurnAmountIn (Scalar,  default 0.0, OPTIONAL)")
    print("  FUNCTION OUTPUTS: [0] BurntBaseColor   [1] EmberEmissive")
    print("")
    print("  TWO WAYS TO DRIVE THE BURN -- burn = Max(GS_BurnAmount, BurnAmountIn):")
    print("    PROPS    set the PARAMETER. UGSBurnFXComponent makes a MID per burning actor")
    print("             and writes GS_BurnAmount on it. Leave BurnAmountIn unwired (= 0).")
    print("    CROP /   drive the INPUT. ~275k wheat instances share ONE MID, so there is no")
    print("    GROUND   per-instance parameter to set; per-location burn can only come from")
    print("             sampling the world burn mask by world position, and a TextureSample")
    print("             cannot feed a ScalarParameter. Leave GS_BurnAmount at 0.")
    print("    BOTH     the stronger wins -- a crate in a burnt field should read at least as")
    print("             charred as the ground under it. That is why this is Max, not a branch.")

    if NOTES:
        print("-" * 78)
        print("  NOTES:")
        for n in NOTES:
            print("    - %s" % n)

    print("-" * 78)
    print("  MANUAL ACTIONS (things Python cannot do -- a human must finish these):")
    for m in MANUAL_ACTIONS:
        print("    - %s" % m)
    print("    - CROP MATERIAL. This is the wiring that turns the world burn mask into")
    print("      burnt wheat; nothing in C++ or in this script can do it, because a master")
    print("      material's graph is authored, not generated.")
    print("")
    print("      Crop material (duplicate M_Grass_Master -> /Game/VFX/Burn/M_GS_Crop_Master):")
    print("        UV   = (AbsoluteWorldPosition.xy - GS_BurnMaskOrigin.xy) / GS_BurnMaskSize.xy")
    print("        Mask = TextureSample(GS_BurnMask, UV)")
    print("        Mask.R -> MF_GS_BurnDissolve.BurnAmountIn")
    print("        BaseColor -> MF_GS_BurnDissolve.BaseColor")
    print("        MF output BurntBaseColor -> Base Color;  EmberEmissive -> Emissive")
    print("        Then reparent a GS-owned copy of MI_VillageWheat to M_GS_Crop_Master.")
    print("")
    print("      GS_BurnMask / GS_BurnMaskOrigin / GS_BurnMaskSize are published onto every")
    print("      bound crop MID by UGSBurnMaskSubsystem -- the names are its contract, and a")
    print("      parameter the material does not declare is a silent no-op, so a typo here")
    print("      shows up as crop that never chars rather than as any kind of error.")
    print("      Leave GS_BurnAmount alone on the crop: it stays 0 and the Max takes the mask.")
    print("    - AGSFireVolume currently defaults SmokeSystem to")
    print("      /Game/VolcanoEnvironmentVFX/VFX/Niagara/NS_FlameSmoke, NOT NS_GS_SmokeColumn.")
    print("      Repoint it in GSFireVolume.cpp (or per-instance) if the hand-tuned column")
    print("      is the one you want on screen.")
    print("    - No screenshot route from the agent: whether any of this READS RIGHT is")
    print("      Michael's call. Everything above is existence + compile + read-back only.")
    print("=" * 78)


# ==============================================================================
# MAIN
# ==============================================================================

def main():
    print("")
    print("=" * 78)
    print("GS BURN BUILD -- gs_build_burn_material.py   (FORCE_RECREATE=%s)" % FORCE_RECREATE)
    print("=" * 78)

    if not pie_guard():
        return False

    del _COMPILE_QUEUE[:]
    del NOTES[:]
    del MANUAL_ACTIONS[:]

    for fn in (step1_material_function, step2_demo_material,
               step3_smoke_column, step4_smolder):
        try:
            fn()
        except Exception:
            traceback.print_exc()
            fail(fn.__name__, "unhandled exception -- continuing with the next step")

    # Materials are done and verified now.  The Niagara systems still need their
    # compile, which cannot be waited on synchronously; the compile phase prints
    # a second summary when it finishes.
    print_summary()
    start_compile_phase()
    return True


if __name__ == "__main__":
    main()
