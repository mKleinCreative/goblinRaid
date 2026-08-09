"""
Procedural war-lance blockout for the GoblinSiege Knight defender.
Grounded medieval-realism style, matching SM_Sword_Arming01 and
SM_Crossbow_Militia01: plain turned-wood shaft, polished steel vamplate
and head, dark leather grip wrap.

Run headless:
    blender --background --python build_lance.py

Builds SM_Lance_War01, saves a .blend, exports FBX (UE-ready axes),
renders a preview PNG, and writes a UV-layout reference PNG. Pure bmesh
construction (no bpy.ops mesh edits) so it runs reliably with no
window/context. The UV layout is rasterized straight from the mesh's UV
data (bpy.ops.uv.export_layout needs a live Image Editor area, which
doesn't exist in headless --background mode).

Axis convention (matches the sword/crossbow): local Z runs from the butt
(-Z) through the grip (Z=0, the hand-socket attach point, just behind the
vamplate -- where the couching hand sits) to the head tip (+Z). Local X/Y
are the shaft's round cross-section.
"""
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
TEX_DIR = os.path.join(OUT_DIR, "Textures")
os.makedirs(TEX_DIR, exist_ok=True)

# ---------------------------------------------------------------------------
# Dimensions (meters)
# ---------------------------------------------------------------------------
SHAFT_SIDES = 12
# Shaft cross-section stations: (z, radius)
SHAFT_STATIONS = [
    (-0.58, 0.018),  # near the butt, before the end cap
    (-0.35, 0.022),  # thickest point, rear third (counterbalances the head)
    (-0.05, 0.021),  # rear of grip
    (0.15, 0.021),   # front of grip, base of the vamplate mount
    (0.55, 0.019),
    (1.10, 0.016),
    (1.60, 0.012),
    (1.92, 0.008),   # base of the head
]

BUTT_CAP_Z = -0.60
BUTT_CAP_RADIUS = 0.022
BUTT_CAP_SEGMENTS = 12

GRIP_Z0, GRIP_Z1 = -0.13, 0.13
GRIP_RADIUS = 0.023
GRIP_SEGMENTS = 14

VAMPLATE_Z0, VAMPLATE_Z1 = 0.13, 0.23
VAMPLATE_RADIUS0, VAMPLATE_RADIUS1 = 0.024, 0.10
VAMPLATE_SEGMENTS = 20

HEAD_Z0, HEAD_Z1 = 1.92, 2.10
HEAD_BASE_RADIUS = 0.009
HEAD_SEGMENTS = 12

# ---------------------------------------------------------------------------
# Scene reset
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'

# ---------------------------------------------------------------------------
# Materials -- plain wood shaft, polished steel fittings, dark leather grip
# ---------------------------------------------------------------------------
def make_material(name, base_color, metallic, roughness):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = base_color
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return mat

mat_wood = make_material("M_Lance_Wood", (0.30, 0.19, 0.10, 1.0), 0.0, 0.45)
mat_steel = make_material("M_Lance_Steel", (0.68, 0.69, 0.71, 1.0), 1.0, 0.22)
mat_grip = make_material("M_Lance_Grip", (0.02, 0.02, 0.02, 1.0), 0.0, 0.55)

MAT_WOOD, MAT_STEEL, MAT_GRIP = 0, 1, 2

bm = bmesh.new()

def add_faces_with_material(build_fn, material_index):
    before = set(bm.faces)
    build_fn()
    for f in bm.faces:
        if f not in before:
            f.material_index = material_index

def ring(radius, z, sides):
    pts = []
    for i in range(sides):
        theta = 2 * math.pi * i / sides
        pts.append(bm.verts.new((radius * math.cos(theta), radius * math.sin(theta), z)))
    return pts

# --- Shaft: long tapered round pole, butt to base of the head --------------
def build_shaft():
    n = SHAFT_SIDES
    rings = [ring(r, z, n) for z, r in SHAFT_STATIONS]
    bm.faces.new(tuple(reversed(rings[0])))   # butt-side cap
    for i in range(len(rings) - 1):
        a, b = rings[i], rings[i + 1]
        for j in range(n):
            bm.faces.new((a[j], a[(j + 1) % n], b[(j + 1) % n], b[j]))
    bm.faces.new(tuple(rings[-1]))            # cap into the head base

