# SUPERSEDED 2026-09-10 (#415) - THIS IS A RESEARCH ARTIFACT, NOT A SHIPPING PATH.
#
# The character was ultimately re-rigged with **AccuRig**, which emits Manny bone naming directly
# and solved in one pass the single step this script does worst: BONE FITTING. AccuRig's Erika
# landed at 180.37 uu against Manny's 180.54, A-pose within 1.6 degrees, zero unweighted verts.
#
# What this script produced, by contrast, passed all eleven of its own assertions and was still
# wrong in six ways - `spine_05` above the top of the head, `spine_04/05` carrying no skin weight at
# all (Manny's most heavily weighted torso bone), twist bones 26 cm off the arm they twist, a twist
# ramp inverted for the upper arms and thighs, orphan verts welded to corrective bones that nothing
# drives, and no physics asset. Its output was deleted.
#
# The durable lesson is in the header below and worth keeping: build FROM the target skeleton rather
# than renaming toward it, and never trust a rig check that only reads names, counts or bounds -
# those stay correct while a rig is broken, because they are all reference-pose measurements.
#
# Use a purpose-built auto-rigger. Do not hand-write bone fitting again.

# Build a Goblin Siege character rig ON Manny's skeleton, by fitting Manny to the character mesh.
#
# WHY THIS REPLACES `conform_to_manny.py` (#415, 2026-09-10). The old script conformed a rig by
# RENAMING Mixamo bones to Manny names. That produced a rig that passed every name-based check and
# was still structurally wrong in four independent ways, each found only by Michael looking at the
# screen:
#
#   1. A phantom `Hips` bone at index 0. Blender's FBX importer absorbs a PARENTLESS ROOT BONE into
#      the armature OBJECT, and the exporter writes that object back out as a node, which Unreal
#      re-imports as a bone. Mixamo's root joint is `Hips` at hip height, so the round trip minted a
#      parentless `Hips` above `root`. `USkeleton::IsCompatibleMesh` (Skeleton.cpp:690-707) walks
#      every mesh bone and requires the name, or an ancestor's name, to exist in the skeleton;
#      `Hips` is bone 0 with no parent, so the walk cannot recover and the function returns false on
#      its FIRST iteration. That is a silent gate: `SkeletalMeshComponent.cpp:957` uses it to decide
#      whether to spawn an anim instance at all, so ACF's own AnimBP attached to NOTHING, with no
#      warning and no error. One bone.
#   2. `root` at Z = 173.81 instead of 0 - the same artifact, since the absorbed object carried
#      Mixamo's hip-height location. Root motion and the ground plane were both off by 1.74 m.
#   3. `pelvis` coincident with `spine_01`. With `Hips` absorbed there was no pelvis to rename, so
#      the old script SYNTHESISED one at `spine_01`'s head. That put the pelvis ~17 cm too high:
#      legs measured 2.01x Manny while the torso measured 1.14x. No uniform rescale fixes a
#      proportion error.
#   4. A 90-degree bone-roll mismatch on the arm chain. Renaming does not touch orientation: our arm
#      bones aimed down local -Y where Manny's aim down local +X, so any rotation authored for Manny
#      drove the wrong axis.
#
# THE FIX IS TO INVERT THE DIRECTION. Start from Manny's actual skeleton and fit it to the character
# mesh, instead of renaming the character's bones toward Manny. Names, hierarchy, bone rolls, twist
# bones, the five spine joints and the seven IK bones are then correct BY CONSTRUCTION, because we
# never author them - we move Manny's.
#
# THE ROUND TRIP BECOMES SELF-CORRECTING. Measured on ACF's own `SKM_Manny` export: Blender absorbs
# its parentless `root` into an armature object named `root` AT THE ORIGIN. Re-exporting emits that
# object as a node and Unreal re-imports it as `root` at index 0, at the origin - which is exactly
# what we want. So defect 1 and defect 2 disappear on their own, provided we DO NOT create a `root`
# bone ourselves and DO NOT move or rename the armature object. Both are asserted below.
#
# USAGE
#   blender.exe --background --factory-startup --python Tools/Rig/build_manny_rig.py -- \
#       <manny.fbx> <character.fbx> <out.fbx> <human|goblin>
#
# Both input FBXs come from Unreal (`unreal.Exporter.run_asset_export_task` on the SkeletalMesh):
# the Manny reference is `/AscentCombatFramework/Animation/SkeletalMeshes/SKM_Manny`, and the
# character is its `_baked` mesh.
#
# WHAT THIS SCRIPT DELIBERATELY DOES NOT DO: it does not delete a single Manny bone. Twists,
# correctives, `weapon_l/r`, `interaction` and `center_of_mass` all survive even though the source
# mesh has no weight for them, because the cost of carrying an unweighted bone is zero and the cost
# of a missing one is a silently disabled anim node. `FAnimNode_StrideWarping::IsValidToEvaluate`
# and `FAnimNode_OrientationWarping::IsValidToEvaluate` both return FALSE WITH NO LOG when a bone
# they need is absent - which is how the project lost stride warping without noticing.

