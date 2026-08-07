#!/usr/bin/env python3
"""
Goblin Siege — goal-oriented coding agent.

Reads the GDD, scans the C++ codebase, works out which GDD features are missing,
argues for one of them, and writes the code.

    python gsagent.py scan-gdd     # 1. what does the design document require?
    python gsagent.py scan-code    # 2. what exists? (static, no API calls, free)
    python gsagent.py gaps         # 3. per feature: implemented / partial / missing
    python gsagent.py plan         # 4. rank the missing work and pick one
    python gsagent.py generate     # 5. write the code for the pick
    python gsagent.py run          # all five, in order

Every stage writes its output to out/ as JSON, so a later stage can be re-run
without repeating the earlier ones, and so every decision is inspectable.

The agent deliberately does NOT compile. GoblinSiege's CLAUDE.md constraint 1:
cloud agent sessions cannot execute on the dev machine — builds are a human step.
The agent instead files a queue ticket and prints the exact build command.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
from collections import Counter
from dataclasses import dataclass
from pathlib import Path

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
GDD = REPO / "goblin-siege-design-document.md"
SRC = REPO / "GoblinSiege 5.8" / "Source" / "GoblinSiege"
OUT = ROOT / "out"
GEN = OUT / "generated"

MODEL = "claude-opus-5"
MAX_FEATURES = 14

USAGE = Counter()


# =============================================================================
# Credentials + Claude plumbing
# =============================================================================

DOTENV = [ROOT / ".env", REPO / ".env", REPO / "GoblinSiege 5.8" / ".env",
          REPO / "content-pipeline" / ".env"]


def load_dotenv() -> None:
    for p in DOTENV:
        if not p.exists():
            continue
        for line in p.read_text(encoding="utf-8-sig").splitlines():
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                os.environ.setdefault(k.strip(), v.strip().strip("'\""))
        return


def _transient(exc: BaseException) -> bool:
    import anthropic
    import httpx
    return isinstance(exc, (anthropic.APIConnectionError, anthropic.APITimeoutError,
                            anthropic.RateLimitError, anthropic.InternalServerError,
                            httpx.RemoteProtocolError, httpx.ReadError, httpx.ReadTimeout))


def ask(system: str, user: str, schema: dict, effort: str = "high",
        max_tokens: int = 32000) -> dict:
    """One structured-output call. Streams, retries transient stream drops."""
    import anthropic

    client = anthropic.Anthropic()
    for attempt in range(4):
        try:
            with client.messages.stream(
                model=MODEL,
                max_tokens=max_tokens,
                system=system,
                messages=[{"role": "user", "content": user}],
                thinking={"type": "adaptive"},
                output_config={"effort": effort,
                               "format": {"type": "json_schema", "schema": schema}},
            ) as stream:
                msg = stream.get_final_message()
            break
        except Exception as exc:  # noqa: BLE001
            if _transient(exc) and attempt < 3:
                d = 2 ** (attempt + 1)
                print(f"    transient {type(exc).__name__}; retry in {d}s")
                time.sleep(d)
                continue
            raise

    if msg.stop_reason == "refusal":
        raise RuntimeError(f"model declined: {msg.stop_details}")
    USAGE["in"] += msg.usage.input_tokens
    USAGE["out"] += msg.usage.output_tokens
    USAGE["calls"] += 1

    text = next(b.text for b in msg.content if b.type == "text")
    if msg.stop_reason == "max_tokens":
        # Truncated JSON parses as a cryptic "Unterminated string". Say what actually
        # happened, and keep the partial output so the run is not simply lost.
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "truncated_response.txt").write_text(text, encoding="utf-8")
        raise RuntimeError(
            f"response hit the {max_tokens}-token output cap and was truncated "
            f"({msg.usage.output_tokens} output tokens). Partial output saved to "
            f"out/truncated_response.txt. Raise max_tokens or narrow the request."
        )
    try:
        return json.loads(text)
    except json.JSONDecodeError as exc:
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "bad_response.txt").write_text(text, encoding="utf-8")
        raise RuntimeError(
            f"model returned unparseable JSON ({exc}); saved to out/bad_response.txt"
        ) from exc


def save(name: str, data) -> Path:
    OUT.mkdir(parents=True, exist_ok=True)
    p = OUT / name
    p.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")
    return p


def need(name: str):
    p = OUT / name
    if not p.exists():
        sys.exit(f"missing {p.name} — run the earlier stage first")
    return json.loads(p.read_text(encoding="utf-8"))


# =============================================================================
# STAGE 1 — read the GDD
# =============================================================================

def split_gdd() -> list[dict]:
    """Heading-scoped sections, so a feature can cite the section it came from."""
    lines = GDD.read_text(encoding="utf-8-sig").splitlines()
    stack: list[str] = []
    out, buf = [], []

    def flush():
        if buf and any(l.strip() for l in buf):
            out.append({"heading": " > ".join(stack), "text": "\n".join(buf).strip()})
        buf.clear()

    for ln in lines:
        m = re.match(r"^(#{1,3})\s+(.*)$", ln)
        if m:
            flush()
            d = len(m.group(1))
            stack = stack[: d - 1]
            while len(stack) < d - 1:
                stack.append("(untitled)")
            stack.append(m.group(2).strip().replace("**", ""))
        else:
            buf.append(ln)
    flush()
    return out


FEATURE_SCHEMA = {
    "type": "object",
    "properties": {
        "features": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "id": {"type": "string", "description": "short snake_case id"},
                    "name": {"type": "string"},
                    "gdd_section": {"type": "string", "description": "e.g. 2.6"},
                    "player_behavior": {
                        "type": "string",
                        "description": "what the player sees or does, one or two sentences",
                    },
                    "acceptance_signals": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "concrete things that would exist in a UE C++ codebase "
                                       "if this were built — class/component/subsystem roles, "
                                       "not filenames",
                    },
                    "keywords": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "identifier-ish words to search the codebase for",
                    },
                    "never_cut": {
                        "type": "boolean",
                        "description": "true if 4.5 lists it under 'Never cut'",
                    },
                    "cut_order_rank": {
                        "type": "integer",
                        "description": "position in 4.5's pre-committed cut order, 1 = first to "
                                       "be cut; 0 if not in the cut order",
                    },
                    "schedule_week": {
                        "type": "integer",
                        "description": "week from 4.5's schedule, 0 if unscheduled",
                    },
                },
                "required": ["id", "name", "gdd_section", "player_behavior",
                             "acceptance_signals", "keywords", "never_cut",
                             "cut_order_rank", "schedule_week"],
                "additionalProperties": False,
            },
        }
    },
    "required": ["features"],
    "additionalProperties": False,
}


def stage_scan_gdd() -> None:
    secs = split_gdd()
    mech = [s for s in secs if s["heading"].startswith(("2.", "# 2", "2 ")) or "Game Mechanics" in s["heading"]]
    sched = [s for s in secs if "4.5" in s["heading"] or "Schedule" in s["heading"]]
    body = "\n\n".join(f"## {s['heading']}\n{s['text']}" for s in (mech + sched))

    print(f"  GDD: {len(secs)} sections, feeding {len(mech + sched)} to extraction")
    data = ask(
        "You extract an implementable feature list from a game design document. You are precise "
        "about what the document actually says and you never invent a requirement it does not "
        "state. Prefer player-facing mechanics that a programmer would build as a distinct "
        "system, not tone or scope commentary.",
        f"""Extract the implementable features this design document requires, at most {MAX_FEATURES},
