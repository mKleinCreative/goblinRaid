"""
Procedural hunting horn blockout for the GoblinSiege support-summon prop.
Grounded medieval-realism style, matching SM_Sword_Arming01,
SM_Crossbow_Militia01 and SM_Lance_War01: aged ox-horn body, polished
iron mouthpiece ferrule, bands and bell rim, dark leather grip wrap.

Run headless:
    blender --background --python build_hunting_horn.py

Builds SM_HuntingHorn_Signal01, saves a .blend, exports FBX (UE-ready
axes), renders a preview PNG, and writes a UV-layout reference PNG. Pure
bmesh construction (no bpy.ops mesh edits) so it runs reliably with no
window/context. The UV layout is rasterized straight from the mesh's UV
data (bpy.ops.uv.export_layout needs a live Image Editor area, which
doesn't exist in headless --background mode).

Axis convention: the horn sweeps through a circular arc that starts in
the +Z direction and curves out toward +X. The origin sits at the
mouthpiece tip (-Z end, where the horn meets the lips / the hand grips
it to raise it); the arc curves away to the flared bell at the far end.
Cross-sections are circles perpendicular to the sweep at every station,
so at the mouthpiece the tube's round cross-section lies in the local
XY plane, same as the sword/lance grip convention.

An iron suspension chain loops from a point near the mouthpiece to a
point near the bell, along the outer (convex) side of the curve, so the
horn can be slung across a character's body. Bands, ferrule and chain
links are built dense enough to push the mesh past 5000 tris.
"""
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Vector

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
TEX_DIR = os.path.join(OUT_DIR, "Textures")
os.makedirs(TEX_DIR, exist_ok=True)

# ---------------------------------------------------------------------------
# Dimensions (meters, degrees)
# ---------------------------------------------------------------------------
ARC_RADIUS = 0.30          # radius of the circle the horn's centerline follows
ARC_SWEEP_DEG = 112.0      # total curve from mouthpiece to bell

SIDES = 24                 # roundness of the tube cross-section

# Body loft stations: (angle_deg from mouthpiece, tube radius)
HORN_STATIONS = [
    (0.0, 0.0105),   # mouthpiece tip
    (7.0, 0.0115),   # short parallel mouthpiece lip
    (20.0, 0.0155),
    (38.0, 0.0210),
    (56.0, 0.0290),
    (74.0, 0.0380),
    (92.0, 0.0470),
    (104.0, 0.0540),  # base of the bell, right before the flare
]

BELL_FLARE_ANGLE = ARC_SWEEP_DEG
BELL_FLARE_RADIUS = 0.076
BELL_LIP_ANGLE = ARC_SWEEP_DEG + 7.0      # push further in for a deeper bore
BELL_LIP_RADIUS = 0.054

FERRULE_ANGLE0, FERRULE_ANGLE1 = 3.0, 12.0
FERRULE_RADIUS = 0.017

BAND_ANGLES = [24.0, 66.0]
BAND_HALF_WIDTH_DEG = 5.5      # thick collar, not a thin scratch of a ring
BAND_SHOULDER_DEG = 1.3        # angular width of the step up/down at each edge
BAND_RADIUS_BUMP = 0.009       # how far the band stands proud of the base tube

GRIP_ANGLE0, GRIP_ANGLE1 = 44.0, 58.0
GRIP_RADIUS_BUMP = 0.003

# Suspension chain: iron links looping along the outer curve from near the
# mouthpiece to near the bell, so the horn can be worn slung on a strap.
CHAIN_ANGLE_A, CHAIN_ANGLE_B = 13.0, 99.0
CHAIN_LINKS = 36                # tight spacing so consecutive links overlap/interlock
CHAIN_SAG = 0.05                # how far the chain droops outward at its midpoint
CHAIN_STANDOFF = 0.0015         # near-flush at the ends so the chain reads as attached
CHAIN_LINK_MAIN_RADIUS = 0.010
CHAIN_LINK_TUBE_RADIUS = 0.0026
CHAIN_MAIN_SEGS = 12
CHAIN_TUBE_SEGS = 8

