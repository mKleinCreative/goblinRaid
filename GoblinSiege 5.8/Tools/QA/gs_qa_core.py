"""
gs_qa_core - shared harness for the Goblin Siege QA tooling (Assignment 9).

Two consumers sit on top of this file:

  gs_qa_agent.py   the adversarial tester - drives the pawn, tries to break things,
                   writes a structured findings report.
  gs_qa_tests.py   the deterministic unit/integration suite - fixed inputs, fixed
                   asserts, readable pass/fail output.

Everything here runs INSIDE the Unreal editor's Python VM. There is no separate
process and no MCP round-trip: a script is handed to the editor with
-ExecutePythonScript, it registers a Slate post-tick callback, and from then on it
owns a per-frame slice of the editor's game thread until it quits.

Why a tick callback and not a plain loop: a `while` loop in editor Python blocks the
game thread, so PIE never ticks and nothing you are testing ever runs. Every routine
in this tooling is therefore a *generator* that yields how long it wants to wait:

    def my_routine(ctx):
        ctx.pawn.jump()
        yield 0.5                 # let half a second pass
        assert_true(ctx.pawn.get_actor_location().z > 0)

`Scheduler` drives those generators one frame at a time. `yield <float>` waits that
many seconds of wall clock; `yield None` waits exactly one frame.

Engine-specific landmines this file already works around (all from
"GoblinSiege 5.8/CLAUDE.md", confirmed on this install, not assumed):
  * unreal.EditorAssetLibrary silently lies -> use EditorAssetSubsystem.
  * StartPIE is asynchronous and returns before the world exists -> poll for it.
  * There is no Python path to a live GAS attribute -> read the BlueprintPure
    accessors the project already exposes (GetHealth / GetStamina / ...).
"""

import csv
import datetime
import json
import os
import time
import traceback

import unreal

# --------------------------------------------------------------------------- paths

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SAVED_DIR = os.path.join(PROJECT_DIR, "Saved")
REPORT_DIR = os.path.join(SAVED_DIR, "QAReports")
LOG_FILE = os.path.join(SAVED_DIR, "Logs", "MyProject.log")

DEFAULT_MAPS = [
    "/Game/Maps/L_Tutorial_Island",
    "/Game/Maps/Test/L_CombatArena",
]


def ensure_report_dir():
    if not os.path.isdir(REPORT_DIR):
        os.makedirs(REPORT_DIR)
    return REPORT_DIR


def stamp():
    return datetime.datetime.now().strftime("%Y%m%d_%H%M%S")


def now_iso():
    return datetime.datetime.now().replace(microsecond=0).isoformat()


# --------------------------------------------------------------------------- output

_LOG_PREFIX = "[GSQA]"


def say(message):
    """One line to the editor log AND to stdout, so the .ps1 launcher can tee it."""
    text = "%s %s" % (_LOG_PREFIX, message)
    unreal.log(text)
    try:
        print(text)
    except Exception:
        pass


def warn(message):
    unreal.log_warning("%s %s" % (_LOG_PREFIX, message))


def fail(message):
    unreal.log_error("%s %s" % (_LOG_PREFIX, message))


# --------------------------------------------------------------------------- config

def cfg(name, default):
    """Read a GSQA_* environment variable. The .ps1 launchers set these; running the
    script by hand from the editor Output Log with none of them set is also fine."""
    raw = os.environ.get("GSQA_" + name)
    if raw is None or raw == "":
        return default
    if isinstance(default, bool):
        return raw.strip().lower() in ("1", "true", "yes", "on")
    if isinstance(default, int):
        try:
            return int(raw)
        except ValueError:
            return default
    if isinstance(default, float):
        try:
            return float(raw)
        except ValueError:
            return default
    if isinstance(default, list):
        return [p.strip() for p in raw.split(",") if p.strip()]
    return raw


# --------------------------------------------------------------------------- engine access

def game_world():
    """The PIE world, or None when PIE is not running."""
    try:
        sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = sub.get_game_world()
        return world if world else None
    except Exception:
        return None


