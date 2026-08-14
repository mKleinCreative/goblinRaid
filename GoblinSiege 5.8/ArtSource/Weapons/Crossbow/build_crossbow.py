"""
Procedural militia-crossbow blockout for GoblinSiege human defenders.
Grounded medieval-realism style, matching SM_Sword_Arming01: plain wood
stock, polished steel prod/fittings, dark cord string. The bolt is a
separate asset (see build_bolt.py, SM_Crossbow_Bolt01) so it can be
spawned/attached independently as a projectile -- the top rail here is
where it sits when socketed onto the weapon.

Run headless:
    blender --background --python build_crossbow.py

Builds SM_Crossbow_Militia01, saves a .blend, exports FBX (UE-ready axes),
renders a preview PNG, and writes a UV-layout reference PNG. Pure bmesh
construction (no bpy.ops mesh edits) so it runs reliably with no
window/context. The UV layout is rasterized straight from the mesh's UV
data rather than via bpy.ops.uv.export_layout, which needs a live Image
Editor area and errors out in headless --background mode.

Axis convention (matches the sword): local Z runs from the butt (-Z)
through the grip (Z=0, the hand-socket attach point) to the prod tip
(+Z). Local X is left-right (prod span). Local Y is up-down.
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
# Stock cross-section stations: (z, half_width_x, half_height_y)
STOCK_STATIONS = [
    (-0.28, 0.017, 0.027),  # butt plate (flared for the shoulder)
    (-0.20, 0.013, 0.019),  # taper into the wrist
    (-0.04, 0.012, 0.016),  # rear of grip
    (0.00, 0.011, 0.014),   # grip / origin -- hand-socket attach point
    (0.10, 0.012, 0.015),   # front of grip, base of forestock
    (0.40, 0.010, 0.012),   # nose
]
STOCK_RING_SIDES = 8       # chamfered-octagon cross-section, not a bare box
STOCK_CHAMFER = 0.35       # corner cut, fraction of the smaller half-dim

RAIL_Z0, RAIL_Z1 = 0.06, 0.40   # top rail/groove spans grip -> nose
RAIL_HALF_WIDTH, RAIL_HEIGHT = 0.006, 0.005

PROD_Z = 0.34               # prod mount, mortised into the forestock
PROD_Y_OFFSET = 0.010        # mounted slightly above stock centerline
PROD_HALF_SPAN = 0.32
PROD_RECURVE = 0.035         # forward sweep of the tips
PROD_SEGMENTS = 14
PROD_BASE_HEIGHT, PROD_BASE_DEPTH = 0.024, 0.020
PROD_TIP_HEIGHT, PROD_TIP_DEPTH = 0.010, 0.008

NUT_POS = Vector((0.0, 0.020, 0.10))
NUT_RADIUS, NUT_LENGTH, NUT_SEGMENTS = 0.009, 0.020, 16

STRING_RADIUS = 0.0018

# Trigger guard / stirrup are smooth loops: a straight lerp between two
# attach points plus a sine "dip" bulge, sampled into many round-strut
# segments instead of a handful of flat box segments.
GUARD_START = Vector((0.0, -0.012, -0.020))
GUARD_END = Vector((0.0, -0.014, 0.045))
GUARD_DIP = Vector((0.0, -0.034, 0.0))
GUARD_SEGMENTS = 16
GUARD_STRUT_RADIUS = 0.0032

TRIGGER_PIVOT = Vector((0.0, -0.013, 0.005))
TRIGGER_TIP = Vector((0.0, -0.033, 0.020))
TRIGGER_THICKNESS = (0.006, 0.012)

STIRRUP_START = Vector((0.014, 0.012, 0.395))
STIRRUP_END = Vector((-0.014, 0.012, 0.395))
STIRRUP_DIP = Vector((0.0, -0.052, 0.075))
STIRRUP_SEGMENTS = 20
STIRRUP_STRUT_RADIUS = 0.0042

# ---------------------------------------------------------------------------
# Scene reset
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'

# ---------------------------------------------------------------------------
# Materials -- plain wood stock, polished steel fittings, dark cord string
# ---------------------------------------------------------------------------
def make_material(name, base_color, metallic, roughness):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = base_color
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return mat

mat_wood = make_material("M_Crossbow_Wood", (0.24, 0.15, 0.08, 1.0), 0.0, 0.50)
mat_steel = make_material("M_Crossbow_Steel", (0.68, 0.69, 0.71, 1.0), 1.0, 0.22)
mat_string = make_material("M_Crossbow_String", (0.05, 0.04, 0.03, 1.0), 0.0, 0.60)

MAT_WOOD, MAT_STEEL, MAT_STRING = 0, 1, 2

bm = bmesh.new()

def add_faces_with_material(build_fn, material_index):
    before = set(bm.faces)
    build_fn()
    for f in bm.faces:
        if f not in before:
            f.material_index = material_index

def add_strut(p0, p1, thickness, depth=None, round_=False, segments=8):
    """Oriented box (or, with round_=True, cylinder) between two points --
    string segments, guard/stirrup loops, trigger lever. Mirrors the sword
    guard's segmented-cube technique, generalized to any two points."""
    p0, p1 = Vector(p0), Vector(p1)
    diff = p1 - p0
    length = diff.length
    direction = diff.normalized()
    rot = direction.to_track_quat('X', 'Z')
    base = Matrix.Translation((p0 + p1) / 2) @ rot.to_matrix().to_4x4()
    if round_:
        cyl_mat = base @ Matrix.Rotation(math.radians(90), 4, 'Y')
        bmesh.ops.create_cone(
            bm, cap_ends=True, cap_tris=True, segments=segments,
            radius1=thickness, radius2=thickness, depth=length,
            matrix=cyl_mat, calc_uvs=True,
        )
    else:
        if depth is None:
            depth = thickness
        mat = base @ Matrix.Diagonal((length, depth, thickness, 1.0))
        bmesh.ops.create_cube(bm, size=1.0, matrix=mat, calc_uvs=True)

