"""Code Architect — configuration.

All paths resolve against --project-root so the same code runs on the
Windows repo (D:\\goblinRaid\\GoblinSiege 5.8) and on a staged snapshot
in a cloud workspace. Nothing here reaches the network.
"""
from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path

# Canonical Windows-side locations (from CLAUDE.md — confirmed, not assumed)
WIN_ENGINE_ROOT = r"D:\Epic Games\UE_5.8"
WIN_PROJECT_ROOT = r"D:\goblinRaid\GoblinSiege 5.8"
WIN_UPROJECT = WIN_PROJECT_ROOT + r"\MyProject.uproject"

MODULE_NAME = "GoblinSiege"

# Content filename prefixes that constitute the "data layer" sweep
CONTENT_PREFIXES = ("DA_", "BT_", "BB_", "GE_", "DT_", "GA_", "BP_GS", "IA_", "IMC_")

# Heuristics for "stubbed" classification
STUB_CPP_MAX_BYTES = 1200          # a .cpp this small next to a UCLASS header is scaffold
STUB_MARKERS = ("TODO", "STUB", "NOT IMPLEMENTED", "NotImplemented", "checkNoEntry")


@dataclass
class Config:
    project_root: Path
    provider: str = "anthropic"          # anthropic | fixture
    model: str = os.environ.get("CA_MODEL", "claude-sonnet-4-5")
    run_id: str = ""                     # set by architect.main
    features_path: Path = None           # type: ignore
    gdd_path: Path = None                # type: ignore
    out_dir: Path = None                 # type: ignore
    state_path: Path = None              # type: ignore
    dry_run: bool = False                # never write into Source/ regardless (MVP default: True in CLI)
    scan_only: bool = False
    extra: dict = field(default_factory=dict)

    def __post_init__(self):
        self.project_root = Path(self.project_root)
        tool_root = Path(__file__).resolve().parent.parent
        if self.features_path is None:
            self.features_path = tool_root / "features.json"
        if self.gdd_path is None:
            for cand in (self.project_root / "docs" / "goblin-siege-gdd.md",
                         tool_root / "docs" / "goblin-siege-gdd.md"):
                if cand.exists():
                    self.gdd_path = cand
                    break
            else:
                self.gdd_path = tool_root / "docs" / "goblin-siege-gdd.md"
        if self.out_dir is None:
            self.out_dir = tool_root / "out"
        if self.state_path is None:
            self.state_path = self.project_root / "AGENT_STATE.md"

    @property
    def source_dir(self) -> Path:
        return self.project_root / "Source" / MODULE_NAME

    @property
    def content_dir(self) -> Path:
        return self.project_root / "Content"
