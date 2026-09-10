# Conform a Goblin Siege rig to ACF's Manny bone standard.
#
# WHY THIS EXISTS. ACF is authored against `ACF_UE5Manny`: `root`, `pelvis`, `spine_01..05`,
# `thigh_l/r`, `calf_l/r`, `foot_l/r`, `upperarm_l/r`, `lowerarm_l/r`, `hand_l/r`, plus the IK bones
# `ik_foot_root`, `ik_foot_l/r`, `ik_hand_root`, `ik_hand_gun`, `ik_hand_l/r`. Neither GS rig matched:
# the human is Mixamo (`Hips`, `LeftUpLeg`, `Spine1`), the goblin is its own convention (`Root`,
# `L_Thigh`, `Spine01`), and **neither had a single `ik_*` bone**.
#
# That mismatch is not cosmetic. `ACF_BaseMoveset` - the full-body base for `ACF_Template_ABP` - is
# built from 5 StrideWarping + 5 OrientationWarping nodes, and
# `FAnimNode_StrideWarping::IsValidToEvaluate` returns **false with no log at all** when `pelvis` or
# `ik_foot_root` are absent. The character animates, the editor compiles clean, and what is silently
# lost is stride scaled to ground speed and lower-body strafe orientation - uniform foot-sliding that
# reads as a blendspace-tuning problem. Conforming the rig is what makes ACF's own template usable
# instead of the `ACF_SimpleTemplate_ABP` fallback.
#
# USAGE
#   blender.exe --background --python Tools/Rig/conform_to_manny.py -- <in.fbx> <out.fbx> <human|goblin>
#
# The input FBX comes from Unreal (`unreal.Exporter.run_asset_export_task` on the SkeletalMesh).
#
# TWO BLENDER IMPORTER DEFECTS THIS SCRIPT WORKS AROUND - do not remove either guard:
#   1. Blender's FBX importer absorbs a PARENTLESS ROOT BONE into the armature OBJECT. The human
#      therefore arrives as three separate roots (`spine_01`, `thigh_l`, `thigh_r`) with no pelvis at
#      all, and the goblin loses the `Root` it genuinely authors. Both would import to Unreal as
#      multi-root skeletons.
#   2. Consequently `root` must be created UNCONDITIONALLY and `pelvis` reparented under it, even for
#      a rig whose source already has a root bone.
#
# ONE MAPPING SUBTLETY, measured from the goblin hierarchy: it is
# `Hip -> {Pelvis -> thighs, Waist -> Spine01}`. `Pelvis` parents the LEGS, not the spine. So
# `Hip` becomes `pelvis` and `Pelvis` is left as an extra bone between `pelvis` and the thighs -
# renaming it `spine_01` would hang both legs off the spine chain. ACF ignores bones it does not
# reference.
#
# Verify after every run by re-importing the output and checking: a single root, and
# pelvis / ik_foot_root / ik_foot_l/r / foot_l / thigh_l all present.

import bpy, sys, mathutils
argv = sys.argv[sys.argv.index("--")+1:]
src, dst, rig = argv[0], argv[1], argv[2]

HUMAN = {
 "Hips":"pelvis","Spine":"spine_01","Spine1":"spine_02","Spine2":"spine_03",
 "Neck":"neck_01","Head":"head",
 "LeftShoulder":"clavicle_l","LeftArm":"upperarm_l","LeftForeArm":"lowerarm_l","LeftHand":"hand_l",
 "RightShoulder":"clavicle_r","RightArm":"upperarm_r","RightForeArm":"lowerarm_r","RightHand":"hand_r",
 "LeftUpLeg":"thigh_l","LeftLeg":"calf_l","LeftFoot":"foot_l","LeftToeBase":"ball_l",
 "RightUpLeg":"thigh_r","RightLeg":"calf_r","RightFoot":"foot_r","RightToeBase":"ball_r",
}
for side,S in (("Left","l"),("Right","r")):
    for f,mf in (("Thumb","thumb"),("Index","index"),("Middle","middle"),("Ring","ring"),("Pinky","pinky")):
        for i in (1,2,3):
            HUMAN["%sHand%s%d"%(side,f,i)] = "%s_0%d_%s"%(mf,i,S)