# ---------------------------------------------------------------------------
# Scene reset
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'

# ---------------------------------------------------------------------------
# Materials -- aged ox-horn body, polished iron fittings, brown leather wrap
# ---------------------------------------------------------------------------
def make_material(name, base_color, metallic, roughness):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = base_color
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return mat

# Procedural grain: mottled base-color variation, roughness variation and a
# fine bump, all driven by object-space noise so the pattern is baked-in
# rather than view-dependent. This is what gets baked out to PNG maps below
# -- without it every part would bake to a perfectly flat, uniform texture.
def add_grain(mat, color_dark, color_light, rough_min, rough_max,
               bump_strength, bump_distance,
               color_scale=8.0, color_contrast=0.6, rough_scale=40.0, bump_scale=60.0):
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    coord = nt.nodes.new("ShaderNodeTexCoord")

    noise_c = nt.nodes.new("ShaderNodeTexNoise")
    noise_c.inputs["Scale"].default_value = color_scale
    noise_c.inputs["Detail"].default_value = 4.0
    nt.links.new(coord.outputs["Object"], noise_c.inputs["Vector"])
    ramp_c = nt.nodes.new("ShaderNodeValToRGB")
    ramp_c.color_ramp.elements[0].position = max(0.0, 0.5 - color_contrast / 2)
    ramp_c.color_ramp.elements[1].position = min(1.0, 0.5 + color_contrast / 2)
    nt.links.new(noise_c.outputs["Fac"], ramp_c.inputs["Fac"])
    mix_c = nt.nodes.new("ShaderNodeMixRGB")
    mix_c.inputs["Color1"].default_value = (*color_dark, 1.0)
    mix_c.inputs["Color2"].default_value = (*color_light, 1.0)
    nt.links.new(ramp_c.outputs["Color"], mix_c.inputs["Fac"])
    nt.links.new(mix_c.outputs["Color"], bsdf.inputs["Base Color"])

    noise_r = nt.nodes.new("ShaderNodeTexNoise")
    noise_r.inputs["Scale"].default_value = rough_scale
    nt.links.new(coord.outputs["Object"], noise_r.inputs["Vector"])
    maprange_r = nt.nodes.new("ShaderNodeMapRange")
    maprange_r.inputs["To Min"].default_value = rough_min
    maprange_r.inputs["To Max"].default_value = rough_max
    nt.links.new(noise_r.outputs["Fac"], maprange_r.inputs["Value"])
    nt.links.new(maprange_r.outputs["Result"], bsdf.inputs["Roughness"])

    noise_b = nt.nodes.new("ShaderNodeTexNoise")
    noise_b.inputs["Scale"].default_value = bump_scale
    nt.links.new(coord.outputs["Object"], noise_b.inputs["Vector"])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = bump_strength
    bump.inputs["Distance"].default_value = bump_distance
    nt.links.new(noise_b.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

mat_horn = make_material("M_HuntingHorn_Horn", (0.40, 0.27, 0.13, 1.0), 0.0, 0.35)
mat_iron = make_material("M_HuntingHorn_Iron", (0.30, 0.31, 0.33, 1.0), 1.0, 0.42)
mat_leather = make_material("M_HuntingHorn_Leather", (0.14, 0.08, 0.05, 1.0), 0.0, 0.6)
mat_opening = make_material("M_HuntingHorn_Opening", (0.01, 0.01, 0.01, 1.0), 0.0, 0.8)

# Natural ox-horn mottling: broad amber/cream streaks, mid roughness variation.
add_grain(mat_horn, (0.28, 0.17, 0.07), (0.52, 0.38, 0.20),
          0.28, 0.48, bump_strength=0.35, bump_distance=0.0008,
          color_scale=6.0, color_contrast=0.7, rough_scale=30.0, bump_scale=45.0)
# Forged iron: dark oxidised patches against duller worn-bright patches, fine scratches.
add_grain(mat_iron, (0.13, 0.14, 0.15), (0.48, 0.49, 0.51),
          0.30, 0.55, bump_strength=0.3, bump_distance=0.0004,
          color_scale=5.0, color_contrast=0.5, rough_scale=140.0, bump_scale=160.0)
# Leather: fine grain, both in color and in the bump map.
add_grain(mat_leather, (0.08, 0.045, 0.025), (0.19, 0.12, 0.07),
          0.45, 0.7, bump_strength=0.45, bump_distance=0.0006,
          color_scale=25.0, color_contrast=0.6, rough_scale=90.0, bump_scale=220.0)

MAT_HORN, MAT_IRON, MAT_LEATHER, MAT_OPENING = 0, 1, 2, 3

bm = bmesh.new()

def add_faces_with_material(build_fn, material_index):
    before = set(bm.faces)
    build_fn()
    for f in bm.faces:
        if f not in before:
            f.material_index = material_index

# ---------------------------------------------------------------------------
# Curve helpers -- the horn's centerline is an arc of radius ARC_RADIUS in
# the XZ plane, starting at the origin heading +Z and bending toward +X.
# ---------------------------------------------------------------------------
def curve_point(angle_rad):
    return Vector((
        ARC_RADIUS * (1.0 - math.cos(angle_rad)),
        0.0,
        ARC_RADIUS * math.sin(angle_rad),
    ))

def curve_frame(angle_rad):
    n = Vector((-math.cos(angle_rad), 0.0, math.sin(angle_rad)))  # radial (in-plane)
    b = Vector((0.0, 1.0, 0.0))                                    # binormal
    return n, b

def curved_ring(angle_deg, radius, sides=SIDES):
    angle_rad = math.radians(angle_deg)
    center = curve_point(angle_rad)
    n, b = curve_frame(angle_rad)
    pts = []
    for i in range(sides):
        theta = 2 * math.pi * i / sides
        offset = radius * (math.cos(theta) * n + math.sin(theta) * b)
        pts.append(bm.verts.new((center + offset)[:]))
    return pts

def loft(rings, cap_start=False, cap_end=False):
    n = len(rings[0])
    if cap_start:
        bm.faces.new(tuple(reversed(rings[0])))
    for i in range(len(rings) - 1):
        a, ring_b = rings[i], rings[i + 1]
        for j in range(n):
            bm.faces.new((a[j], a[(j + 1) % n], ring_b[(j + 1) % n], ring_b[j]))
    if cap_end:
        bm.faces.new(tuple(rings[-1]))

# --- Body: tapered horn tube, mouthpiece tip to base of the bell -----------
def build_body():
    rings = [curved_ring(a, r) for a, r in HORN_STATIONS]
    loft(rings, cap_start=True, cap_end=False)

# --- Bell: flare out to the rim, then a recessed lip to fake a hollow bore -
def build_bell():
    base_angle, base_radius = HORN_STATIONS[-1]
    r_base = curved_ring(base_angle, base_radius)
    r_flare = curved_ring(BELL_FLARE_ANGLE, BELL_FLARE_RADIUS)
    r_lip = curved_ring(BELL_LIP_ANGLE, BELL_LIP_RADIUS)
    loft([r_base, r_flare, r_lip], cap_start=False, cap_end=False)
    bm.faces.new(tuple(reversed(r_lip)))  # recessed cap, reads as the bore

# --- Mouthpiece ferrule: short iron collar near the lips -------------------
def build_ferrule():
    r0 = curved_ring(FERRULE_ANGLE0, FERRULE_RADIUS)
    r1 = curved_ring(FERRULE_ANGLE1, FERRULE_RADIUS)
    loft([r0, r1])

# --- Decorative iron bands: thick collars with square shoulders, not a
# thin scratch of a ring -- outer edges sit flush with the body, then step
# up to the full bump radius so the band reads as a raised, flat-topped cuff.
def build_band(center_angle):
    a_out0 = center_angle - BAND_HALF_WIDTH_DEG
    a_in0 = a_out0 + BAND_SHOULDER_DEG
    a_in1 = center_angle + BAND_HALF_WIDTH_DEG - BAND_SHOULDER_DEG
    a_out1 = center_angle + BAND_HALF_WIDTH_DEG
    bump = BAND_RADIUS_BUMP
    rings = [
        curved_ring(a_out0, _radius_at_angle(a_out0)),
        curved_ring(a_in0, _radius_at_angle(a_in0) + bump),
        curved_ring(a_in1, _radius_at_angle(a_in1) + bump),
        curved_ring(a_out1, _radius_at_angle(a_out1)),
    ]
    loft(rings)

# --- Leather grip wrap, standing slightly proud of the horn body -----------
def build_grip():
    radius0 = _radius_at_angle(GRIP_ANGLE0) + GRIP_RADIUS_BUMP
    radius1 = _radius_at_angle(GRIP_ANGLE1) + GRIP_RADIUS_BUMP
    r0 = curved_ring(GRIP_ANGLE0, radius0)
    r1 = curved_ring(GRIP_ANGLE1, radius1)
    loft([r0, r1])

def _radius_at_angle(angle_deg):
    for i in range(len(HORN_STATIONS) - 1):
        a0, r0 = HORN_STATIONS[i]
        a1, r1 = HORN_STATIONS[i + 1]
        if a0 <= angle_deg <= a1:
            t = (angle_deg - a0) / (a1 - a0)
            return r0 + (r1 - r0) * t
    return HORN_STATIONS[-1][1]

# --- Generic torus builder, used for the suspension-chain links ------------
def build_torus(center, u, v, main_radius, tube_radius, main_segs, tube_segs):
    u = u.normalized()
    v = v.normalized()
    normal = u.cross(v).normalized()
    rows = []
    for i in range(main_segs):
        a = 2 * math.pi * i / main_segs
        radial = math.cos(a) * u + math.sin(a) * v
        c = center + main_radius * radial
        row = []
        for j in range(tube_segs):
            theta = 2 * math.pi * j / tube_segs
            offset = tube_radius * (math.cos(theta) * radial + math.sin(theta) * normal)
            row.append(bm.verts.new((c + offset)[:]))
        rows.append(row)
    for i in range(main_segs):
        row_a, row_b = rows[i], rows[(i + 1) % main_segs]
        for j in range(tube_segs):
            bm.faces.new((row_a[j], row_a[(j + 1) % tube_segs],
                           row_b[(j + 1) % tube_segs], row_b[j]))

# --- Suspension chain: alternating-orientation links draped along the
# outer curve, sagging outward at the midpoint like slack carry-chain.
def build_chain():
    for i in range(CHAIN_LINKS):
        t = i / (CHAIN_LINKS - 1)
        angle_deg = CHAIN_ANGLE_A + (CHAIN_ANGLE_B - CHAIN_ANGLE_A) * t
        angle_rad = math.radians(angle_deg)
        sag = CHAIN_SAG * math.sin(math.pi * t)
        n_vec, b_vec = curve_frame(angle_rad)
        tangent = Vector((math.sin(angle_rad), 0.0, math.cos(angle_rad)))
        standoff = _radius_at_angle(angle_deg) + CHAIN_STANDOFF + sag
        center = curve_point(angle_rad) + standoff * n_vec
        u, v = (tangent, b_vec) if i % 2 == 0 else (tangent, n_vec)
        build_torus(center, u, v, CHAIN_LINK_MAIN_RADIUS, CHAIN_LINK_TUBE_RADIUS,
                     CHAIN_MAIN_SEGS, CHAIN_TUBE_SEGS)

add_faces_with_material(build_body, MAT_HORN)
add_faces_with_material(build_bell, MAT_IRON)
bm.faces.ensure_lookup_table()
bm.faces[-1].material_index = MAT_OPENING  # the recessed lip cap only
add_faces_with_material(build_ferrule, MAT_IRON)
for angle in BAND_ANGLES:
    add_faces_with_material(lambda a=angle: build_band(a), MAT_IRON)
add_faces_with_material(build_grip, MAT_LEATHER)
add_faces_with_material(build_chain, MAT_IRON)

bmesh.ops.recalc_face_normals(bm, faces=bm.faces)

mesh = bpy.data.meshes.new("SM_HuntingHorn_Signal01")
bm.to_mesh(mesh)
bm.free()

mesh.materials.append(mat_horn)
mesh.materials.append(mat_iron)
mesh.materials.append(mat_leather)
mesh.materials.append(mat_opening)

obj = bpy.data.objects.new("SM_HuntingHorn_Signal01", mesh)
scene.collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)

