#!/usr/bin/env python3
"""
gs_editor.py - answer "is the Unreal editor actually running?" correctly.

    python tools/gs_editor.py status
    python tools/gs_editor.py require-closed      # exit 2 if it is running - gate a build on this
    python tools/gs_editor.py wait-closed --timeout 180

Written 2026-08-05 after a false positive cost a build.

THE BUG THIS EXISTS TO PREVENT
------------------------------
The obvious check is wrong:

    $p = Get-Process UnrealEditor -ErrorAction SilentlyContinue
    if ($p) { "running" } else { "closed" }          # <-- WRONG

`Get-Process` hands back a cached .NET System.Diagnostics.Process object, and that object can
outlive the process it describes. After the editor exits you can still get an object back, with:

    HasExited  : True
    Threads    : 1
    MainWindowTitle : ''

so the naive truthiness test reports "running" for an editor that has been gone for minutes. That
is exactly what happened: the editor had shut down cleanly (the log ends `LogExit: Exiting`), the
build was aborted as "STILL RUNNING", and the real state had to be dug out by hand.

The fix is not to add `-not $_.HasExited` to the PowerShell one-liner - it is to stop asking .NET
for a cached object at all. `tasklist` queries the live OS process table, so an exited process is
simply absent and there is nothing stale to misread.

WHY IT MATTERS BEYOND THE ONE MISREAD
-------------------------------------
Both failure directions are expensive and neither is obvious from the error:
  - False "running"  -> the build is skipped, and you debug stale binaries.
  - False "closed"   -> UBT refuses with "Unable to build while Live Coding is active", which does
                        not name the editor as the culprit at all.
So `require-closed` also reports the MCP port, because an editor too dead to answer HTTP but still
holding the DLL is its own confusing middle state.
"""

import argparse
import socket
import subprocess
import sys
import time

EDITOR_IMAGE = "UnrealEditor.exe"
MCP_HOST, MCP_PORT = "127.0.0.1", 8000


def editor_pids():
    """PIDs of live UnrealEditor.exe processes, from the OS table - never a cached object."""
    try:
        out = subprocess.run(
            ["tasklist", "/FI", f"IMAGENAME eq {EDITOR_IMAGE}", "/NH", "/FO", "CSV"],
            capture_output=True, text=True, timeout=30,
        ).stdout
    except (OSError, subprocess.SubprocessError) as exc:
        print(f"tasklist failed ({exc}) - cannot determine editor state", file=sys.stderr)
        return None  # unknown, deliberately distinct from "none running"

    pids = []
    for line in out.splitlines():
        # tasklist prints a "INFO: No tasks..." banner rather than nothing when there is no match.
        if not line.startswith('"'):
            continue
        parts = [p.strip('"') for p in line.split('","')]
        if len(parts) >= 2 and parts[0].lower() == EDITOR_IMAGE.lower():
            try:
                pids.append(int(parts[1]))
            except ValueError:
                pass
    return pids


def mcp_listening():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(1.5)
        return s.connect_ex((MCP_HOST, MCP_PORT)) == 0


def status_line():
    pids = editor_pids()
    if pids is None:
        return "UNKNOWN", "could not query the process table"
    if not pids:
        return "CLOSED", "no live UnrealEditor.exe"
    mcp = "MCP listening" if mcp_listening() else "MCP NOT listening (started without -ExecCmds=ModelContextProtocol.StartServer, or still loading)"
    return "RUNNING", f"pid(s) {', '.join(str(p) for p in pids)}; {mcp}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["status", "require-closed", "wait-closed"])
    ap.add_argument("--timeout", type=int, default=180)
    args = ap.parse_args()

    if args.mode == "status":
        state, detail = status_line()
        print(f"EDITOR {state} - {detail}")
        return 0

    if args.mode == "require-closed":
        state, detail = status_line()
        print(f"EDITOR {state} - {detail}")
        if state == "RUNNING":
            print("Refusing: close the editor before building, or UBT will stop with "
                  "'Unable to build while Live Coding is active'.")
            return 2
        # UNKNOWN is treated as closed-enough to try: a failed tasklist should not permanently
        # block a build, and UBT itself will refuse safely if the editor really is up.
        return 0

    deadline = time.time() + args.timeout
    while time.time() < deadline:
        state, detail = status_line()
        if state != "RUNNING":
            print(f"EDITOR {state} - {detail}")
            return 0
        time.sleep(3)

    print(f"TIMEOUT after {args.timeout}s - editor still running")
    return 3


if __name__ == "__main__":
    sys.exit(main())
