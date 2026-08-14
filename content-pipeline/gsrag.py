#!/usr/bin/env python3
"""
Goblin Siege — dynamic content pipeline.

Retrieval-augmented generation over the project's own design documents, with a
critic agent that checks every generated line back against retrieved canon and a
reviser that applies only what the critic found.

    python gsrag.py --list                 # show the jobs and the gap each one fills
    python gsrag.py --retrieval-only       # exercise retrieval + chunking, no API calls
    python gsrag.py barks whispers prompts # generate -> critique -> revise -> write

Knowledge base: goblin-siege-design-document.md plus the four race design briefs,
one directory up. Nothing is invented outside what retrieval puts in context.

Outputs land in ./out:
    <job>.csv          UE DataTable-ready rows (the GDD's rule: a joke is a spreadsheet row)
    <job>.trace.json   full audit — queries, retrieved chunks, draft, findings, final
    <job>.trace.md     query / retrieved chunk / output side by side
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import re
import sys
import time
from collections import Counter
from dataclasses import dataclass, field, asdict
from pathlib import Path

import numpy as np

# Section signs and em dashes in headings must not kill the run on a cp1252 console.
for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):  # not a reconfigurable stream
        pass

# --------------------------------------------------------------------------------------
# Configuration
# --------------------------------------------------------------------------------------

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
OUT = ROOT / "out"

MODEL = "claude-opus-5"

# Knowledge base: the project's own design docs. (key, path, label)
SOURCES = [
    ("GDD", REPO / "goblin-siege-design-document.md", "Goblin Siege GDD (final draft)"),
    ("GOB", REPO / "race-design-goblins.md", "Race brief — Goblins"),
    ("HUM", REPO / "race-design-humans.md", "Race brief — Humans"),
    ("ELF", REPO / "race-design-elves.md", "Race brief — Elves"),
    ("DWA", REPO / "race-design-dwarves.md", "Race brief — Dwarves"),
]

# The GDD supersedes the race briefs where they disagree: the briefs predate the
# 2026-07-23/24 scope decisions (one class this slice, hamlet not palisade town).
AUTHORITY = (
    "The GDD is authoritative. The race design briefs are older reference material — "
    "where a brief contradicts the GDD (older class names, palisade-era assumptions, "
    "archetypes not in this slice), the GDD wins and the brief is stale."
)

MAX_CHUNK_CHARS = 1600
CHUNK_OVERLAP_PARAS = 1

STOPWORDS = set(
    """a an the and or but if then than that this these those of in on at to for from by with
    is are was were be been being it its as not no do does did so such can may will would
    you your they their he she his her we our i""".split()
)

# Terms that must never appear in tier-1 slice content. Cheap deterministic gate that
# runs before the critic — catches the corpus's own known traps.
BANNED_TERMS = {
    "slasher": "The class is the Scout (GDD 2.3). 'Slasher' is the stale race-brief name.",
    "brute": "The Brute is post-slice roadmap, not in the tutorial slice (GDD 2.3).",
    "shaman": "Both Shaman kits are post-slice roadmap, not in the tutorial slice (GDD 2.3).",
    "pennybrook": "Tier-2 content, not in this slice (GDD 2.8).",
    "silverford": "Tier-3 content, not in this slice (GDD 2.8).",
    "palisade": "The slice settlement is an unwalled hamlet; palisades are tier 2 (GDD 2.8).",
    "battering ram": "Tier-2 breach layer, deferred from this slice (GDD 2.8).",
    "catapult": "Tier-2 breach layer, deferred from this slice (GDD 2.8).",
}


# --------------------------------------------------------------------------------------
# Corpus: heading-aware markdown chunking
# --------------------------------------------------------------------------------------


@dataclass
class Chunk:
    id: str
    source: str
    label: str
    heading: str
    text: str
    tokens: list[str] = field(default_factory=list, repr=False)
    heading_tokens: set[str] = field(default_factory=set, repr=False)

    def cite(self) -> str:
        return f"[{self.id}] {self.label} — {self.heading}"


def tokenize(text: str) -> list[str]:
    words = re.findall(r"[a-z0-9]+", text.lower())
    return [w for w in words if w not in STOPWORDS and len(w) > 1]


def _slug(heading: str) -> str:
    """Section number when the heading has one, else the first couple of words."""
    m = re.match(r"^(\d+(?:\.\d+)*)", heading.strip())
    if m:
        return m.group(1)
    words = re.findall(r"[A-Za-z]+", heading)[:2]
    return "-".join(w.lower() for w in words) or "body"


def split_markdown(key: str, path: Path, label: str) -> list[Chunk]:
    """Split on headings, then subdivide oversized sections on paragraph boundaries."""
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    stack: list[str] = []
    sections: list[tuple[str, list[str]]] = []
    buf: list[str] = []

    def flush():
        if buf and any(ln.strip() for ln in buf):
            sections.append((" > ".join(stack) if stack else "(preamble)", list(buf)))
        buf.clear()

    for ln in lines:
        m = re.match(r"^(#{1,3})\s+(.*)$", ln)
        if m:
            flush()
            depth = len(m.group(1))
            title = m.group(2).strip().replace("**", "")
            stack = stack[: depth - 1]
            while len(stack) < depth - 1:
                stack.append("(untitled)")
            stack.append(title)
        else:
            buf.append(ln)
    flush()

    chunks: list[Chunk] = []
    seen: Counter[str] = Counter()
    for heading, body in sections:
        paras = [p.strip() for p in "\n".join(body).split("\n\n") if p.strip()]
        if not paras:
            continue
        # Pack paragraphs up to MAX_CHUNK_CHARS, overlapping by CHUNK_OVERLAP_PARAS.
        parts: list[list[str]] = []
        cur: list[str] = []
        for p in paras:
            if cur and sum(len(x) for x in cur) + len(p) > MAX_CHUNK_CHARS:
                parts.append(cur)
                cur = cur[-CHUNK_OVERLAP_PARAS:] if CHUNK_OVERLAP_PARAS else []
            cur.append(p)
        if cur:
            parts.append(cur)

        slug = _slug(heading.split(" > ")[-1])
        for part in parts:
            n = seen[f"{key}:{slug}"]
            seen[f"{key}:{slug}"] += 1
            text = "\n\n".join(part)
            chunks.append(
                Chunk(
                    id=f"{key}§{slug}#{n}",
                    source=key,
                    label=label,
                    heading=heading,
                    text=text,
                    tokens=tokenize(heading + " " + text),
                    heading_tokens=set(tokenize(heading)),
                )
            )
    return chunks


def load_corpus() -> list[Chunk]:
    chunks: list[Chunk] = []
    for key, path, label in SOURCES:
        if not path.exists():
            sys.exit(f"missing knowledge-base document: {path}")
        chunks.extend(split_markdown(key, path, label))
    return chunks


# --------------------------------------------------------------------------------------
# Retrieval: Okapi BM25 with a heading-match boost
# --------------------------------------------------------------------------------------


class Retriever:
    """
    Lexical BM25 over the chunked design docs.

    Anthropic ships no embeddings endpoint, and the knowledge base is one project's
    design docs (~200 chunks) whose vocabulary is highly specific — 'Warren', 'soft
    signal', 'First Spark Unseen', 'raid-response pool'. Exact-term matching is the
    right tool at this scale, it is deterministic, and it needs no second vendor.
    An optional Claude reranker (--rerank) reorders the BM25 candidates by relevance
    to the job, which is where semantic judgement actually pays.
    """

    def __init__(self, chunks: list[Chunk], k1: float = 1.5, b: float = 0.75):
        self.chunks = chunks
        self.k1, self.b = k1, b
        self.N = len(chunks)
        self.lens = np.array([len(c.tokens) for c in chunks], dtype=np.float64)
        self.avglen = float(self.lens.mean()) if self.N else 0.0
        self.tf: list[Counter[str]] = [Counter(c.tokens) for c in chunks]
        df: Counter[str] = Counter()
        for c in chunks:
            df.update(set(c.tokens))
        self.idf = {
            t: math.log(1 + (self.N - n + 0.5) / (n + 0.5)) for t, n in df.items()
        }

    def _scores(self, query: str) -> np.ndarray:
        qt = tokenize(query)
        scores = np.zeros(self.N, dtype=np.float64)
        if not qt:
            return scores
        norm = self.k1 * (1 - self.b + self.b * self.lens / (self.avglen or 1.0))
        for t in qt:
            idf = self.idf.get(t)
            if idf is None:
                continue
            f = np.array([tf.get(t, 0) for tf in self.tf], dtype=np.float64)
            scores += idf * (f * (self.k1 + 1)) / (f + norm)
        # A section whose *heading* answers the query is usually the section you want.
        uniq = set(qt)
        boost = np.array(
            [
                1.0 + 0.35 * (len(uniq & c.heading_tokens) / len(uniq))
                for c in self.chunks
            ]
        )
        return scores * boost

    def search(self, query: str, k: int = 5) -> list[tuple[Chunk, float]]:
        s = self._scores(query)
        idx = np.argsort(-s)[:k]
        return [(self.chunks[i], float(s[i])) for i in idx if s[i] > 0]

    def multi_search(
        self, queries: list[str], k_per: int = 4, k_total: int = 18
    ) -> tuple[list[Chunk], list[dict]]:
        """Union of per-query top-k, deduped by chunk, keeping each chunk's best hit."""
        best: dict[str, tuple[Chunk, float, str]] = {}
        trace: list[dict] = []
        for q in queries:
            hits = self.search(q, k_per)
            trace.append(
                {
                    "query": q,
                    "hits": [
                        {"chunk_id": c.id, "score": round(s, 3), "heading": c.heading}
                        for c, s in hits
                    ],
                }
            )
            for c, s in hits:
                if c.id not in best or s > best[c.id][1]:
                    best[c.id] = (c, s, q)
        ranked = sorted(best.values(), key=lambda t: -t[1])[:k_total]
        return [c for c, _, _ in ranked], trace