# Angle-based smooth shading: unlike the sword/crossbow/lance's flat blockout
# look, this is a round horn -- it needs its curved surfaces (body, bell,
# bands, chain links) to shade smoothly in Unreal instead of showing facets.
# shade_smooth_by_angle marks every polygon smooth and then flags only the
# edges whose face angle exceeds the threshold as sharp (mouthpiece/bell
# caps, band shoulders), so those stay crisp while the round surfaces blend.
# It writes real per-edge sharp flags into the mesh, not a viewport-only
# modifier, so the split normals bake straight into the FBX export below.
bpy.ops.object.shade_smooth_by_angle(angle=math.radians(35))

# Non-overlapping unwrap (per-primitive default UVs all land in the same
# 0-1 square otherwise) -- same fix as the sword/crossbow/lance.
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(
    angle_limit=math.radians(66), island_margin=0.02, scale_to_bounds=True
)
bpy.ops.object.mode_set(mode='OBJECT')

# ---------------------------------------------------------------------------
# UV layout reference PNG -- rasterized straight from the mesh's UV data.
# Same approach as the crossbow/lance (bpy.ops.uv.export_layout needs a live
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

export_uv_layout(mesh, os.path.join(TEX_DIR, "UV_Layout_HuntingHorn.png"))

# ---------------------------------------------------------------------------
# Bake the procedural materials down to PNG maps (BaseColor, Roughness,
# Normal, AO) using the UV layout above, so Unreal gets real textures
# instead of four flat, uniform colors. One image per map is shared across
# all four material slots -- each slot gets a Bake Target image node wired
# to nothing (Cycles just needs it present and active to know where to
# write that slot's UV islands), and since every slot points at the same
# image, all islands land in the one combined texture.
# ---------------------------------------------------------------------------
BAKE_DIR = os.path.join(TEX_DIR, "Bakes")
os.makedirs(BAKE_DIR, exist_ok=True)
BAKE_RESOLUTION = 2048

