"""Headless Blender bake for the human meshes.

Implements the recipe validated on the goblin 2026-07-24 (canonical-scale.md):
bake the target scale into armature+mesh DATA, keep every object transform at
identity, then export with FBX_SCALE_NONE so nothing lands on a node transform.

Usage:  blender --background --python gs_bake_human.py -- <src.fbx> <dst.fbx> <target_uu>
"""
import bpy, sys, os
from mathutils import Matrix

argv = sys.argv[sys.argv.index("--") + 1:]
SRC, DST, FACTOR = argv[0], argv[1], float(argv[2])

# Optional 4th arg: "Old=New,Old2=New2" bone renames, applied before export.
# Blender renames the matching vertex groups with the bone, but we do it explicitly
# too because a silent mismatch here unbinds the mesh from that bone.
RENAMES = {}
if len(argv) > 3 and argv[3].strip():
    for pair in argv[3].split(','):
        if '=' in pair:
            a, b = pair.split('=', 1)
            RENAMES[a.strip()] = b.strip()

bpy.ops.wm.read_homefile(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)

arm = next((o for o in bpy.data.objects if o.type == 'ARMATURE'), None)
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
assert arm is not None, "no armature imported"
print("IMPORTED armature=%s bones=%d meshes=%d" % (arm.name, len(arm.data.bones), len(meshes)))
for o in bpy.data.objects:
    print("   pre  %-22s scale=%s rot=%s" % (
        o.name, tuple(round(v, 6) for v in o.scale),
        tuple(round(v, 4) for v in o.rotation_euler)))

# 1. bake every object transform into data, so nothing rides on a node transform
bpy.ops.object.select_all(action='SELECT')
bpy.context.view_layer.objects.active = arm
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in bpy.data.objects:
    print("   post %-22s scale=%s rot=%s" % (
        o.name, tuple(round(v, 6) for v in o.scale),
        tuple(round(v, 4) for v in o.rotation_euler)))

# 2. measure current height in blender units (Z after the Y-up -> Z-up apply)
zs = []
for m in meshes:
    for v in m.data.vertices:
        zs.append(v.co.z)
h = max(zs) - min(zs)
factor = FACTOR
print("HEIGHT_BLENDER %.6f   FACTOR %.4f   PREDICTED %.4f" % (h, factor, h * factor))

# 3. bake the factor into DATA (armature first, then each mesh), never onto objects
S = Matrix.Scale(factor, 4)
arm.data.transform(S)
for m in meshes:
    m.data.transform(S)

zs = [v.co.z for m in meshes for v in m.data.vertices]
print("HEIGHT_AFTER_BAKE %.4f" % (max(zs) - min(zs)))

# 3b. bone renames, so every human can share one skeleton
for old, new in RENAMES.items():
    b = arm.data.bones.get(old)
    if b is None:
        print("RENAME_SKIP %s (not present)" % old)
        continue
    b.name = new
    print("RENAMED bone %s -> %s" % (old, new))
    for m in meshes:
        vg = m.vertex_groups.get(old)
        if vg is not None:
            vg.name = new
            print("   vertex group %s -> %s on %s" % (old, new, m.name))
print("FINAL_BONES %s" % sorted([b.name for b in arm.data.bones]))

# 4. the exported armature object must be named 'Armature' (goblin recipe)
for o in bpy.data.objects:
    if o.type == 'ARMATURE':
        o.name = 'Armature'

# 5. unit trick: scale_length 0.01 + apply_unit_scale makes 1 blender unit == 1 uu
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = 0.01

os.makedirs(os.path.dirname(DST), exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.export_scene.fbx(
    filepath=DST,
    use_selection=True,
    object_types={'ARMATURE', 'MESH'},
    global_scale=1.0,
    apply_unit_scale=True,
    apply_scale_options='FBX_SCALE_NONE',
    add_leaf_bones=False,
    bake_anim=False,
    mesh_smooth_type='FACE',
    path_mode='COPY',
    embed_textures=False,
)
print("EXPORTED %s  (%d bytes)" % (DST, os.path.getsize(DST) if os.path.exists(DST) else -1))
print("DONE")