def render_context(chunks: list[Chunk]) -> str:
    return "\n\n".join(
        f"<chunk id=\"{c.id}\" source=\"{c.label}\" section=\"{c.heading}\">\n{c.text}\n</chunk>"
        for c in chunks
    )


# --------------------------------------------------------------------------------------
# Claude plumbing
# --------------------------------------------------------------------------------------

USAGE = Counter()
_FALLBACK_OK = True  # server-side refusal fallback; disabled on first rejection
MAX_ATTEMPTS = 4


def _is_transient(exc: BaseException) -> bool:
    """Worth retrying: the connection or the server faltered, not the request."""
    import anthropic
    import httpx

    if isinstance(
        exc,
        (
            anthropic.APIConnectionError,
            anthropic.APITimeoutError,
            anthropic.RateLimitError,
            anthropic.InternalServerError,
            httpx.RemoteProtocolError,
            httpx.ReadError,
            httpx.ReadTimeout,
        ),
    ):
        return True
    return isinstance(exc, anthropic.APIStatusError) and exc.status_code >= 500


DOTENV_CANDIDATES = [
    ROOT / ".env",
    REPO / ".env",
    REPO / "GoblinSiege 5.8" / ".env",
]


def load_dotenv() -> None:
    """Read the first .env found (all gitignored) so the key survives shell hops."""
    for path in DOTENV_CANDIDATES:
        if not path.exists():
            continue
        for line in path.read_text(encoding="utf-8-sig").splitlines():
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            os.environ.setdefault(k.strip(), v.strip().strip("'\""))
        print(f"credentials: loaded from {path}")
        return


