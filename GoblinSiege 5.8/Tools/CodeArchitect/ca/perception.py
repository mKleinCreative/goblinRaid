"""Shared codebase-perception module.

This is the mechanized form of the 2026-07-28 re-baseline method:
every claim about the project is established against the live tree,
never inferred from docs. It is deliberately a standalone module with
no LLM dependency so Bark Foundry (and any other crew) can import it
— closing the S04 audit's "read scene files: not implemented" gap.

Outputs a CodebaseReport:
  - classes: every UCLASS/USTRUCT/UENUM declared in Source/<module>,
    with file, size, and wired/stubbed classification
  - symbols: a searchable concatenation index (symbol -> files)
  - content: data-layer filename sweep (DA_/BT_/GE_/... prefixes)
  - folders: which Source subfolders exist
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass, field, asdict
from pathlib import Path

from .config import Config, CONTENT_PREFIXES, STUB_CPP_MAX_BYTES, STUB_MARKERS

_DECL_RE = re.compile(
    r"\b(?:UCLASS|USTRUCT|UENUM|UINTERFACE)\s*\((?:[^()]|\([^()]*\))*\)\s*"
    r"(?:class|struct|enum\s+class)\s+(?:\w+_API\s+)?([AUFES]?\w+)",
    re.MULTILINE,
)
_PLAIN_DECL_RE = re.compile(
    r"^(?:class|struct|enum\s+class)\s+(?:\w+_API\s+)?([AUFES]\w{2,})\s*[:;{]", re.MULTILINE
)


@dataclass
class ClassInfo:
    name: str
    header: str
    cpp: str | None
    header_bytes: int
    cpp_bytes: int
    status: str            # "wired" | "stubbed" | "header-only"
    evidence: str


@dataclass
class CodebaseReport:
    project_root: str
    source_files: int = 0
    folders: list[str] = field(default_factory=list)
    classes: dict[str, ClassInfo] = field(default_factory=dict)
    symbol_index: dict[str, list[str]] = field(default_factory=dict)
    content_assets: dict[str, list[str]] = field(default_factory=dict)  # prefix -> asset names
    content_scanned: bool = False
    notes: list[str] = field(default_factory=list)

    # ---- queries used by gap detection ----
    def has_class(self, name: str) -> bool:
        return name in self.classes

    def class_status(self, name: str) -> str | None:
        c = self.classes.get(name)
        return c.status if c else None

    def has_symbol(self, sym: str) -> bool:
        return sym in self.symbol_index or any(sym in k for k in self.symbol_index)

    def has_content(self, name_prefix: str) -> bool:
        for assets in self.content_assets.values():
            if any(a.startswith(name_prefix) for a in assets):
                return True
        return False

    def to_json(self) -> str:
        d = asdict(self)
        return json.dumps(d, indent=2, default=str)


def _classify(header: Path, cpp: Path | None) -> tuple[str, str]:
    hb = header.stat().st_size if header.exists() else 0
    if cpp is None or not cpp.exists():
        return "header-only", f"no .cpp beside {header.name}"
    cb = cpp.stat().st_size
    text = cpp.read_text(encoding="utf-8", errors="ignore")
    markers = [m for m in STUB_MARKERS if m in text]
    if cb < STUB_CPP_MAX_BYTES:
        return "stubbed", f"{cpp.name} is {cb}B (< {STUB_CPP_MAX_BYTES}B scaffold threshold)"
    if markers:
        return "stubbed", f"{cpp.name} carries markers: {', '.join(markers[:3])}"
    return "wired", f"{cpp.name} {cb}B, no stub markers"


def scan(cfg: Config) -> CodebaseReport:
    rep = CodebaseReport(project_root=str(cfg.project_root))
    src = cfg.source_dir
    if not src.exists():
        rep.notes.append(f"SOURCE MISSING: {src}")
        return rep

    rep.folders = sorted(p.name for p in src.iterdir() if p.is_dir())

    headers: dict[str, Path] = {}
    for f in sorted(src.rglob("*")):
        if f.suffix not in (".h", ".cpp", ".cs"):
            continue
        rep.source_files += 1
        text = f.read_text(encoding="utf-8", errors="ignore")
        rel = str(f.relative_to(cfg.project_root))
        for m in list(_DECL_RE.finditer(text)) + list(_PLAIN_DECL_RE.finditer(text)):
            rep.symbol_index.setdefault(m.group(1), []).append(rel)
        # cheap symbol index for a few load-bearing non-decl markers
        for sym in ("ReportGSNoise", "Vault", "Mantle", "Climb", "RaidClock", "AlarmPhase",
                    "Takedown", "CoinToss", "RunicSite", "Portal", "Courier", "Score"):
            if sym in text:
                rep.symbol_index.setdefault(sym, []).append(rel)
        if f.suffix == ".h":
            headers[f.stem] = f

    for stem, header in headers.items():
        cpp = header.with_suffix(".cpp")
        text = header.read_text(encoding="utf-8", errors="ignore")
        for m in _DECL_RE.finditer(text):
            name = m.group(1)
            status, why = _classify(header, cpp if cpp.exists() else None)
            rep.classes[name] = ClassInfo(
                name=name,
                header=str(header.relative_to(cfg.project_root)),
                cpp=str(cpp.relative_to(cfg.project_root)) if cpp.exists() else None,
                header_bytes=header.stat().st_size,
                cpp_bytes=cpp.stat().st_size if cpp.exists() else 0,
                status=status,
                evidence=why,
            )

    # ---- content data-layer sweep ----
    cdir = cfg.content_dir
    if cdir.exists():
        rep.content_scanned = True
        for f in [*cdir.rglob("*.uasset"), *cdir.rglob("*.umap")]:
            for p in CONTENT_PREFIXES + ("L_",):
                if f.stem.startswith(p):
                    rep.content_assets.setdefault(p, []).append(f.stem)
                    break
        for p in rep.content_assets:
            rep.content_assets[p] = sorted(set(rep.content_assets[p]))
    else:
        rep.notes.append(f"CONTENT NOT SCANNED (missing at {cdir}) — data-layer facts limited to Source evidence")

    return rep