# --- Butt cap: small steel counterweight knob -------------------------------
def build_butt_cap():
    z, r = SHAFT_STATIONS[0]
    mat = Matrix.Translation((0, 0, (BUTT_CAP_Z + z) / 2))
    bmesh.ops.create_cone(
        bm, cap_ends=True, cap_tris=True, segments=BUTT_CAP_SEGMENTS,
        radius1=0.002, radius2=r, depth=z - BUTT_CAP_Z,
        matrix=mat, calc_uvs=True,
    )

# --- Grip: leather-wrapped sleeve over the hand-hold section ----------------
def build_grip():
    z = (GRIP_Z0 + GRIP_Z1) / 2
    mat = Matrix.Translation((0, 0, z))
    bmesh.ops.create_cone(
        bm, cap_ends=True, cap_tris=True, segments=GRIP_SEGMENTS,
        radius1=GRIP_RADIUS, radius2=GRIP_RADIUS, depth=GRIP_Z1 - GRIP_Z0,
        matrix=mat, calc_uvs=True,
    )

# --- Vamplate: flared steel hand-guard cone ---------------------------------
def build_vamplate():
    z = (VAMPLATE_Z0 + VAMPLATE_Z1) / 2
    mat = Matrix.Translation((0, 0, z))
    bmesh.ops.create_cone(
        bm, cap_ends=True, cap_tris=True, segments=VAMPLATE_SEGMENTS,
        radius1=VAMPLATE_RADIUS0, radius2=VAMPLATE_RADIUS1,
        depth=VAMPLATE_Z1 - VAMPLATE_Z0,
        matrix=mat, calc_uvs=True,
    )

# --- Head: steel spike point -------------------------------------------------
def build_head():
    z = (HEAD_Z0 + HEAD_Z1) / 2
    mat = Matrix.Translation((0, 0, z))
    bmesh.ops.create_cone(
        bm, cap_ends=True, cap_tris=True, segments=HEAD_SEGMENTS,
        radius1=HEAD_BASE_RADIUS, radius2=0.0004, depth=HEAD_Z1 - HEAD_Z0,
        matrix=mat, calc_uvs=True,
    )

add_faces_with_material(build_shaft, MAT_WOOD)
add_faces_with_material(build_butt_cap, MAT_STEEL)
add_faces_with_material(build_grip, MAT_GRIP)
add_faces_with_material(build_vamplate, MAT_STEEL)
add_faces_with_material(build_head, MAT_STEEL)

bmesh.ops.recalc_face_normals(bm, faces=bm.faces)

mesh = bpy.data.meshes.new("SM_Lance_War01")
bm.to_mesh(mesh)
bm.free()

for p in mesh.polygons:
    p.use_smooth = False  # flat-shaded hard-surface look, matches the sword

mesh.materials.append(mat_wood)
mesh.materials.append(mat_steel)
mesh.materials.append(mat_grip)

obj = bpy.data.objects.new("SM_Lance_War01", mesh)
scene.collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)

# Non-overlapping unwrap (per-primitive default UVs all land in the same
# 0-1 square otherwise) -- same fix as the sword/crossbow.
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(
    angle_limit=math.radians(66), island_margin=0.02, scale_to_bounds=True
)
bpy.ops.object.mode_set(mode='OBJECT')