import bpy
import sys
import math
import mathutils
from collections import defaultdict

argv = sys.argv[sys.argv.index("--") + 1:]
if len(argv) < 4:
    raise SystemExit("usage: -- <manny.fbx> <character.fbx> <out.fbx> <human|goblin>")
MANNY_FBX, CHAR_FBX, OUT_FBX, RIG_KIND = argv[0], argv[1], argv[2], argv[3]

# ---------------------------------------------------------------------------------------------
# Source-bone -> Manny-bone mapping.
#
# Carried over verbatim from `conform_to_manny.py`, where it was correct AS A MAPPING - it was the
# rename STRATEGY that was wrong, not the correspondence. Here it says "put Manny's `pelvis` where
# the character's `Hips` joint is", which is the same information used the right way round.
#
# ONE SUBTLETY, measured from the goblin hierarchy: it is `Hip -> {Pelvis -> thighs, Waist ->
# Spine01}`. `Pelvis` parents the LEGS, not the spine. So `Hip` is the pelvis and the goblin's own
# `Pelvis` bone has no Manny counterpart.
# ---------------------------------------------------------------------------------------------

HUMAN = {
    "Hips": "pelvis", "Spine": "spine_01", "Spine1": "spine_02", "Spine2": "spine_03",
    "Neck": "neck_01", "Head": "head",
    "LeftShoulder": "clavicle_l", "LeftArm": "upperarm_l", "LeftForeArm": "lowerarm_l",
    "LeftHand": "hand_l",
    "RightShoulder": "clavicle_r", "RightArm": "upperarm_r", "RightForeArm": "lowerarm_r",
    "RightHand": "hand_r",
    "LeftUpLeg": "thigh_l", "LeftLeg": "calf_l", "LeftFoot": "foot_l", "LeftToeBase": "ball_l",
    "RightUpLeg": "thigh_r", "RightLeg": "calf_r", "RightFoot": "foot_r", "RightToeBase": "ball_r",
}
for _side, _s in (("Left", "l"), ("Right", "r")):
    for _f, _mf in (("Thumb", "thumb"), ("Index", "index"), ("Middle", "middle"),
                    ("Ring", "ring"), ("Pinky", "pinky")):
        for _i in (1, 2, 3):
            HUMAN["%sHand%s%d" % (_side, _f, _i)] = "%s_0%d_%s" % (_mf, _i, _s)

