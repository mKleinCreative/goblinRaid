# -*- coding: utf-8 -*-
"""
================================================================================
gs_build_aim_materials.py -- Goblin Siege | aim arc + landing reticle materials
2026-08-05.  Target: UE 5.8, project "GoblinSiege 5.8".
================================================================================

WHY THIS EXISTS

  UGSAimComponent (Source/GoblinSiege/Combat/) predicts the trajectory of a
  thrown torch or a loosed arrow and draws it with real primitives -- a pool of
  USplineMeshComponents for the ribbon and a UDecalComponent for the landing
  marker -- rather than DrawDebug lines, which are compiled out of a shipping
  build.  Real primitives need real materials, and until those exist the
  component resolves nothing, warns once, and draws NOTHING.  That is the
  "we can't see where we're aiming" symptom this script fixes.

WHAT THIS BUILDS

  1. /Game/VFX/Aim/M_GS_AimArc       Unlit, translucent, two-sided.  The tube
     the ribbon is made of.  Solid down the middle, TRANSPARENT AT THE EDGES,
     via a Fresnel falloff -- not a UV gradient.  That choice is deliberate: the
     ribbon is built from spline-mesh segments of whatever mesh you assign, and
     a UV gradient would look completely different on a cylinder, a cube and a
     custom strip.  Fresnel is measured against the camera, so the tube reads as
     a soft round beam no matter which mesh is driving it and no matter how the
     segment is stretched or twisted along the arc.

  2. /Game/VFX/Aim/M_GS_AimLanding   Deferred decal, emissive blend.  The
     reticle: RED CENTRE FADING TO TRANSPARENT EDGES, built as a radial gradient
     off UV0, exactly the "soft dot" shape asked for.  A decal rather than a
     flat mesh so the marker lies ON the ground it is marking -- on a hillside a
     flat disc either floats above the slope or sinks into it, which is precisely
     where a landing indicator most needs to be right.

  Both are generated from nodes.  NO TEXTURE ASSET is involved, so there is no
  import step, nothing to keep in sync, and the falloff is a parameter you can
  scrub in the material instance rather than a baked image you would have to
  re-author to change.

THE C++ CONTRACT -- DO NOT RENAME

      Colour          VectorParameter, on BOTH materials

  UGSAimComponent::UpdateArcVisual does

      ArcMID->SetVectorParameterValue(ArcColourParameterName, Colour);

  where ArcColourParameterName defaults to the literal "Colour" (British
  spelling, matching the rest of this codebase).  SetVectorParameterValue against
  a name that does not exist is a SILENT NO-OP -- no warning, no error, the arc
  simply never changes colour.  Same failure mode as GS_BurnAmount in
  gs_build_burn_material.py, and the same rule: rename it here and you must
  rename ArcColourParameterName in C++ to match.

  READ THIS BEFORE ASKING WHY THE ARC IS NOT RED.  The red set here is only the
  material's DEFAULT.  At runtime the component overrides it every frame with a
  per-mode colour, and neither mode is red:

      torch  ->  UGSAimComponent::TorchArcColour   (1.0, 0.45, 0.1)  orange
      bow    ->  UGSAimComponent::BowArcColour     (0.75, 0.85, 0.95) pale blue

  Both are EditDefaultsOnly on the aim component, so if you want red in play,
  change them on BP_GSPlayerCharacter -- no recompile needed.  The red default
  here is what you see in the material editor preview and in any use that does
  not go through the MID.

OTHER PARAMETERS (all safe to retune; none are referenced from C++)

      Brightness      scalar, emissive multiplier.  >1 to bloom.
      Opacity         scalar, overall alpha.
      EdgeFalloff     scalar, on the reticle only.  Higher = tighter hot centre
                      and a wider soft skirt.  1 = linear ramp.

HOW TO RUN

  Editor Output Log -> cmd dropdown -> Python.  NOT while PIE is running.

      exec(open(r"D:/goblinRaid/GoblinSiege 5.8/Content/Python/gs_build_aim_materials.py").read())

  or, with Content/Python on the editor python path:

      import gs_build_aim_materials as g
      g.main()

  Individual steps are re-runnable:
      g.step1_arc_material()   g.step2_landing_material()
      g.step3_report_assignment()   g.print_summary()

  Idempotent: existing assets are detected and left alone.  Set FORCE_RECREATE
  below to rebuild from scratch instead.

BINDING PROJECT RULES HONOURED HERE
  * PIE guard.  Asset writes during PIE silently no-op on this project.
  * save_asset() returns True when it saved nothing.  Saving is
    EditorLoadingAndSavingUtils.save_packages([pkg], only_dirty=False) and is
    VERIFIED BY FILE MTIME (goblin-siege-asset-persistence-rules.md).
  * EditorAssetSubsystem, not EditorAssetLibrary.  CLAUDE.md, 2026-08-04:
    EditorAssetLibrary silently LIES in this build -- does_asset_exist() returns
    False and load_asset() returns None for assets that demonstrably exist.
    Every call here goes through the subsystem, with the library only as a
    fallback for builds where the subsystem is missing.
  * Node input pin names are probed from a candidate list, never assumed --
    the _conn helper is lifted from gs_build_burn_material.py for that reason.
  * Material parameter defaults are LINEAR, not sRGB.
  * The agent has no screenshot route.  Nothing here claims anything "looks
    right" -- only that it exists, compiled, and read back correct.  Final
    visual judgement is Michael's.
================================================================================
"""