def editor_world():
    try:
        sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        return sub.get_editor_world()
    except Exception:
        return None


def player_pawn(world=None):
    world = world or game_world()
    if not world:
        return None
    try:
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        return pawn if pawn else None
    except Exception:
        return None


def player_controller(world=None):
    world = world or game_world()
    if not world:
        return None
    try:
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        return pc if pc else None
    except Exception:
        return None


def console(command, world=None):
    """Fire a console command at the PIE world (GS.Raid.*, GS.Combat.*, ...)."""
    world = world or game_world()
    if not world:
        return False
    try:
        unreal.SystemLibrary.execute_console_command(world, command)
        return True
    except Exception as exc:
        warn("console '%s' failed: %s" % (command, exc))
        return False


def actors_of_class(klass, world=None):
    world = world or game_world()
    if not world or klass is None:
        return []
    try:
        return list(unreal.GameplayStatics.get_all_actors_of_class(world, klass))
    except Exception:
        return []


def find_class(path):
    """Load a UClass by object path, tolerating the two spellings of a BP class."""
    sub = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    for candidate in (path, path + "_C"):
        try:
            loaded = sub.load_asset(candidate)
        except Exception:
            loaded = None
        if loaded is None:
            continue
        if isinstance(loaded, unreal.Blueprint):
            gen = loaded.get_editor_property("generated_class")
            if gen:
                return gen
        return loaded
    return None


def subsystem_of(world, type_name):
    """Fetch a UWorldSubsystem by unreal.<TypeName>. Returns None if the type is not
    exposed to Python or the subsystem does not exist in this world."""
    world = world or game_world()
    if not world:
        return None
    klass = getattr(unreal, type_name, None)
    if klass is None:
        return None
    # USubsystemBlueprintLibrary is the only route to a UWorldSubsystem from Python.
    # There is no unreal.get_world_subsystem() the way there is for editor/engine
    # subsystems, which is easy to assume and silently returns nothing.
    try:
        found = unreal.SubsystemBlueprintLibrary.get_world_subsystem(world, klass)
        if found:
            return found
    except Exception:
        pass
    for getter in ("get_world_subsystem", "get_subsystem"):
        fn = getattr(unreal, getter, None)
        if fn is None:
            continue
        try:
            found = fn(world, klass)
            if found:
                return found
        except Exception:
            continue
    return None


def enum_value(type_name, member, fallback=0):
    """unreal.<Enum>.<MEMBER>, or the raw int if the enum is not exposed.

    A UFUNCTION taking a uint8 enum will not accept a bare Python int in every
    build, so resolve the real member where we can and keep the int as a fallback.
    """
    klass = getattr(unreal, type_name, None)
    if klass is None:
        return fallback
    value = getattr(klass, member, None)
    return fallback if value is None else value


def hit_actor(hit_result):
    """The actor a line trace hit. FHitResult exposes this through
    BreakHitResult, not as a readable property, in 5.x."""
    if hit_result is None:
        return None
    try:
        broken = unreal.GameplayStatics.break_hit_result(hit_result)
        return broken[9]                # blocking, initial_overlap, time, distance,
                                        # location, impact_point, normal, impact_normal,
                                        # phys_mat, HIT_ACTOR, ...
    except Exception:
        return None


def component_of(actor, prop_name, getter_name=None):
    """A native component on an actor.

    Components like CharacterMovement and CapsuleComponent are reflected as
    PROPERTIES in this build, not as get_*() methods - `pawn.get_character_movement()`
    does not exist and returns nothing, which reads exactly like "the character has no
    movement component". Confirmed the hard way: the first suite run reported
    movement_mode=unknown and "no capsule component" on a pawn that plainly had both.
    """
    if actor is None:
        return None
    if getter_name:
        fn = getattr(actor, getter_name, None)
        if fn is not None:
            try:
                found = fn()
                if found:
                    return found
            except Exception:
                pass
    try:
        found = actor.get_editor_property(prop_name)
        if found:
            return found
    except Exception:
        pass
    return getattr(actor, prop_name, None)