covering the game's mechanics broadly rather than many variations of one system.

For each, give acceptance_signals: the concrete things that would exist in an Unreal Engine 5 C++
codebase if the feature were actually built (a subsystem that owns X, a component that does Y, a
data-driven table of Z). These are what a code scanner will look for, so make them about roles and
responsibilities, not guessed file names.

Also record the scheduling metadata the document states about each feature: whether section 4.5
lists it under "Never cut", its position in 4.5's pre-committed cut order if present, and the week
of 4.5's schedule it belongs to.

DESIGN DOCUMENT:

{body}""",
        FEATURE_SCHEMA,
    )
    feats = data["features"]
    save("features.json", feats)
    print(f"  extracted {len(feats)} features")
    for f in feats:
        flag = " [never-cut]" if f["never_cut"] else ""
        print(f"    §{f['gdd_section']:<5} {f['name']}{flag}")


# =============================================================================
# STAGE 2 — scan the codebase (static; no API calls)
# =============================================================================

RE_CLASS = re.compile(r"\bclass\s+(?:GOBLINSIEGE_API\s+)?([AUIF][A-Za-z0-9_]+)\s*(?::|$)", re.M)
RE_STRUCT = re.compile(r"\bstruct\s+(?:GOBLINSIEGE_API\s+)?(F[A-Za-z0-9_]+)\s*(?::|\{|$)", re.M)
RE_ENUM = re.compile(r"\benum\s+class\s+(E[A-Za-z0-9_]+)")
RE_UFUNC = re.compile(r"UFUNCTION\([^)]*\)\s*(?:virtual\s+)?[\w:<>,\s\*&]+?\b(\w+)\s*\(", re.S)
RE_TAG = re.compile(r'"(GS\.[A-Za-z0-9_.]+)"')
RE_SUBSYS = re.compile(r"public\s+U(?:World|GameInstance|LocalPlayer|EngineSubsystem)Subsystem")


def stage_scan_code() -> None:
    if not SRC.exists():
        sys.exit(f"source tree not found: {SRC}")
    files, symbols, tags = [], {}, set()
    for p in sorted(SRC.rglob("*")):
        if p.suffix.lower() not in (".h", ".cpp"):
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        rel = str(p.relative_to(SRC)).replace("\\", "/")
        classes = RE_CLASS.findall(text)
        entry = {
            "path": rel,
            "lines": text.count("\n") + 1,
            "classes": sorted(set(classes)),
            "structs": sorted(set(RE_STRUCT.findall(text))),
            "enums": sorted(set(RE_ENUM.findall(text))),
            "ufunctions": sorted(set(RE_UFUNC.findall(text)))[:40],
            "tags": sorted(set(RE_TAG.findall(text))),
            "is_subsystem": bool(RE_SUBSYS.search(text)),
        }
        files.append(entry)
        for s in entry["classes"] + entry["structs"] + entry["enums"]:
            symbols.setdefault(s, rel)
        tags.update(entry["tags"])

    index = {
        "root": str(SRC),
        "file_count": len(files),
        "symbol_count": len(symbols),
        "tags": sorted(tags),
        "files": files,
        "symbols": symbols,
    }
    save("codebase.json", index)
    print(f"  {len(files)} files, {len(symbols)} symbols, {len(tags)} gameplay tags")
    print(f"  subsystems: {sum(1 for f in files if f['is_subsystem'])}")


def search_code(index: dict, keywords: list[str], k: int = 6) -> list[dict]:
    """
    Rank files for a feature. Symbol and path matches score far above body text —
    a raw text grep for 'bind' hits every delegate binding in the project, which is
    exactly the false positive a gap detector must not fall for.
    """
    kws = [k.lower() for k in keywords if len(k) > 2]
    scored = []
    for f in index["files"]:
        score = 0
        blob_sym = " ".join(f["classes"] + f["structs"] + f["enums"] + f["ufunctions"]).lower()
        blob_path = f["path"].lower()
        blob_tag = " ".join(f["tags"]).lower()
        for kw in kws:
            if kw in blob_path:
                score += 6
            if kw in blob_sym:
                score += 4
            if kw in blob_tag:
                score += 3
        if score:
            scored.append((score, f))
    scored.sort(key=lambda t: -t[0])
    return [{"path": f["path"], "score": s, "classes": f["classes"],
             "structs": f["structs"], "enums": f["enums"], "tags": f["tags"]}
            for s, f in scored[:k]]


# =============================================================================
# STAGE 3 — detect gaps
# =============================================================================

GAP_SCHEMA = {
    "type": "object",
    "properties": {
        "verdict": {"type": "string", "enum": ["IMPLEMENTED", "PARTIAL", "MISSING"]},
        "confidence": {"type": "string", "enum": ["high", "medium", "low"]},
        "evidence": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "symbol_or_path": {"type": "string"},
                    "supports": {"type": "string",
                                 "description": "which acceptance signal this covers"},
                },
                "required": ["symbol_or_path", "supports"],
                "additionalProperties": False,
            },
        },
        "missing_pieces": {"type": "array", "items": {"type": "string"}},
        "reasoning": {"type": "string"},
    },
    "required": ["verdict", "confidence", "evidence", "missing_pieces", "reasoning"],
    "additionalProperties": False,
}

GAP_SYSTEM = """You decide whether a game feature is present in a C++ codebase, from a symbol index.

