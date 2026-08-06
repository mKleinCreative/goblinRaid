"""Socket placement preview rig (2026-08-01).

Spawns the goblin in open sky with weapon meshes parked at candidate socket
transforms, then renders orbit views to PNG via a SceneCapture2D so socket
placement can be judged visually instead of guessed.

Everything it spawns carries the tag GS_SOCKET_PREVIEW; call teardown() when done.
Nothing here saves an asset - candidate offsets live on the preview actors until
bake() writes them to the real sockets.
"""
import math
import unreal

TAG = "GS_SOCKET_PREVIEW"
MESH = "/Game/Characters/ScoutV2/GOB_Scout_v2.GOB_Scout_v2"
WPN = "/Game/_Import/Weapons/%s.%s"
OUT = "D:/goblinRaid/GoblinSiege 5.8/Saved/Screenshots/WindowsEditor"
BASE = unreal.Vector(-14200, 59000, 9000)   # open sky, well above the hamlet

EAS = lambda: unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
UES = lambda: unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def _tagged():
    return [a for a in EAS().get_all_level_actors() if TAG in [str(t) for t in a.tags]]


def teardown():
    n = 0
    for a in _tagged():
        EAS().destroy_actor(a)
        n += 1
    return n


def build(pairs):
    """pairs: [(mesh_name, socket_name), ...]"""
    teardown()
    gob = EAS().spawn_actor_from_class(unreal.SkeletalMeshActor, BASE, unreal.Rotator(0, 0, 0))
    gob.tags = [TAG]
    gob.set_actor_label("PREVIEW_Goblin")
    gob.skeletal_mesh_component.set_skeletal_mesh_asset(unreal.load_object(None, MESH))
    for mesh, sock in pairs:
        a = EAS().spawn_actor_from_class(unreal.StaticMeshActor, BASE, unreal.Rotator(0, 0, 0))
        a.tags = [TAG]
        a.set_actor_label("PREVIEW_" + mesh)
        a.static_mesh_component.set_static_mesh(unreal.load_object(None, WPN % (mesh, mesh)))
        a.set_actor_enable_collision(False)
    return gob


def _gob():
    for a in _tagged():
        if a.get_actor_label() == "PREVIEW_Goblin":
            return a
    return None


def place(offsets):
    """offsets: {mesh_name: (socket, (x,y,z), (pitch,yaw,roll))} in BONE space.

    The candidate offset is composed onto the socket's current (unplaced) transform,
    so it reads exactly as it would if baked onto the socket itself.
    """
    smc = _gob().skeletal_mesh_component
    acts = {a.get_actor_label(): a for a in _tagged()}
    for mesh, (sock, loc, rot) in offsets.items():
        a = acts.get("PREVIEW_" + mesh)
        if not a:
            continue
        sockxf = smc.get_socket_transform(sock, unreal.RelativeTransformSpace.RTS_WORLD)
        local = unreal.Transform(unreal.Vector(*loc), unreal.Rotator(rot[0], rot[1], rot[2]), unreal.Vector(1, 1, 1))
        a.set_actor_transform(local.multiply(sockxf), False, False)
    return True


def shoot(name, yaw_offset=0.0, dist=520.0, height=130.0, pitch_deg=-6.0, fov=38.0):
    """Orbit the capture around the goblin. yaw_offset 0 = looking at the character's front."""
    w = UES().get_editor_world()
    for a in _tagged():
        if a.get_actor_label() == "PREVIEW_Capture":
            EAS().destroy_actor(a)
    centre = unreal.Vector(BASE.x, BASE.y, BASE.z + height)
    # character faces +Y in mesh space and the actor is unrotated, so "front" is +Y
    ang = math.radians(90.0 + yaw_offset)
    cam = unreal.Vector(centre.x + dist * math.cos(ang), centre.y + dist * math.sin(ang), centre.z + 40.0)
    d = unreal.Vector(centre.x - cam.x, centre.y - cam.y, centre.z - cam.z)
    yaw = math.degrees(math.atan2(d.y, d.x))
    cap = EAS().spawn_actor_from_class(unreal.SceneCapture2D, cam, unreal.Rotator(0.0, pitch_deg, yaw))
    cap.set_actor_label("PREVIEW_Capture")
    cap.tags = [TAG]
    c = cap.capture_component2d
    rt = unreal.RenderingLibrary.create_render_target2d(w, 1100, 1100, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    c.set_editor_property("texture_target", rt)
    c.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_BASE_COLOR)
    c.set_editor_property("fov_angle", fov)
    c.capture_scene()
    unreal.RenderingLibrary.export_render_target(w, rt, OUT, name)
    return OUT + "/" + name