def movement_component(pawn):
    return component_of(pawn, "character_movement", "get_character_movement")


def capsule_component(pawn):
    return component_of(pawn, "capsule_component", "get_capsule_component")


def movement_mode_of(pawn):
    mc = movement_component(pawn)
    if not mc:
        return "unknown"
    try:
        return str(mc.get_editor_property("movement_mode")).upper()
    except Exception:
        return "unknown"


def safe(obj, method, *args, **kwargs):
    """Call a UFUNCTION that may not be exposed in this build. Returns (ok, value).

    Every reflected call in this tooling goes through here. A QA harness that dies
    because one accessor was renamed is worse than useless - it looks like a clean run.
    """
    if obj is None:
        return False, None
    fn = getattr(obj, method, None)
    if fn is None:
        return False, None
    try:
        return True, fn(*args, **kwargs)
    except Exception as exc:
        return False, exc


def get(obj, method, default=None):
    ok, value = safe(obj, method)
    return value if ok and not isinstance(value, Exception) else default


def vec(x, y, z):
    return unreal.Vector(float(x), float(y), float(z))


def dist(a, b):
    return float(((a.x - b.x) ** 2 + (a.y - b.y) ** 2 + (a.z - b.z) ** 2) ** 0.5)


def length(v):
    return float((v.x * v.x + v.y * v.y + v.z * v.z) ** 0.5)


def is_finite(v):
    for c in (v.x, v.y, v.z):
        if c != c or c == float("inf") or c == float("-inf"):
            return False
    return True


def line_trace(world, start, end, ignore=None):
    """(bool hit, HitResult). Visibility channel, simple collision."""
    try:
        return unreal.SystemLibrary.line_trace_single(
            world, start, end,
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
            False, ignore or [],
            unreal.DrawDebugTrace.NONE, True,
            unreal.LinearColor(1, 0, 0, 1), unreal.LinearColor(0, 1, 0, 1), 0.0)
    except Exception:
        return False, None


# --------------------------------------------------------------------------- PIE

class PIE(object):
    """Start / stop Play In Editor and know when the world is actually usable.

    StartPIE on this install is asynchronous - it returns before the world exists
    (CLAUDE.md, Editor Python gotchas). `ready()` is the only trustworthy signal.
    """

    TOOLSET = "EditorToolset.EditorAppToolset"

    @staticmethod
    def _tool(name, args):
        try:
            unreal.ToolsetRegistry.execute_tool(PIE.TOOLSET, name, json.dumps(args))
            return True
        except Exception as exc:
            warn("%s failed: %s" % (name, exc))
            return False

    @staticmethod
    def open_map(map_path):
        try:
            sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
            sub.load_level(map_path)
            return True
        except Exception as exc:
            fail("could not open %s: %s" % (map_path, exc))
            return False

    @staticmethod
    def start():
        if PIE._tool("StartPIE", {"options": {
                "bSimulate": False,
                "playMode": "PlayMode_InViewPort",
                "warmupSeconds": 3.0}}):
            return True
        # Fallback for an install where the MCP toolsets are not loaded.
        try:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_play_simulate()
            return True
        except Exception as exc:
            fail("no way to start PIE: %s" % exc)
            return False

    @staticmethod
    def stop():
        if not PIE._tool("StopPIE", {}):
            try:
                unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            except Exception:
                pass

    @staticmethod
    def ready():
        """True once PIE has a world AND a possessed player pawn with a location."""
        world = game_world()
        if not world:
            return False
        pawn = player_pawn(world)
        if not pawn:
            return False
        try:
            return is_finite(pawn.get_actor_location())
        except Exception:
            return False


# --------------------------------------------------------------------------- log tailing

