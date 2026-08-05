"""GDD parser — extracts the feature/system inventory from the design document.

Parses §12.1's systems-inventory markdown table (| # | System | Status |) and
§12.2's block table. The parse is deliberately structural, not semantic: the
GDD's *status column is treated as a claim, never as truth* — verification
against the codebase belongs to perception + gap detection, mirroring the
project's own re-baseline rule ("checked against the live tree, not inferred
from docs").

Cross-check: every parsed system id must be covered by features.json; drift in
either direction is reported so the knowledge file cannot silently rot.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class GddSystem:
    sys_id: str
    name: str
    claimed_status: str


@dataclass
class GddModel:
    systems: list[GddSystem] = field(default_factory=list)
    blocks: dict[str, str] = field(default_factory=dict)      # "A" -> contents text
    never_cut: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)

    def system(self, sys_id: str) -> GddSystem | None:
        return next((s for s in self.systems if s.sys_id == sys_id), None)


_ROW = re.compile(r"^\|\s*(\d+b?)\s*\|\s*(.+?)\s*\|\s*(.+?)\s*\|\s*$")
_BLOCK_ROW = re.compile(r"^\|\s*\*\*([A-H])\*\*[^|]*\|\s*(.+?)\s*\|\s*(.+?)\s*\|\s*$")


def parse(gdd_path: Path) -> GddModel:
    model = GddModel()
    if not gdd_path.exists():
        model.warnings.append(f"GDD not found at {gdd_path}")
        return model
    text = gdd_path.read_text(encoding="utf-8", errors="ignore")

    in_inventory = False
    for line in text.splitlines():
        if "12.1 Systems inventory" in line:
            in_inventory = True
            continue
        if in_inventory and line.startswith("### "):
            in_inventory = False
        if in_inventory:
            m = _ROW.match(line)
            if m and m.group(1) != "#":
                name = re.sub(r"[*~`]", "", m.group(2)).strip()
                model.systems.append(GddSystem(m.group(1), name, m.group(3)))
        m = _BLOCK_ROW.match(line)
        if m:
            model.blocks[m.group(1)] = m.group(2)

    nc = re.search(r"\*\*Never cut:\*\*(.+)", text)
    if nc:
        model.never_cut = [p.strip(" .*") for p in re.split(r",|·", nc.group(1)) if p.strip()]

    if not model.systems:
        model.warnings.append("No systems-inventory rows parsed from §12.1 — GDD format drifted?")
    return model


def coverage_check(model: GddModel, features: dict) -> list[str]:
    """Both-direction drift check between the GDD table and features.json."""
    warnings = []
    known = {f.get("gdd_system") for f in features["features"] if f.get("gdd_system")}
    for s in model.systems:
        if s.sys_id not in known:
            warnings.append(f"GDD system {s.sys_id} ({s.name}) has no features.json entry — agent is blind to it")
    for f in features["features"]:
        gid = f.get("gdd_system")
        if gid and model.systems and not model.system(gid):
            warnings.append(f"features.json entry '{f['key']}' points at GDD system {gid} which no longer exists")
    return warnings