def dip_loop(p_start, p_end, dip, n):
    """N+1 points lerped start->end with a sine-shaped bulge (dip) added
    at the midpoint -- a smooth "D"-loop through two attach points."""
    pts = []
    for i in range(n + 1):
        frac = i / n
        pts.append(p_start.lerp(p_end, frac) + dip * math.sin(math.pi * frac))
    return pts

def strut_loop(points, radius, round_=True, segments=8):
    for i in range(len(points) - 1):
        add_strut(points[i], points[i + 1], radius, round_=round_, segments=segments)

# --- Stock: tapered chamfered-octagon-section stock, butt to nose ----------
def ring(hw, hh, z):
    c = min(hw, hh) * STOCK_CHAMFER
    pts_2d = [
        (hw - c, hh), (hw, hh - c),
        (hw, -hh + c), (hw - c, -hh),
        (-hw + c, -hh), (-hw, -hh + c),
        (-hw, hh - c), (-hw + c, hh),
    ]
    return [bm.verts.new((x, y, z)) for x, y in pts_2d]

def build_stock():
    n = STOCK_RING_SIDES
    rings = [ring(hw, hh, z) for z, hw, hh in STOCK_STATIONS]
    bm.faces.new(tuple(reversed(rings[0])))   # butt cap
    for i in range(len(rings) - 1):
        a, b = rings[i], rings[i + 1]
        for j in range(n):
            bm.faces.new((a[j], a[(j + 1) % n], b[(j + 1) % n], b[j]))
    bm.faces.new(tuple(rings[-1]))            # nose cap

# --- Top rail / bolt groove -------------------------------------------------
def build_rail():
    top_y = 0.014  # approx stock top surface over the rail's span
    mid_z = (RAIL_Z0 + RAIL_Z1) / 2
    mat = Matrix.Translation((0, top_y + RAIL_HEIGHT / 2, mid_z))
    bmesh.ops.create_cube(
        bm, size=1.0, matrix=mat @ Matrix.Diagonal(
            (RAIL_HALF_WIDTH * 2, RAIL_HEIGHT, RAIL_Z1 - RAIL_Z0, 1.0)
        ), calc_uvs=True,
    )

# --- Prod: recurved steel bow arm, mortised through the forestock ----------
def build_prod():
    def profile(t):
        x = t * PROD_HALF_SPAN
        z = PROD_Z + PROD_RECURVE * (t ** 1.6)
        h = PROD_BASE_HEIGHT + (PROD_TIP_HEIGHT - PROD_BASE_HEIGHT) * t
        d = PROD_BASE_DEPTH + (PROD_TIP_DEPTH - PROD_BASE_DEPTH) * t
        return Vector((x, PROD_Y_OFFSET, z)), h, d

    for side in (-1, 1):
        pts = [profile(i / PROD_SEGMENTS) for i in range(PROD_SEGMENTS + 1)]
        for i in range(PROD_SEGMENTS):
            p0, h0, d0 = pts[i]
            p1, h1, d1 = pts[i + 1]
            p0 = Vector((p0.x * side, p0.y, p0.z))
            p1 = Vector((p1.x * side, p1.y, p1.z))
            add_strut(p0, p1, (h0 + h1) / 2, (d0 + d1) / 2)

def prod_tip(side):
    z = PROD_Z + PROD_RECURVE
    return Vector((side * PROD_HALF_SPAN, PROD_Y_OFFSET, z))

# --- Bowstring: strung and drawn back to the nut ----------------------------
def build_string():
    add_strut(prod_tip(-1), NUT_POS, STRING_RADIUS, round_=True, segments=6)
    add_strut(prod_tip(1), NUT_POS, STRING_RADIUS, round_=True, segments=6)