GOBLIN = {
    "Hip": "pelvis", "Waist": "spine_01", "Spine01": "spine_02", "Spine02": "spine_03",
    "NeckTwist01": "neck_01", "NeckTwist02": "neck_02", "Head": "head",
}
for _side, _s in (("L", "l"), ("R", "r")):
    GOBLIN["%s_Clavicle" % _side] = "clavicle_%s" % _s
    GOBLIN["%s_Upperarm" % _side] = "upperarm_%s" % _s
    GOBLIN["%s_Forearm" % _side] = "lowerarm_%s" % _s
    GOBLIN["%s_Hand" % _side] = "hand_%s" % _s
    GOBLIN["%s_Thigh" % _side] = "thigh_%s" % _s
    GOBLIN["%s_Calf" % _side] = "calf_%s" % _s
    GOBLIN["%s_Foot" % _side] = "foot_%s" % _s
    GOBLIN["%s_ToeBase" % _side] = "ball_%s" % _s
    for _n in (1, 2):
        GOBLIN["%s_UpperarmTwist0%d" % (_side, _n)] = "upperarm_twist_0%d_%s" % (_n, _s)
        GOBLIN["%s_ForearmTwist0%d" % (_side, _n)] = "lowerarm_twist_0%d_%s" % (_n, _s)
        GOBLIN["%s_ThighTwist0%d" % (_side, _n)] = "thigh_twist_0%d_%s" % (_n, _s)
        GOBLIN["%s_CalfTwist0%d" % (_side, _n)] = "calf_twist_0%d_%s" % (_n, _s)

SRC_TO_MANNY = HUMAN if RIG_KIND == "human" else GOBLIN

# Fraction of a parent bone's weight handed to its `_twist_01_` child, ramped along the bone so the
# twist takes over toward the far end. ACF's clips key `_twist_01_` (8 tracks per clip, measured);
# they never key `_twist_02_`. With no weight on the twist, that rotation lands nowhere and the
# forearm shears instead of rolling - the "hands look strange" defect.
TWIST_PARENTS = {
    "lowerarm_l": "lowerarm_twist_01_l", "lowerarm_r": "lowerarm_twist_01_r",
    "upperarm_l": "upperarm_twist_01_l", "upperarm_r": "upperarm_twist_01_r",
    "thigh_l": "thigh_twist_01_l", "thigh_r": "thigh_twist_01_r",
    "calf_l": "calf_twist_01_l", "calf_r": "calf_twist_01_r",
}
TWIST_MAX = 0.6


def log(*a):
    print(*a)
    sys.stdout.flush()


def fail(msg):
    raise SystemExit("BUILD FAILED: %s" % msg)


# ---------------------------------------------------------------------------------------------
# Load both rigs into one scene.
# ---------------------------------------------------------------------------------------------

bpy.ops.wm.read_factory_settings(use_empty=True)

bpy.ops.import_scene.fbx(filepath=MANNY_FBX)
manny_arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
manny_arm.name = "MannyRig"
for o in [o for o in bpy.data.objects if o.type == 'MESH']:
    bpy.data.objects.remove(o, do_unlink=True)   # Manny's own mesh is not wanted, only his bones
log("MANNY: %d bones" % len(manny_arm.data.bones))

before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=CHAR_FBX)
new_objs = [o for o in bpy.data.objects if o not in before]
char_arm = next(o for o in new_objs if o.type == 'ARMATURE')
char_meshes = [o for o in new_objs if o.type == 'MESH']
if not char_meshes:
    fail("character FBX contained no mesh")
log("CHARACTER: armature object %r at %s, %d bones, %d mesh(es)"
    % (char_arm.name, tuple(round(c, 3) for c in char_arm.location),
       len(char_arm.data.bones), len(char_meshes)))

# The absorbed root. Blender ate the character's parentless root bone into this object; its NAME is
# the lost bone's name and its LOCATION is that bone's position. For a Mixamo rig that is
# `Hips` at hip height - the exact pair of facts that produced defects 1 and 2.
absorbed_name = char_arm.name
absorbed_loc = char_arm.matrix_world.translation.copy()
log("CHARACTER absorbed root bone: %r at %s"
    % (absorbed_name, tuple(round(c, 3) for c in absorbed_loc)))


# THE TWO FBXs DO NOT SHARE A UNIT SCALE, and mixing them silently produces nonsense. Unreal's
# exporter writes Manny at centimetre scale with an identity armature object, while the character
# arrives with a 0.01-scaled armature object, i.e. metres. Reading `head_local` off one and
# `matrix_world @ head_local` off the other put the fit on a 0.0165 scale factor - caught by the
# assertions on the very first run, which is what they are for. Everything below is resolved into
# MANNY'S ARMATURE SPACE, which is the space the edit bones are actually written in.
TO_MANNY = manny_arm.matrix_world.inverted()