original_engine = scene.render.engine
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 32

all_mats = [mat_horn, mat_iron, mat_leather, mat_opening]
bake_targets = {}
for mat in all_mats:
    node = mat.node_tree.nodes.new("ShaderNodeTexImage")
    node.name = "BakeTarget"
    bake_targets[mat.name] = node

# BaseColor can't use the usual DIFFUSE+COLOR-pass-filter trick: a fully
# metallic Principled BSDF (the iron material) has zero diffuse component
# by definition, so that pass bakes pure black for every iron part. Route
# each material's Base Color input into a temporary Emission shader instead
# -- that captures the color regardless of Metallic value -- bake as EMIT,
# then restore the normal Principled output for the passes that follow.
def bake_base_color(image):
    rerouted = []
    for mat in all_mats:
        nt = mat.node_tree
        bsdf = nt.nodes.get("Principled BSDF")
        out_node = nt.nodes.get("Material Output")
        old_link = out_node.inputs["Surface"].links[0]
        old_from_socket = old_link.from_socket
        emit = nt.nodes.new("ShaderNodeEmission")
        if bsdf.inputs["Base Color"].is_linked:
            src = bsdf.inputs["Base Color"].links[0].from_socket
            nt.links.new(src, emit.inputs["Color"])
        else:
            emit.inputs["Color"].default_value = bsdf.inputs["Base Color"].default_value
        nt.links.new(emit.outputs["Emission"], out_node.inputs["Surface"])
        node = bake_targets[mat.name]
        node.image = image
        nt.nodes.active = node
        rerouted.append((nt, out_node, old_from_socket, emit))

    bpy.ops.object.bake(type='EMIT', margin=16)

    for nt, out_node, old_from_socket, emit in rerouted:
        nt.links.new(old_from_socket, out_node.inputs["Surface"])
        nt.nodes.remove(emit)