# ---------------------------------------------------------------------------
# UV layout reference PNG -- rasterized straight from the mesh's UV data.
# Same approach as the crossbow/bolt (bpy.ops.uv.export_layout needs a live
# Image Editor area that doesn't exist in headless --background mode).
# ---------------------------------------------------------------------------
def export_uv_layout(mesh_, filepath, size=2048, line_rgba=(0.0, 0.85, 1.0, 1.0)):
    arr = np.zeros((size, size, 4), dtype=np.float32)
    uv_layer = mesh_.uv_layers.active.data

    def set_px(x, y):
        for ox in (0, 1):
            for oy in (0, 1):
                px, py = x + ox, y + oy
                if 0 <= px < size and 0 <= py < size:
                    arr[py, px] = line_rgba

    def draw_line(x0, y0, x1, y1):
        x0, y0, x1, y1 = int(round(x0)), int(round(y0)), int(round(x1)), int(round(y1))
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx + dy
        while True:
            set_px(x0, y0)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    for poly in mesh_.polygons:
        loop_idx = list(poly.loop_indices)
        n = len(loop_idx)
        for i in range(n):
            uv0 = uv_layer[loop_idx[i]].uv
            uv1 = uv_layer[loop_idx[(i + 1) % n]].uv
            x0, y0 = uv0.x * (size - 1), (1.0 - uv0.y) * (size - 1)
            x1, y1 = uv1.x * (size - 1), (1.0 - uv1.y) * (size - 1)
            draw_line(x0, y0, x1, y1)

    img = bpy.data.images.new("UVLayout", width=size, height=size, alpha=True)
    img.pixels.foreach_set(arr.ravel())
    img.filepath_raw = filepath
    img.file_format = 'PNG'
    img.save()

export_uv_layout(mesh, os.path.join(TEX_DIR, "UV_Layout_Lance.png"))

# ---------------------------------------------------------------------------
# Preview render -- same camera/light ratios that worked for the sword,
# scaled up for the lance's much greater length.
# ---------------------------------------------------------------------------
world = bpy.data.worlds.new("World")
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes.get("Background")
if bg:
    bg.inputs[0].default_value = (0.12, 0.125, 0.14, 1.0)
    bg.inputs[1].default_value = 1.0

def look_at(obj_, target, up=Vector((0, 0, 1))):
    direction = (target - obj_.location).normalized()
    rot_quat = direction.to_track_quat('-Z', 'Y')
    obj_.rotation_euler = rot_quat.to_euler()

z_min, z_max = BUTT_CAP_Z, HEAD_Z1
target = Vector((0, 0, (z_min + z_max) / 2))
scale = (z_max - z_min) / 0.85  # sword's overall length

cam_data = bpy.data.cameras.new("Camera")
cam_data.lens = 35
cam = bpy.data.objects.new("Camera", cam_data)
scene.collection.objects.link(cam)
cam.location = target + Vector((0.55, -0.8, 0.0)) * scale
look_at(cam, target)
scene.camera = cam

key_data = bpy.data.lights.new("Key", type='AREA')
key_data.energy = 45
key_data.size = 0.9 * scale
key = bpy.data.objects.new("Key", key_data)
key.location = target + Vector((1.0, -1.0, 1.0)) * scale
look_at(key, target)
scene.collection.objects.link(key)

fill_data = bpy.data.lights.new("Fill", type='AREA')
fill_data.energy = 18
fill_data.size = 1.2 * scale
fill = bpy.data.objects.new("Fill", fill_data)
fill.location = target + Vector((-1.2, -0.5, -0.2)) * scale
look_at(fill, target)
scene.collection.objects.link(fill)

rim_data = bpy.data.lights.new("Rim", type='AREA')
rim_data.energy = 25
rim_data.size = 0.6 * scale
rim = bpy.data.objects.new("Rim", rim_data)
rim.location = target + Vector((-0.3, 1.4, 0.6)) * scale
look_at(rim, target)
scene.collection.objects.link(rim)

scene.view_settings.view_transform = 'Standard'
scene.render.resolution_x = 500
scene.render.resolution_y = 1400
scene.render.filepath = os.path.join(OUT_DIR, "preview.png")
bpy.ops.render.render(write_still=True)

# ---------------------------------------------------------------------------
# Save .blend source + FBX export (UE convention: -Y forward, Z up)
# ---------------------------------------------------------------------------
blend_path = os.path.join(OUT_DIR, "SM_Lance_War01.blend")
bpy.ops.wm.save_as_mainfile(filepath=blend_path)

fbx_path = os.path.join(OUT_DIR, "SM_Lance_War01.fbx")
bpy.ops.export_scene.fbx(
    filepath=fbx_path,
    use_selection=False,
    object_types={'MESH'},
    axis_forward='-Y',
    axis_up='Z',
    apply_scale_options='FBX_SCALE_ALL',
    global_scale=1.0,
    bake_space_transform=True,
)

print("DONE:", blend_path, fbx_path)