def src_joint(name):
    """Head of a character bone in MANNY'S armature space, including the absorbed root."""
    if name == absorbed_name:
        return TO_MANNY @ absorbed_loc
    b = char_arm.data.bones.get(name)
    if not b:
        return None
    return TO_MANNY @ (char_arm.matrix_world @ b.head_local)


def mesh_to_manny(mesh):
    """Mesh-local -> Manny armature space, for weight maths that compares against bone positions."""
    return TO_MANNY @ mesh.matrix_world


# ---------------------------------------------------------------------------------------------
# Fit. Every Manny bone with a counterpart snaps to the character's joint; every bone without one
# keeps its position RELATIVE TO ITS PARENT, expressed in the parent's own frame and normalised by
# the parent's length, so twists and correctives ride along correctly at the new proportions.
# ---------------------------------------------------------------------------------------------

targets = {}
missing_src = []
for src, manny in SRC_TO_MANNY.items():
    w = src_joint(src)
    if w is None:
        missing_src.append(src)
        continue
    if manny not in manny_arm.data.bones:
        fail("mapping names a Manny bone that does not exist: %s" % manny)
    targets[manny] = w
log("FIT: %d of %d mapped joints resolved%s"
    % (len(targets), len(SRC_TO_MANNY),
       ("; source bones absent: %s" % missing_src) if missing_src else ""))
if len(targets) < 12:
    fail("only %d joints resolved - the mapping does not match this rig" % len(targets))

# Uniform pre-scale so unmapped bones start near the right size. Derived from the mapped joints
# actually present, not from a hardcoded height: the ratio of character span to Manny span.
manny_pts = [manny_arm.data.bones[n].head_local for n in targets]
char_pts = list(targets.values())


def span(points):
    zs = [p.z for p in points]
    return max(zs) - min(zs)


scale = span(char_pts) / span(manny_pts) if span(manny_pts) > 1e-6 else 1.0
log("FIT: uniform pre-scale %.4f (character span %.2f vs Manny span %.2f)"
    % (scale, span(char_pts), span(manny_pts)))

# Capture Manny's rest state before editing: head, tail, and the bone's world Z axis, which is what
# encodes ROLL. Re-applying that axis after moving a bone is what preserves Manny's axis convention
# and keeps defect 4 from coming back.
rest = {}
for b in manny_arm.data.bones:
    rest[b.name] = {
        "head": b.head_local.copy(),
        "tail": b.tail_local.copy(),
        "zaxis": (b.matrix_local.to_3x3() @ mathutils.Vector((0.0, 0.0, 1.0))).normalized(),
        "parent": b.parent.name if b.parent else None,
        "length": b.length,
    }

bpy.context.view_layer.objects.active = manny_arm
bpy.ops.object.mode_set(mode='EDIT')
eb = manny_arm.data.edit_bones

# Resolve heads parent-first so a child can lean on its parent's already-final position.
order = []
seen = set()


def emit(name):
    if name in seen:
        return
    p = rest[name]["parent"]
    if p:
        emit(p)
    seen.add(name)
    order.append(name)


for b in manny_arm.data.bones:
    emit(b.name)

new_head = {}
for name in order:
    if name in targets:
        new_head[name] = targets[name].copy()
        continue
    p = rest[name]["parent"]
    if p is None or p not in new_head:
        new_head[name] = rest[name]["head"] * scale
        continue
    # Offset from the parent's head, in the parent's rest frame, rescaled by how much that parent's
    # own segment changed length. Falls back to the uniform scale when the parent is degenerate.
    off = rest[name]["head"] - rest[p]["head"]
    pl = rest[p]["length"]
    if pl > 1e-6:
        pc = [c for c in rest if rest[c]["parent"] == p and c in new_head and c in targets]
        if pc:
            ratio = (new_head[pc[0]] - new_head[p]).length / max(
                (rest[pc[0]]["head"] - rest[p]["head"]).length, 1e-6)
        else:
            ratio = scale
    else:
        ratio = scale
    new_head[name] = new_head[p] + off * ratio