You are given every class, struct, enum and gameplay tag in the project, plus the files that
best match the feature's keywords. Judge only from that evidence.

Rules:
- Cite a real symbol or path for every claim of presence. An acceptance signal you cannot tie to
  a symbol is NOT covered.
- A keyword appearing inside unrelated code is not evidence. `Bind` matches every delegate
  binding; `Horn` may be a mesh name. Require a symbol whose *role* matches.
- IMPLEMENTED means every acceptance signal has a symbol behind it. PARTIAL means some do.
  MISSING means essentially none do.
- Absence of evidence in a complete symbol index IS evidence of absence — say so plainly and use
  high confidence when the index is complete and nothing matches."""


def stage_gaps() -> None:
    feats, index = need("features.json"), need("codebase.json")
    all_syms = sorted(index["symbols"].keys())
    tag_list = ", ".join(index["tags"]) or "(none)"
    results = []
    for i, f in enumerate(feats, 1):
        cands = search_code(index, f["keywords"] + [f["name"]])
        cand_txt = "\n".join(
            f"- {c['path']} (match {c['score']}) classes={c['classes']} "
            f"structs={c['structs']} enums={c['enums']} tags={c['tags']}"
            for c in cands
        ) or "(no file matched any keyword)"
        data = ask(
            GAP_SYSTEM,
            f"""FEATURE — {f['name']} (GDD §{f['gdd_section']})
Player behaviour: {f['player_behavior']}
Acceptance signals:
{chr(10).join('  - ' + s for s in f['acceptance_signals'])}
Keywords searched: {', '.join(f['keywords'])}