import os
import time
import traceback

import unreal

# ------------------------------------------------------------------------------
# CONFIG
# ------------------------------------------------------------------------------

FORCE_RECREATE = False

AIM_DIR      = "/Game/VFX/Aim"
ARC_PATH     = "/Game/VFX/Aim/M_GS_AimArc"
LANDING_PATH = "/Game/VFX/Aim/M_GS_AimLanding"

# The C++ contract name. See the header. DO NOT RENAME without also changing
# UGSAimComponent::ArcColourParameterName.
COLOUR_PARAM = "Colour"

# Linear. Pure sRGB red (#FF0000) is linear (1,0,0); this is nudged very slightly
# warm so it does not read as a flat UI red against grass and thatch.
RED_LINEAR = (1.0, 0.035, 0.015, 1.0)

MTIME_FRESH_WINDOW = 600.0

# Suggested starter mesh for the ribbon. Symmetric on all three axes, which
# matters: UGSAimComponent calls SetForwardAxis(ESplineMeshAxis::X), and a mesh
# whose long axis is Z (the engine Cylinder) would be stretched along the wrong
# axis and look wrong. A cube is 100x100x100 so X is as good as any, and scaled
# to ArcSegmentWidth (4uu) nobody can tell it is not round.
SUGGESTED_ARC_MESH = "/Engine/BasicShapes/Cube"


# ------------------------------------------------------------------------------
# LEDGER + LOGGING  (same shape as gs_build_burn_material.py)
# ------------------------------------------------------------------------------

RESULTS = {}
NOTES = []
MANUAL_ACTIONS = []


def log(msg):
    print("[GS-AIM] %s" % msg)


def ok(step, msg):
    print("[GS-AIM] PASS  | %-26s | %s" % (step, msg))
    return True


def fail(step, msg):
    print("[GS-AIM] FAIL  | %-26s | %s" % (step, msg))
    return False


def warn(step, msg):
    print("[GS-AIM] WARN  | %-26s | %s" % (step, msg))
    return True


def note(msg):
    NOTES.append(msg)
    log("NOTE  | %s" % msg)


def manual(msg):
    MANUAL_ACTIONS.append(msg)
    log("MANUAL| %s" % msg)