for name in order:
    e = eb[name]
    h = new_head[name]
    direction = (rest[name]["tail"] - rest[name]["head"])
    if direction.length < 1e-6:
        direction = mathutils.Vector((0.0, 0.0, 1.0))
    # Aim at the primary child where there is one, exactly as Manny does; otherwise keep Manny's
    # direction, rescaled.
    kids = [c for c in rest if rest[c]["parent"] == name]
    aim = None
    for c in kids:
        if c in targets or (rest[c]["head"] - rest[name]["head"]).length > 1e-6:
            if c in targets and name in targets:
                aim = new_head[c] - h
                break
    if aim is None or aim.length < 1e-6:
        aim = direction * scale
    e.head = h
    e.tail = h + aim
    if e.length < 1e-5:
        e.tail = h + direction.normalized() * max(rest[name]["length"] * scale, 0.5)
    e.align_roll(rest[name]["zaxis"])       # restore Manny's roll convention

bpy.ops.object.mode_set(mode='OBJECT')
log("FIT: placed %d bones (%d snapped to character joints, %d derived)"
    % (len(order), len(targets), len(order) - len(targets)))

# BAKE THE MESH INTO MANNY'S ARMATURE SPACE BEFORE ANYTHING ELSE TOUCHES IT.
#
# The character mesh arrives parented to the CHARACTER armature, which carries its own scale (0.01,
# metres) and location. Manny's armature has a different one. Re-parenting with a bare
# `mesh.parent = manny_arm` leaves `matrix_parent_inverse` stale, so the mesh's effective transform
# silently changes: the bones stay right and the GEOMETRY moves. The first version of this script
# did exactly that and shipped a mesh spanning Z -42.75..31.20 against a skeleton whose pelvis is at
# Z 173.81 - about a quarter size and sunk below the origin. On screen: "the rig is underneath the
# map, it's like you're not binding to the right place."
#
# Baking the vertices into the armature's own space and matching the object transform makes
# mesh-local and armature-local the SAME space, so there is no transform left to get wrong, and the
# weight maths below becomes identity.
MW = manny_arm.matrix_world.copy()
for mesh in char_meshes:
    mesh.data.transform(MW.inverted() @ mesh.matrix_world)
    mesh.matrix_world = MW.copy()
log("BAKE: %d mesh(es) transformed into the armature's space" % len(char_meshes))

# ---------------------------------------------------------------------------------------------
# Weights. Rename the character's vertex groups onto Manny names, then bleed a ramped share onto
# the `_twist_01_` bones.
# ---------------------------------------------------------------------------------------------