BEST-MATCHING FILES:
{cand_txt}

EVERY SYMBOL IN THE PROJECT ({len(all_syms)}):
{', '.join(all_syms)}

EVERY GAMEPLAY TAG IN THE PROJECT:
{tag_list}

Is this feature implemented?""",
            GAP_SCHEMA,
            effort="medium",
        )
        data["feature_id"] = f["id"]
        data["name"] = f["name"]
        data["gdd_section"] = f["gdd_section"]
        data["candidates"] = cands
        results.append(data)
        print(f"  [{i}/{len(feats)}] {data['verdict']:<12} {f['name']} ({data['confidence']})")
    save("gaps.json", results)
    tally = Counter(r["verdict"] for r in results)
    print(f"  {dict(tally)}")


# =============================================================================
# STAGE 4 — prioritise
# =============================================================================

PLAN_SCHEMA = {
    "type": "object",
    "properties": {
        "ranked": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "feature_id": {"type": "string"},
                    "name": {"type": "string"},
                    "rank": {"type": "integer"},
                    "rationale": {"type": "string"},
                    "criteria": {
                        "type": "object",
                        "properties": {
                            "never_cut": {"type": "boolean"},
                            "unblocks": {"type": "array", "items": {"type": "string"}},
                            "schedule_week": {"type": "integer"},
                            "one_pass_buildable": {"type": "boolean"},
                            "risk": {"type": "string", "enum": ["low", "medium", "high"]},
                        },
                        "required": ["never_cut", "unblocks", "schedule_week",
                                     "one_pass_buildable", "risk"],
                        "additionalProperties": False,
                    },
                },
                "required": ["feature_id", "name", "rank", "rationale", "criteria"],
                "additionalProperties": False,
            },
        },
        "choice": {"type": "string", "description": "feature_id to build now"},
        "why_this_one": {"type": "string"},
        "why_not_the_runner_up": {"type": "string"},
    },
    "required": ["ranked", "choice", "why_this_one", "why_not_the_runner_up"],
    "additionalProperties": False,
}

PLAN_SYSTEM = """You are the planning layer of a coding agent working on a seven-week game slice
built by one developer plus AI agents.

Rank the missing work and choose exactly one feature to build next. Weigh, in roughly this order:

1. What the design document protects. Features it lists as "Never cut" outrank everything; features
   near the front of its pre-committed cut order are the FIRST things sacrificed, so building them
   now is poor value.
2. What unblocks other work. A framework several systems ride on beats a leaf feature.
3. Where the schedule already is. Work whose week has arrived beats work weeks away.
4. Whether one pass can produce something real. Prefer a feature a single generation can deliver
   coherently over one needing art, animation, level content, or many subsystems at once.
5. Whether existing, already-built assets are waiting on it. Finished work that nothing can consume
   is a stronger claim than work with no consumer yet.

Be decisive and concrete. Name the runner-up and say why it lost — a ranking with no rejected
alternative is not a decision."""


def stage_plan() -> None:
    feats = {f["id"]: f for f in need("features.json")}
    gaps = need("gaps.json")
    todo = [g for g in gaps if g["verdict"] in ("MISSING", "PARTIAL")]
    if not todo:
        sys.exit("no gaps found — nothing to build")

    lines = []
    for g in todo:
        f = feats.get(g["feature_id"], {})
        lines.append(
            f"""### {g['name']}  (id: {g['feature_id']}, GDD §{g['gdd_section']})
verdict: {g['verdict']} ({g['confidence']} confidence)
never_cut: {f.get('never_cut')}   cut_order_rank: {f.get('cut_order_rank')}   schedule_week: {f.get('schedule_week')}
player behaviour: {f.get('player_behavior','')}
missing: {'; '.join(g['missing_pieces'])}
scanner reasoning: {g['reasoning']}"""
        )

    extra = ""
    csvs = sorted((REPO / "content-pipeline" / "out").glob("*.csv")) if (REPO / "content-pipeline" / "out").exists() else []
    if csvs:
        extra = ("\nALREADY-PRODUCED ASSETS SITTING IN THE REPO, UNCONSUMED:\n"
                 + "\n".join(f"- content-pipeline/out/{c.name}" for c in csvs)
                 + "\n(These were generated from the GDD by a separate pipeline. If a missing "
                   "feature is the thing that would consume them, that counts under criterion 5.)")

    data = ask(
        PLAN_SYSTEM,
        f"""These GDD features are missing or partial in the codebase. Rank them and pick one to build now.