# ------------------------------------------------------------------------------
# ASSET SUBSYSTEM WRAPPERS
# CLAUDE.md 2026-08-04: EditorAssetLibrary is a silent no-op in this build --
# it does not raise, it just reports False/None for assets that exist, which
# reads as "the asset is missing". Everything goes through the subsystem.
# ------------------------------------------------------------------------------

def _aes():
    try:
        return unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    except Exception:
        return None


def asset_exists(pkg_path):
    sub = _aes()
    if sub is not None:
        try:
            return sub.does_asset_exist(pkg_path)
        except Exception:
            pass
    try:
        return unreal.EditorAssetLibrary.does_asset_exist(pkg_path)
    except Exception:
        return False


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


def delete_asset(pkg_path):
    sub = _aes()
    if sub is not None:
        try:
            return sub.delete_asset(pkg_path)
        except Exception:
            pass
    try:
        return unreal.EditorAssetLibrary.delete_asset(pkg_path)
    except Exception:
        return False


def ensure_dir(pkg_dir):
    sub = _aes()
    try:
        if sub is not None:
            if not sub.does_directory_exist(pkg_dir):
                sub.make_directory(pkg_dir)
            return True
        if not unreal.EditorAssetLibrary.does_directory_exist(pkg_dir):
            unreal.EditorAssetLibrary.make_directory(pkg_dir)
        return True
    except Exception:
        traceback.print_exc()
        return False


# ------------------------------------------------------------------------------
# PIE GUARD  (fire-vfx-asset-inventory.md rule 11)
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
        print("!! GS-AIM REFUSING TO RUN -- PLAY IN EDITOR IS ACTIVE")
        print("!! %s" % why)
        print("!! Asset writes during PIE silently no-op on this project.")
        print("!! Stop PIE (Esc), then re-run this script.")
        print(bar)
        return False
    return ok("pie-guard", "PIE not active (%s)" % why)


# ------------------------------------------------------------------------------
# PERSISTENCE -- save_asset() lies, verify by mtime
# ------------------------------------------------------------------------------

def content_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())


def disk_path_for(pkg_path):
    if not pkg_path.startswith("/Game/"):
        return None
    rel = pkg_path[len("/Game/"):].replace("/", os.sep)
    return os.path.join(content_dir(), rel + ".uasset")


def save_and_verify(pkg_path, step_label=None):
    step_label = step_label or ("save:" + pkg_path.rsplit("/", 1)[-1])
    try:
        asset = load_asset(pkg_path)
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
# GRAPH HELPERS
# ------------------------------------------------------------------------------

def lcolor(t):
    return unreal.LinearColor(t[0], t[1], t[2], t[3])


