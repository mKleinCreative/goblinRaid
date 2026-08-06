# gs_add_burn_specular.py
#
# Adds a Specular pin + BurntSpecular output to /Game/VFX/Burn/MF_GS_BurnChar,
# so specular falls to 0 as a surface chars (Michael, 2026-07-31: "specularity
# needs to change to 0 as well when it's burnt").
#
# Char is soot. Soot is not shiny - leaving specular at the material's authored
# value is what makes a burnt surface still read as "wet wood" rather than ash.
#
# Idempotent: re-running detects the pin and does nothing.
#
# Run from the editor: Output Log -> Cmd dropdown -> Python, then
#   exec(open(r"D:\goblinRaid\GoblinSiege 5.8\Content\Python\gs_add_burn_specular.py").read())

import unreal, os, time

MF_PATH   = "/Game/VFX/Burn/MF_GS_BurnChar"
DEMO_PATH = "/Game/VFX/Burn/M_GS_BurnDemo"
MF_DISK   = r"D:\goblinRaid\GoblinSiege 5.8\Content\VFX\Burn\MF_GS_BurnChar.uasset"

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(tag, msg):
    print("[GS-SPEC] %-7s | %s" % (tag, msg))


def find_char_alpha(mf):
    """The Saturate feeding the char Lerp's Alpha.

    Identified by authored graph position (-280, -120) rather than by walking
    connections: UE 5.8's Python layer does not expose expression input pins,
    so position is the only reliable handle on a node we authored ourselves.
    Falls back to the Saturate nearest that point if the graph was nudged.
    """
    best, best_d = None, None
    for e in MEL.get_material_function_expressions(mf):
        if e.get_class().get_name() != "MaterialExpressionSaturate":
            continue
        x = e.get_editor_property("material_expression_editor_x")
        y = e.get_editor_property("material_expression_editor_y")
        d = (x + 280) ** 2 + (y + 120) ** 2
        if best_d is None or d < best_d:
            best, best_d = e, d
    return best


def main():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        log("ABORT", "PIE is running - stop it first (PIE-active asset writes silently no-op).")
        return

    mf = EAL.load_asset(MF_PATH)
    if not mf:
        log("FAIL", "cannot load " + MF_PATH)
        return

    existing = [str(e.get_editor_property("input_name"))
                for e in MEL.get_material_function_expressions(mf)
                if e.get_class().get_name() == "MaterialExpressionFunctionInput"]
    if "Specular" in existing:
        log("SKIP", "Specular pin already present - nothing to do.")
        return

    char = find_char_alpha(mf)
    if not char:
        log("FAIL", "could not locate the char-alpha Saturate node.")
        return

    def N(cls, x, y):
        return MEL.create_material_expression_in_function(mf, cls, x, y)

    # Specular pin. Default 0.5 = UE's own default, so a caller that leaves it
    # unwired gets standard specular that still fades to 0 as it burns.
    spec = N(unreal.MaterialExpressionFunctionInput, -1000, -40)
    spec.set_editor_property("input_name", "Specular")
    spec.set_editor_property("input_type", unreal.FunctionInputType.FUNCTION_INPUT_SCALAR)
    spec.set_editor_property("sort_priority", 3)
    spec.set_editor_property("use_preview_value_as_default", True)
    pv = unreal.Vector4f()
    for axis, val in (("x", 0.5), ("y", 0.0), ("z", 0.0), ("w", 1.0)):
        pv.set_editor_property(axis, val)
    spec.set_editor_property("preview_value", pv)

    # BurntSpecular = Specular * (1 - charAlpha).  Multiplicative, not a lerp to
    # a constant, so it composes with whatever the calling material authored.
    inv = N(unreal.MaterialExpressionOneMinus, -160, 30)
    MEL.connect_material_expressions(char, "", inv, "")

    mul = N(unreal.MaterialExpressionMultiply, -20, 10)
    MEL.connect_material_expressions(spec, "", mul, "A")
    MEL.connect_material_expressions(inv, "", mul, "B")

    out = N(unreal.MaterialExpressionFunctionOutput, 160, 10)
    out.set_editor_property("output_name", "BurntSpecular")
    out.set_editor_property("sort_priority", 2)
    MEL.connect_material_expressions(mul, "", out, "")

    MEL.update_material_function(mf)

    before = os.path.getmtime(MF_DISK) if os.path.exists(MF_DISK) else 0
    unreal.EditorLoadingAndSavingUtils.save_packages([mf.get_outermost()], False)
    time.sleep(0.6)
    after = os.path.getmtime(MF_DISK) if os.path.exists(MF_DISK) else 0
    log("PASS" if after != before else "WARN",
        "MF_GS_BurnChar saved (mtime advanced: %s)" % (after != before))

    # Wire the demo so the change is visible without hand-editing.
    demo = EAL.load_asset(DEMO_PATH)
    if demo:
        try:
            call = next(e for e in MEL.get_material_expressions(demo)
                        if e.get_class().get_name() == "MaterialExpressionMaterialFunctionCall")
            okc = MEL.connect_material_property(call, "BurntSpecular",
                                                unreal.MaterialProperty.MP_SPECULAR)
            MEL.recompile_material(demo)
            unreal.EditorLoadingAndSavingUtils.save_packages([demo.get_outermost()], False)
            log("PASS" if okc else "WARN", "M_GS_BurnDemo specular wired: %s" % okc)
        except StopIteration:
            log("WARN", "no function-call node found in the demo material.")

    pins = [str(e.get_editor_property("input_name"))
            for e in MEL.get_material_function_expressions(mf)
            if e.get_class().get_name() == "MaterialExpressionFunctionInput"]
    outs = [str(e.get_editor_property("output_name"))
            for e in MEL.get_material_function_expressions(mf)
            if e.get_class().get_name() == "MaterialExpressionFunctionOutput"]
    log("VERIFY", "pins: %s" % pins)
    log("VERIFY", "outputs: %s" % outs)
    log("NOTE", "Wire BurntSpecular -> Specular in the crop master alongside "
                "BurntBaseColor -> Base Color.")


main()