{chr(10).join(lines)}
{extra}""",
        PLAN_SCHEMA,
    )
    save("plan.json", data)
    print("  ranking:")
    for r in sorted(data["ranked"], key=lambda r: r["rank"]):
        c = r["criteria"]
        print(f"    {r['rank']}. {r['name']}  "
              f"[{'never-cut' if c['never_cut'] else 'cuttable'}, W{c['schedule_week']}, "
              f"risk {c['risk']}]")
    print(f"\n  CHOICE: {data['choice']}")
    print(f"  {data['why_this_one']}")


# =============================================================================
# STAGE 5 — generate
# =============================================================================

GEN_SCHEMA = {
    "type": "object",
    "properties": {
        "files": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "path": {"type": "string",
                             "description": "relative to Source/GoblinSiege/, e.g. Barks/GSBarkTypes.h"},
                    "purpose": {"type": "string"},
                    "content": {"type": "string"},
                },
                "required": ["path", "purpose", "content"],
                "additionalProperties": False,
            },
        },
        "summary": {"type": "string"},
        "integration_steps": {"type": "array", "items": {"type": "string"}},
        "risks": {"type": "array", "items": {"type": "string"}},
        "ticket_title": {"type": "string"},
    },
    "required": ["files", "summary", "integration_steps", "risks", "ticket_title"],
    "additionalProperties": False,
}

GEN_SYSTEM = """You write Unreal Engine 5.8 C++ for the GoblinSiege module, matching the
conventions of the code you are shown.

Hard requirements:
- Every UCLASS/USTRUCT/UENUM header: `#pragma once`, `#include "CoreMinimal.h"`, and its own
  `#include "<Name>.generated.h"` LAST among includes.
- Exported types use the `GOBLINSIEGE_API` macro.
- Open every file with a short comment naming the design-document section it implements, in the
  style of the exemplars.
- Tabs for indentation, matching the exemplars.
- Only use modules already in GoblinSiege.Build.cs. Do not invent dependencies.
- Do not reference symbols that do not exist in the provided symbol index. If you need something
  that is not there, define it yourself in one of your files.

Write complete, compilable files — no `// TODO: implement`, no elided bodies. Prefer a small
coherent slice that genuinely works over a large sketch that does not. Data-driven where the
design document says data-driven.

Scope discipline: 2 to 5 files. Build the spine of the feature properly rather than every
adjacent convenience. Anything you consciously leave for a follow-up goes in `risks`, not into a
half-written sixth file."""


def stage_generate() -> None:
    plan, gaps, index = need("plan.json"), need("gaps.json"), need("codebase.json")
    feats = {f["id"]: f for f in need("features.json")}
    choice = plan["choice"]
    feat = feats.get(choice)
    gap = next((g for g in gaps if g["feature_id"] == choice), None)
    if not feat or not gap:
        sys.exit(f"plan chose '{choice}' but it is not in features/gaps")

    # Style exemplars: real files from the project, so generated code matches house style.
    exemplars = []
    for rel in ("Alarm/GSAlarmTypes.h", "Core/GSGameState.h"):
        p = SRC / rel
        if p.exists():
            exemplars.append(f"----- {rel} -----\n{p.read_text(encoding='utf-8', errors='replace')[:3500]}")

    sections = {s["heading"]: s["text"] for s in split_gdd()}
    gdd_ctx = "\n\n".join(
        f"## {h}\n{t}" for h, t in sections.items()
        if feat["gdd_section"] in h or any(k.lower() in t.lower() for k in feat["keywords"][:4])
    )[:14000]

    print(f"  generating: {feat['name']} (§{feat['gdd_section']})")
    data = ask(
        GEN_SYSTEM,
        f"""Implement this feature for the GoblinSiege module.

FEATURE — {feat['name']} (GDD §{feat['gdd_section']})
{feat['player_behavior']}

Acceptance signals it must satisfy:
{chr(10).join('  - ' + s for s in feat['acceptance_signals'])}

What the codebase scan found missing:
{chr(10).join('  - ' + m for m in gap['missing_pieces'])}

Why this was chosen: {plan['why_this_one']}

RELEVANT DESIGN DOCUMENT SECTIONS:
{gdd_ctx}

HOUSE STYLE — real files from this project, match them:
{chr(10).join(exemplars)}

EVERY SYMBOL THAT ALREADY EXISTS (do not redeclare, do not reference anything absent):
{', '.join(sorted(index['symbols'].keys()))}

EXISTING GAMEPLAY TAGS:
{', '.join(index['tags']) or '(none)'}