for mesh in char_meshes:
    # Snapshot the source group names BEFORE renaming, so we can tell which mapped source bones
    # arrived with no vertex group at all - that is how the absorbed root's lost weight is found.
    src_group_names = set(g.name for g in mesh.vertex_groups)
    renamed = 0
    for src, manny in SRC_TO_MANNY.items():
        g = mesh.vertex_groups.get(src)
        if g:
            g.name = manny
            renamed += 1
    # Anything still named after a source bone has no Manny counterpart (Mixamo leaf ends, eyes,
    # the goblin's own `Pelvis`). Fold it into its mapped ancestor rather than dropping the weight.
    manny_names = set(b.name for b in manny_arm.data.bones)
    strays = [g for g in mesh.vertex_groups if g.name not in manny_names]
    folded = defaultdict(int)
    for g in strays:
        host = None
        b = char_arm.data.bones.get(g.name)
        while b is not None and host is None:
            b = b.parent
            if b is not None and SRC_TO_MANNY.get(b.name) in manny_names:
                host = SRC_TO_MANNY[b.name]
        if host is None:
            host = "pelvis"
        hg = mesh.vertex_groups.get(host) or mesh.vertex_groups.new(name=host)
        gi = g.index
        for v in mesh.data.vertices:
            for vg in v.groups:
                if vg.group == gi and vg.weight > 0.0:
                    cur = 0.0
                    for vg2 in v.groups:
                        if vg2.group == hg.index:
                            cur = vg2.weight
                    hg.add([v.index], cur + vg.weight, 'REPLACE')
                    folded[g.name] += 1
        mesh.vertex_groups.remove(g)
    log("WEIGHTS %s: renamed %d group(s); folded %d stray group(s) into their mapped ancestor: %s"
        % (mesh.name, renamed, len(folded), dict(folded)))

    # Twist distribution.
    idx = {g.name: g.index for g in mesh.vertex_groups}
    for parent, twist in TWIST_PARENTS.items():
        if parent not in idx or parent not in rest:
            continue
        tg = mesh.vertex_groups.get(twist) or mesh.vertex_groups.new(name=twist)
        idx[twist] = tg.index
        pb = manny_arm.data.bones.get(parent)
        if not pb or pb.length < 1e-6:
            continue
        head, vec = pb.head_local, (pb.tail_local - pb.head_local)
        L2 = vec.length_squared
        M = mesh_to_manny(mesh)
        moved = 0
        for v in mesh.data.vertices:
            w = 0.0
            for vg in v.groups:
                if vg.group == idx[parent]:
                    w = vg.weight
            if w <= 0.0:
                continue
            t = max(0.0, min(1.0, ((M @ v.co) - head).dot(vec) / L2))
            share = w * TWIST_MAX * t
            if share <= 1e-5:
                continue
            mesh.vertex_groups[parent].add([v.index], w - share, 'REPLACE')
            tg.add([v.index], share, 'ADD')
            moved += 1
        log("WEIGHTS: %-16s -> %-24s %d vertex(es)" % (parent, twist, moved))

    # Orphan rescue. `conform_to_manny.py` shipped with `if total <= 0.0: continue`, written to dodge
    # a divide-by-zero, which silently abandoned every vertex that had no weight at all. Unreal then
    # pins those to bone 0 (`MeshUtilities.cpp:4043`, "Missing influence on vert N"). Rig them here.
    orphans = 0
    M = mesh_to_manny(mesh)
    for v in mesh.data.vertices:
        total = sum(g.weight for g in v.groups)
        if total > 1e-6:
            continue
        p = M @ v.co
        best, best_d = None, None
        for b in manny_arm.data.bones:
            if b.name == "root" or b.name.startswith("ik_"):
                continue
            seg = b.tail_local - b.head_local
            L2 = seg.length_squared
            if L2 < 1e-9:
                dist = (p - b.head_local).length
            else:
                u = max(0.0, min(1.0, (p - b.head_local).dot(seg) / L2))
                dist = (p - (b.head_local + seg * u)).length
            if best_d is None or dist < best_d:
                best, best_d = b.name, dist
        if best:
            g = mesh.vertex_groups.get(best) or mesh.vertex_groups.new(name=best)
            g.add([v.index], 1.0, 'REPLACE')
            orphans += 1
    if orphans:
        log("WEIGHTS: rigged %d orphan vertex(es) to their nearest deform bone" % orphans)

    # RESTORE THE ABSORBED ROOT'S LOST WEIGHT BEFORE NORMALISING. THIS ORDER IS LOAD-BEARING.
    #
    # Blender absorbs the parentless root bone into the armature object (see the header), and it
    # takes that bone's VERTEX GROUP with it. On Erika that is `Hips`: 1,092 vertices arrive holding
    # only partial weight from `Spine` and the two `UpLeg` bones, at a median sum of 0.492.
    #
    # Normalising those scales up the SURVIVORS, which makes hip vertices follow the spine and the
    # thighs at full strength with nothing holding them at the hips. Michael, looking at exactly
    # that on 2026-09-10: "some verts are going a little crazy during the animation and pulling out
    # of the hips." Commit 692d748 fixed it once in the old script; the first draft of THIS script
    # reintroduced it, and the vertex count in the log (1,092, the same number) is what gave it
    # away. Hand each vertex its missing fraction as weight on the absorbed root's Manny
    # counterpart, and only then normalise what is still off.
    host = SRC_TO_MANNY.get(absorbed_name, "pelvis")
    lost = [s for s in SRC_TO_MANNY if s not in src_group_names]
    if lost:
        log("WEIGHTS: source group(s) lost on import: %s -> restoring deficit to %r"
            % (sorted(lost), host))
    hg = mesh.vertex_groups.get(host) or mesh.vertex_groups.new(name=host)
    restored = 0
    zs = []
    M = mesh_to_manny(mesh)
    for v in mesh.data.vertices:
        total = sum(g.weight for g in v.groups)
        if 1e-6 < total < 0.999:
            cur = next((g.weight for g in v.groups if g.group == hg.index), 0.0)
            hg.add([v.index], cur + (1.0 - total), 'REPLACE')
            zs.append((M @ v.co).z)
            restored += 1
    # ALWAYS log the span, pass or fail. On Erika these vertices are the hips and nothing else
    # (Z 131..205 of a mesh running -1..299), which is why sending the deficit to `pelvis` is right.
    # A character whose lost group is somewhere else entirely - a hand, a head - would otherwise
    # have its weight quietly dumped on the pelvis and nobody would know until it deformed on
    # screen. This project has twice shipped a validator that measured nothing; a range that looks
    # wrong is the cheapest possible guard against being the third.
    if restored:
        hb = manny_arm.data.bones.get(host)
        log("WEIGHTS: restored the missing fraction to %r on %d vertex(es), Z %.1f..%.1f "
            "(%r is at Z %.1f)"
            % (host, restored, min(zs), max(zs), host, hb.head_local.z if hb else float('nan')))
    else:
        log("WEIGHTS: no vertex needed its missing fraction restored")

    fixed = 0
    for v in mesh.data.vertices:
        total = sum(g.weight for g in v.groups)
        if total > 1.001:
            for vg in v.groups:
                mesh.vertex_groups[vg.group].add([v.index], vg.weight / total, 'REPLACE')
            fixed += 1
    log("WEIGHTS: renormalised %d over-weighted vertex(es)" % fixed)

    # Rebind the mesh to the new armature.
    for m in list(mesh.modifiers):
        if m.type == 'ARMATURE':
            mesh.modifiers.remove(m)
    mod = mesh.modifiers.new(name="Armature", type='ARMATURE')
    mod.object = manny_arm
    mesh.parent = manny_arm
    # Must be set explicitly. Assigning `.parent` alone leaves a stale parent inverse and moves the
    # geometry out from under the skeleton - see the BAKE note above.
    mesh.matrix_parent_inverse = manny_arm.matrix_world.inverted()
    mesh.matrix_world = MW.copy()