def _mx(mat, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(mat, cls, x, y)


def _conn(a, a_out, b, b_in_candidates):
    """connect_material_expressions with fallback input names.

    Lifted verbatim in spirit from gs_build_burn_material.py: single-input nodes
    expose an unnamed input, '' is the usual spelling but is not guaranteed
    across engine builds, so each call site passes candidates and we take the
    first that returns True. A wrong guess here is a silently unwired graph.
    """
    if isinstance(b_in_candidates, str):
        b_in_candidates = [b_in_candidates]
    for name in b_in_candidates:
        try:
            if unreal.MaterialEditingLibrary.connect_material_expressions(a, a_out, b, name):
                return 1
        except Exception:
            pass
    log("  connect FAILED: %s.%s -> %s.%s" % (
        a.get_name(), a_out or "<default>", b.get_name(), b_in_candidates))
    return 0


def _prop(node, name, value, label=""):
    """set_editor_property that reports rather than throws. Node property names
    drift between engine versions (Fresnel's 'exponent' especially), and a
    hard failure here would abandon an otherwise-good graph."""
    try:
        node.set_editor_property(name, value)
        return True
    except Exception:
        warn("prop", "could not set %s.%s%s -- leaving the node default"
             % (node.get_name(), name, (" (%s)" % label) if label else ""))
        return False


def _colour_and_brightness(mat, x, y):
    """The Colour vector parameter (THE C++ contract name) times a Brightness
    scalar. Returns (multiply_node, colour_node)."""
    colour = _mx(mat, unreal.MaterialExpressionVectorParameter, x, y)
    _prop(colour, "parameter_name", COLOUR_PARAM)
    _prop(colour, "default_value", lcolor(RED_LINEAR))
    _prop(colour, "group", "GS Aim")

    bright = _mx(mat, unreal.MaterialExpressionScalarParameter, x, y + 130)
    _prop(bright, "parameter_name", "Brightness")
    _prop(bright, "default_value", 5.0)
    _prop(bright, "group", "GS Aim")

    mul = _mx(mat, unreal.MaterialExpressionMultiply, x + 300, y + 30)
    n = 0
    n += _conn(colour, "", mul, ["A"])
    n += _conn(bright, "", mul, ["B"])
    if n != 2:
        warn("colour", "emissive tint only %d/2 wired" % n)
    return mul, colour


# ==============================================================================
# STEP 1 -- M_GS_AimArc
#
#   Emissive = Colour * Brightness
#   Opacity  = (1 - Fresnel) * Opacity
#
# FRESNEL, NOT A UV GRADIENT, and this is the one real design decision in the
# file.  The ribbon is a chain of USplineMeshComponents built from whatever mesh
# is assigned to UGSAimComponent::ArcSegmentMesh, each one stretched and bent
# along one segment of the predicted path.  A UV-space gradient would land
# completely differently on a cylinder (U wraps the circumference), a cube (six
# flat faces) and a hand-authored strip, so the material would only ever look
# right for one of them.  Fresnel is measured against the CAMERA: 1 at the
# silhouette, 0 face-on.  One minus that is opaque down the middle of the tube
# and transparent at its edges, for every mesh, at every stretch and twist.
# ==============================================================================

def step1_arc_material():
    step = "1:M_GS_AimArc"
    try:
        if not ensure_dir(AIM_DIR):
            RESULTS[ARC_PATH] = "FAILED"
            return fail(step, "could not create %s" % AIM_DIR)

        if asset_exists(ARC_PATH) and not FORCE_RECREATE:
            RESULTS[ARC_PATH] = "SKIPPED"
            ok(step, "already exists -- left untouched (FORCE_RECREATE=False)")
            _verify_colour_param(load_asset(ARC_PATH), "1:verify-arc")
            return True
        if asset_exists(ARC_PATH):
            delete_asset(ARC_PATH)
            log("  deleted existing %s (FORCE_RECREATE)" % ARC_PATH)

        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = tools.create_asset("M_GS_AimArc", AIM_DIR,
                                 unreal.Material, unreal.MaterialFactoryNew())
        if mat is None:
            RESULTS[ARC_PATH] = "FAILED"
            return fail(step, "create_asset returned None")

        # Unlit so the arc is never shaded by the world -- it is a HUD element
        # that happens to live in 3D, and a trajectory preview that goes dim in
        # shadow is a preview you cannot read at night, which is when this game
        # is played. Two-sided because a thin tube is seen from inside at the
        # near end, where the arc passes the camera.
        _prop(mat, "material_domain", unreal.MaterialDomain.MD_SURFACE)
        _prop(mat, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        _prop(mat, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        _prop(mat, "two_sided", True)

        mul_em, _colour = _colour_and_brightness(mat, -900, -180)

        fres = _mx(mat, unreal.MaterialExpressionFresnel, -900, 160)
        _prop(fres, "exponent", 2.5, "Fresnel falloff sharpness")
        _prop(fres, "base_reflect_fraction", 0.04)

        one_m = _mx(mat, unreal.MaterialExpressionOneMinus, -640, 160)

        opac = _mx(mat, unreal.MaterialExpressionScalarParameter, -900, 300)
        _prop(opac, "parameter_name", "Opacity")
        _prop(opac, "default_value", 0.85)
        _prop(opac, "group", "GS Aim")

        mul_op = _mx(mat, unreal.MaterialExpressionMultiply, -380, 200)

        n = 0
        n += _conn(fres, "", one_m, ["", "Input"])
        n += _conn(one_m, "", mul_op, ["A"])
        n += _conn(opac, "", mul_op, ["B"])
        if n != 3:
            warn(step, "opacity chain only %d/3 wired -- open the graph and check" % n)
        else:
            ok(step, "opacity = (1 - Fresnel) * Opacity  (3/3)")

        mel = unreal.MaterialEditingLibrary
        p = 0
        try:
            if mel.connect_material_property(mul_em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
                p += 1
        except Exception:
            traceback.print_exc()
        try:
            if mel.connect_material_property(mul_op, "", unreal.MaterialProperty.MP_OPACITY):
                p += 1
        except Exception:
            traceback.print_exc()
        if p != 2:
            warn(step, "only %d/2 material properties connected" % p)
        else:
            ok(step, "Emissive + Opacity connected")

        mel.recompile_material(mat)
        RESULTS[ARC_PATH] = "CREATED"
        save_and_verify(ARC_PATH, "1:save-arc")
        _verify_colour_param(mat, "1:verify-arc")
        return ok(step, "built at %s" % ARC_PATH)

    except Exception:
        traceback.print_exc()
        RESULTS[ARC_PATH] = "FAILED"
        return fail(step, "exception -- see traceback above")


# ==============================================================================
# STEP 2 -- M_GS_AimLanding   (THE RETICLE: red centre, transparent edges)
#
#   d        = Distance(UV0, (0.5,0.5))      0 at centre, 0.5 at edge midpoint
#   gradient = Power( Clamp(1 - d*2), EdgeFalloff )
#   Emissive = Colour * Brightness * gradient
#   Opacity  = gradient * Opacity
#
# A soft round dot with nothing at the rim, which is exactly the shape asked
# for, and it generalises: anything that wants a reticle can use this material
# or copy these seven nodes.  Built from primitives rather than the engine's
# RadialGradientExponential material function on purpose -- a function call adds
# a hard asset reference and a set of pin names to get wrong, for maths that is
# four nodes long.
#
# DEFERRED DECAL domain with the EMISSIVE blend mode: emissive + opacity only,
# no base colour, no normal, no roughness.  A Translucent decal blend would
# write the reticle into the GBuffer as though it were paint on the ground, and
# it would then be lit -- which for a marker that must stay readable in shadow
# is the wrong answer.
# ==============================================================================

def step2_landing_material():
    step = "2:M_GS_AimLanding"
    try:
        if not ensure_dir(AIM_DIR):
            RESULTS[LANDING_PATH] = "FAILED"
            return fail(step, "could not create %s" % AIM_DIR)

        if asset_exists(LANDING_PATH) and not FORCE_RECREATE:
            RESULTS[LANDING_PATH] = "SKIPPED"
            ok(step, "already exists -- left untouched (FORCE_RECREATE=False)")
            _verify_colour_param(load_asset(LANDING_PATH), "2:verify-landing")
            return True
        if asset_exists(LANDING_PATH):
            delete_asset(LANDING_PATH)
            log("  deleted existing %s (FORCE_RECREATE)" % LANDING_PATH)

        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = tools.create_asset("M_GS_AimLanding", AIM_DIR,
                                 unreal.Material, unreal.MaterialFactoryNew())
        if mat is None:
            RESULTS[LANDING_PATH] = "FAILED"
            return fail(step, "create_asset returned None")

        _prop(mat, "material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
        _prop(mat, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        _prop(mat, "decal_blend_mode", unreal.DecalBlendMode.DBM_EMISSIVE)

        # ---- the radial gradient: 1 at the centre, 0 at the rim ------------
        uv = _mx(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 260)

        centre = _mx(mat, unreal.MaterialExpressionConstant2Vector, -1200, 400)
        _prop(centre, "r", 0.5)
        _prop(centre, "g", 0.5)

        dist = _mx(mat, unreal.MaterialExpressionDistance, -980, 300)

        # const_b = 2 turns "0 .. 0.5 from centre to edge" into a clean 0..1.
        span = _mx(mat, unreal.MaterialExpressionMultiply, -820, 300)
        _prop(span, "const_b", 2.0)

        inv = _mx(mat, unreal.MaterialExpressionOneMinus, -660, 300)

        # Clamp before Power: outside the unit disc 1-d*2 goes negative, and a
        # negative base under a fractional exponent is a NaN, which shows up as
        # a black square rather than an empty one.
        clamp = _mx(mat, unreal.MaterialExpressionClamp, -520, 300)

        falloff = _mx(mat, unreal.MaterialExpressionScalarParameter, -700, 460)
        _prop(falloff, "parameter_name", "EdgeFalloff")
        _prop(falloff, "default_value", 2.0)
        _prop(falloff, "group", "GS Aim")

        grad = _mx(mat, unreal.MaterialExpressionPower, -360, 320)

        g = 0
        g += _conn(uv, "", dist, ["A"])
        g += _conn(centre, "", dist, ["B"])
        g += _conn(dist, "", span, ["A"])
        g += _conn(span, "", inv, ["", "Input"])
        g += _conn(inv, "", clamp, ["", "Input"])
        g += _conn(clamp, "", grad, ["Base", ""])
        g += _conn(falloff, "", grad, ["Exp", "Exponent"])
        if g != 7:
            warn(step, "radial gradient only %d/7 wired -- open the graph and check" % g)
        else:
            ok(step, "radial gradient wired: Power(Clamp(1 - dist*2), EdgeFalloff)  (7/7)")

        # ---- emissive = Colour * Brightness * gradient ---------------------
        mul_cb, _colour = _colour_and_brightness(mat, -1200, -260)

        mul_em = _mx(mat, unreal.MaterialExpressionMultiply, -160, -120)
        e = 0
        e += _conn(mul_cb, "", mul_em, ["A"])
        e += _conn(grad, "", mul_em, ["B"])

        # ---- opacity = gradient * Opacity ----------------------------------
        opac = _mx(mat, unreal.MaterialExpressionScalarParameter, -360, 560)
        _prop(opac, "parameter_name", "Opacity")
        _prop(opac, "default_value", 1.0)
        _prop(opac, "group", "GS Aim")

        mul_op = _mx(mat, unreal.MaterialExpressionMultiply, -160, 420)
        e += _conn(grad, "", mul_op, ["A"])
        e += _conn(opac, "", mul_op, ["B"])
        if e != 4:
            warn(step, "emissive/opacity only %d/4 wired" % e)

        mel = unreal.MaterialEditingLibrary
        p = 0
        try:
            if mel.connect_material_property(mul_em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
                p += 1
        except Exception:
            traceback.print_exc()
        try:
            if mel.connect_material_property(mul_op, "", unreal.MaterialProperty.MP_OPACITY):
                p += 1
        except Exception:
            traceback.print_exc()
        if p != 2:
            warn(step, "only %d/2 material properties connected" % p)
        else:
            ok(step, "Emissive + Opacity connected")

        mel.recompile_material(mat)
        RESULTS[LANDING_PATH] = "CREATED"
        save_and_verify(LANDING_PATH, "2:save-landing")
        _verify_colour_param(mat, "2:verify-landing")
        return ok(step, "built at %s" % LANDING_PATH)

    except Exception:
        traceback.print_exc()
        RESULTS[LANDING_PATH] = "FAILED"
        return fail(step, "exception -- see traceback above")


# ------------------------------------------------------------------------------
# VERIFY THE ONE THING C++ DEPENDS ON
# ------------------------------------------------------------------------------

def _verify_colour_param(mat, step):
    """Does a vector parameter literally named 'Colour' exist?  That is exactly
    what UGSAimComponent's SetVectorParameterValue looks for, and a miss there is
    silent -- the arc would render in the material's default red forever and
    nothing would say why."""
    if mat is None:
        return fail(step, "no material to verify")
    try:
        vectors = [str(x) for x in unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)]
        scalars = [str(x) for x in unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)]
    except Exception:
        traceback.print_exc()
        return fail(step, "get_*_parameter_names threw -- verify by hand")

    log("  vectors: %s" % vectors)
    log("  scalars: %s" % scalars)
    if COLOUR_PARAM not in vectors:
        return fail(step, "'%s' MISSING from vector parameters -- the C++ contract is NOT "
                          "satisfied and the arc will never be tinted" % COLOUR_PARAM)
    return ok(step, "'%s' present -- UGSAimComponent can tint this material" % COLOUR_PARAM)


# ------------------------------------------------------------------------------
# STEP 3 -- what a human still has to do
# Assignment is deliberately NOT automated. The targets are TSoftObjectPtr
# properties on a default subobject inside BP_GSPlayerCharacter's CDO, and
# writing those from Python is exactly the kind of "reported success, changed
# nothing" operation this project has been bitten by before. Printing precise
# instructions is honest; a silent half-write is not.
# ------------------------------------------------------------------------------

def step3_report_assignment():
    manual("Open BP_GSPlayerCharacter -> select the AimComponent -> Details, and set:")
    manual("    GoblinSiege|Aim|Visual : Arc Material          = %s" % ARC_PATH)
    manual("    GoblinSiege|Aim|Visual : Landing Decal Material= %s" % LANDING_PATH)
    manual("    GoblinSiege|Aim|Visual : Arc Segment Mesh      = %s" % SUGGESTED_ARC_MESH)
    manual("Then set GoblinSiege|Abilities : Bow Shot Ability Class = GSGA_BowShot")
    manual("         (or a Blueprint child of it) -- without it, ranged mode falls")
    manual("         through to the melee path rather than firing.")
    note("Arc Segment Mesh: use a mesh that is SYMMETRIC or whose long axis is X.")
    note("  UGSAimComponent calls SetForwardAxis(ESplineMeshAxis::X). "
         "/Engine/BasicShapes/Cylinder runs along Z and will look wrong; Cube is safe.")
    note("The red set here is only the material DEFAULT. In play the component "
         "overrides it per mode (torch orange, bow pale blue) via the MID. To get "
         "red in play, change TorchArcColour / BowArcColour on the AimComponent.")
    return True


# ------------------------------------------------------------------------------
# SUMMARY
# ------------------------------------------------------------------------------

def print_summary():
    print("")
    print("=" * 78)
    print("[GS-AIM] SUMMARY")
    print("=" * 78)
    for path, state in sorted(RESULTS.items()):
        print("  %-10s %s" % (state, path))
    if NOTES:
        print("")
        print("  NOTES")
        for n in NOTES:
            print("    - %s" % n)
    if MANUAL_ACTIONS:
        print("")
        print("  STILL TO DO BY HAND")
        for m in MANUAL_ACTIONS:
            print("    - %s" % m)
    print("=" * 78)
    print("")


def main():
    del NOTES[:]
    del MANUAL_ACTIONS[:]
    RESULTS.clear()

    print("")
    print("[GS-AIM] gs_build_aim_materials.py -- aim arc + landing reticle")
    if not pie_guard():
        return False

    step1_arc_material()
    step2_landing_material()
    step3_report_assignment()
    print_summary()
    return True


# exec(open(...).read()) from the Output Log runs in the __main__ namespace, so
# this fires and the script does its work in one paste. An `import` sets __name__
# to the module name instead, which does NOT auto-run - that route is for when you
# want to call the individual steps by hand.
if __name__ == "__main__":
    main()