class LogTail(object):
    """Incremental reader over Saved/Logs/MyProject.log.

    The engine reports a whole class of failure only to the log - ensure()s, Blueprint
    runtime errors, GAS warnings, LogScript call stacks. An agent that only watches
    actor state is blind to all of it, so the QA agent watches the log too and
    attributes each line to whatever behaviour was running when it appeared.
    """

    INTERESTING = (
        "Error:", "Fatal:", "Ensure condition failed", "Assertion failed",
        "LogScript: Warning", "LogScript: Error", "Blueprint Runtime Error",
        "AccessNone", "Infinite loop",
    )

    # Lines that are noise on this project: they fire every run, on a clean editor,
    # and burying a real finding under 400 of them helps nobody.
    IGNORE = (
        "LogDerivedDataCache", "LogShaderCompilers", "LogSlate", "LogAudioMixer",
        "LogVirtualization", "LogHttp", "LogTurnkeySupport", "LogChaosDD",
        "LogPython", "[GSQA]",
    )

    def __init__(self, path=LOG_FILE):
        self.path = path
        self.offset = 0
        self.seek_to_end()

    def seek_to_end(self):
        try:
            self.offset = os.path.getsize(self.path)
        except OSError:
            self.offset = 0

    def read_new(self):
        lines = []
        try:
            size = os.path.getsize(self.path)
        except OSError:
            return lines
        if size < self.offset:          # log rotated
            self.offset = 0
        if size == self.offset:
            return lines
        try:
            with open(self.path, "r", encoding="utf-8", errors="replace") as handle:
                handle.seek(self.offset)
                chunk = handle.read()
                self.offset = handle.tell()
        except OSError:
            return lines
        for line in chunk.splitlines():
            if any(bad in line for bad in self.IGNORE):
                continue
            if any(key in line for key in self.INTERESTING):
                lines.append(line.strip())
        return lines


# --------------------------------------------------------------------------- findings

SEVERITIES = ("critical", "high", "medium", "low")


class Finding(object):
    """One structured, actionable defect record.

    The assignment asks for location / error type / game context. This carries all
    three plus what a developer actually needs to act on it: the detector that fired,
    the evidence it fired on, and a deterministic repro (seed + step + console lines).
    """

    def __init__(self, error_type, severity, summary, detector,
                 location=None, context=None, evidence="", repro=None):
        self.error_type = error_type
        self.severity = severity if severity in SEVERITIES else "medium"
        self.summary = summary
        self.detector = detector
        self.location = location or {}
        self.context = context or {}
        self.evidence = evidence
        self.repro = repro or []
        self.first_seen = now_iso()
        self.count = 1

    def key(self):
        """Dedup key: same defect, same place, same behaviour = one finding with a
        count, not 200 rows. Location is bucketed to 5m so a fuzzer jittering inside
        one broken doorway does not produce a hundred 'distinct' bugs."""
        loc = self.location
        bucket = tuple(int(float(loc.get(axis, 0.0)) // 500.0) for axis in ("x", "y", "z"))
        return (self.error_type, loc.get("map", ""), bucket, self.context.get("behaviour", ""))

    def to_dict(self):
        return {
            "error_type": self.error_type,
            "severity": self.severity,
            "summary": self.summary,
            "detector": self.detector,
            "first_seen": self.first_seen,
            "occurrences": self.count,
            "location": self.location,
            "game_context": self.context,
            "evidence": self.evidence,
            "repro": self.repro,
        }


class FindingLog(object):
    def __init__(self):
        self.by_key = {}
        self.order = []

    def add(self, finding):
        key = finding.key()
        existing = self.by_key.get(key)
        if existing:
            existing.count += 1
            return existing
        self.by_key[key] = finding
        self.order.append(finding)
        say("FINDING [%s] %s - %s" % (finding.severity, finding.error_type, finding.summary))
        return finding

    def __len__(self):
        return len(self.order)

    def sorted(self):
        rank = dict((name, i) for i, name in enumerate(SEVERITIES))
        return sorted(self.order, key=lambda f: (rank.get(f.severity, 9), -f.count))


# --------------------------------------------------------------------------- report writing

def write_json(path, payload):
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=False)
    say("wrote %s" % path)
    return path


def write_text(path, text):
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)
    say("wrote %s" % path)
    return path


