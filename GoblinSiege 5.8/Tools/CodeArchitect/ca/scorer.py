"""Gap detection + utility scoring — the reasoning layer.

Deterministic on purpose. The assignment asks for the *reasoning layer*:
how the agent decides what to build and in what order. Here that layer is
auditable arithmetic over verified perception facts, not a vibe inside a
prompt — the LLM is reserved for code generation, where it earns its keep.

  gap detection:  evidence item -> present/stubbed/missing (from perception)
  utility:        (impact*2 + urgency + 3*unblocked_missing) / effort
  hard gates:     requires_editor  -> never generatable headless
                  depends_on gaps  -> blocked until prerequisite lands
                  block order      -> earlier blocks outrank later ones on ties

Every candidate is scored and logged with its full rationale — the
blackboard shows the ranking BEFORE anything is generated.
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

from .perception import CodebaseReport

W_IMPACT, W_URGENCY, W_UNBLOCKS = 2.0, 1.0, 3.0
BLOCK_TIEBREAK = {b: i for i, b in enumerate("ABCDEFGH")}


@dataclass
class EvidenceResult:
    item: str
    kind: str          # class | symbol | content
    state: str         # present | stubbed | missing | unknown
    detail: str = ""


@dataclass
class Gap:
    key: str
    name: str
    block: str
    gdd_system: str | None
    evidence: list[EvidenceResult]
    missing: list[str]
    stubbed: list[str]
    completeness: float           # fraction of evidence present
    impact: int
    urgency: int
    effort: int
    unblocks: list[str]
    requires_editor: bool
    blocked_by: list[str] = field(default_factory=list)
    utility: float = 0.0
    eligible: bool = False
    rationale: str = ""


def load_features(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _check_evidence(feat: dict, rep: CodebaseReport) -> list[EvidenceResult]:
    out: list[EvidenceResult] = []
    ev = feat.get("evidence", {})
    for cls in ev.get("classes", []):
        st = rep.class_status(cls)
        if st == "wired":
            out.append(EvidenceResult(cls, "class", "present", rep.classes[cls].evidence))
        elif st in ("stubbed", "header-only"):
            out.append(EvidenceResult(cls, "class", "stubbed", rep.classes[cls].evidence))
        else:
            out.append(EvidenceResult(cls, "class", "missing", "no declaration found in Source"))
    for sym in ev.get("symbols", []):
        out.append(EvidenceResult(sym, "symbol", "present" if rep.has_symbol(sym) else "missing",
                                  f"symbol index {'hit' if rep.has_symbol(sym) else 'miss'}"))
    for asset in ev.get("content", []):
        if not rep.content_scanned:
            out.append(EvidenceResult(asset, "content", "unknown", "Content/ not scanned this run"))
        else:
            out.append(EvidenceResult(asset, "content", "present" if rep.has_content(asset) else "missing",
                                      "filename sweep"))
    return out


def detect_and_score(features: dict, rep: CodebaseReport) -> list[Gap]:
    gaps: dict[str, Gap] = {}
    for feat in features["features"]:
        if feat.get("deferred"):
            continue  # tier-2+ content: known, tracked, never scored (GDD marks it DEFERRED)
        ev = _check_evidence(feat, rep)
        known = [e for e in ev if e.state != "unknown"]
        missing = [e.item for e in ev if e.state == "missing"]
        stubbed = [e.item for e in ev if e.state == "stubbed"]
        present = [e for e in known if e.state == "present"]
        completeness = (len(present) / len(known)) if known else 0.0
        gaps[feat["key"]] = Gap(
            key=feat["key"], name=feat["name"], block=feat.get("block", "?"),
            gdd_system=feat.get("gdd_system"), evidence=ev, missing=missing, stubbed=stubbed,
            completeness=round(completeness, 2),
            impact=feat["impact"], urgency=feat["urgency"], effort=feat["effort"],
            unblocks=feat.get("unblocks", []), requires_editor=feat.get("requires_editor", False),
        )

    # score only real gaps (something missing or stubbed)
    for g in gaps.values():
        if not g.missing and not g.stubbed:
            g.rationale = "complete — all evidence present and wired"
            continue
        unblocked_missing = sum(
            1 for k in g.unblocks if k in gaps and (gaps[k].missing or gaps[k].stubbed)
        )
        g.utility = round(
            (W_IMPACT * g.impact + W_URGENCY * g.urgency + W_UNBLOCKS * unblocked_missing)
            / max(1, g.effort), 2)
        feat = next(f for f in features["features"] if f["key"] == g.key)
        g.blocked_by = [
            d for d in feat.get("depends_on", [])
            if d in gaps and gaps[d].completeness < 0.5
        ]
        g.eligible = not g.requires_editor and not g.blocked_by
        why = [f"impact {g.impact}", f"urgency {g.urgency}",
               f"unblocks {unblocked_missing} open feature(s) [{', '.join(g.unblocks) or '-'}]",
               f"effort {g.effort}", f"block {g.block}", f"completeness {g.completeness:.0%}"]
        if g.requires_editor:
            why.append("REQUIRES_EDITOR -> never headless-generatable; route to supervised session")
        if g.blocked_by:
            why.append(f"BLOCKED by {g.blocked_by}")
        g.rationale = "; ".join(why)

    ranked = sorted(
        (g for g in gaps.values() if g.missing or g.stubbed),
        key=lambda g: (-g.utility, BLOCK_TIEBREAK.get(g.block, 99), g.effort),
    )
    complete = [g for g in gaps.values() if not (g.missing or g.stubbed)]
    return ranked + complete


def pick(ranked: list[Gap]) -> Gap | None:
    return next((g for g in ranked if g.eligible and (g.missing or g.stubbed)), None)