# --- Nut boss: where the drawn string catches -------------------------------
def build_nut():
    mat = Matrix.Translation(NUT_POS) @ Matrix.Rotation(1.5708, 4, 'Z')
    bmesh.ops.create_cone(
        bm, cap_ends=True, cap_tris=True, segments=NUT_SEGMENTS,
        radius1=NUT_RADIUS, radius2=NUT_RADIUS, depth=NUT_LENGTH,
        matrix=mat, calc_uvs=True,
    )

# --- Trigger guard + trigger -------------------------------------------------
def build_trigger_guard():
    pts = dip_loop(GUARD_START, GUARD_END, GUARD_DIP, GUARD_SEGMENTS)
    strut_loop(pts, GUARD_STRUT_RADIUS, round_=True, segments=8)

def build_trigger():
    add_strut(TRIGGER_PIVOT, TRIGGER_TIP, *TRIGGER_THICKNESS)

# --- Stirrup: foot loop at the nose for spanning ----------------------------
def build_stirrup():
    pts = dip_loop(STIRRUP_START, STIRRUP_END, STIRRUP_DIP, STIRRUP_SEGMENTS)
    strut_loop(pts, STIRRUP_STRUT_RADIUS, round_=True, segments=8)

add_faces_with_material(build_stock, MAT_WOOD)
add_faces_with_material(build_rail, MAT_WOOD)
add_faces_with_material(build_prod, MAT_STEEL)
add_faces_with_material(build_string, MAT_STRING)
add_faces_with_material(build_nut, MAT_STEEL)
add_faces_with_material(build_trigger_guard, MAT_STEEL)
add_faces_with_material(build_trigger, MAT_STEEL)
add_faces_with_material(build_stirrup, MAT_STEEL)

bmesh.ops.recalc_face_normals(bm, faces=bm.faces)

mesh = bpy.data.meshes.new("SM_Crossbow_Militia01")
bm.to_mesh(mesh)
bm.free()

for p in mesh.polygons:
    p.use_smooth = False  # flat-shaded hard-surface look, matches the sword

mesh.materials.append(mat_wood)
mesh.materials.append(mat_steel)
mesh.materials.append(mat_string)

obj = bpy.data.objects.new("SM_Crossbow_Militia01", mesh)
scene.collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)

# Non-overlapping unwrap (per-primitive default UVs all land in the same
# 0-1 square otherwise) -- same fix as the sword.
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(
    angle_limit=math.radians(66), island_margin=0.02, scale_to_bounds=True
)
bpy.ops.object.mode_set(mode='OBJECT')

# ---------------------------------------------------------------------------
# UV layout reference PNG -- rasterized straight from the mesh's UV data.
# bpy.ops.uv.export_layout needs a live Image Editor area, which doesn't
# exist in headless --background mode, so this draws the wireframe by hand
# into a transparent-background image (overlayable on any texture pass).
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

export_uv_layout(mesh, os.path.join(TEX_DIR, "UV_Layout_Crossbow.png"))

# ---------------------------------------------------------------------------
# Preview render -- 3/4 view framing the whole silhouette (stock + prod)
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

z_min = STOCK_STATIONS[0][0]
z_max = STIRRUP_START.z + STIRRUP_DIP.z
target = Vector((0, -0.005, (z_min + z_max) / 2))

cam_data = bpy.data.cameras.new("Camera")
cam_data.lens = 32
cam = bpy.data.objects.new("Camera", cam_data)
scene.collection.objects.link(cam)
cam.location = target + Vector((0.75, -1.15, 0.05))
look_at(cam, target)
scene.camera = cam

key_data = bpy.data.lights.new("Key", type='AREA')
key_data.energy = 55
key_data.size = 0.9
key = bpy.data.objects.new("Key", key_data)
key.location = target + Vector((1.0, -1.0, 1.0))
look_at(key, target)
scene.collection.objects.link(key)

fill_data = bpy.data.lights.new("Fill", type='AREA')
fill_data.energy = 20
fill_data.size = 1.2
fill = bpy.data.objects.new("Fill", fill_data)
fill.location = target + Vector((-1.2, -0.5, -0.2))
look_at(fill, target)
scene.collection.objects.link(fill)

rim_data = bpy.data.lights.new("Rim", type='AREA')
rim_data.energy = 30
rim_data.size = 0.6
rim = bpy.data.objects.new("Rim", rim_data)
rim.location = target + Vector((-0.3, 1.4, 0.6))
look_at(rim, target)
scene.collection.objects.link(rim)

scene.view_settings.view_transform = 'Standard'
scene.render.resolution_x = 1200
scene.render.resolution_y = 900
scene.render.filepath = os.path.join(OUT_DIR, "preview.png")
bpy.ops.render.render(write_still=True)

# ---------------------------------------------------------------------------
# Save .blend source + FBX export (UE convention: -Y forward, Z up)
# ---------------------------------------------------------------------------
blend_path = os.path.join(OUT_DIR, "SM_Crossbow_Militia01.blend")
bpy.ops.wm.save_as_mainfile(filepath=blend_path)

fbx_path = os.path.join(OUT_DIR, "SM_Crossbow_Militia01.fbx")
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