CSV_COLUMNS = [
    "severity", "error_type", "summary", "detector", "occurrences",
    "map", "x", "y", "z", "actor",
    "behaviour", "step", "seed",
    "health", "max_health", "stamina", "movement_mode", "speed",
    "is_channelling", "horde_active", "horde_reserve", "raid_ended",
    "evidence", "repro", "first_seen",
]


def write_csv(path, findings):
    """findings are the same dicts Finding.to_dict() produces - the CSV is generated
    from the merged session, not from live objects."""
    with open(path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=CSV_COLUMNS, extrasaction="ignore")
        writer.writeheader()
        for f in findings:
            if not isinstance(f, dict):
                f = f.to_dict()
            row = {
                "severity": f["severity"],
                "error_type": f["error_type"],
                "summary": f["summary"],
                "detector": f["detector"],
                "occurrences": f["occurrences"],
                "evidence": f.get("evidence", ""),
                "repro": " | ".join(f.get("repro", [])),
                "first_seen": f.get("first_seen", ""),
            }
            location = f.get("location", {})
            context = f.get("game_context", {})
            for k in ("map", "x", "y", "z", "actor"):
                row[k] = location.get(k, "")
            for k in ("behaviour", "step", "seed", "health", "max_health", "stamina",
                      "movement_mode", "speed", "is_channelling", "horde_active",
                      "horde_reserve", "raid_ended"):
                row[k] = context.get(k, "")
            writer.writerow(row)
    say("wrote %s" % path)
    return path


# --------------------------------------------------------------------------- scheduler

class StopRoutine(Exception):
    pass


class Scheduler(object):
    """Drives generator routines one editor frame at a time.

    A routine yields:
        None        -> resume next frame
        <float>     -> resume after that many seconds of wall clock
    Raising StopRoutine inside a routine ends it early without an error.
    """

    def __init__(self):
        self.stack = []
        self.resume_at = 0.0

    def push(self, routine):
        self.stack.append(routine)

    def clear(self):
        self.stack = []

    @property
    def busy(self):
        return bool(self.stack)

    def tick(self):
        if not self.stack:
            return
        if time.perf_counter() < self.resume_at:
            return
        routine = self.stack[-1]
        try:
            request = next(routine)
        except (StopIteration, StopRoutine):
            self.stack.pop()
            return
        if isinstance(request, (int, float)) and not isinstance(request, bool):
            self.resume_at = time.perf_counter() + float(request)


# --------------------------------------------------------------------------- driver