Write the files.""",
        GEN_SCHEMA,
        max_tokens=64000,   # whole C++ files; 32k truncates mid-string
    )

    GEN.mkdir(parents=True, exist_ok=True)
    written, conflicts = [], []
    for f in data["files"]:
        rel = f["path"].replace("\\", "/").lstrip("/")
        if (SRC / rel).exists():
            conflicts.append(rel)          # never silently overwrite hand-written code
            continue
        dest = GEN / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(f["content"], encoding="utf-8")
        written.append(rel)
        print(f"    {rel}  ({len(f['content'].splitlines())} lines)")

    data["written"] = written
    data["conflicts"] = conflicts
    data["feature"] = feat
    save("generated.json", data)
    if conflicts:
        print(f"  !! {len(conflicts)} file(s) already exist in Source/ and were NOT written:")
        for c in conflicts:
            print(f"       {c}")
    write_report(plan, gaps, data)


def write_report(plan: dict, gaps: list, gen: dict) -> None:
    tally = Counter(g["verdict"] for g in gaps)
    lines = [
        "# Agent run report", "",
        f"Model: `{MODEL}`  ·  {USAGE['calls']} calls  ·  "
        f"{USAGE['in']:,} in / {USAGE['out']:,} out tokens", "",
        "## Gap analysis", "",
        f"{tally.get('IMPLEMENTED',0)} implemented · {tally.get('PARTIAL',0)} partial · "
        f"{tally.get('MISSING',0)} missing", "",
        "| Feature | GDD | Verdict | Confidence | Evidence |", "|---|---|---|---|---|",
    ]
    for g in gaps:
        ev = "; ".join(e["symbol_or_path"] for e in g["evidence"][:3]) or "_none_"
        lines.append(f"| {g['name']} | §{g['gdd_section']} | **{g['verdict']}** | "
                     f"{g['confidence']} | {ev} |")

    lines += ["", "## Prioritisation", "",
              "| # | Feature | Never cut | Week | Risk | Rationale |", "|---|---|---|---|---|---|"]
    for r in sorted(plan["ranked"], key=lambda r: r["rank"]):
        c = r["criteria"]
        lines.append(f"| {r['rank']} | {r['name']} | {'yes' if c['never_cut'] else 'no'} | "
                     f"W{c['schedule_week']} | {c['risk']} | {r['rationale']} |")

    lines += ["", f"**Chose:** {plan['choice']}", "", plan["why_this_one"], "",
              f"**Runner-up rejected because:** {plan['why_not_the_runner_up']}", "",
              "## Generated", "", gen["summary"], ""]
    for f in gen["files"]:
        mark = " _(NOT written — already exists)_" if f["path"] in gen.get("conflicts", []) else ""
        lines.append(f"- `{f['path']}` — {f['purpose']}{mark}")
    lines += ["", "### Integration steps", ""]
    lines += [f"{i}. {s}" for i, s in enumerate(gen["integration_steps"], 1)]
    lines += ["", "### Risks the agent flagged", ""]
    lines += [f"- {r}" for r in gen["risks"]]
    (OUT / "REPORT.md").write_text("\n".join(lines), encoding="utf-8")
    print(f"  wrote {(OUT / 'REPORT.md').relative_to(ROOT)}")


# =============================================================================

RE_TYPEREF = re.compile(r"\b([AUFE]GS[A-Za-z0-9_]+)\b")
RE_CALL = re.compile(r"(?:->|\.|::)([A-Za-z_]\w+)\s*\(")

ENGINE = Path(os.environ.get("UE_ENGINE_DIR", r"D:\Epic Games\UE_5.8"))


def engine_symbols() -> set[str]:
    """
    Every identifier declared as a function across the engine's Runtime headers.

    Without this, a static checker cannot tell `UWorld::LineTraceTestByChannel` (real)
    from a hallucinated method, and reports every engine call as unresolved. The walk
    takes a couple of minutes, so it is cached; delete out/engine_symbols.json to rebuild.
    Degrades to an empty set if the engine is not on this machine, in which case
    unresolved calls are reported as advisory rather than blocking.
    """
    cache = OUT / "engine_symbols.json"
    if cache.exists():
        return set(json.loads(cache.read_text(encoding="utf-8")))
    runtime = ENGINE / "Engine" / "Source" / "Runtime"
    if not runtime.exists():
        print(f"    (engine headers not found at {runtime} — call check is advisory)")
        return set()
    print("    building engine symbol cache (one-time, ~1-2 min)…")
    syms: set[str] = set()
    pat = re.compile(r"\b([A-Za-z_]\w{2,})\s*\(")
    for p in runtime.rglob("*.h"):
        try:
            syms.update(pat.findall(p.read_text(encoding="utf-8", errors="replace")))
        except OSError:
            continue
    OUT.mkdir(parents=True, exist_ok=True)
    cache.write_text(json.dumps(sorted(syms)), encoding="utf-8")
    print(f"    cached {len(syms):,} engine symbols")
    return syms


def run_verify() -> dict:
    """
    Static cross-check of the generated code against the real codebase, before a
    human is asked to compile it. Catches the failure this agent is most prone to:
    confidently calling a symbol that does not exist. Not a compiler — it cannot
    check types, overloads or includes — but it turns the most common class of
    hallucination into a caught error instead of a build break.

    Returns the report so the refiner can consume it; `stage_verify` prints it.
    """
    index = need("codebase.json")
    known_types = set(index["symbols"])
    if not GEN.exists():
        sys.exit("nothing generated yet — run `generate` first")

    gen_files = sorted(GEN.rglob("*.h")) + sorted(GEN.rglob("*.cpp"))
    local_types, local_calls, blob = set(), set(), ""
    for p in gen_files:
        t = p.read_text(encoding="utf-8", errors="replace")
        blob += "\n" + t
        local_types |= set(RE_CLASS.findall(t)) | set(RE_STRUCT.findall(t)) | set(RE_ENUM.findall(t))
        # Delegate types are declared by macro, not by `struct`/`class`.
        local_types |= set(re.findall(r"DECLARE_\w*DELEGATE\w*\(\s*(F\w+)", t))
        local_calls |= set(RE_UFUNC.findall(t))
        local_calls |= set(re.findall(r"\b(?:void|bool|float|int32|double|FString|FVector|FText)\s+U?A?GS\w+::(\w+)\s*\(", t))
        local_calls |= set(re.findall(r"^\s*(?:UFUNCTION[^\n]*\n\s*)?(?:virtual\s+)?[\w:<>,\s\*&]+?\b(\w+)\s*\([^;]*\)\s*(?:const)?\s*;", t, re.M))

    # 1. Type references must exist in the project or be defined by the generated files.
    used_types = set(RE_TYPEREF.findall(blob))
    unknown_types = sorted(t for t in used_types if t not in known_types and t not in local_types)

    # 2. Called methods must appear somewhere in the real source, or be locally defined.
    src_blob = ""
    for p in SRC.rglob("*"):
        if p.suffix.lower() in (".h", ".cpp"):
            src_blob += p.read_text(encoding="utf-8", errors="replace")
    engine_ok = re.compile(r"^(Get|Set|Is|Has|Add|Remove|Find|Broadcast|Contains|Num|Empty|Init|"
                           r"Max|Min|Clamp|Lerp|Size|Normalize|Dot|Cross|Sqrt|Abs|Reserve|Reset|"
                           r"Append|Emplace|Push|Pop|Last|Log|Printf|Format|ToString|Equals|Len|"
                           r"Super|Begin|End|Tick|Cast|Bind|Execute|Sort|Value|Key)")
    eng = engine_symbols()
    called = set(RE_CALL.findall(blob))
    unknown_calls = sorted(
        c for c in called
        if c not in local_calls
        and not engine_ok.match(c)
        and c not in src_blob
        and c not in eng
        and c not in local_types and c not in known_types   # `UGSFoo::UGSFoo(` is a ctor
    )
    calls_advisory = not eng

    # 3. Header hygiene the compiler will punish.
    hygiene = []
    for p in gen_files:
        t = p.read_text(encoding="utf-8", errors="replace")
        rel = str(p.relative_to(GEN)).replace("\\", "/")
        if p.suffix == ".h":
            if "#pragma once" not in t:
                hygiene.append(f"{rel}: missing #pragma once")
            if re.search(r"\b(UCLASS|USTRUCT|UENUM)\s*\(", t):
                gen_inc = f'#include "{p.stem}.generated.h"'
                if gen_inc not in t:
                    hygiene.append(f"{rel}: reflected types but no {p.stem}.generated.h")
                else:
                    incs = re.findall(r'#include\s+"([^"]+)"', t)
                    if incs and incs[-1] != f"{p.stem}.generated.h":
                        hygiene.append(f"{rel}: {p.stem}.generated.h must be the LAST include")
        if p.suffix == ".cpp":
            # This module sets PublicIncludePaths.Add(ModuleDirectory), so the house
            # convention is a module-root-relative path: #include "Stealth/GSFoo.h".
            incs = re.findall(r'#include\s+"([^"]+)"', t)
            if not any(i == f"{p.stem}.h" or i.endswith(f"/{p.stem}.h") for i in incs):
                hygiene.append(f"{rel}: does not include its own header")

    report = {
        "files_checked": [str(p.relative_to(GEN)).replace("\\", "/") for p in gen_files],
        "unknown_type_references": unknown_types,
        "unknown_method_calls": unknown_calls,
        "header_hygiene": hygiene,
        "calls_check_advisory": calls_advisory,
        "clean": not (unknown_types or hygiene or (unknown_calls and not calls_advisory)),
    }
    save("verify.json", report)
    return report


def print_verify(report: dict) -> None:
    print(f"  checked {len(report['files_checked'])} files")
    for label, key in (("unknown type refs", "unknown_type_references"),
                       ("unresolved calls", "unknown_method_calls"),
                       ("header hygiene", "header_hygiene")):
        items = report[key]
        if items:
            print(f"  {label}: {len(items)}")
            for i in items[:12]:
                print(f"    - {i}")
        else:
            print(f"  {label}: none")
    print("\n  VERDICT:", "clean — ready for a human compile" if report["clean"]
          else "issues found; fix before compiling")


def stage_verify() -> None:
    print_verify(run_verify())


REFINE_SCHEMA = {
    "type": "object",
    "properties": {
        "files": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "path": {"type": "string"},
                    "content": {"type": "string"},
                    "what_changed": {"type": "string"},
                },
                "required": ["path", "content", "what_changed"],
                "additionalProperties": False,
            },
        },
        "diagnosis": {"type": "string"},
    },
    "required": ["files", "diagnosis"],
    "additionalProperties": False,
}

REFINE_SYSTEM = """You repair generated Unreal Engine 5.8 C++ against specific, already-diagnosed
findings from a static checker.