bpy.data.objects.remove(char_arm, do_unlink=True)

# The armature OBJECT becomes bone 0 on the Unreal side. Manny's own round trip names it `root` and
# leaves it at the origin, which is exactly the bone we want; anything else re-creates defect 1.
manny_arm.name = "root"
manny_arm.location = (0.0, 0.0, 0.0)
manny_arm.rotation_euler = (0.0, 0.0, 0.0)
manny_arm.scale = (1.0, 1.0, 1.0)

# ---------------------------------------------------------------------------------------------
# Assert, export, re-import, assert again. The re-import is the point: it is the only way to catch
# what the FBX round trip does to the armature object, which is the defect that started all this.
# ---------------------------------------------------------------------------------------------

MANNY_BONES = set(rest.keys())


def check(arm, meshes, label):
    problems = []
    parentless = [b.name for b in arm.data.bones if b.parent is None]
    if arm.name != "root":
        problems.append("armature object is %r, must be 'root' (it becomes bone 0 in Unreal)" % arm.name)
    if arm.matrix_world.translation.length > 1e-4:
        problems.append("armature object is at %s, must be the origin"
                        % tuple(round(c, 3) for c in arm.matrix_world.translation))
    stray = sorted(b.name for b in arm.data.bones if b.name not in MANNY_BONES)
    if stray:
        problems.append("%d bone(s) outside Manny's set: %s" % (len(stray), stray[:8]))
    pel = arm.data.bones.get("pelvis")
    sp1 = arm.data.bones.get("spine_01")
    if pel and sp1 and (pel.head_local - sp1.head_local).length < 0.5:
        problems.append("pelvis and spine_01 are coincident (%.3f apart) - the #411 proportion bug"
                        % (pel.head_local - sp1.head_local).length)
    for req in ("pelvis", "spine_01", "spine_05", "ik_foot_root", "ik_foot_l", "ik_foot_r",
                "foot_l", "thigh_l", "ik_hand_root", "hand_l"):
        if req not in arm.data.bones:
            problems.append("missing bone required by ACF's anim stack: %s" % req)
    # DOES THE MESH ACTUALLY SIT ON THE SKELETON? Every other check here reads bones or weights, and
    # all of them passed on a build whose geometry was a quarter size and buried below the origin.
    # A rig is a mesh AND a skeleton; checking only one half is how that reached the screen.
    tm = arm.matrix_world.inverted()
    mz = []
    for mesh in meshes:
        M = tm @ mesh.matrix_world
        for v in mesh.data.vertices:
            mz.append((M @ v.co).z)
    bz = [b.head_local.z for b in arm.data.bones]
    if mz:
        m_lo, m_hi = min(mz), max(mz)
        b_lo, b_hi = min(bz), max(bz)
        log("CHECK[%s]: mesh Z %.2f..%.2f vs bone Z %.2f..%.2f" % (label, m_lo, m_hi, b_lo, b_hi))
        if m_lo > b_lo + 15.0 or m_hi < b_hi - 15.0:
            problems.append("mesh (Z %.2f..%.2f) does not enclose the skeleton (Z %.2f..%.2f) - "
                            "the geometry is not bound where the bones are"
                            % (m_lo, m_hi, b_lo, b_hi))
        if abs(m_lo) > 25.0:
            problems.append("mesh floor is at Z %.2f, not the ground - the character will stand "
                            "buried or hovering" % m_lo)

    nw, bad = 0, 0
    for mesh in meshes:
        for v in mesh.data.vertices:
            t = sum(g.weight for g in v.groups)
            if t <= 1e-6:
                nw += 1
            elif abs(t - 1.0) > 1e-3:
                bad += 1
    if nw:
        problems.append("%d vertex(es) with no skin weight" % nw)
    if bad:
        problems.append("%d vertex(es) whose weights do not sum to 1.0" % bad)
    log("CHECK[%s]: object=%r loc=%s bones=%d parentless=%s verts=%d"
        % (label, arm.name, tuple(round(c, 3) for c in arm.matrix_world.translation),
           len(arm.data.bones), parentless, sum(len(m.data.vertices) for m in meshes)))
    return problems


pre = check(manny_arm, char_meshes, "pre-export")
if pre:
    for p in pre:
        log("  PROBLEM: %s" % p)
    fail("%d problem(s) before export" % len(pre))

for o in bpy.data.objects:
    o.select_set(o.type in ('ARMATURE', 'MESH'))
bpy.context.view_layer.objects.active = manny_arm
bpy.ops.export_scene.fbx(filepath=OUT_FBX, use_selection=True, add_leaf_bones=False,
                         bake_anim=False, use_armature_deform_only=False,
                         primary_bone_axis='Y', secondary_bone_axis='X')
log("EXPORTED: %s" % OUT_FBX)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=OUT_FBX)
rt_arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
rt_meshes = [o for o in bpy.data.objects if o.type == 'MESH']
post = check(rt_arm, rt_meshes, "re-import")
if post:
    for p in post:
        log("  PROBLEM: %s" % p)
    fail("%d problem(s) survived the FBX round trip" % len(post))

log("OK: %s is a Manny-contract rig (%d bones)" % (OUT_FBX, len(rt_arm.data.bones)))
