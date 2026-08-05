"""Runs automatically on editor startup (UE scans Content/Python for init_unreal.py).

Starts the in-editor MCP server so an agent can reach the editor without a human
remembering to type ModelContextProtocol.StartServer in the console. Added 2026-08-02,
after the desktop app's MCP proxy dropped three times in one session and every recovery
needed someone at the keyboard.

The server listens on 127.0.0.1:8000/mcp. D:\\goblinRaid\\gs_run.ps1 talks to it directly.
"""
import unreal

_handle = None


def _start(delta_seconds):
    """Fires on the first slate tick, then unregisters itself.

    Deferred rather than run at import time because the editor world does not exist yet
    when Content/Python is scanned, and execute_console_command needs a world context.
    """
    global _handle
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if world is None:
            return  # not ready yet - try again next tick
        unreal.SystemLibrary.execute_console_command(world, "ModelContextProtocol.StartServer")
        unreal.log("[GoblinSiege] init_unreal: MCP server start requested (127.0.0.1:8000).")
    except Exception as e:
        unreal.log_warning("[GoblinSiege] init_unreal: MCP server start failed: %s" % e)

    if _handle is not None:
        unreal.unregister_slate_post_tick_callback(_handle)
        _handle = None


_handle = unreal.register_slate_post_tick_callback(_start)