Fix the documented failures and nothing else. Do not start over from scratch, do not restructure
working code, and do not make unrelated "improvements" — every byte you change that was not named
in a finding is a byte nobody asked you to touch and nobody will review.

Return only the files you actually changed, complete and compilable. If a finding looks like a
false positive in the checker rather than a defect in the code, say so in `diagnosis` and return
that file unchanged rather than contorting the code to satisfy a bad rule."""

MAX_REFINE_PASSES = 3


def stage_refine() -> None:
    """
    GER's Refine step with a circuit breaker. Up to three passes at the findings;
    then stop and hand a problem statement back rather than looping or shipping.
    """
    if not GEN.exists():
        sys.exit("nothing generated yet — run `generate` first")

    history = []
    for p in range(1, MAX_REFINE_PASSES + 1):
        report = run_verify()
        findings = (report["unknown_type_references"] + report["unknown_method_calls"]
                    + report["header_hygiene"])
        if report["clean"]:
            print(f"  pass {p}: clean — nothing to refine")
            save("refine.json", {"passes": history, "outcome": "clean"})
            return

        print(f"  pass {p}: {len(findings)} finding(s) -> refining")
        files = {str(f.relative_to(GEN)).replace("\\", "/"):
                 f.read_text(encoding="utf-8", errors="replace")
                 for f in sorted(GEN.rglob("*.h")) + sorted(GEN.rglob("*.cpp"))}
        data = ask(
            REFINE_SYSTEM,
            f"""A static checker found these problems in generated code. Fix exactly these.