def client():
    import anthropic

    if not (os.environ.get("ANTHROPIC_API_KEY") or os.environ.get("ANTHROPIC_AUTH_TOKEN")):
        # An `ant auth login` profile also works; only bail if there is nothing at all.
        cfg = Path(os.environ.get("APPDATA", "~")).expanduser() / "Anthropic"
        if not cfg.exists():
            sys.exit(
                "No Anthropic credentials found.\n"
                f"Create {ROOT / '.env'} containing:\n"
                "    ANTHROPIC_API_KEY=sk-ant-...\n"
                "(that file is gitignored), or run `ant auth login`."
            )
    return anthropic.Anthropic()


def call_json(system: str, user: str, schema: dict, effort: str = "high") -> dict:
    """One structured-output call. Streams (large max_tokens) and returns parsed JSON."""
    global _FALLBACK_OK
    cli = client()
    kwargs = dict(
        model=MODEL,
        max_tokens=32000,
        system=system,
        messages=[{"role": "user", "content": user}],
        thinking={"type": "adaptive"},
        output_config={
            "effort": effort,
            "format": {"type": "json_schema", "schema": schema},
        },
    )

    def _run(beta: bool):
        if beta:
            return cli.beta.messages.stream(
                betas=["server-side-fallback-2026-07-01"], fallbacks="default", **kwargs
            )
        return cli.messages.stream(**kwargs)

    attempt = 0
    while True:
        beta = _FALLBACK_OK
        try:
            with _run(beta) as stream:
                msg = stream.get_final_message()
            break
        except Exception as exc:  # noqa: BLE001 - beta surface may not exist on this SDK
            # Only downgrade when the *fallback params themselves* were rejected.
            # Anything else (billing, rate limit, auth) must surface as-is, or an
            # unrelated failure silently turns off refusal handling.
            text = str(exc).lower()
            rejected_params = isinstance(exc, TypeError) or any(
                s in text for s in ("fallback", "unexpected keyword", "unknown beta")
            )
            if beta and rejected_params:
                _FALLBACK_OK = False
                print(
                    f"  (server-side fallback unavailable: {type(exc).__name__}; "
                    f"continuing without it)"
                )
                continue  # not an attempt — retry immediately without the beta
            # The SDK retries failed *requests*; a stream that dies after headers
            # (RemoteProtocolError) is not covered, and these calls are long.
            if _is_transient(exc) and attempt < MAX_ATTEMPTS - 1:
                attempt += 1
                delay = 2**attempt
                print(
                    f"  transient {type(exc).__name__}; "
                    f"retry {attempt}/{MAX_ATTEMPTS - 1} in {delay}s"
                )
                time.sleep(delay)
                continue
            raise

    if msg.stop_reason == "refusal":
        raise RuntimeError(f"model declined the request: {msg.stop_details}")

    USAGE["input"] += msg.usage.input_tokens
    USAGE["output"] += msg.usage.output_tokens
    USAGE["calls"] += 1

    text = next((b.text for b in msg.content if b.type == "text"), None)
    if not text:
        raise RuntimeError("no text block in response")
    return json.loads(text)