class Driver(object):
    """Owns the Slate post-tick callback and the top-level state machine that walks
    a list of maps: open -> PIE -> run -> stop -> next -> report -> quit.

    Subclasses supply `run_map(map_path)` (a generator) and `finish()`.
    """

    PIE_TIMEOUT = 120.0

    def __init__(self, maps, quit_when_done=True):
        self.maps = list(maps)
        self.quit_when_done = quit_when_done
        self.handle = None
        self.sched = Scheduler()
        self.started = time.perf_counter()
        self.aborted = False
        self.finished = False
        # False whenever PIE is absent or tearing down. Anything that touches an actor
        # must check this first - see release_world() for what goes wrong otherwise.
        self.pie_active = False

    # -- lifecycle ---------------------------------------------------------

    def start(self):
        ensure_report_dir()
        self.sched.push(self._main())
        self.handle = unreal.register_slate_post_tick_callback(self._on_tick)
        say("driver armed - %d map(s)" % len(self.maps))

    def _on_tick(self, delta_seconds):
        if self.finished:
            return
        try:
            self.sched.tick()
            if not self.sched.busy:
                self._shutdown()
        except Exception:
            fail("driver crashed:\n%s" % traceback.format_exc())
            self.aborted = True
            self._shutdown()

    def _shutdown(self):
        if self.finished:
            return
        self.finished = True
        if self.handle is not None:
            try:
                unreal.unregister_slate_post_tick_callback(self.handle)
            except Exception:
                pass
            self.handle = None
        try:
            PIE.stop()
        except Exception:
            pass
        try:
            self.finish()
        except Exception:
            fail("finish() crashed:\n%s" % traceback.format_exc())
        if self.quit_when_done:
            say("quitting editor")
            try:
                unreal.SystemLibrary.quit_editor()
            except Exception as exc:
                warn("quit_editor failed: %s" % exc)

    # -- top level ---------------------------------------------------------

    def _main(self):
        yield 5.0                       # let the editor settle after startup
        for map_path in self.maps:
            say("=== map %s" % map_path)
            if not PIE.open_map(map_path):
                continue
            yield 3.0
            PIE.start()
            waited = 0.0
            while not PIE.ready() and waited < self.PIE_TIMEOUT:
                yield 0.5
                waited += 0.5
            if not PIE.ready():
                fail("PIE never came up on %s after %.0fs - skipping" % (map_path, waited))
                PIE.stop()
                yield 3.0
                continue
            say("PIE up on %s after %.1fs" % (map_path, waited))
            self.pie_active = True
            # A couple of seconds of settle: BeginPlay, GAS init and the raid director
            # all land on the first frames, and measuring during that is noise.
            yield 2.0
            for step in self.run_map(map_path):
                yield step
            # Write what we have BEFORE tearing PIE down. The teardown can take the
            # editor with it (an access violation inside python311.dll during the
            # reference collector's pass), and a report that only exists after a
            # successful teardown is a report you lose exactly when the run got
            # interesting. Every finish() here is idempotent and overwrites in place.
            self.safe_finish()
            self.release_world()
            PIE.stop()
            yield 3.0

    # -- to override -------------------------------------------------------

    def safe_finish(self):
        try:
            self.finish()
        except Exception:
            fail("finish() crashed: %s" % traceback.format_exc())

    def release_world(self):
        """Drop every reference to a PIE actor before PIE is torn down.

        This is not tidiness. A Python wrapper that outlives the UObject it points at
        gets visited by the reference collector during PIE teardown and dereferences
        freed memory: the editor died with EXCEPTION_ACCESS_VIOLATION inside
        python311.dll, one frame after "Shutting down PIE online subsystems", on the
        very first run that got far enough to try a second map. Subclasses override
        this to null their own handles; the base clears the flag and collects.
        """
        self.pie_active = False
        try:
            import gc
            gc.collect()
        except Exception:
            pass

    def run_map(self, map_path):
        raise NotImplementedError
        yield  # pragma: no cover

    def finish(self):
        pass


# --------------------------------------------------------------------------- re-entrancy

def claim_singleton(tag):
    """True the first time, False on every re-entrant launch in this editor process.

    -ExecCmds is re-fired by the editor on PIE start, and this tooling starts PIE
    itself - so without this guard the script re-arms a second driver, which opens the
    map, which starts PIE, which re-fires -ExecCmds, forever. Observed live: six full
    passes of the suite in nine minutes, none of which ever reached its own report.
    """
    import builtins
    key = "_GSQA_CLAIMED_" + tag
    if getattr(builtins, key, False):
        return False
    setattr(builtins, key, True)
    return True


# --------------------------------------------------------------------------- sessions

def session_id():
    """Identifier shared by every editor launch that belongs to one logical run.

    The launchers start ONE editor PER MAP. That is not an accident: tearing PIE down
    mid-run has repeatedly taken the editor with it (an access violation inside
    python311.dll during the reference collector's pass over Python-held UObjects), and
    the crash reporter then RELAUNCHES the editor, which re-arms the script, which opens
    a map, which starts PIE... A launch that plays one map and then exits never performs
    a mid-run teardown at all. The session id is what stitches the per-map launches back
    into a single report.
    """
    return cfg("SESSION", "solo")


def session_path(kind):
    return os.path.join(ensure_report_dir(), "_session_%s_%s.json" % (kind, session_id()))


def session_load(kind):
    path = session_path(kind)
    if not os.path.isfile(path):
        return {}
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except Exception:
        return {}


def session_save(kind, payload):
    path = session_path(kind)
    try:
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(payload, handle)
    except Exception as exc:
        warn("could not write the session file: %s" % exc)
    return path
