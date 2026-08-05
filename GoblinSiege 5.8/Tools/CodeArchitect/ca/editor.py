r"""Editor & build control (Windows / doomsday side).

Michael's ask 2026-08-04: "figure out a way to reopen the editor if it's not
open" + keep the rebuild/relaunch behavior. All knowledge here is ported from
the repo's CLAUDE.md (confirmed on this machine, not assumed):

- Full build (editor CLOSED) is the clean path after any new UCLASS:
    & "D:\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" --%
      MyProjectEditor Win64 Development -Project="...\MyProject.uproject" -WaitMutex
- Live Coding cannot register new UCLASS/UPROPERTY — never rely on it here,
  since the Architect's whole job is new classes.
- BuildAndLaunchGame.ps1 (Plugins/VibeUE/) stops the editor, builds, relaunches.
- Stranded-UBT bug: UBT finishes writing DLLs but never exits -> editor waits
  forever. Detect via a `Launching UnrealBuildTool...` log line with no
  `HotReload took` after it; fix by killing the orphaned dotnet process.

No `$` in any PowerShell we emit (the agent/MCP bridge strips them) and every
path is quoted (two paths in this project contain spaces).
"""
from __future__ import annotations

import subprocess
import sys
import time
from pathlib import Path

from .config import WIN_ENGINE_ROOT, WIN_PROJECT_ROOT, WIN_UPROJECT

EDITOR_EXE = WIN_ENGINE_ROOT + r"\Engine\Binaries\Win64\UnrealEditor.exe"
BUILD_BAT = WIN_ENGINE_ROOT + r"\Engine\Build\BatchFiles\Build.bat"
BUILD_LAUNCH_PS1 = WIN_PROJECT_ROOT + r"\Plugins\VibeUE\BuildAndLaunchGame.ps1"
LOG_FILE = WIN_PROJECT_ROOT + r"\Saved\Logs\MyProject.log"


def _is_windows() -> bool:
    return sys.platform.startswith("win")


def build_command() -> list[str]:
    """Full editor-closed build (the clean path for new UCLASS types)."""
    return ["cmd", "/c", BUILD_BAT, "MyProjectEditor", "Win64", "Development",
            f"-Project={WIN_UPROJECT}", "-WaitMutex"]


def launch_command() -> list[str]:
    return [EDITOR_EXE, WIN_UPROJECT]


def editor_pids() -> list[int]:
    """UnrealEditor processes serving THIS project (cmdline match)."""
    if not _is_windows():
        return []
    out = subprocess.run(
        ["wmic", "process", "where", "name='UnrealEditor.exe'", "get", "ProcessId,CommandLine", "/format:csv"],
        capture_output=True, text=True).stdout
    pids = []
    for line in out.splitlines():
        if "MyProject" in line or "goblinRaid" in line:
            tail = line.rsplit(",", 1)[-1].strip()
            if tail.isdigit():
                pids.append(int(tail))
    return pids


def editor_running() -> bool:
    return bool(editor_pids())


def compiling_now() -> bool:
    """No cl.exe means no compilation, full stop (CLAUDE.md)."""
    if not _is_windows():
        return False
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq cl.exe"], capture_output=True, text=True).stdout
    return "cl.exe" in out


def _dotnet_running() -> bool:
    if not _is_windows():
        return False
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq dotnet.exe"], capture_output=True, text=True).stdout
    return "dotnet.exe" in out


def stranded_ubt() -> bool:
    """The 4-hour bug: a hot-reload's 'Launching UnrealBuildTool...' with no 'HotReload took'
    after it, while an orphaned dotnet still sits around. Two false-positive guards learned on
    first live run (2026-08-04): the editor's STARTUP logs a UBT launch too — but with
    -Mode=ValidatePlatforms, which never HotReloads and must not count — and a log line with no
    surviving dotnet process is history, not a hang."""
    p = Path(LOG_FILE)
    if not p.exists():
        return False
    text = p.read_text(encoding="utf-8", errors="ignore")
    pos = len(text)
    while True:
        last_launch = text.rfind("Launching UnrealBuildTool", 0, pos)
        if last_launch == -1:
            return False
        line_end = text.find("\n", last_launch)
        line = text[last_launch:line_end if line_end != -1 else None]
        if "ValidatePlatforms" not in line:
            break
        pos = last_launch  # startup validation launch — keep looking earlier
    if "HotReload took" in text[last_launch:]:
        return False
    return _dotnet_running()


def kill_stranded_dotnet() -> None:
    if _is_windows():
        subprocess.run(["taskkill", "/IM", "dotnet.exe", "/F"], capture_output=True)


def launch_editor() -> int | None:
    if not _is_windows():
        print(f"[editor] not on Windows — would run: {launch_command()}")
        return None
    proc = subprocess.Popen(launch_command())
    print(f"[editor] launched UnrealEditor pid {proc.pid}")
    return proc.pid


def run_build(require_editor_closed: bool = True) -> int:
    """Editor-closed full build. Refuses (or reports) if the editor is open."""
    if require_editor_closed and editor_running():
        print("[build] editor is OPEN — a full build with new UCLASS types needs it closed. "
              "Use BuildAndLaunchGame.ps1 (stops editor, builds, relaunches) or close it first.")
        return 2
    if not _is_windows():
        print(f"[build] not on Windows — would run: {' '.join(build_command())}")
        return 0
    return subprocess.run(build_command()).returncode


def build_and_relaunch(strict: bool = False, skip_build: bool = False) -> int:
    """The proven path: Plugins/VibeUE/BuildAndLaunchGame.ps1 stops the editor,
    builds, relaunches. Falls back to run_build + launch_editor if absent."""
    if not _is_windows():
        print(f"[build+launch] not on Windows — would run BuildAndLaunchGame.ps1"
              f"{' -StrictRebuild' if strict else ''}{' -SkipBuild' if skip_build else ''}")
        return 0
    if Path(BUILD_LAUNCH_PS1).exists():
        args = ["powershell", "-ExecutionPolicy", "Bypass", "-File", BUILD_LAUNCH_PS1]
        if strict:
            args.append("-StrictRebuild")
        if skip_build:
            args.append("-SkipBuild")
        return subprocess.run(args).returncode
    rc = 0 if skip_build else run_build(require_editor_closed=True)
    if rc == 0 and not editor_running():
        launch_editor()
    return rc


def ensure_editor(wait_seconds: int = 0) -> bool:
    """Michael's ask: reopen the editor if it's not open. Also self-heals the
    stranded-UBT state before deciding the editor is 'fine'."""
    if editor_running():
        if stranded_ubt() and not compiling_now():
            print("[editor] stranded-UBT state detected (UBT log line with no HotReload completion, "
                  "no cl.exe running) — killing orphaned dotnet")
            kill_stranded_dotnet()
        return True
    launch_editor()
    deadline = time.time() + wait_seconds
    while wait_seconds and time.time() < deadline:
        if editor_running():
            return True
        time.sleep(3)
    return editor_running()