# --------------------------------------------------------------------------------------
# Job definitions — three content types, each filling a gap the GDD itself names
# --------------------------------------------------------------------------------------


def _items_schema(item_props: dict, required: list[str]) -> dict:
    item_props = dict(item_props)
    item_props["sources"] = {
        "type": "array",
        "items": {"type": "string"},
        "description": "chunk ids from the provided context that this row is grounded in",
    }
    return {
        "type": "object",
        "properties": {
            "items": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": item_props,
                    "required": required + ["sources"],
                    "additionalProperties": False,
                },
            }
        },
        "required": ["items"],
        "additionalProperties": False,
    }


@dataclass
class Job:
    key: str
    title: str
    gap: str
    queries: list[str]
    n_items: int
    schema: dict
    columns: list[str]
    brief: str


# Must match the enumerator names in Source/GoblinSiege/Alarm/GSAlarmTypes.h
# (EGSAlarmPhase: Quiet/Suspicious/Raid/Razed). UE matches DataTable enum cells to
# enumerator names exactly, so the GDD's shouty "QUIET" would fail to import.
ALARM = ["Quiet", "Suspicious", "Raid", "Razed"]

JOBS: dict[str, Job] = {
    "barks": Job(
        key="barks",
        title="NPC bark sheet — livestock & stealth extension pass",
        gap=(
            "GDD 3.2 (The Writer): the approved bark sheet is 'now due a livestock-and-"
            "stealth extension pass'. The livestock system (2.7) and the lean-five stealth "
            "layer (2.4) both landed after the sheet was approved, so the states they "
            "introduce — a startled flock, a coin scramble, a discovered corpse, an overdue "
            "patrol — have no lines."
        ),
        queries=[
            "guard perception confirm suspicious corpse discovered investigate",
            "livestock chickens sheep pigs pens startled flock squawking noise alarm",
            "civilians routines panic bucket brigade well watchman doze bell",
            "tone comedy barks propaganda goblins extinct badger denial shearing day",
            "coin toss lure greedy guards scramble distraction",
            "forest patrol torches archer signal horn overdue check in waypoint",
            "alarm phases QUIET SUSPICIOUS RAID RAZED escalation soft signal",
        ],
        n_items=18,
        schema=_items_schema(
            {
                "speaker": {
                    "type": "string",
                    "enum": ["Guard", "Militia", "Archer", "Watchman", "Civilian", "PatrolArcher"],
                },
                "alarm_state": {"type": "string", "enum": ALARM},
                "trigger": {
                    "type": "string",
                    "description": "the AI state or event that fires this line, e.g. FlockStartled",
                },
                "line": {"type": "string", "description": "the spoken bark, one sentence"},
                "note": {"type": "string", "description": "why this line is right for the state"},
            },
            ["speaker", "alarm_state", "trigger", "line", "note"],
        ),
        columns=["Name", "speaker", "alarm_state", "trigger", "line", "note", "sources"],
        brief=(
            "Write NPC barks for the states the livestock and stealth systems introduced. "
            "Cover the flock alarm, a coin scramble, a discovered corpse, a half-confirmed "
            "sighting that decayed, an overdue patrol, the well being fouled, the bucket "
            "brigade, and the watchman's doze. Spread them across the alarm phases. The "
            "humans' first reaction to a goblin is disbelief, not fear — the kingdom told "
            "everyone goblins were extinct, and that denial should visibly strain as "
            "evidence piles up."
        ),
    ),
    "whispers": Job(
        key="whispers",
        title="His Eternal Darkness — whisper and verdict lines",
        gap=(
            "GDD 3.2 (The Writer): 'new capture/purge-triggered Overlord verdict lines "
            "(2.10)' are outstanding. 2.10 asks for a raid that comes home with a rope "
            "chain of prisoners to earn a different flavour of verdict than one that comes "
            "home merely rich, and the clock nudges at 10/5/2 minutes have no lines either."
        ),
        queries=[
            "His Eternal Darkness whispers verdict adequate rats layered subtitles register",
            "capture bind prisoners rope chain None Left Behind tribute proof they remember us",
            "raid clock 30 minutes ten five two minute nudges portal collapse grace window",
            "score deeds loot banking extraction multiplier end screen letter grade personal best",
            "windmill ablaze detonation window torch two stage set piece",
            "now is the time of the goblins propaganda vengeance theme tone escalates",
            "runic site standing stones portal extraction courier banking",
        ],
        n_items=14,
        schema=_items_schema(
            {
                "trigger": {
                    "type": "string",
                    "description": "the event key that fires it, e.g. Clock.TenMinutes",
                },
                "category": {
                    "type": "string",
                    "enum": ["ClockNudge", "ObjectiveBark", "EndVerdict", "MilestoneWhisper"],
                },
                "line": {"type": "string", "description": "the whispered line"},
                "note": {"type": "string", "description": "the canon condition it answers"},
            },
            ["trigger", "category", "line", "note"],
        ),
        columns=["Name", "trigger", "category", "line", "note", "sources"],
        brief=(
            "Write His Eternal Darkness's lines: the 10/5/2-minute clock nudges, the "
            "windmill Stage-1 bark's siblings for the granary and the field, milestone "
            "whispers as loot and prisoners cross into the stones, and end-of-raid verdicts "
            "— including the distinct capture-heavy verdict 2.10 asks for versus a merely "
            "rich one, and a verdict for a wipe. Apocalyptic whispered gravitas applied to "
            "stealing pigs. He mocks more than he mourns at tier 1."
        ),
    ),
    "prompts": Job(
        key="prompts",
        title="Tutorial hamlet — first-time prompt rows",
        gap=(
            "GDD 2.8 specifies a skippable teaching layer — every objective and mechanic "
            "carries 'a bark from His Eternal Darkness plus a one-line HUD note naming the "
            "objective, the payoff, and the one control that does it' — and 3.1 lists it as "
            "a Settlement Generator Agent deliverable for week 4. Only one instance (the "
            "windmill Stage-1 bark) is actually written."
        ),
        queries=[
            "tutorial hamlet prompts first time dismissible HUD note one control payoff",
            "burn objectives granary field windmill how each burns telegraphs",
            "takedown bind civilians hold E channel unaware from behind",
            "courier point command horde pig loot sack rejoins pool",
            "patrol soft signal escalation suspicious overdue decay",
            "war horn horde treeline summon go loud on purpose",
            "universal kit torch toss dodge roll crouch interact controls",
            "Warren plant respawn banking loot prisoners dug hole",
        ],
        n_items=9,
        schema=_items_schema(
            {
                "trigger": {
                    "type": "string",
                    "description": "first-time encounter key, e.g. FirstSight.Windmill",
                },
                "overlord_bark": {"type": "string", "description": "the whispered teaching line"},
                "hud_line": {
                    "type": "string",
                    "description": "one HUD line: the objective, the payoff, the control",
                },
                "control": {"type": "string", "description": "the single input, e.g. Hold E"},
            },
            ["trigger", "overlord_bark", "hud_line", "control"],
        ),
        columns=["Name", "trigger", "overlord_bark", "hud_line", "control", "sources"],
        brief=(
            "Write the first-time prompt rows for the tutorial hamlet: one per mechanic the "
            "hamlet is laid out to teach. Each row is a short Overlord bark plus a one-line "
            "HUD note that names the objective, the payoff, and the one control that does "
            "it. Every control must be one the slice actually ships.\n\n"
            "The HUD line is a glance, not a manual. Hard constraints, added after the "
            "Design Steward filed repeated REGISTER_DRIFT findings on over-stuffed lines:\n"
            "- ONE objective, ONE payoff, ONE control. Under ~90 characters.\n"
            "- No chained mechanical clauses. If a mechanic has a second stage or a "
            "  follow-up consequence, that is a SEPARATE prompt that fires later — do not "
            "  fold it into the first-sight line.\n"
            "- Model it on the one instance the GDD already writes out: "
            "  'Windmill: Ablaze — needs a window shot.'\n"
            "The Overlord bark carries the flavour; the HUD line carries the instruction. "
            "Do not make the HUD line do both."
        ),
    ),
}


