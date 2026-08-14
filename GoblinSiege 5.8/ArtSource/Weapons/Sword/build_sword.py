"""
Procedural arming-sword blockout for GoblinSiege humans.
Reference: medieval-arming-sword-Knight-side-sword-1313.jpg (wheel pommel,
curved crescent guard, black leather grip, near-parallel blade tapering
sharply near the tip).

Run headless:
    blender --background --python build_sword.py

Builds SM_Sword_Arming01, saves a .blend, exports FBX (UE-ready axes),
and renders a preview PNG. Pure bmesh construction (no bpy.ops mesh
edits) so it runs reliably with no window/context.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

OUT_DIR = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------------------
# Dimensions (meters)
# ---------------------------------------------------------------------------
POMMEL_RADIUS = 0.028
POMMEL_THICKNESS = 0.014
BOSS_RADIUS = 0.010
BOSS_HEIGHT = 0.007

GRIP_RADIUS = 0.017
GRIP_LENGTH = 0.115
GRIP_SEGMENTS = 12

GUARD_HALF_WIDTH = 0.095
GUARD_BASE_HEIGHT = 0.015   # cross-section height near the blade
GUARD_BASE_DEPTH = 0.017    # cross-section thickness near the blade
GUARD_TIP_HEIGHT = 0.007
GUARD_TIP_DEPTH = 0.009
GUARD_CURVE_RISE = 0.011    # how far the quillon tips sweep toward the tip
GUARD_SEGMENTS = 6          # per side, piecewise-linear approximation

BLADE_BASE_HALF_WIDTH = 0.0215
BLADE_MID_HALF_WIDTH = 0.0195
BLADE_MID_FRACTION = 0.70   # where the blade starts tapering hard to a point
BLADE_THICKNESS_FACTOR = 0.15  # ratio of flat-side thickness to edge width
BLADE_LENGTH = 0.73

# ---------------------------------------------------------------------------
# Scene reset
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'

# ---------------------------------------------------------------------------
# Materials — polished steel blade/fittings, black leather grip
# ---------------------------------------------------------------------------
def make_material(name, base_color, metallic, roughness):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = base_color
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return mat

mat_blade = make_material("M_Sword_Blade", (0.80, 0.81, 0.83, 1.0), 1.0, 0.12)
mat_fittings = make_material("M_Sword_Fittings", (0.68, 0.69, 0.71, 1.0), 1.0, 0.22)
mat_grip = make_material("M_Sword_Grip", (0.02, 0.02, 0.02, 1.0), 0.0, 0.55)

MAT_BLADE, MAT_FITTINGS, MAT_GRIP = 0, 1, 2

# ---------------------------------------------------------------------------
# Layout — stack parts along Z from the pommel up, then re-center so the
# object origin sits at the grip midpoint (the hand-socket attach point).
# ---------------------------------------------------------------------------
grip_bottom_z = POMMEL_THICKNESS
grip_top_z = grip_bottom_z + GRIP_LENGTH
guard_bottom_z = grip_top_z
guard_top_z = guard_bottom_z + GUARD_BASE_HEIGHT
blade_bottom_z = guard_top_z
blade_top_z = blade_bottom_z + BLADE_LENGTH

grip_center_z = grip_bottom_z + GRIP_LENGTH / 2
origin_offset = grip_center_z

bm = bmesh.new()
uv_layer = bm.loops.layers.uv.new("UVMap")

def add_faces_with_material(build_fn, material_index):
    before = set(bm.faces)
    build_fn()
    for f in bm.faces:
        if f not in before:
            f.material_index = material_index

# --- Pommel: flat wheel disc + small center boss on the outward face -------
def build_pommel():
    z_center = POMMEL_THICKNESS / 2 - origin_offset
    disc_mat = Matrix.Translation((0, 0, z_center))
    bmesh.ops.create_cone(
        bm, cap_ends=True, cap_tris=True, segments=14,
        radius1=POMMEL_RADIUS, radius2=POMMEL_RADIUS, depth=POMMEL_THICKNESS,
        matrix=disc_mat, calc_uvs=True,
    )
    boss_z = -BOSS_HEIGHT / 2 - origin_offset
    boss_mat = Matrix.Translation((0, 0, boss_z)) @ Matrix.Diagonal((1, 1, 0.6, 1))
    bmesh.ops.create_icosphere(bm, subdivisions=1, radius=BOSS_RADIUS, matrix=boss_mat, calc_uvs=True)

# --- Grip: smooth faceted cylinder -----------------------------------------
def build_grip():
    z = (grip_bottom_z + grip_top_z) / 2 - origin_offset
    mat = Matrix.Translation((0, 0, z))
    bmesh.ops.create_cone(
        bm, cap_ends=True, cap_tris=True, segments=GRIP_SEGMENTS,
        radius1=GRIP_RADIUS, radius2=GRIP_RADIUS, depth=GRIP_LENGTH,
        matrix=mat, calc_uvs=True,
    )

# --- Guard: piecewise-linear crescent, curving toward the blade tip --------
def build_guard():
    z0 = (guard_bottom_z + guard_top_z) / 2 - origin_offset

    def profile(t):
        # t in [0,1] from center to tip
        x = t * GUARD_HALF_WIDTH
        z = z0 + GUARD_CURVE_RISE * (t ** 1.6)
        h = GUARD_BASE_HEIGHT + (GUARD_TIP_HEIGHT - GUARD_BASE_HEIGHT) * t
        d = GUARD_BASE_DEPTH + (GUARD_TIP_DEPTH - GUARD_BASE_DEPTH) * t
        return Vector((x, 0, z)), h, d

    for side in (-1, 1):
        pts = [profile(i / GUARD_SEGMENTS) for i in range(GUARD_SEGMENTS + 1)]
        for i in range(GUARD_SEGMENTS):
            p0, h0, d0 = pts[i]
            p1, h1, d1 = pts[i + 1]
            p0 = Vector((p0.x * side, p0.y, p0.z))
            p1 = Vector((p1.x * side, p1.y, p1.z))
            mid = (p0 + p1) / 2
            seg_len = (p1 - p0).length
            dx, dz = (p1.x - p0.x), (p1.z - p0.z)
            theta = -math.atan2(dz, dx)
            h_mid = (h0 + h1) / 2
            d_mid = (d0 + d1) / 2
            seg_mat = (
                Matrix.Translation(mid)
                @ Matrix.Rotation(theta, 4, 'Y')
                @ Matrix.Diagonal((seg_len * 1.02, d_mid, h_mid, 1.0))
            )
            bmesh.ops.create_cube(bm, size=1.0, matrix=seg_mat, calc_uvs=True)

# --- Blade: diamond cross-section, near-parallel then sharp taper ---------
def build_blade():
    def ring(half_width, z):
        t = BLADE_THICKNESS_FACTOR
        return [
            bm.verts.new((half_width, 0, z)),
            bm.verts.new((0, half_width * t, z)),
            bm.verts.new((-half_width, 0, z)),
            bm.verts.new((0, -half_width * t, z)),
        ]

    z_base = blade_bottom_z - origin_offset
    z_mid = blade_bottom_z + BLADE_LENGTH * BLADE_MID_FRACTION - origin_offset
    z_tip = blade_top_z - origin_offset

    base_ring = ring(BLADE_BASE_HALF_WIDTH, z_base)
    mid_ring = ring(BLADE_MID_HALF_WIDTH, z_mid)
    apex = bm.verts.new((0, 0, z_tip))

    def set_uvs(face, uvs):
        for loop, uv in zip(face.loops, uvs):
            loop[uv_layer].uv = uv

    v_base, v_mid, v_tip = 0.0, 0.72, 1.0

    cap = bm.faces.new((base_ring[0], base_ring[1], base_ring[2], base_ring[3]))
    set_uvs(cap, [(0.1, 0.05), (0.2, 0.05), (0.2, 0.15), (0.1, 0.15)])  # small unused corner

    for i in range(4):
        a, b = base_ring[i], base_ring[(i + 1) % 4]
        c, d = mid_ring[i], mid_ring[(i + 1) % 4]
        f = bm.faces.new((a, b, d, c))
        u0, u1 = i / 4, (i + 1) / 4
        set_uvs(f, [(u0, v_base), (u1, v_base), (u1, v_mid), (u0, v_mid)])
    for i in range(4):
        c, d = mid_ring[i], mid_ring[(i + 1) % 4]
        f = bm.faces.new((c, d, apex))
        u0, u1 = i / 4, (i + 1) / 4
        set_uvs(f, [(u0, v_mid), (u1, v_mid), ((u0 + u1) / 2, v_tip)])

add_faces_with_material(build_pommel, MAT_FITTINGS)
add_faces_with_material(build_grip, MAT_GRIP)
add_faces_with_material(build_guard, MAT_FITTINGS)
add_faces_with_material(build_blade, MAT_BLADE)

bmesh.ops.recalc_face_normals(bm, faces=bm.faces)

mesh = bpy.data.meshes.new("SM_Sword_Arming01")
bm.to_mesh(mesh)
bm.free()

for p in mesh.polygons:
    p.use_smooth = False  # flat-shaded hard-surface look

mesh.materials.append(mat_blade)
mesh.materials.append(mat_fittings)
mesh.materials.append(mat_grip)

obj = bpy.data.objects.new("SM_Sword_Arming01", mesh)
scene.collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)

# Per-primitive default UVs all land in the same 0-1 square (every guard
# segment, the grip, the pommel, etc. overlap). Replace with a real
# non-overlapping unwrap so the mesh is actually texturable/bakeable.
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(
    angle_limit=math.radians(66), island_margin=0.02, scale_to_bounds=True
)
bpy.ops.object.mode_set(mode='OBJECT')

# ---------------------------------------------------------------------------
# Preview render — camera + lights, framed on the full sword
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

sword_bottom_z = -POMMEL_THICKNESS / 2 - 0  # relative to origin already offset
sword_top_z = blade_top_z - origin_offset
sword_bottom_z = 0 - origin_offset
sword_mid_z = (sword_top_z + sword_bottom_z) / 2
target = Vector((0, 0, sword_mid_z))

cam_data = bpy.data.cameras.new("Camera")
cam_data.lens = 35
cam = bpy.data.objects.new("Camera", cam_data)
scene.collection.objects.link(cam)
cam.location = target + Vector((0.55, -0.8, 0.0))
look_at(cam, target)
scene.camera = cam

key_data = bpy.data.lights.new("Key", type='AREA')
key_data.energy = 45
key_data.size = 0.9
key = bpy.data.objects.new("Key", key_data)
key.location = target + Vector((1.0, -1.0, 1.0))
look_at(key, target)
scene.collection.objects.link(key)

fill_data = bpy.data.lights.new("Fill", type='AREA')
fill_data.energy = 18
fill_data.size = 1.2
fill = bpy.data.objects.new("Fill", fill_data)
fill.location = target + Vector((-1.2, -0.5, -0.2))
look_at(fill, target)
scene.collection.objects.link(fill)

rim_data = bpy.data.lights.new("Rim", type='AREA')
rim_data.energy = 25
rim_data.size = 0.6
rim = bpy.data.objects.new("Rim", rim_data)
rim.location = target + Vector((-0.3, 1.4, 0.6))
look_at(rim, target)
scene.collection.objects.link(rim)

scene.view_settings.view_transform = 'Standard'
scene.render.resolution_x = 700
scene.render.resolution_y = 1050
scene.render.filepath = os.path.join(OUT_DIR, "preview.png")
bpy.ops.render.render(write_still=True)

# ---------------------------------------------------------------------------
# Save .blend source + FBX export (UE convention: -Y forward, Z up)
# ---------------------------------------------------------------------------
blend_path = os.path.join(OUT_DIR, "SM_Sword_Arming01.blend")
bpy.ops.wm.save_as_mainfile(filepath=blend_path)

fbx_path = os.path.join(OUT_DIR, "SM_Sword_Arming01.fbx")
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
