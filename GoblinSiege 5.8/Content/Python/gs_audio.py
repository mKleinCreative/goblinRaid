"""Audition the GoblinSiege audio cues from the console.

Written for ticket #298 (the variation cue layer). Audio cannot be verified by
read-back - two programmatic measurement routes were tried and both returned
nothing (see the plan's "Verification is by ear" section) - so the only real test
is listening, and listening needs a trigger that does not require an agent in the
loop. This is that trigger.

USAGE - type into the console (` key), in PIE or in the editor viewport:

    py import gs_audio          (once per editor session)
    py gs_audio.bark()          one goblin bark
    py gs_audio.horde()         six at once - does it read as a crowd?
    py gs_audio.sampler()       one of each major cue, spread out
    py gs_audio.play("Hit_Flesh")
    py gs_audio.play("Foot_Dirt_Walk", 4)
    py gs_audio.ls()            list every cue name
    py gs_audio.ls("Vox")       list the ones matching a substring

Names are the cue name with the SC_GS_ prefix dropped: "Vox_Gob_Bark" means
/Game/Audio/Cues/SC_GS_Vox_Gob_Bark.

If you hear nothing, check that PIE is running. As of #298 the project sets
[Audio] UnfocusedVolumeMultiplier=1.0 in DefaultEngine.ini, so sound should keep
playing when the window is not focused - but that needs an editor restart to take
effect.
"""

import unreal

CUE_DIR = "/Game/Audio/Cues"
_PREFIX = "SC_GS_"


def _world():
    """PIE world if one is running, else the editor world."""
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return ues.get_game_world() or ues.get_editor_world()


def _cue_paths():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    f = unreal.ARFilter(class_names=["SoundCue"], recursive_paths=True,
                        package_paths=[CUE_DIR])
    return sorted(str(a.package_name) for a in ar.get_assets(f))


def ls(match=""):
    """Print every cue name, optionally filtered by substring (case-insensitive)."""
    names = [p.rsplit("/", 1)[-1] for p in _cue_paths()]
    names = [n for n in names if match.lower() in n.lower()]
    for n in names:
        print("   ", n[len(_PREFIX):] if n.startswith(_PREFIX) else n)
    print("%d cue(s)" % len(names))
    return len(names)


def play(name, times=1):
    """Play a cue by short name. play("Hit_Flesh") or play("Foot_Dirt_Walk", 4)."""
    w = _world()
    if w is None:
        print("gs_audio: no world - is the editor still loading?")
        return False
    path = "%s/%s%s" % (CUE_DIR, _PREFIX, name[len(_PREFIX):] if name.startswith(_PREFIX) else name)
    cue = unreal.load_asset(path)
    if cue is None:
        print("gs_audio: no cue at %s  (try gs_audio.ls())" % path)
        return False
    for _ in range(max(1, int(times))):
        unreal.GameplayStatics.play_sound2d(w, cue, 1.0, 1.0, 0.0)
    print("gs_audio: played %s x%d" % (name, times))
    return True


def bark():
    """One goblin bark. Hit it repeatedly (up-arrow, enter) to judge the pitch spread."""
    return play("Vox_Gob_Bark")


def horde(n=6, laughs=2):
    """A goblin crowd: N barks with a couple of laughs mixed through it.

    SW_Humanoid_Small_1 is a single voice actor, so the per-play pitch
    randomisation is the only thing making this a crowd rather than one goblin
    with an echo. Confirmed by ear 2026-08-24: the barks read as a group.

    The laughs are not decoration. The game is a satire and the horde is meant to
    be enjoying itself - a crowd that only barks reads as wolves, and a couple of
    Glee cues through the same crowd is what makes them goblins. Keep the ratio
    low: laughter is a seasoning, and at parity it stops sounding like a threat.
    Whatever ratio sounds right here is the ratio the horde bark logic should use
    in game.
    """
    play("Vox_Gob_Bark", n)
    if laughs > 0:
        play("Vox_Gob_Glee", laughs)
    return True


def smash(radius=5000.0):
    """Break the nearest breakable actor to the player, to hear gameplay audio fire.

    This goes through the real path - UGSBreakableComponent::Break broadcasts
    OnBroken, the Blueprint's bound event fires, and it plays its own BreakSound at
    its own location. It is not a cue preview: if you hear this, the delegate wiring
    works. Break is idempotent, so each actor answers once per PIE session.
    """
    w = _world()
    if w is None:
        print("gs_audio: no world")
        return False
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    origin = pawn.get_actor_location() if pawn else unreal.Vector(0, 0, 0)
    best, best_d = None, radius
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        comps = [c for c in a.get_components_by_class(unreal.ActorComponent)
                 if type(c).__name__ == "GSBreakableComponent"]
        if not comps:
            continue
        d = (a.get_actor_location() - origin).length()
        if d < best_d:
            best, best_d = (a, comps[0]), d
    if best is None:
        print("gs_audio: no breakable actor within %.0f uu" % radius)
        return False
    actor, comp = best
    loc = actor.get_actor_location()
    comp.break_(loc, unreal.Vector(0, 0, 0))
    snd = actor.get_editor_property("BreakSound") if hasattr(actor, "get_editor_property") else None
    print("gs_audio: broke %s at %.0f uu (BreakSound=%s)" % (
        actor.get_name(), best_d, snd.get_name() if snd else "NONE - not wired"))
    return True


def sampler():
    """One of each major cue, so the whole layer can be heard in a few seconds."""
    for n in ["Foot_Dirt_Walk", "Foot_Grass_Run", "Swing_Light", "Hit_Flesh",
              "Hit_Armor", "Block_Wood", "Bow_Release", "Body_Fall",
              "Vox_Gob_Bark", "Vox_Hum_Death", "Door_Open", "Loot_Coins",
              "Break_Glass", "UI_Objective"]:
        play(n)
    return True