# --------------------------------------------------------------------------------------
# Generate -> critique -> revise
# --------------------------------------------------------------------------------------

WRITER_SYSTEM = f"""You are the Writer agent on Goblin Siege (GDD section 3.2, agent 9). You draft all
systemic text as data-table content: NPC bark sheets keyed to AI states and His Eternal
Darkness's whisper lines. Adding a joke must be a spreadsheet row, never code.

You write ONLY from the retrieved design-document context you are given. It is the game's
canon. Do not import fantasy-genre defaults, and do not invent mechanics, place names,
numbers, classes, or systems that the context does not contain. If the context does not
support a line, write a different line.

{AUTHORITY}

Voice: satire, not grimdark. The humans of Groatsworth county are pompous, greedy and a
little bit stupid. His Eternal Darkness applies apocalyptic whispered gravitas to petty
theft. Barks are the cheapest comedy per byte — short, spoken, in character. Every line
must be playable as a single audible bark or one HUD line.

Each row cites the chunk ids it is grounded in."""

CRITIC_SYSTEM = f"""You are the Design Steward agent on Goblin Siege (GDD section 3.2, agent 7). You keep the
decisions ledger and you are the reason the game reads as coherent. You are reviewing
draft content written by the Writer agent.

Your job is adversarial: find where the draft contradicts canon or drifts out of register.
You are given retrieved canon chunks. Every finding MUST cite the chunk id that proves it —
a finding you cannot ground in a quoted chunk is not a finding, and you must not raise it.

{AUTHORITY}

Check for:
1. LORE_BREAK — a mechanic, number, name, class, place, or capability that contradicts the
   retrieved canon or does not exist in this slice. Wrong score values, wrong pool sizes,
   wrong controls, post-slice content treated as shipped, stale race-brief names.
2. TONE_DRIFT — grimdark, epic-fantasy solemnity, or generic-orc menace where the canon
   register is comic satire and strained denial; or an Overlord line that is merely
   threatening instead of pompously underwhelmed.
3. REGISTER_DRIFT — a line that cannot be delivered as one spoken bark or one HUD line:
   too long, too written, stage directions, exposition a character would not say.

Pass anything that is genuinely fine. Do not invent problems to look thorough, and do not
flag a line merely because you would have phrased it differently. For each real finding,
give a concrete corrected replacement that keeps the row's trigger and intent."""