BAKE_PASSES = [
    ("Roughness", 'ROUGHNESS', None, True),
    ("Normal", 'NORMAL', None, True),
    ("AO", 'AO', None, True),
]

base_color_img = bpy.data.images.new("T_HuntingHorn_BaseColor", width=BAKE_RESOLUTION,
                                      height=BAKE_RESOLUTION, alpha=False)
bake_base_color(base_color_img)
base_color_img.filepath_raw = os.path.join(BAKE_DIR, "SM_HuntingHorn_Signal01_BaseColor.png")
base_color_img.file_format = 'PNG'
base_color_img.save()

for map_name, bake_type, pass_filter, is_data in BAKE_PASSES:
    img = bpy.data.images.new(f"T_HuntingHorn_{map_name}", width=BAKE_RESOLUTION,
                               height=BAKE_RESOLUTION, alpha=False)
    if is_data:
        img.colorspace_settings.name = 'Non-Color'
    for mat in all_mats:
        node = bake_targets[mat.name]
        node.image = img
        mat.node_tree.nodes.active = node
    kwargs = dict(type=bake_type, margin=16)
    if pass_filter:
        kwargs["pass_filter"] = pass_filter
    bpy.ops.object.bake(**kwargs)
    img.filepath_raw = os.path.join(BAKE_DIR, f"SM_HuntingHorn_Signal01_{map_name}.png")
    img.file_format = 'PNG'
    img.save()