GOBLIN = {
 "Root":"root","Hip":"pelvis","Waist":"spine_01","Spine01":"spine_02","Spine02":"spine_03",
 "NeckTwist01":"neck_01","NeckTwist02":"neck_02","Head":"head",
}
for S,s in (("L","l"),("R","r")):
    GOBLIN.update({
      "%s_Clavicle"%S:"clavicle_%s"%s, "%s_Upperarm"%S:"upperarm_%s"%s,
      "%s_Forearm"%S:"lowerarm_%s"%s, "%s_Hand"%S:"hand_%s"%s,
      "%s_Thigh"%S:"thigh_%s"%s, "%s_Calf"%S:"calf_%s"%s,
      "%s_Foot"%S:"foot_%s"%s, "%s_ToeBase"%S:"ball_%s"%s,
      "%s_UpperarmTwist01"%S:"upperarm_twist_01_%s"%s, "%s_UpperarmTwist02"%S:"upperarm_twist_02_%s"%s,
      "%s_ForearmTwist01"%S:"lowerarm_twist_01_%s"%s, "%s_ForearmTwist02"%S:"lowerarm_twist_02_%s"%s,
      "%s_ThighTwist01"%S:"thigh_twist_01_%s"%s, "%s_ThighTwist02"%S:"thigh_twist_02_%s"%s,
      "%s_CalfTwist01"%S:"calf_twist_01_%s"%s, "%s_CalfTwist02"%S:"calf_twist_02_%s"%s,
    })

MAP = HUMAN if rig=="human" else GOBLIN
NEEDS_ROOT = (rig=="human")

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm=[o for o in bpy.data.objects if o.type=='ARMATURE'][0]
bpy.context.view_layer.objects.active=arm

renamed=0; missing=[]
for old,new in MAP.items():
    b=arm.data.bones.get(old)
    if b: b.name=new; renamed+=1
    else: missing.append(old)

bpy.ops.object.mode_set(mode='EDIT')
eb=arm.data.edit_bones

# Blender's FBX importer absorbs a parentless root bone into the armature OBJECT, so "Hips"
# vanished and spine_01 / thigh_l / thigh_r came in as three separate roots. Rebuild the pelvis
# they should all hang from, or the rig imports to Unreal with three root bones and no pelvis.
if "pelvis" not in eb:
    kids=[eb[n] for n in ("spine_01","thigh_l","thigh_r") if n in eb]
    if kids:
        sp=eb.get("spine_01") or kids[0]
        pv=eb.new("pelvis")
        pv.head=sp.head.copy()
        _d=(sp.tail-sp.head)
        _len=max(1.0,_d.length*0.5)
        pv.tail=sp.head.copy()+_d.normalized()*_len
        pv.use_connect=False
        for k in kids:
            k.parent=pv; k.use_connect=False
        print("SYNTHESIZED pelvis, reparented:",[k.name for k in kids])
def mk(name, parent, head, tail):
    if name in eb: return eb[name]
    n=eb.new(name); n.head=head; n.tail=tail
    n.parent = eb[parent] if parent and parent in eb else None
    n.use_connect=False
    return n

# Unconditional: the FBX importer absorbs a parentless root bone into the armature object on BOTH
# rigs, so neither arrives with one even though the goblin authored `Root`. Everything - pelvis and
# both IK roots - must descend from a single `root` or Unreal imports a multi-root skeleton.
# ORIENT root AND pelvis ALONG THE BODY, NOT ALONG WORLD Z.
#
# Measured 2026-09-10: after the FBX round trip the body axis in this space is +Y - `spine_01` runs
# (0, 1.00, -0.08) and `thigh_l` runs (0, -1.00, 0.02). An earlier version of this script created
# `root` and `pelvis` with their tails at (0,0,10), i.e. up world Z, leaving their local axes about
# 90 degrees off every other bone in the rig. The ref pose still looked correct, because head
# positions were right - but the Pelvis Motion op applies the retargeted pelvis rotation in LOCAL
# space, so it went about the wrong axes and the character animated permanently tilted sideways.
# Michael: "she's also tilted sideways."
#
# Take the direction from `spine_01` so these two bones share the rig's own convention.
_sp = eb.get("spine_01")
if _sp is not None:
    _up = (_sp.tail - _sp.head).normalized()