FINDINGS:
{chr(10).join('  - ' + f for f in findings)}

CURRENT FILES:
{chr(10).join(f'----- {k} -----{chr(10)}{v}' for k, v in files.items())}""",
            REFINE_SCHEMA,
            max_tokens=64000,
        )
        for f in data["files"]:
            dest = GEN / f["path"].replace("\\", "/").lstrip("/")
            if dest.exists():
                dest.write_text(f["content"], encoding="utf-8")
                print(f"    rewrote {f['path']}: {f['what_changed']}")
        history.append({"pass": p, "findings": findings, "diagnosis": data["diagnosis"]})

    # Circuit breaker: three passes spent, still failing. Escalate with a statement.
    final = run_verify()
    if final["clean"]:
        print("  clean after the final pass")
        save("refine.json", {"passes": history, "outcome": "clean"})
        return

    remaining = (final["unknown_type_references"] + final["unknown_method_calls"]
                 + final["header_hygiene"])
    statement = {
        "outcome": "ESCALATED",
        "passes": history,
        "still_failing": remaining,
        "problem_statement": (
            f"{MAX_REFINE_PASSES} refine passes did not clear {len(remaining)} finding(s). "
            f"Last diagnosis: {history[-1]['diagnosis'] if history else 'n/a'}. "
            f"Needs a human decision — the checker may be wrong, or the generated design may "
            f"need to change rather than be patched."
        ),
    }
    save("refine.json", statement)
    print(f"\n  CIRCUIT BREAKER: {statement['problem_statement']}")
    for r in remaining:
        print(f"    - {r}")


STAGES = {
    "scan-gdd": stage_scan_gdd,
    "scan-code": stage_scan_code,
    "gaps": stage_gaps,
    "plan": stage_plan,
    "generate": stage_generate,
    "verify": stage_verify,
    "refine": stage_refine,
}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("stage", choices=list(STAGES) + ["run"])
    args = ap.parse_args()

    load_dotenv()
    if not GDD.exists():
        sys.exit(f"GDD not found: {GDD}")

    todo = list(STAGES) if args.stage == "run" else [args.stage]
    t0 = time.time()
    for s in todo:
        print(f"\n=== {s} ===")
        STAGES[s]()
    if USAGE["calls"]:
        print(f"\ndone in {time.time()-t0:.0f}s — {USAGE['calls']} calls, "
              f"{USAGE['in']:,} in / {USAGE['out']:,} out tokens")


if __name__ == "__main__":
    main()