CRITIC_SCHEMA = {
    "type": "object",
    "properties": {
        "findings": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "row": {"type": "string", "description": "the Name of the offending row"},
                    "verdict": {
                        "type": "string",
                        "enum": ["LORE_BREAK", "TONE_DRIFT", "REGISTER_DRIFT"],
                    },
                    "quote": {"type": "string", "description": "the exact offending text"},
                    "canon_rule": {"type": "string", "description": "the rule it violates"},
                    "evidence_chunk_id": {
                        "type": "string",
                        "description": "chunk id from the context that proves the rule",
                    },
                    "correction": {"type": "string", "description": "the corrected text"},
                },
                "required": [
                    "row",
                    "verdict",
                    "quote",
                    "canon_rule",
                    "evidence_chunk_id",
                    "correction",
                ],
                "additionalProperties": False,
            },
        },
        "summary": {"type": "string"},
    },
    "required": ["findings", "summary"],
    "additionalProperties": False,
}


def row_name(job: Job, i: int, item: dict) -> str:
    trig = re.sub(r"[^A-Za-z0-9]+", "", item.get("trigger", "") or "Row")
    return f"{job.key.capitalize()}_{i:02d}_{trig[:28]}"


def lint(job: Job, items: list[dict], valid_ids: set[str]) -> list[dict]:
    """Deterministic pre-critic: banned tier-1 terms and unciteable rows."""
    out = []
    for it in items:
        blob = " ".join(str(v) for k, v in it.items() if k != "sources").lower()
        for term, why in BANNED_TERMS.items():
            if re.search(rf"\b{re.escape(term)}\b", blob):
                out.append(
                    {
                        "row": it["_name"],
                        "verdict": "LORE_BREAK",
                        "quote": term,
                        "canon_rule": why,
                        "evidence_chunk_id": "lint",
                        "correction": "(deterministic lint — see canon_rule)",
                        "by": "lint",
                    }
                )
        cited = [s for s in it.get("sources", []) if s in valid_ids]
        if not cited:
            out.append(
                {
                    "row": it["_name"],
                    "verdict": "LORE_BREAK",
                    "quote": str(it.get("sources", [])),
                    "canon_rule": "Every row must cite at least one real retrieved chunk id.",
                    "evidence_chunk_id": "lint",
                    "correction": "(reground this row in retrieved canon)",
                    "by": "lint",
                }
            )
    return out


def generate(job: Job, ctx: str) -> list[dict]:
    user = f"""{job.brief}

Produce exactly {job.n_items} rows.

RETRIEVED CANON — this is everything you know about Goblin Siege:

{ctx}"""
    data = call_json(WRITER_SYSTEM, user, job.schema, effort="high")
    items = data["items"]
    for i, it in enumerate(items, 1):
        it["_name"] = row_name(job, i, it)
    return items