else:
    _up = mathutils.Vector((0, 1, 0))

if "root" not in eb:
    r=mk("root", None, mathutils.Vector((0,0,0)), _up * 10.0)
for orphan in ("pelvis",):
    b=eb.get(orphan)
    if b and b.parent is None: b.parent=eb["root"]; b.use_connect=False

fl=eb.get("foot_l"); fr=eb.get("foot_r"); hl=eb.get("hand_l"); hr=eb.get("hand_r")
Z=mathutils.Vector((0,0,0)); T=mathutils.Vector((0,0,10))
mk("ik_foot_root","root",Z,T)
if fl: mk("ik_foot_l","ik_foot_root",fl.head.copy(),fl.tail.copy())
if fr: mk("ik_foot_r","ik_foot_root",fr.head.copy(),fr.tail.copy())
mk("ik_hand_root","root",Z,T)
if hr: mk("ik_hand_gun","ik_hand_root",hr.head.copy(),hr.tail.copy())
if hr: mk("ik_hand_r","ik_hand_gun",hr.head.copy(),hr.tail.copy())
if hl: mk("ik_hand_l","ik_hand_gun",hl.head.copy(),hl.tail.copy())
bpy.ops.object.mode_set(mode='OBJECT')

# RESTORE THE WEIGHT THE IMPORTER DELETED, ONTO `pelvis`.
#
# Defect 1 above absorbs the parentless root bone into the armature object - and on the human it
# takes the bone's VERTEX GROUP with it. Measured on the source human rig: no `Hips` bone AND no
# `Hips` vertex group survive the import, so every weight that belonged to the hips is simply gone.
# What is left is 947 vertices holding only partial weight from `Spine` and the two `UpLeg` bones,
# summing to a median of 0.492, spanning Z 131..205 in a mesh that runs -1..299 - i.e. 44%-69% of
# body height, the hip and waist region exactly.
#
# DO NOT "FIX" THIS BY NORMALIZING. An earlier version of this script scaled the surviving weights up
# to sum 1, which makes hip vertices follow the spine and the thighs at full strength with nothing
# holding them at the hips. Michael, looking at the result: "some verts are going a little crazy
# during the animation and pulling out of the hips." Normalising redistributes the missing influence
# onto the wrong bones; it does not restore it.
#
# The deficit belongs to the bone that was lost, and `pelvis` is the bone standing in its place at
# the same position. So give each vertex its missing fraction (1 - sum) as `pelvis` weight, and only
# then normalise whatever is still off.
for ob in [o for o in bpy.data.objects if o.type == 'MESH']:
    if not ob.vertex_groups:
        continue
    bone_names = {b.name for b in arm.data.bones}
    pelvis_group = ob.vertex_groups.get("pelvis") or ob.vertex_groups.new(name="pelvis")
    restored = 0
    renorm = 0
    for v in ob.data.vertices:
        total = sum(g.weight for g in v.groups
                    if ob.vertex_groups[g.group].name in bone_names)
        if total <= 0.0:
            continue
        if total < 0.999:
            pelvis_group.add([v.index], 1.0 - total, 'REPLACE')
            restored += 1
        elif total > 1.001:
            for g in v.groups:
                ob.vertex_groups[g.group].add([v.index], g.weight / total, 'REPLACE')
            renorm += 1
    print("PELVIS WEIGHT RESTORED %s: %d vertex(es); renormalized %d" % (ob.name, restored, renorm))
bpy.context.view_layer.objects.active = arm

print("RENAMED:",renamed," MISSING:",missing)
print("FINAL BONES:",len(arm.data.bones))
print("IK PRESENT:",[b.name for b in arm.data.bones if b.name.startswith("ik_")])
print("HAS ROOT:", "root" in arm.data.bones)
bpy.ops.export_scene.fbx(filepath=dst, add_leaf_bones=False, use_armature_deform_only=False,
                         bake_anim=False, object_types={'ARMATURE','MESH'}, path_mode='COPY')
print("WROTE:",dst)
