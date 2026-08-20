"""
Procedural tiling rope segment for the GoblinSiege grappling hook (#173).

Run headless:
    "D:\\blender.exe" --background --python build_rope.py

Builds SM_Rope_Segment01, saves a .blend, exports FBX (UE-ready axes),
renders a preview PNG, and writes a UV-layout reference PNG. Pure bmesh
construction (no bpy.ops mesh edits) so it runs reliably with no
window/context, same as build_bolt.py / build_hunting_horn.py.

WHAT MAKES THIS ONE DIFFERENT FROM THE OTHER PROPS
--------------------------------------------------
The weapon scripts end by BAKING their procedural materials down to PNG
maps. This one ships no textures at all: MI_GS_Rope is a FLAT COLOUR
material instance, and the three-strand geometry below does all the work
of making it read as rope.

That is a deliberate reversal of the original plan, and the reason is
worth keeping. Michael asked (2026-08-12) for the rope to reuse the
village well's rope texture for continuity --
    Content/DreamscapeSeries/DreamscapeFarmlands/Textures/Structures/T_Well_C
which does contain rope coiled around the well's windlass. The plan was
to UV-map this mesh onto that coil.

Then the texture was measured: T_Well_C IS 32 x 32 PIXELS. It is a
hand-painted palette atlas -- which is why it looked soft and detailed in
a screenshot; it was magnified enormously. The coil column is roughly two
pixels wide. A UV island that narrow on a shared atlas bleeds its
neighbours under bilinear filtering, and mips make it worse: at mip 2 the
whole atlas is 8 px across and the rope's column is half a pixel, so a
rope seen from across the village would sample stone and timber. The
mitigations (clamp, mip bias, no mips) all live on the SHARED parent
material M_Props_Master and would hit the well and every other Dreamscape
prop.

So the continuity is kept at the PALETTE level instead: MI_GS_Rope's base
colour is picked to match the atlas's rope pixels, which is also how this
art style works -- flat hand-painted colour, silhouette and shading doing
the rest. Zero bleed risk, and nothing shared gets touched.

The UVs below are a clean 0..1 unwrap rather than an atlas island, so if
a dedicated rope texture is ever authored it drops straight on and tiles
in V with no re-unwrap.

Axis convention: the rope runs along local +Z, origin at the bottom end
(Z=0), matching every other long prop in ArtSource (bolt nock-to-tip,
sword grip-to-point, lance, horn). The spline mesh component in UE must
therefore be set to ESplineMeshAxis::Z. Note UGSAimComponent's arc pool
uses X for its segments -- the forward axis is a per-component setting,
so this costs nothing, but do not assume X by copying that code.
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
# Dimensions (meters; 1 m = 100 uu)
# ---------------------------------------------------------------------------
SEGMENT_LENGTH = 0.50       # 50uu. GSRopeComponent derives its segment count
                            # as ceil(RopeLength / 50), so this number and
                            # that one are the same fact -- if you change it
                            # here, it changes there.
ROPE_RADIUS = 0.030         # 3uu radius / 6uu diameter. Real climbing rope is
                            # thinner; this reads at gameplay distance.

RING_SEGS = 12              # faces around the circumference
LEN_SEGS = 16               # faces along the segment. Needs ~8 rows per twist
                            # turn or the helix renders as flat chevrons.
STRANDS = 3                 # three-strand laid rope, which is what rope is
TWIST_TURNS = 2             # MUST be an integer or the segment stops tiling.
                            # 2 turns over 50uu = a 25uu lay length, ~4x the
                            # rope diameter, which is what laid rope looks
                            # like. 1 turn read as a bent cylinder.
LOBE_DEPTH = 0.22           # how far the strands stand proud, as a fraction
                            # of ROPE_RADIUS

# ---------------------------------------------------------------------------
# UVs: a clean full 0..1 unwrap. Circumference -> U, length -> V, so a future
# rope texture tiles along V by repeating the segment. Not an atlas island --
# see the header for why that plan was abandoned.
# ---------------------------------------------------------------------------
U0, U1 = 0.0, 1.0
V0, V1 = 0.0, 1.0

# ---------------------------------------------------------------------------
# Scene reset
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'

# ---------------------------------------------------------------------------
# Material -- preview only. The shipping look comes from MI_GS_Rope in UE,
# which samples T_Well_C. Nothing here is baked or exported as a texture.
# ---------------------------------------------------------------------------
mat = bpy.data.materials.new("M_Rope_Preview")
mat.use_nodes = True
bsdf = mat.node_tree.nodes.get("Principled BSDF")
bsdf.inputs["Base Color"].default_value = (0.44, 0.34, 0.19, 1.0)
bsdf.inputs["Metallic"].default_value = 0.0
bsdf.inputs["Roughness"].default_value = 0.78

# ---------------------------------------------------------------------------
# Geometry
#
# A grid of (RING_SEGS + 1) columns by (LEN_SEGS + 1) rows. The extra column
# duplicates the first one in POSITION but carries u = U1 instead of u = U0 --
# that is the UV seam. Without the duplicate, the last quad's UVs run
# backwards across the whole island and one strip of the rope renders as a
# smeared mirror of the entire atlas column.
# ---------------------------------------------------------------------------
bm = bmesh.new()
uv_layer = bm.loops.layers.uv.new("UVMap")

def surface_point(i, j):
    """Position of grid vertex (i around, j along)."""
    theta = 2.0 * math.pi * (i / RING_SEGS)
    t = j / LEN_SEGS
    z = t * SEGMENT_LENGTH
    # The strand ridges rotate as we travel along the rope. At t = 1 the
    # phase has advanced by exactly TWIST_TURNS full turns, so the ring is
    # congruent with the one at t = 0 and the segment tiles.
    phase = theta - 2.0 * math.pi * TWIST_TURNS * t
    r = ROPE_RADIUS * (1.0 + LOBE_DEPTH * math.cos(STRANDS * phase))
    return Vector((r * math.cos(theta), r * math.sin(theta), z))

def uv_at(i, j):
    u = U0 + (i / RING_SEGS) * (U1 - U0)
    v = V0 + (j / LEN_SEGS) * (V1 - V0)
    return (u, v)

grid = [[bm.verts.new(surface_point(i, j)) for j in range(LEN_SEGS + 1)]
        for i in range(RING_SEGS + 1)]
bm.verts.ensure_lookup_table()

for i in range(RING_SEGS):
    for j in range(LEN_SEGS):
        f = bm.faces.new((grid[i][j], grid[i + 1][j],
                          grid[i + 1][j + 1], grid[i][j + 1]))
        for loop, (gi, gj) in zip(f.loops, ((i, j), (i + 1, j),
                                            (i + 1, j + 1), (i, j + 1))):
            loop[uv_layer].uv = uv_at(gi, gj)

# --- Caps ------------------------------------------------------------------
# Repeated end to end these sit inside the rope and are never seen, but the
# very top and very bottom of a hung rope would otherwise be open tubes.
# Their UVs collapse to the island centre: a flat sample of rope colour,
# which is right for an end-on cut and costs no extra texture space.
CAP_UV = ((U0 + U1) * 0.5, (V0 + V1) * 0.5)

def build_cap(j, flip):
    centre = bm.verts.new(Vector((0.0, 0.0, j / LEN_SEGS * SEGMENT_LENGTH)))
    for i in range(RING_SEGS):
        a, b = grid[i][j], grid[i + 1][j]
        verts = (centre, b, a) if flip else (centre, a, b)
        f = bm.faces.new(verts)
        for loop in f.loops:
            loop[uv_layer].uv = CAP_UV

build_cap(0, flip=True)          # -Z end
build_cap(LEN_SEGS, flip=False)  # +Z end

bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])

mesh = bpy.data.meshes.new("SM_Rope_Segment01")
bm.to_mesh(mesh)
bm.free()
mesh.materials.append(mat)

obj = bpy.data.objects.new("SM_Rope_Segment01", mesh)
scene.collection.objects.link(obj)

# Smooth the round surface, keep the cap rims crisp. Real per-edge sharp
# flags, so the split normals bake into the FBX rather than living in a
# viewport-only modifier.
bpy.context.view_layer.objects.active = obj
obj.select_set(True)
bpy.ops.object.shade_smooth_by_angle(angle=math.radians(50))

tri_count = sum(len(p.vertices) - 2 for p in mesh.polygons)
print("MESH: {0} verts, {1} polys, {2} tris".format(
    len(mesh.vertices), len(mesh.polygons), tri_count))

# ---------------------------------------------------------------------------
# UV layout reference PNG -- rasterized straight from the mesh's UV data.
# bpy.ops.uv.export_layout needs a live Image Editor area, which does not
# exist in --background. Same helper as the crossbow/lance/horn.
#
# Read this one against the atlas: every island edge should sit inside the
# coil column, and the tall direction of the island is the rope's LENGTH.
# ---------------------------------------------------------------------------
def export_uv_layout(mesh_, filepath, size=2048, line_rgba=(0.0, 0.85, 1.0, 1.0)):
    arr = np.zeros((size, size, 4), dtype=np.float32)
    uvs = mesh_.uv_layers.active.data

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
            uv0 = uvs[loop_idx[i]].uv
            uv1 = uvs[loop_idx[(i + 1) % n]].uv
            draw_line(uv0.x * (size - 1), (1.0 - uv0.y) * (size - 1),
                      uv1.x * (size - 1), (1.0 - uv1.y) * (size - 1))

    img = bpy.data.images.new("UVLayout", width=size, height=size, alpha=True)
    img.pixels.foreach_set(arr.ravel())
    img.filepath_raw = filepath
    img.file_format = 'PNG'
    img.save()

uv_path = os.path.join(TEX_DIR, "UV_Layout_Rope.png")
export_uv_layout(mesh, uv_path)
print("WROTE:", uv_path)

# ---------------------------------------------------------------------------
# Preview render -- three segments stacked, because one segment on its own
# cannot show the thing most likely to be wrong. The join between copies is
# what proves the helix closes.
# ---------------------------------------------------------------------------
for k in (1, 2):
    dup = obj.copy()
    dup.data = obj.data
    dup.location = (0.0, 0.0, k * SEGMENT_LENGTH)
    scene.collection.objects.link(dup)

cam_data = bpy.data.cameras.new("PreviewCam")
cam_data.lens = 50
cam = bpy.data.objects.new("PreviewCam", cam_data)
cam.location = (1.45, -1.45, 0.75)
cam.rotation_euler = (math.radians(90), 0.0, math.radians(45))
scene.collection.objects.link(cam)
scene.camera = cam

key_data = bpy.data.lights.new("Key", type='AREA')
key_data.energy = 110
key_data.size = 1.6
key = bpy.data.objects.new("Key", key_data)
key.location = (1.4, -1.6, 2.0)
key.rotation_euler = (math.radians(48), 0.0, math.radians(42))
scene.collection.objects.link(key)

scene.render.engine = 'BLENDER_EEVEE'
scene.render.resolution_x = 600
scene.render.resolution_y = 1000
scene.render.film_transparent = False
# A mid-grey world, not black: the silhouette is half of what this preview is
# for, and a black background hides the rope's outline against the frame edge.
world = bpy.data.worlds.new("PreviewWorld")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.16, 0.16, 0.18, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
scene.world = world
scene.render.filepath = os.path.join(OUT_DIR, "preview.png")
bpy.ops.render.render(write_still=True)
print("WROTE:", scene.render.filepath)

# ---------------------------------------------------------------------------
# Save + export. Only the single segment goes to the FBX -- the two preview
# copies are deleted first so the export is one tiling unit and nothing else.
# ---------------------------------------------------------------------------
for o in [o for o in scene.collection.objects if o.name.startswith("SM_Rope_Segment01.")]:
    bpy.data.objects.remove(o, do_unlink=True)

blend_path = os.path.join(OUT_DIR, "SM_Rope_Segment01.blend")
bpy.ops.wm.save_as_mainfile(filepath=blend_path)

fbx_path = os.path.join(OUT_DIR, "SM_Rope_Segment01.fbx")
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

# ---------------------------------------------------------------------------
# VERIFY -- what a human has to look at, because none of it can be asserted
# from in here.
#
# 1. In the static mesh editor with MI_GS_Rope applied, it must read as ROPE
#    at arm's length: the three strands visible in silhouette, spiralling, not
#    a bent cylinder. preview.png stacks three copies precisely so the join is
#    visible -- if you can see where one segment ends and the next begins, the
#    helix is not closing and TWIST_TURNS has stopped being a whole number.
# 2. Hold it beside the village well in the level. The colours must sit in the
#    same palette. That is the whole point of the flat-colour choice, and it
#    is a judgement only a human eye makes.
# 3. Check it at distance, not just up close -- a 6uu-diameter rope three
#    storeys long is mostly seen from far away, and this is where the flat
#    colour either holds up or reads as plastic.
# ---------------------------------------------------------------------------