def critique(job: Job, items: list[dict], ctx: str) -> list[dict]:
    draft = json.dumps(
        [{k: v for k, v in it.items() if k != "sources"} for it in items],
        indent=2,
        ensure_ascii=False,
    )
    user = f"""Review this draft {job.title} against the retrieved canon below.

DRAFT ROWS:
{draft}

RETRIEVED CANON:

{ctx}"""
    data = call_json(CRITIC_SYSTEM, user, CRITIC_SCHEMA, effort="high")
    findings = data["findings"]
    for f in findings:
        f["by"] = "critic"
    print(f"  critic: {data['summary']}")
    return findings


def revise(job: Job, items: list[dict], findings: list[dict], ctx: str) -> list[dict]:
    user = f"""The Design Steward reviewed your draft and filed the findings below. Apply them.

Return the FULL set of {len(items)} rows. Change only the rows named in the findings —
every other row must come back byte-identical, including its trigger and sources. Keep each
row's trigger unchanged even where you rewrite its text.

FINDINGS:
{json.dumps(findings, indent=2, ensure_ascii=False)}

YOUR DRAFT (each row's "_name" is how the findings above refer to it):
{json.dumps(items, indent=2, ensure_ascii=False)}

RETRIEVED CANON:

{ctx}"""
    data = call_json(WRITER_SYSTEM, user, job.schema, effort="high")
    items2 = data["items"]
    for i, it in enumerate(items2, 1):
        it["_name"] = row_name(job, i, it)
    return items2


def rerank(job: Job, chunks: list[Chunk]) -> list[Chunk]:
    """Optional semantic pass over the BM25 candidate set."""
    schema = {
        "type": "object",
        "properties": {
            "keep": {"type": "array", "items": {"type": "string"}},
            "why": {"type": "string"},
        },
        "required": ["keep", "why"],
        "additionalProperties": False,
    }
    listing = "\n".join(f"[{c.id}] {c.heading}\n{c.text[:280]}" for c in chunks)
    data = call_json(
        "You rank retrieved design-document chunks by usefulness for a content-writing task.",
        f"Task: {job.title}\n\n{job.brief}\n\nCandidates:\n{listing}\n\n"
        "Return the chunk ids that are actually useful for this task, most useful first. "
        "Drop candidates that are off-topic. Keep at least 8.",
        schema,
        effort="low",
    )
    order = {cid: i for i, cid in enumerate(data["keep"])}
    kept = [c for c in chunks if c.id in order]
    kept.sort(key=lambda c: order[c.id])
    print(f"  rerank: {len(chunks)} -> {len(kept)} chunks")
    return kept or chunks


# --------------------------------------------------------------------------------------
# Output
# --------------------------------------------------------------------------------------


def write_csv(job: Job, items: list[dict]) -> Path:
    path = OUT / f"{job.key}.csv"
    with path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(job.columns)
        for it in items:
            row = []
            for col in job.columns:
                if col == "Name":
                    row.append(it["_name"])
                elif col == "sources":
                    row.append(" ".join(it.get("sources", [])))
                else:
                    row.append(it.get(col, ""))
            w.writerow(row)
    return path


def write_trace(job: Job, trace: dict) -> tuple[Path, Path]:
    jpath = OUT / f"{job.key}.trace.json"
    jpath.write_text(json.dumps(trace, indent=2, ensure_ascii=False), encoding="utf-8")

    md = [
        f"# Retrieval trace — {job.title}",
        "",
        f"**Gap this fills.** {job.gap}",
        "",
        "## 1. Queries and what they retrieved",
        "",
    ]
    for q in trace["retrieval"]:
        md.append(f"**Query:** `{q['query']}`")
        md.append("")
        for h in q["hits"]:
            md.append(f"- `{h['chunk_id']}` (BM25 {h['score']}) — {h['heading']}")
        md.append("")

    md += ["## 2. Retrieved chunk -> generated line", ""]
    by_id = {c["id"]: c for c in trace["context"]}
    for it in trace["final"][:6]:
        cited = [by_id[s] for s in it.get("sources", []) if s in by_id]
        head = it.get("line") or it.get("hud_line") or it.get("overlord_bark") or ""
        md.append(f"### {it['_name']}")
        md.append("")
        md.append(f"**Output:** {head}")
        md.append("")
        for c in cited[:2]:
            excerpt = " ".join(c["text"].split())[:420]
            md.append(f"**Retrieved `{c['id']}`** ({c['heading']}):")
            md.append("")
            md.append(f"> {excerpt}…")
            md.append("")

    md += ["## 3. Critic findings", ""]
    if not trace["rounds"]:
        md.append("_none_")
    for r in trace["rounds"]:
        md.append(f"### Round {r['round']} — {len(r['findings'])} finding(s)")
        md.append("")
        for f in r["findings"]:
            md.append(f"- **{f['verdict']}** in `{f['row']}` (by {f['by']})")
            md.append(f"  - offending: {f['quote']}")
            md.append(f"  - canon: {f['canon_rule']} — evidence `{f['evidence_chunk_id']}`")
            md.append(f"  - correction: {f['correction']}")
        md.append("")

    mpath = OUT / f"{job.key}.trace.md"
    mpath.write_text("\n".join(md), encoding="utf-8")
    return jpath, mpath


