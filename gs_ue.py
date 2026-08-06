#!/usr/bin/env python3
"""
gs_ue.py - run a Python file inside the RUNNING Unreal editor, via the in-editor MCP server.

    python gs_ue.py path\\to\\script.py [--timeout 180]

Requires the editor to be up with its MCP server started - either run
`ModelContextProtocol.StartServer` once in the editor console, or launch the editor with:

    UnrealEditor.exe "D:\\goblinRaid\\GoblinSiege 5.8\\MyProject.uproject" -ExecCmds=ModelContextProtocol.StartServer

Written 2026-08-05 to replace gs_ue.ps1, which could not be made to work reliably. Three separate
transport bugs, each of which presented identically - the call hangs forever with NO output at all,
not even the script's own failure message, which reads exactly like "the editor is busy" and is not:

  1. Accept: text/event-stream.  MCP's Streamable HTTP lets the SERVER choose between a single JSON
     response and an SSE stream, and it picks the stream whenever the client says it accepts one.
     The stream is then held open and the client blocks on it forever. Ask for application/json only.

  2. Invoke-WebRequest.  Even with the Accept header fixed, it still hung on this endpoint under
     Windows PowerShell 5.1, and -TimeoutSec did not fire. curl.exe with identical URL, headers and
     body answered in 0.34 s.

  3. Nested `powershell -File ...`.  Launching the .ps1 as a child powershell.exe hung regardless of
     what the script did internally, so even a curl-based rewrite of it stalled.

Python's urllib has none of these problems: stdlib only, honours its timeout, and json.dumps
escapes arbitrary Python source correctly - which is the one genuinely fiddly part of the job.
"""

import argparse
import json
import sys
import urllib.error
import urllib.request

URL = "http://127.0.0.1:8000/mcp"


def rpc(body, sid=None, timeout=180):
    """POST one JSON-RPC message. Returns (parsed_or_none, session_id_header)."""
    data = json.dumps(body).encode("utf-8")
    req = urllib.request.Request(URL, data=data, method="POST")
    req.add_header("Content-Type", "application/json")
    # application/json ONLY - see bug 1 in the module docstring.
    req.add_header("Accept", "application/json")
    if sid:
        req.add_header("Mcp-Session-Id", sid)

    with urllib.request.urlopen(req, timeout=timeout) as resp:
        raw = resp.read().decode("utf-8", errors="replace")
        out_sid = resp.headers.get("Mcp-Session-Id")

    if not raw.strip():
        return None, out_sid
    try:
        return json.loads(raw), out_sid
    except json.JSONDecodeError:
        return {"_raw": raw}, out_sid


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pyfile")
    ap.add_argument("--timeout", type=int, default=180)
    args = ap.parse_args()

    try:
        with open(args.pyfile, "r", encoding="utf-8") as fh:
            code = fh.read()
    except OSError as exc:
        print(f"PY FILE NOT READABLE: {exc}")
        return 1

    # ---- initialize
    try:
        _, sid = rpc({
            "jsonrpc": "2.0", "id": 1, "method": "initialize",
            "params": {"protocolVersion": "2024-11-05", "capabilities": {},
                       "clientInfo": {"name": "gs-ue", "version": "3"}},
        }, timeout=30)
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        print(f"INIT FAILED: {exc}")
        print("Is the editor running with ModelContextProtocol.StartServer?")
        return 1

    if not sid:
        print("INIT FAILED: no Mcp-Session-Id header in the response.")
        return 1

    # ---- initialized notification (fire and forget; a 202 with an empty body is normal)
    try:
        rpc({"jsonrpc": "2.0", "method": "notifications/initialized"}, sid, timeout=20)
    except Exception:
        pass

    # ---- run the code
    try:
        resp, _ = rpc({
            "jsonrpc": "2.0", "id": 2, "method": "tools/call",
            "params": {"name": "execute_python_code", "arguments": {"code": code}},
        }, sid, timeout=args.timeout)
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        print(f"CALL FAILED: {exc}")
        return 2

    if resp is None:
        print("CALL FAILED: empty response.")
        return 2
    if "_raw" in resp:
        print(resp["_raw"])
        return 0
    if resp.get("error"):
        print("RPC ERROR: " + json.dumps(resp["error"]))
        return 3

    # ---- unwrap: envelope -> content[].text -> the tool's own JSON blob
    failed = False
    for item in resp.get("result", {}).get("content", []):
        text = item.get("text", "")
        try:
            inner = json.loads(text)
        except json.JSONDecodeError:
            print(text)
            continue

        if inner.get("output"):
            print(inner["output"])
        if inner.get("success") is False:
            print("PYTHON FAILED: " + text)
            failed = True

    return 4 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
