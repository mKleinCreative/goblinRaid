"""
Procedural crossbow-bolt blockout for SM_Crossbow_Militia01 -- a separate
asset from the crossbow itself so it can be spawned/attached independently
as a projectile (fired, socketed loaded on the weapon's rail, or dropped
as a pickup).

Run headless:
    blender --background --python build_bolt.py

Builds SM_Crossbow_Bolt01, saves a .blend, exports FBX (UE-ready axes),
renders a preview PNG, and writes a UV-layout reference PNG. Pure bmesh
construction (no bpy.ops mesh edits) so it runs reliably with no
window/context.

Axis convention (matches the crossbow): local Z runs from the nock (Z=0,
the origin -- where it rests against the string / attaches to the
crossbow's rail socket) to the head tip (+Z). This keeps a projectile's
"forward" axis pointing away from its attach/spawn origin, same as the
crossbow's own grip-at-origin convention.
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
SHAFT_RADIUS = 0.004
SHAFT_LENGTH = 0.36          # nock (Z=0) to base of the head
SHAFT_SEGMENTS = 16
HEAD_LENGTH = 0.03
HEAD_BASE_RADIUS = SHAFT_RADIUS * 1.4
HEAD_SEGMENTS = 16

FLETCH_Z0, FLETCH_Z1 = 0.02, 0.09   # near the nock end
FLETCH_SPAN, FLETCH_THICKNESS = 0.016, 0.0012

NOCK_RADIUS, NOCK_LENGTH = SHAFT_RADIUS * 1.2, 0.010
NOCK_SEGMENTS = 16

# ---------------------------------------------------------------------------
# Scene reset
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'

# ---------------------------------------------------------------------------
# Materials -- wood shaft, steel head, dull feather fletching
# ---------------------------------------------------------------------------
def make_material(name, base_color, metallic, roughness):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = base_color
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return mat

mat_wood = make_material("M_Bolt_Wood", (0.24, 0.15, 0.08, 1.0), 0.0, 0.50)
mat_steel = make_material("M_Bolt_Steel", (0.68, 0.69, 0.71, 1.0), 1.0, 0.22)
mat_fletch = make_material("M_Bolt_Fletching", (0.55, 0.53, 0.48, 1.0), 0.0, 0.55)

MAT_WOOD, MAT_STEEL, MAT_FLETCH = 0, 1, 2

bm = bmesh.new()

def add_faces_with_material(build_fn, material_index):
    before = set(bm.faces)
    build_fn()
    for f in bm.faces:
        if f not in before:
            f.material_index = material_index

# --- Nock: small steel cap at the rear where the string sits ---------------
def build_nock():
    mat = Matrix.Translation((0, 0, NOCK_LENGTH / 2))
    bmesh.ops.create_cone(
        bm, cap_ends=True, cap_tris=True, segments=NOCK_SEGMENTS,
        radius1=NOCK_RADIUS, radius2=SHAFT_RADIUS, depth=NOCK_LENGTH,
        matrix=mat, calc_uvs=True,
    )

# --- Shaft: plain wood dowel -------------------------------------------------
def build_shaft():
    z = NOCK_LENGTH + SHAFT_LENGTH / 2
    mat = Matrix.Translation((0, 0, z))
    bmesh.ops.create_cone(
        bm, cap_ends=True, cap_tris=True, segments=SHAFT_SEGMENTS,
        radius1=SHAFT_RADIUS, radius2=SHAFT_RADIUS, depth=SHAFT_LENGTH,
        matrix=mat, calc_uvs=True,
    )

# --- Head: steel bodkin point -----------------------------------------------
def build_head():
    z = NOCK_LENGTH + SHAFT_LENGTH + HEAD_LENGTH / 2
    mat = Matrix.Translation((0, 0, z))
    bmesh.ops.create_cone(
        bm, cap_ends=True, cap_tris=True, segments=HEAD_SEGMENTS,
        radius1=HEAD_BASE_RADIUS, radius2=0.0004, depth=HEAD_LENGTH,
        matrix=mat, calc_uvs=True,
    )

# --- Fletching: two crossed feather fins near the nock ----------------------
def build_fletching():
    z = NOCK_LENGTH + (FLETCH_Z0 + FLETCH_Z1) / 2
    length = FLETCH_Z1 - FLETCH_Z0
    for axis in ('X', 'Y'):
        mat = Matrix.Translation((0, 0, z))
        if axis == 'X':
            mat = mat @ Matrix.Diagonal((FLETCH_THICKNESS, FLETCH_SPAN, length, 1.0))
        else:
            mat = mat @ Matrix.Diagonal((FLETCH_SPAN, FLETCH_THICKNESS, length, 1.0))
        bmesh.ops.create_cube(bm, size=1.0, matrix=mat, calc_uvs=True)

add_faces_with_material(build_nock, MAT_STEEL)
add_faces_with_material(build_shaft, MAT_WOOD)
add_faces_with_material(build_head, MAT_STEEL)
add_faces_with_material(build_fletching, MAT_FLETCH)

bmesh.ops.recalc_face_normals(bm, faces=bm.faces)

mesh = bpy.data.meshes.new("SM_Crossbow_Bolt01")
bm.to_mesh(mesh)
bm.free()

for p in mesh.polygons:
    p.use_smooth = False

mesh.materials.append(mat_wood)
mesh.materials.append(mat_steel)
mesh.materials.append(mat_fletch)

obj = bpy.data.objects.new("SM_Crossbow_Bolt01", mesh)
scene.collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)

bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(
    angle_limit=math.radians(66), island_margin=0.02, scale_to_bounds=True
)
bpy.ops.object.mode_set(mode='OBJECT')

# ---------------------------------------------------------------------------
# UV layout reference PNG -- rasterized straight from the mesh's UV data
# (bpy.ops.uv.export_layout needs a live Image Editor area, which doesn't
# exist in headless --background mode). Same approach as the crossbow.
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

export_uv_layout(mesh, os.path.join(TEX_DIR, "UV_Layout_Bolt.png"))

# ---------------------------------------------------------------------------
# Preview render
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

# Same camera/light ratios that worked for the sword (distance and light
# size scaled to this object's length; lens and energies kept as-is --
# scaling both distance and size together keeps exposure consistent).
total_length = NOCK_LENGTH + SHAFT_LENGTH + HEAD_LENGTH
target = Vector((0, 0, total_length / 2))
scale = total_length / 0.85  # sword's overall length

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
scene.render.resolution_y = 1000
scene.render.filepath = os.path.join(OUT_DIR, "preview_bolt.png")
bpy.ops.render.render(write_still=True)

# ---------------------------------------------------------------------------
# Save .blend source + FBX export (UE convention: -Y forward, Z up)
# ---------------------------------------------------------------------------
blend_path = os.path.join(OUT_DIR, "SM_Crossbow_Bolt01.blend")
bpy.ops.wm.save_as_mainfile(filepath=blend_path)

fbx_path = os.path.join(OUT_DIR, "SM_Crossbow_Bolt01.fbx")
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
