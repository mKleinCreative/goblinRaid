"""
Import SM_Sword_Arming01 into the project and (once you have a rigged human
character) attach a hand socket for it.

This uses Unreal's BUILT-IN Python console -- no VibeUE / API key needed.
Run it from: Tools > Execute Python Script  (or paste into the Python
console under Window > Developer Tools > Output Log).

Usage:
  1. Right now, with no character yet: just run this file as-is. It will
     import the FBX and stop -- SKELETON_PATH is blank so step 2 is skipped.
  2. Once you have a rigged human skeleton in the project: set SKELETON_PATH
     below, run with LIST_BONES_ONLY = True first to print its bone names,
     find the right-hand bone in the printed list, set HAND_BONE_NAME to it,
     set LIST_BONES_ONLY = False, and run again to create the socket.
"""
import unreal

FBX_SOURCE = r"C:\Users\rando\goblinRaid\Art\Weapons\Sword\SM_Sword_Arming01.fbx"
DEST_PATH = "/Game/Weapons/Sword"
MESH_NAME = "SM_Sword_Arming01"

# Fill these in once you have a character (see docstring above).
SKELETON_PATH = ""          # e.g. "/Game/Characters/Humans/KnightDPelegrini/SK_KnightDPelegrini_Skeleton"
HAND_BONE_NAME = ""         # e.g. "hand_r" -- find it via LIST_BONES_ONLY
SOCKET_NAME = "Socket_Weapon_R"
LIST_BONES_ONLY = True


def import_static_mesh():
    dest_obj_path = f"{DEST_PATH}/{MESH_NAME}.{MESH_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(dest_obj_path):
        print("SKIP (already imported):", dest_obj_path)
        return unreal.EditorAssetLibrary.load_asset(dest_obj_path)

    task = unreal.AssetImportTask()
    task.filename = FBX_SOURCE
    task.destination_path = DEST_PATH
    task.destination_name = MESH_NAME
    task.automated = True
    task.save = True
    task.replace_existing = True

    fbx_options = unreal.FbxImportUI()
    fbx_options.import_mesh = True
    fbx_options.import_as_skeletal = False
    fbx_options.static_mesh_import_data.combine_meshes = True
    task.options = fbx_options

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    print("IMPORTED:", dest_obj_path)
    return unreal.EditorAssetLibrary.load_asset(dest_obj_path)


def list_bones(skeleton_path):
    skeleton = unreal.EditorAssetLibrary.load_asset(skeleton_path)
    if not skeleton:
        print("ERROR: could not load skeleton at", skeleton_path)
        return
    names = unreal.SkeletalMeshEditingLibrary.get_bone_names(skeleton) if hasattr(
        unreal, "SkeletalMeshEditingLibrary"
    ) else skeleton.get_editor_property("reference_skeleton").get_raw_bone_names()
    print(f"Bones on {skeleton_path}:")
    for n in names:
        print(" ", n)


def add_socket(skeleton_path, bone_name, socket_name):
    skeleton = unreal.EditorAssetLibrary.load_asset(skeleton_path)
    if not skeleton:
        print("ERROR: could not load skeleton at", skeleton_path)
        return
    socket = unreal.SkeletalMeshSocket(outer=skeleton)
    socket.set_editor_property("socket_name", socket_name)
    socket.set_editor_property("bone_name", bone_name)
    # Sword pivot is already at the grip midpoint with the blade along +Z,
    # so start with an identity offset and nudge rotation in-editor once
    # you can see it in the hand -- grip orientation vs. bone axes varies
    # per rig.
    socket.set_editor_property("relative_location", unreal.Vector(0, 0, 0))
    socket.set_editor_property("relative_rotation", unreal.Rotator(0, 0, 0))
    socket.set_editor_property("relative_scale", unreal.Vector(1, 1, 1))
    sockets = skeleton.get_editor_property("sockets")
    sockets.append(socket)
    skeleton.set_editor_property("sockets", sockets)
    unreal.EditorAssetLibrary.save_loaded_asset(skeleton)
    print(f"ADDED socket '{socket_name}' on bone '{bone_name}' -> {skeleton_path}")


mesh = import_static_mesh()

if SKELETON_PATH:
    if LIST_BONES_ONLY:
        list_bones(SKELETON_PATH)
    elif HAND_BONE_NAME:
        add_socket(SKELETON_PATH, HAND_BONE_NAME, SOCKET_NAME)
    else:
        print("Set HAND_BONE_NAME (see printed bone list) before disabling LIST_BONES_ONLY.")
else:
    print("No SKELETON_PATH set -- import done, skipping socket step. Fill in SKELETON_PATH later.")