# --------------------------------------------------------------------------------------
# Driver
# --------------------------------------------------------------------------------------


def run_job(job: Job, retr: Retriever, use_rerank: bool, max_rounds: int) -> None:
    print(f"\n=== {job.key}: {job.title} ===")
    chunks, rtrace = retr.multi_search(job.queries, k_per=4, k_total=18)
    print(f"  retrieved {len(chunks)} chunks from {len(job.queries)} queries")
    if use_rerank:
        chunks = rerank(job, chunks)
    ctx = render_context(chunks)
    valid_ids = {c.id for c in chunks}

    items = generate(job, ctx)
    print(f"  generated {len(items)} rows")
    # Checkpoint the pre-critic draft: it is the expensive call, and the
    # before/after pair is the evidence that the critic changed something.
    draft = json.loads(json.dumps(items))
    (OUT / f"{job.key}.draft.json").write_text(
        json.dumps(draft, indent=2, ensure_ascii=False), encoding="utf-8"
    )

    # Always finish on a check, never on a revision — otherwise the rows that ship are
    # the one version nothing verified. (Observed: round 1 flagged a wrong patrol
    # cadence, the revision "fixed" it to another wrong value, round 2 caught that.)
    rounds = []
    for rnd in range(1, max_rounds + 2):
        findings = lint(job, items, valid_ids) + critique(job, items, ctx)
        rounds.append({"round": rnd, "findings": findings, "verify_only": rnd > max_rounds})
        if not findings:
            print(f"  round {rnd}: clean")
            break
        if rnd > max_rounds:
            print(f"  round {rnd}: {len(findings)} finding(s) UNRESOLVED after "
                  f"{max_rounds} revisions — see trace")
            break
        print(f"  round {rnd}: {len(findings)} finding(s) -> revising")
        items = revise(job, items, findings, ctx)

    csv_path = write_csv(job, items)
    trace = {
        "job": job.key,
        "title": job.title,
        "gap": job.gap,
        "model": MODEL,
        "retrieval": rtrace,
        "context": [
            {"id": c.id, "heading": c.heading, "source": c.label, "text": c.text}
            for c in chunks
        ],
        "draft": draft,
        "rounds": rounds,
        "final": items,
    }
    jp, mp = write_trace(job, trace)
    print(f"  wrote {csv_path.name}, {jp.name}, {mp.name}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("jobs", nargs="*", default=[], help="job keys; default all")
    ap.add_argument("--list", action="store_true", help="list jobs and exit")
    ap.add_argument(
        "--retrieval-only",
        action="store_true",
        help="chunk + retrieve + print, no API calls",
    )
    ap.add_argument("--rerank", action="store_true", help="add a Claude reranking pass")
    ap.add_argument("--rounds", type=int, default=2, help="max critic rounds (default 2)")
    args = ap.parse_args()

    if args.list:
        for j in JOBS.values():
            print(f"{j.key:10s} {j.title}\n           gap: {j.gap}\n")
        return

    load_dotenv()
    chunks = load_corpus()
    retr = Retriever(chunks)
    print(
        f"knowledge base: {len(chunks)} chunks from {len(SOURCES)} documents "
        f"({sum(len(c.tokens) for c in chunks)} indexed terms)"
    )

    keys = args.jobs or list(JOBS)
    for k in keys:
        if k not in JOBS:
            sys.exit(f"unknown job '{k}' (have: {', '.join(JOBS)})")

    if args.retrieval_only:
        for k in keys:
            job = JOBS[k]
            print(f"\n=== {job.key} ===")
            got, tr = retr.multi_search(job.queries, k_per=4, k_total=18)
            for q in tr:
                print(f"\n  query: {q['query']}")
                for h in q["hits"]:
                    print(f"    {h['score']:6.2f}  {h['chunk_id']:<22} {h['heading'][:70]}")
            print(f"\n  -> {len(got)} unique chunks in context")
        return

    OUT.mkdir(exist_ok=True)
    t0 = time.time()
    for k in keys:
        run_job(JOBS[k], retr, args.rerank, args.rounds)
    print(
        f"\ndone in {time.time() - t0:.0f}s — {USAGE['calls']} calls, "
        f"{USAGE['input']:,} in / {USAGE['output']:,} out tokens"
    )


if __name__ == "__main__":
    main()
