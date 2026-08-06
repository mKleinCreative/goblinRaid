"""Per-run blackboard — 'the blackboard is not optional'.

Live visibility of every decision, written BEFORE any file lands anywhere:
  WHAT IT PERCEIVED   the codebase facts this run stands on
  WHAT IT SCORED      every candidate, ranked, with full rationale
  WHAT IT ISSUED      the exact prompts sent to the model
  WHAT IT GENERATED   every file, hash + size, staged (never written to Source/)
  PROMOTED            items flagged for Michael — the agent-side/promotable split

Agent-side by default; anything appended via promote() is additionally
collected into out/PROMOTED.md, the one file a human is expected to read
between runs.
"""
from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path


class Blackboard:
    def __init__(self, out_dir: Path, run_id: str):
        self.dir = Path(out_dir) / "runs" / run_id
        self.dir.mkdir(parents=True, exist_ok=True)
        self.md_path = self.dir / "blackboard.md"
        self.jsonl_path = self.dir / "events.jsonl"
        self.promoted_path = Path(out_dir) / "PROMOTED.md"
        self.run_id = run_id
        self._seq = 0
        self._write_md(f"# Code Architect — run {run_id}\n\n"
                       f"*Every decision below was logged before any file was written anywhere.*\n")

    # ---- primitives ----
    def _write_md(self, text: str):
        with self.md_path.open("a", encoding="utf-8") as f:
            f.write(text + "\n")

    def event(self, kind: str, payload: dict):
        self._seq += 1
        rec = {"seq": self._seq, "t": time.strftime("%Y-%m-%d %H:%M:%S"), "kind": kind, **payload}
        with self.jsonl_path.open("a", encoding="utf-8") as f:
            f.write(json.dumps(rec, default=str) + "\n")
        return rec

    def section(self, title: str):
        self._write_md(f"\n## {title}\n")

    def line(self, text: str):
        self._write_md(text)

    # ---- the four mandated surfaces ----
    def perceived(self, rep):
        self.section("WHAT IT PERCEIVED")
        self.line(f"- Source files scanned: **{rep.source_files}**; folders: {', '.join(rep.folders)}")
        wired = sum(1 for c in rep.classes.values() if c.status == "wired")
        stub = sum(1 for c in rep.classes.values() if c.status != "wired")
        self.line(f"- UCLASS/USTRUCT declarations: **{len(rep.classes)}** ({wired} wired, {stub} stubbed/header-only)")
        if rep.content_scanned:
            counts = {k: len(v) for k, v in sorted(rep.content_assets.items())}
            self.line(f"- Content data-layer sweep: {json.dumps(counts)}")
        for n in rep.notes:
            self.line(f"- NOTE: {n}")
        self.event("perceived", {"classes": len(rep.classes), "files": rep.source_files,
                                 "content_scanned": rep.content_scanned})

    def scored(self, ranked):
        self.section("WHAT IT SCORED")
        self.line("| # | feature | block | utility | eligible | rationale |")
        self.line("|---|---------|-------|---------|----------|-----------|")
        for i, g in enumerate(r for r in ranked if r.missing or r.stubbed):
            elig = "yes" if g.eligible else ("EDITOR" if g.requires_editor else "blocked")
            self.line(f"| {i+1} | {g.name} | {g.block} | {g.utility} | {elig} | {g.rationale} |")
            self.event("scored", {"key": g.key, "utility": g.utility, "eligible": g.eligible,
                                  "missing": g.missing, "stubbed": g.stubbed,
                                  "rationale": g.rationale})

    def issued(self, label: str, model: str, system_prompt: str, user_prompt: str):
        self.section(f"WHAT IT ISSUED — {label}")
        self.line(f"*model:* `{model}`\n")
        self.line("<details><summary>system prompt</summary>\n\n```\n" + system_prompt + "\n```\n</details>\n")
        self.line("<details><summary>user prompt</summary>\n\n```\n" + user_prompt + "\n```\n</details>")
        self.event("issued", {"label": label, "model": model,
                              "system_prompt": system_prompt, "user_prompt": user_prompt})

    def generated(self, files: dict[str, str], notes: str, staging_dir: Path):
        self.section("WHAT IT GENERATED")
        self.line(f"*Staged under `{staging_dir}` — nothing written into Source/ without human review.*\n")
        self.line("| file | bytes | sha1 |")
        self.line("|------|-------|------|")
        for rel, content in files.items():
            h = hashlib.sha1(content.encode()).hexdigest()[:12]
            self.line(f"| {rel} | {len(content.encode())} | `{h}` |")
            self.event("generated", {"file": rel, "bytes": len(content.encode()), "sha1": h})
        if notes:
            self.line("\n**Generator notes / decisions:**\n\n" + notes)

    def promote(self, title: str, body: str):
        """Flag an item for Michael — the only cross-over from agent-side to human-side."""
        self.event("promoted", {"title": title, "body": body})
        with self.promoted_path.open("a", encoding="utf-8") as f:
            f.write(f"\n## [{self.run_id}] {title}\n\n{body}\n")
        self._write_md(f"\n> **PROMOTED to Michael:** {title}")