scene.render.engine = original_engine

# ---------------------------------------------------------------------------
# Preview render -- same camera/light ratios that worked for the sword/lance,
# framed from the mesh's actual bounding box since the horn curves in both
# X and Z rather than running along a single axis.
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

bbox_min = Vector((min(v.co.x for v in mesh.vertices),
                    min(v.co.y for v in mesh.vertices),
                    min(v.co.z for v in mesh.vertices)))
bbox_max = Vector((max(v.co.x for v in mesh.vertices),
                    max(v.co.y for v in mesh.vertices),
                    max(v.co.z for v in mesh.vertices)))
target = (bbox_min + bbox_max) / 2
diag = bbox_max - bbox_min
scale = max(diag.x, diag.y, diag.z) / 0.85  # sword's overall length

cam_data = bpy.data.cameras.new("Camera")
cam_data.lens = 35
cam = bpy.data.objects.new("Camera", cam_data)
scene.collection.objects.link(cam)
cam.location = target + Vector((0.6, -1.3, 0.35)) * scale
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
scene.render.resolution_x = 1400
scene.render.resolution_y = 1100
scene.render.filepath = os.path.join(OUT_DIR, "preview.png")
bpy.ops.render.render(write_still=True)

# ---------------------------------------------------------------------------
# Save .blend source + FBX export (UE convention: -Y forward, Z up)
# ---------------------------------------------------------------------------
blend_path = os.path.join(OUT_DIR, "SM_HuntingHorn_Signal01.blend")
bpy.ops.wm.save_as_mainfile(filepath=blend_path)

fbx_path = os.path.join(OUT_DIR, "SM_HuntingHorn_Signal01.fbx")
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

tri_count = sum(len(p.vertices) - 2 for p in mesh.polygons)
print(f"DONE: {blend_path} {fbx_path} ({len(mesh.polygons)} polys, ~{tri_count} tris)")
