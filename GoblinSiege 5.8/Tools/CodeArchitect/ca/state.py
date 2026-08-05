"""AGENT_STATE.md — the single compact resumable memory (Michael's ruling
2026-08-04: distill one state file rather than having the agent read 69 docs).

Format is the lecture's four surfaces: BUILT / DECISIONS / NEXT / FAILED,
plus a RUNS ledger. The agent rewrites NEXT from each run's ranking and
appends to BUILT/RUNS; DECISIONS and FAILED are curated (append-only from
runs, hand-editable by Michael — the file is meant to be edited and fed back).
"""
from __future__ import annotations

import re
import time
from pathlib import Path

TEMPLATE = """# AGENT_STATE — Goblin Siege Code Architect

*Single-file agent memory. Human-editable; the agent reads it at run start and
rewrites NEXT + appends BUILT/RUNS at run end. Deep context lives in the
project docs; this file is the distillation. Format: BUILT / DECISIONS /
NEXT / FAILED / RUNS.*

## BUILT

## DECISIONS

## NEXT

## FAILED

## RUNS
"""


def load(path: Path) -> str:
    if not path.exists():
        path.write_text(TEMPLATE, encoding="utf-8")
    return path.read_text(encoding="utf-8")


def _replace_section(text: str, section: str, body: str) -> str:
    pattern = re.compile(rf"(## {section}\n)(.*?)(?=\n## |\Z)", re.DOTALL)
    return pattern.sub(lambda m: m.group(1) + body.rstrip() + "\n", text)


def _append_section(text: str, section: str, lines: list[str]) -> str:
    pattern = re.compile(rf"(## {section}\n)(.*?)(?=\n## |\Z)", re.DOTALL)

    def _sub(m):
        existing = m.group(2).rstrip()
        add = "\n".join(lines)
        merged = (existing + "\n" + add).strip("\n")
        return m.group(1) + merged + "\n"
    return pattern.sub(_sub, text)


def update_after_run(path: Path, run_id: str, ranked, built_lines: list[str],
                     failed_lines: list[str], summary: str):
    text = load(path)
    stamp = time.strftime("%Y-%m-%d")
    next_body = "\n".join(
        f"- [{'ELIGIBLE' if g.eligible else ('EDITOR' if g.requires_editor else 'BLOCKED')}] "
        f"u={g.utility} **{g.name}** (block {g.block}) — missing: {', '.join(g.missing + g.stubbed) or '-'}"
        for g in ranked if g.missing or g.stubbed
    )
    text = _replace_section(text, "NEXT", next_body + f"\n\n*(ranking from run {run_id}, {stamp})*")
    if built_lines:
        text = _append_section(text, "BUILT", [f"- {stamp} [{run_id}] {ln}" for ln in built_lines])
    if failed_lines:
        text = _append_section(text, "FAILED", [f"- {stamp} [{run_id}] {ln}" for ln in failed_lines])
    text = _append_section(text, "RUNS", [f"- {stamp} **{run_id}** — {summary}"])
    path.write_text(text, encoding="utf-8")
