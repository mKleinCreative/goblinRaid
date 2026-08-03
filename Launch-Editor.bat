@echo off
REM Launch the editor with the in-editor MCP server already running.
REM
REM Why a .bat and not a one-liner: passing -ExecCmds through the agent/MCP bridge mangles
REM the quote escaping (2026-08-02 - the command line arrived as one malformed argument and
REM the ExecCmds was silently ignored). Quoting inside a file is not touched by anything.
REM
REM The server listens on 127.0.0.1:8000/mcp; D:\goblinRaid\gs_run.ps1 talks to it directly.

start "" "D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\goblinRaid\GoblinSiege 5.8\MyProject.uproject" -ExecCmds="ModelContextProtocol.StartServer"
