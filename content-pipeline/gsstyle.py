#!/usr/bin/env python3
"""
Style Guide Agent for Goblin Siege - Assignment #7.

A Generator -> Evaluator -> Refiner loop that enforces this game's own style rules.
The Evaluator returns a SCORE (1-10) and a REASON; the Refiner rewrites from that
reason; nobody intervenes.

    python gsstyle.py --list                 # the rules and the cases, no API calls
    python gsstyle.py --dry-run              # + retrieval and the deterministic lint, still free
    python gsstyle.py --case all             # the graded run

This is deliberately NOT a new pipeline. Everything that is not specific to style
scoring is imported from gsrag.py, which already owns the knowledge base, BM25
retrieval, structured-output calls, retries and usage accounting. What is new here is
the part gsrag has never had: a numeric score. Its critic is categorical and
exception-based - a clean row produces no output at all - and the brief for this
assignment forbids binary pass/fail.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from copy import deepcopy
from dataclasses import dataclass, field
from pathlib import Path

from gsrag import (
    MODEL,
    OUT,
    USAGE,
    Retriever,
    _items_schema,
    load_corpus,
    load_dotenv,
    render_context,
)
from gsrag import call_json as _api_call_json

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

ROOT = Path(__file__).resolve().parent
STYLE_OUT = OUT / "style"

PASS_THRESHOLD = 9        # a row set scoring this or better ships
MAX_REFINEMENTS = 3       # then stop and report UNRESOLVED rather than loop
MAX_HUD_CHARS = 90        # GDD 2.8: "a one-line HUD note"; see STYLE_GUIDE.md


# --------------------------------------------------------------------------- provider

# Two ways to reach Claude, because the first one died under us.
#
#   api        - gsrag.call_json: real json_schema structured output, billed to API credits.
#   claude-cli - headless `claude -p`, billed to the Max subscription instead. The CLI
#                returns prose, so the schema goes in the prompt and the JSON is dug back
#                out of the reply.
#
# Default is claude-cli: the key in GoblinSiege 5.8/.env returned 401 "API key is invalid"
# on 2026-08-18, so `api` needs a fresh key before it will run.

PROVIDER = "claude-cli"
CLI_TIMEOUT = 900


def _cli_path() -> str | None:
    found = shutil.which("claude") or shutil.which("claude.cmd")
    if found:
        return found
    for cand in (Path.home() / ".local/bin/claude.exe", Path.home() / ".local/bin/claude"):
        if cand.exists():
            return str(cand)
    return None


def extract_json(text: str, want: tuple[str, ...] = ()) -> dict:
    """
    Pull one JSON object out of a prose reply.

    Deliberately not brace-counting: CodeArchitect's live-002 run died that way when a
    closing brace inside a string literal ended the object early. Walk every '{' and let
    the decoder decide where the object really ends.

    `want` is the contract's required top-level keys. The FIRST parseable object is not
    necessarily the right one - a reply that opens with a nested example, or with prose
    containing braces, yields a dict that parses fine and is missing "items". Prefer an
    object that satisfies the contract; fall back to the first parseable dict only if
    nothing does. (Same lesson ca/llm.py records for its own output contract.)
    """
    if not text or not text.strip():
        raise RuntimeError("empty model output")
    cands = []
    fence = re.search(r"```(?:json)?\s*(\{.*\})\s*```", text, re.DOTALL)
    if fence:
        cands.append(fence.group(1))
    cands.append(text)
    last, fallback = None, None
    for cand in cands:
        for i, ch in enumerate(cand):
            if ch != "{":
                continue
            try:
                obj, _ = json.JSONDecoder().raw_decode(cand[i:])
            except json.JSONDecodeError as e:
                last = e
                continue
            if not isinstance(obj, dict):
                continue
            if want and all(k in obj for k in want):
                return obj
            if fallback is None:
                fallback = obj
    if fallback is not None:
        if want and not all(k in fallback for k in want):
            raise RuntimeError(
                f"model output has no object with the required keys {list(want)}; "
                f"got keys {sorted(fallback)[:8]}")
        return fallback
    raise RuntimeError(f"no parseable JSON object in model output ({last})")


def _coerce(text: str, want: tuple[str, ...]) -> dict:
    """
    Get the contract object out of a text-mode reply, whatever shape it arrived in.

    A structured-output API guarantees the wrapper; the CLI does not, and in practice the
    model returns the wrapper, a bare array of rows, or a single row object more or less at
    random. Re-prompting harder does not fix that - accepting the three real shapes does.
    """
    stripped = text.strip()
    if stripped.startswith("```"):
        stripped = re.sub(r"^```(?:json)?\s*|\s*```$", "", stripped, flags=re.DOTALL)

    if want == ("items",):
        try:
            whole = json.loads(stripped)
            if isinstance(whole, list):
                return {"items": whole}
            if isinstance(whole, dict) and "items" in whole:
                return whole
            if isinstance(whole, dict):
                return {"items": [whole]}
        except json.JSONDecodeError:
            pass

    try:
        return extract_json(text, want)
    except RuntimeError:
        if want != ("items",):
            raise
        # last resort: sweep up every row-shaped object in the reply
        rows, i = [], 0
        dec = json.JSONDecoder()
        while i < len(text):
            if text[i] != "{":
                i += 1
                continue
            try:
                obj, end = dec.raw_decode(text[i:])
            except json.JSONDecodeError:
                i += 1
                continue
            if isinstance(obj, dict) and "text" in obj and "trigger" in obj:
                rows.append(obj)
            i += end
        if rows:
            return {"items": rows}
        raise


def _cli_call_json(system: str, user: str, schema: dict, effort: str = "high") -> dict:
    cli = _cli_path()
    if cli is None:
        raise RuntimeError("claude CLI not found; use --provider api with a valid key")
    sys_prompt = (
        f"{system}\n\n"
        "OUTPUT CONTRACT: reply with a single JSON object and nothing else - no prose "
        "before or after, no markdown fence. It must validate against this JSON Schema:\n"
        f"{json.dumps(schema)}"
    )
    cmd = [cli, "-p", "--output-format", "text", "--append-system-prompt", sys_prompt]
    model = os.environ.get("GS_CLI_MODEL")
    if model:
        cmd += ["--model", model]
    last = None
    for attempt in range(3):
        try:
            res = subprocess.run(cmd, input=user, capture_output=True, text=True,
                                 encoding="utf-8", errors="replace", timeout=CLI_TIMEOUT)
            if res.returncode != 0:
                raise RuntimeError(f"claude -p rc={res.returncode}: {res.stderr[:300]}")
            data = _coerce(res.stdout, tuple(schema.get("required", ())))
            USAGE["calls"] += 1
            return data
        except Exception as e:                      # noqa: BLE001 - retry any transport/parse fault
            last = e
            if attempt < 2:
                time.sleep(2 ** attempt)
    raise RuntimeError(f"claude-cli failed after 3 attempts: {last}")


CACHE_DIR = OUT / "style" / "cache"
CACHE_MODE = "record"        # record | replay | off


def _cache_key(system: str, user: str, schema: dict) -> str:
    blob = json.dumps({"p": PROVIDER, "s": system, "u": user, "j": schema},
                      sort_keys=True, ensure_ascii=False)
    return hashlib.sha256(blob.encode("utf-8")).hexdigest()[:32]


def call_json(system: str, user: str, schema: dict, effort: str = "high") -> dict:
    """
    One model call, cached by the exact prompt that produced it.

    The model is not deterministic and cannot be made so. What CAN be made deterministic is
    a RE-RUN: cache every response against a hash of (provider, system, user, schema) and a
    replay reproduces a graded run byte for byte, offline and free. --replay refuses to call
    the model at all, so a cache miss is a loud error rather than a quiet re-roll.
    """
    key = _cache_key(system, user, schema)
    path = CACHE_DIR / (key + ".json")

    if CACHE_MODE in ("record", "replay") and path.exists():
        USAGE["cache_hits"] += 1
        return json.loads(path.read_text(encoding="utf-8"))["response"]

    if CACHE_MODE == "replay":
        raise RuntimeError(
            f"--replay: no cached response for {key}. The prompt differs from the recorded "
            f"run (content, rules or schema changed), so this run cannot be reproduced."
        )

    data = (_api_call_json(system, user, schema, effort=effort) if PROVIDER == "api"
            else _cli_call_json(system, user, schema, effort=effort))

    if CACHE_MODE == "record":
        CACHE_DIR.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps({"provider": PROVIDER, "system": system, "user": user,
                                    "response": data}, indent=2, ensure_ascii=False),
                        encoding="utf-8")
    return data


# --------------------------------------------------------------------------- the guide

# Three constraint types, each quoted from the canonical GDD with the section that
# actually contains the sentence. Assignment #6 was marked down for citing a rule to a
# section that did not carry it, so every quote here was located by line number in
# goblin-siege-design-document.md before being written down.

@dataclass
class Rule:
    id: str
    kind: str                 # vocabulary | tone | formatting
    gdd: str
    summary: str
    quote: str


STYLE_RULES: list[Rule] = [
    Rule(
        id="VOCAB_NAMELESS",
        kind="vocabulary",
        gdd="1 / 2.8",
        summary=(
            "Settlements are nameless. Counties carry names, in the ascending-currency "
            "register (Groatsworth, Pennybrook, Silverford, Highpurse Keep). A generated "
            "line may never christen the hamlet."
        ),
        quote=(
            "outside a tiny, unwalled human farming hamlet - too small to have a name, one "
            "of dozens dotting the county (1); Tier names are *county* names, not town "
            "names (placeholders in the ascending-currency register): the settlements "
            "you'll eventually raid stay nameless (2.8)"
        ),
    ),
    Rule(
        id="VOCAB_SLICE",
        kind="vocabulary",
        gdd="2.3 / 2.8",
        summary=(
            "Only slice content exists. The class is the Scout, never the Slasher. No "
            "Brute, no Shaman, no palisade, no battering ram, no catapult, no Pennybrook "
            "or Silverford. The required objective trio is Market / Statue / Windmill - "
            "the granary was removed from the GDD on 2026-08-14 and does not exist. The "
            "statue is toppled, never burned."
        ),
        quote=(
            "The statue is the one target that doesn't burn: it has to be brought down, "
            "stone on stone (2.8); This hamlet carries all three required objectives in a "
            "single fixed layout - one market, one statue, one windmill (2.8)"
        ),
    ),
    Rule(
        id="TONE_DISBELIEF",
        kind="tone",
        gdd="2.10",
        summary=(
            "First contact is disbelief, not fear. The kingdom told everyone goblins were "
            "extinct, so a villager who sees one doubts their own eyes. 'Run for your "
            "lives' is the single most off-brand line this game can produce."
        ),
        quote=(
            "the kingdom told everyone goblins were extinct ... so when a goblin actually "
            "shows up, the first reaction isn't fear, it's disbelief. Villagers "
            "double-take, mutter \"must've been a badger,\" and keep pottering while their "
            "well gets fouled six feet away (2.10)"
        ),
    ),
    Rule(
        id="TONE_OVERLORD",
        kind="tone",
        gdd="2.1 / 2.10",
        summary=(
            "His Eternal Darkness is pompous, whispered and chronically underwhelmed - "
            "apocalyptic gravitas applied to stealing pigs, with the trailing '...adequate'. "
            "He mocks more than he mourns. Never generic assassin menace, never grimdark."
        ),
        quote=(
            "a whispered verdict from His Eternal Darkness (\"Adequate. I have seen rats do "
            "better. ...adequate.\") (2.1); Tier 1 stays deliberately goofy: "
            "propaganda-as-denial played as comedy, \"must've been a badger,\" an Overlord "
            "who mocks more than he mourns (2.10)"
        ),
    ),
    Rule(
        id="FORMAT_PROMPT",
        kind="formatting",
        gdd="2.8",
        summary=(
            f"A first-time prompt is one Overlord bark plus ONE HUD line naming exactly "
            f"three things: the objective, the payoff, and the one control that does it. "
            f"The HUD line is at most {MAX_HUD_CHARS} characters. No chained clauses, no "
            f"stage directions, no second control."
        ),
        quote=(
            "carries a short, dismissible prompt the first time the player meets it: a bark "
            "from His Eternal Darkness plus a one-line HUD note naming the objective, the "
            "payoff, and the one control that does it (2.8)"
        ),
    ),
    Rule(
        id="FORMAT_SILENT",
        kind="formatting",
        gdd="11 / 2.10",
        summary=(
            "Barks are text-only this slice - the project contains zero audio assets. A "
            "line may not reference a voice file, an SFX cue, or a sound. Text barks carry "
            "the comedy on their own."
        ),
        quote=(
            "barks text-only this slice - the project contains zero audio assets of any "
            "kind; His Eternal Darkness = layered whispers + subtitles (repo export 11)"
        ),
    ),
]

RULES_BY_ID = {r.id: r for r in STYLE_RULES}


def rules_digest() -> str:
    """The style guide as the agents see it. One source of truth for guide and code."""
    out = []
    for r in STYLE_RULES:
        out.append(
            f"[{r.id}] ({r.kind}, GDD {r.gdd})\n"
            f"  RULE: {r.summary}\n"
            f"  CANON: \"{r.quote}\""
        )
    return "\n\n".join(out)


# --------------------------------------------------------------------------- lint

# If code can verify it, use code. This is the project's stated principle and it is why
# a banned class name never costs an API call. These findings are handed to the
# evaluator as evidence, so a deterministic violation cannot be scored away.

BANNED_TERMS: dict[str, str] = {
    "slasher": "VOCAB_SLICE: the class is the Scout (GDD 2.3); 'Slasher' is the stale race-brief name.",
    "brute": "VOCAB_SLICE: the Brute is post-slice roadmap, not in the tutorial slice (GDD 2.3).",
    "shaman": "VOCAB_SLICE: both Shaman kits are post-slice roadmap (GDD 2.3).",
    "pennybrook": "VOCAB_SLICE: tier-2 content, not in this slice (GDD 2.8).",
    "silverford": "VOCAB_SLICE: tier-3 content, not in this slice (GDD 2.8).",
    "palisade": "VOCAB_SLICE: the slice settlement is an unwalled hamlet; palisades are tier 2 (GDD 2.8).",
    "battering ram": "VOCAB_SLICE: tier-2 breach layer, deferred from this slice (GDD 2.8).",
    "catapult": "VOCAB_SLICE: tier-2 breach layer, deferred from this slice (GDD 2.8).",
    "granary": "VOCAB_SLICE: the granary was removed from the GDD on 2026-08-14 (queue #156). The required trio is Market / Statue / Windmill.",
    "granaries": "VOCAB_SLICE: the granary was removed from the GDD on 2026-08-14 (queue #156).",
}

# Sound words that betray an audio cue in a slice with zero audio assets.
AUDIO_TERMS = ("sfx", ".wav", ".ogg", "voice line", "voiceover", "vo cue", "audio cue", "sound cue")

# A settlement given a proper name. The hamlet is nameless; these are the county names
# that ARE legal, so they must not be flagged.
LEGAL_PLACES = {"groatsworth", "highpurse keep", "highpurse"}
PLACE_PATTERN = re.compile(
    r"\b(?:village|hamlet|town|settlement)\s+of\s+([A-Z][A-Za-z'\-]+)"
    r"|\b([A-Z][a-z']+(?:brook|ford|ton|ham|wick|shire|dale|hollow|stead|bury|field))\b"
)


def style_lint(rows: list[dict]) -> list[dict]:
    """Deterministic checks. No API calls, no cost, runs every round."""
    findings: list[dict] = []
    for row in rows:
        name = row.get("name") or row.get("trigger") or "row"
        blob = " ".join(
            str(v) for k, v in row.items() if k not in ("sources", "name")
        )
        low = blob.lower()

        for term, why in BANNED_TERMS.items():
            if re.search(rf"\b{re.escape(term)}\b", low):
                findings.append({
                    "row": name, "rule_id": "VOCAB_SLICE", "quote": term,
                    "why": why, "by": "lint",
                })

        for term in AUDIO_TERMS:
            if term in low:
                findings.append({
                    "row": name, "rule_id": "FORMAT_SILENT", "quote": term,
                    "why": "FORMAT_SILENT: this slice has zero audio assets; barks are text-only.",
                    "by": "lint",
                })

        # The ceiling is the HUD note's, not the bark's. GDD 2.8 caps "a one-line HUD note";
        # a spoken bark is bounded by being sayable, which is a judgement, not a count.
        hud = str(row.get("hud_line", "") or "")
        if hud and len(hud) > MAX_HUD_CHARS:
            findings.append({
                "row": name, "rule_id": "FORMAT_PROMPT", "quote": hud[:60] + "...",
                "why": (f"FORMAT_PROMPT: the HUD note is {len(hud)} characters; GDD 2.8 calls "
                        f"for one HUD line and the ceiling for this project is {MAX_HUD_CHARS}."),
                "by": "lint",
            })

        for m in PLACE_PATTERN.finditer(blob):
            place = (m.group(1) or m.group(2) or "").strip()
            if place and place.lower() not in LEGAL_PLACES:
                findings.append({
                    "row": name, "rule_id": "VOCAB_NAMELESS", "quote": place,
                    "why": ("VOCAB_NAMELESS: the hamlet is 'too small to have a name' (GDD 1). "
                            "Only counties are named, in the ascending-currency register."),
                    "by": "lint",
                })
    return findings


# --------------------------------------------------------------------------- agents

# GDD 2.8 specifies a first-time prompt as TWO things - "a bark from His Eternal Darkness
# PLUS a one-line HUD note" - so they are two fields. Collapsing them into one was a real
# bug: the character ceiling belongs to the HUD note alone, and measuring bark+HUD against
# it made the format case unsatisfiable no matter what the refiner wrote. The banked
# prompts.csv already had this shape (overlord_bark, hud_line); this now matches it.
ROW_PROPS = {
    "speaker": {"type": "string", "description": "who says it (Overlord, Watchman, Militia, Civilian, HUD)"},
    "trigger": {"type": "string", "description": "the game event that fires this line"},
    "text": {"type": "string", "description": "the spoken bark, exactly as the player hears it"},
    "hud_line": {"type": "string",
                 "description": f"the one-line HUD note (max {MAX_HUD_CHARS} chars) if this row is a "
                                f"first-time prompt, otherwise an empty string"},
    "control": {"type": "string", "description": "the one control this teaches, or an empty string"},
}
ROW_SCHEMA = _items_schema(ROW_PROPS, ["speaker", "trigger", "text", "hud_line", "control"])

GENERATOR_SYSTEM = """You are a content generator producing draft in-game text for a video game.

Write the rows you are asked for. Follow the user's brief exactly, including its tone. Do
not self-censor toward some other house style and do not add caveats - a later reviewer
handles style. Return the requested number of rows."""

EVAL_SYSTEM = f"""You are the Style Steward for the video game Goblin Siege. You grade draft
in-game text against the game's own style guide and nothing else.

THE STYLE GUIDE (each rule quotes the design document it comes from):

{rules_digest()}

You are given draft rows, retrieved design-document canon, and any deterministic lint
findings already proven by code.

Score EVERY rule in the guide from 1 to 10 - all of them, including the ones that pass:

  10 = indistinguishable from shipped Goblin Siege content; this rule fully satisfied.
   7-9 = on-brand, with minor drift on this rule.
   4-6 = recognisably this game but breaking this rule outright.
   1-3 = generic fantasy content that could belong to any game.

You do NOT assign an overall score. The overall score is computed from your per-rule scores
by code, so that the same judgements always produce the same number. Your job is the six
judgements and the reason.

Rules:
- A deterministic lint finding is proven. You may not score it away, and your reason must
  name it. Text with an unresolved lint finding cannot score above 6.
- Cite the chunk id that proves a rule whenever the canon supports it. Ground your reasons.
- Your REASON must be specific enough for a rewriter to act on without seeing your notes:
  name the offending row, quote the offending text, and say what the game requires instead.
- Do not invent problems to look thorough. If the text is genuinely on-brand, say 10 and
  say why. An evaluator that flags everything is as useless as one that flags nothing."""

EVAL_SCHEMA = {
    "type": "object",
    "properties": {
        "reason": {"type": "string",
                   "description": "why that score - specific enough to rewrite from, naming rows and quoting offending text"},
        "per_rule": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "rule_id": {"type": "string"},
                    "rule_score": {"type": "integer", "minimum": 1, "maximum": 10},
                    "verdict": {"type": "string", "enum": ["PASS", "FAIL"]},
                    "row": {"type": "string", "description": "offending row, or empty if the rule passed"},
                    "quote": {"type": "string", "description": "the offending text, or empty"},
                    "required_instead": {"type": "string", "description": "what the game requires, or empty"},
                    "evidence_chunk_id": {"type": "string", "description": "chunk id proving the rule, or 'lint'"},
                },
                "required": ["rule_id", "rule_score", "verdict", "row", "quote",
                             "required_instead", "evidence_chunk_id"],
                "additionalProperties": False,
            },
        },
    },
    "required": ["reason", "per_rule"],
    "additionalProperties": False,
}

REFINER_SYSTEM = f"""You are the Refiner for the video game Goblin Siege. You rewrite draft
in-game text so that it scores a perfect 10/10 against the game's style guide.

THE STYLE GUIDE:

{rules_digest()}

You are given the draft, the Style Steward's SCORE and REASON, and the retrieved canon.
The REASON is your instruction set: fix everything it names.

- Return the FULL set of rows. Change only what the reason names; every other row comes
  back byte-identical, including its trigger.
- Keep each row's trigger and speaker unchanged. You are rewriting the line, not the event.
- Do not merely delete an offending word. Replace it with what the game actually has - a
  banned objective becomes the real one, a fearful line becomes a disbelieving one.
- Write in the game's voice: satire, not grimdark. Comedy is the point."""


def generate(brief: str, n: int, ctx: str) -> list[dict]:
    """The Generator. Deliberately naive about house style - that is the demo."""
    user = (
        f"Write exactly {n} rows of in-game text.\n\n"
        f"BRIEF:\n{brief}\n\n"
        f"Reference material about the game (cite chunk ids in 'sources'):\n\n{ctx}"
    )
    data = call_json(GENERATOR_SYSTEM, user, ROW_SCHEMA, effort="medium")
    return data["items"]


LINT_CAP = 6      # a draft with a proven deterministic violation cannot score above this


def aggregate_score(per_rule: list[dict], lint_findings: list[dict]) -> int:
    """
    Compute the headline score from the per-rule judgements. Code, not vibes.

    The guide is a CONJUNCTION - content that breaks one rule is off-brand however well it
    does on the other five - so the overall score is the weakest rule, then hard-capped if
    deterministic lint found anything. Both of those used to be asked of the model in prose;
    asking politely for arithmetic is how a headline score comes back as 3 when the per-rule
    scores were 10/10/1/2/3/10, and how the same draft scores differently twice.
    """
    scores = [int(p["rule_score"]) for p in per_rule if "rule_score" in p]
    score = min(scores) if scores else 1
    if lint_findings:
        score = min(score, LINT_CAP)
    return max(1, min(10, score))


def evaluate(rows: list[dict], lint_findings: list[dict], ctx: str) -> dict:
    """The Evaluator. Returns SCORE + REASON; the SCORE is computed, not chosen."""
    user = (
        "Grade this draft against the style guide.\n\n"
        f"DRAFT ROWS:\n{json.dumps(rows, indent=2, ensure_ascii=False)}\n\n"
        f"DETERMINISTIC LINT FINDINGS (already proven by code - you may not score these away):\n"
        f"{json.dumps(lint_findings, indent=2, ensure_ascii=False) if lint_findings else '(none)'}\n\n"
        f"RETRIEVED CANON:\n\n{ctx}"
    )
    ev = call_json(EVAL_SYSTEM, user, EVAL_SCHEMA, effort="high")
    ev["per_rule"] = ev.get("per_rule", [])
    ev["score"] = aggregate_score(ev["per_rule"], lint_findings)
    ev["scored_by"] = "min(per_rule)" + (f" capped at {LINT_CAP} by lint" if lint_findings else "")
    return ev


def refine(rows: list[dict], ev: dict, ctx: str) -> list[dict]:
    """The Refiner. Rewrites from the evaluator's REASON."""
    user = (
        f"The Style Steward scored your draft {ev['score']}/10.\n\n"
        f"REASON:\n{ev['reason']}\n\n"
        f"PER-RULE BREAKDOWN:\n{json.dumps(ev.get('per_rule', []), indent=2, ensure_ascii=False)}\n\n"
        f"YOUR DRAFT:\n{json.dumps(rows, indent=2, ensure_ascii=False)}\n\n"
        f"RETRIEVED CANON:\n\n{ctx}\n\n"
        "Rewrite so the draft scores 10/10. Return all rows."
    )
    data = call_json(REFINER_SYSTEM, user, ROW_SCHEMA, effort="high")
    return data["items"]


# --------------------------------------------------------------------------- cases

@dataclass
class Case:
    key: str
    title: str
    violation: str
    queries: list[str]
    n_items: int = 3
    brief: str = ""
    banked: list[tuple[str, str]] = field(default_factory=list)


CASES: dict[str, Case] = {
    "tone": Case(
        key="tone",
        title="Tone / register - fear where the game requires disbelief",
        violation="TONE_DISBELIEF, TONE_OVERLORD",
        n_items=3,
        queries=[
            "tone comedy satire propaganda denial badger disbelief villagers",
            "civilians panic alarm reaction goblin sighting",
            "His Eternal Darkness whispers verdict adequate rats register",
        ],
        brief=(
            "Write villager reaction barks for a dark fantasy raid game. A monstrous goblin "
            "raider has just been spotted in the village at dusk. The villagers are "
            "terrified for their lives. Make the lines desperate and frightened, full of "
            "dread and screaming. Also write one line for the goblin's demonic overlord "
            "that is genuinely menacing and threatening."
        ),
    ),
    "vocab": Case(
        key="vocab",
        title="Vocabulary / lore - banked content still names the retired granary",
        violation="VOCAB_SLICE, VOCAB_NAMELESS",
        queries=[
            "tutorial hamlet objectives market statue windmill required trio optional fields",
            "statue king propaganda toppled brought down stone",
            "settlements nameless county names register",
        ],
        # Not staged. These rows were really in out/*.csv and really went stale when
        # queue #156 removed the granary from the GDD on 2026-08-14.
        #
        # FIXED 2026-08-19 (#200): the stale rows were regenerated off this case's own output, so
        # load_banked() now finds NOTHING and re-running --case vocab yields an empty set. That is
        # the case succeeding, not breaking - the demonstration is preserved in
        # out/style/vocab.trace.json and DEMOS.md (2/10 -> 9/10). Do not "repair" it by putting the
        # granary back into the shipped CSVs. If a fresh vocabulary demo is ever needed, point this
        # at whatever is stale THEN; today that is the prisoner/capture rows (bind was cut
        # 2026-08-14) and the well/bucket-brigade rows (deferred 2026-08-19).
        banked=[("prompts.csv", "Granary"), ("whispers.csv", "Granary")],
    ),
    "format": Case(
        key="format",
        title="Formatting / length - a HUD line that is a paragraph",
        violation="FORMAT_PROMPT",
        n_items=3,
        queries=[
            "tutorial prompt HUD note objective payoff one control dismissible",
            "windmill stage ablaze detonation window torch",
            "controls bindings interact traverse torch horn",
        ],
        brief=(
            "Write first-time tutorial prompts for a video game. Be thorough and helpful: "
            "each HUD line should fully explain the objective, why it matters, every "
            "control the player might need, and what happens afterwards. Do not worry "
            "about length - completeness matters more than brevity. Mention the sound "
            "cue that plays with each prompt."
        ),
    ),
}


def load_banked(case: Case) -> list[dict]:
    """Real rows off the banked CSVs - the content that is actually stale in the repo."""
    rows: list[dict] = []
    for fname, needle in case.banked:
        path = OUT / fname
        if not path.exists():
            print(f"  ! banked source missing: {path}")
            continue
        with path.open(encoding="utf-8", newline="") as fh:
            for rec in csv.DictReader(fh):
                blob = " ".join(rec.values())
                if needle.lower() not in blob.lower():
                    continue
                rows.append({
                    "name": rec.get("Name", ""),
                    "speaker": rec.get("speaker") or ("HUD" if rec.get("hud_line") else "Overlord"),
                    "trigger": rec.get("trigger", ""),
                    "text": rec.get("overlord_bark") or rec.get("line") or "",
                    "hud_line": rec.get("hud_line", ""),
                    "control": rec.get("control", ""),
                    "sources": (rec.get("sources", "") or "").split(),
                })
    return rows


# --------------------------------------------------------------------------- the loop


def run_case(case: Case, retr: Retriever, threshold: int, max_refines: int) -> dict:
    print(f"\n=== {case.key}: {case.title}")
    chunks, rtrace = retr.multi_search(case.queries, k_per=4, k_total=16)
    ctx = render_context(chunks)
    print(f"  retrieved {len(chunks)} chunks")

    if case.banked:
        rows = load_banked(case)
        print(f"  loaded {len(rows)} REAL banked row(s) from {', '.join(f for f, _ in case.banked)}")
    else:
        rows = generate(case.brief, case.n_items, ctx)
        print(f"  generated {len(rows)} row(s) from an off-brand brief")

    before = deepcopy(rows)
    history: list[dict] = []
    outcome = "UNRESOLVED"

    # The loop always terminates on an EVALUATION, never a refinement, so the text that
    # ships is text that was scored. gsrag.py records why: a revision once "fixed" a wrong
    # patrol cadence to a different wrong value, and only the next check caught it.
    for attempt in range(1, max_refines + 2):
        lint_findings = style_lint(rows)
        ev = evaluate(rows, lint_findings, ctx)
        verify_only = attempt > max_refines
        history.append({
            "attempt": attempt,
            "score": ev["score"],
            "reason": ev["reason"],
            "per_rule": ev.get("per_rule", []),
            "scored_by": ev.get("scored_by", ""),
            "lint": lint_findings,
            "verify_only": verify_only,
            "rows": deepcopy(rows),
        })
        print(f"  attempt {attempt}: SCORE {ev['score']}/10"
              f"{' (verify only)' if verify_only else ''}"
              f"{f' [{len(lint_findings)} lint]' if lint_findings else ''}")
        print(f"    REASON: {ev['reason'][:160]}{'...' if len(ev['reason']) > 160 else ''}")

        if ev["score"] >= threshold and not lint_findings:
            outcome = "PASS"
            break
        if verify_only:
            print(f"    UNRESOLVED after {max_refines} refinement(s) - reporting rather than shipping")
            break
        rows = refine(rows, ev, ctx)

    return {
        "case": case.key,
        "title": case.title,
        "violation_class": case.violation,
        "model": MODEL if PROVIDER == "api" else "claude-cli",
        "threshold": threshold,
        "outcome": outcome,
        "source": "banked" if case.banked else "generated",
        "brief": case.brief,
        "retrieval": rtrace,
        "before": before,
        "after": rows,
        "history": history,
        "first_score": history[0]["score"] if history else None,
        "final_score": history[-1]["score"] if history else None,
    }


# --------------------------------------------------------------------------- output


def write_case(result: dict) -> Path:
    STYLE_OUT.mkdir(parents=True, exist_ok=True)
    path = STYLE_OUT / f"{result['case']}.trace.json"
    path.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    return path


def _fmt_rows(rows: list[dict]) -> str:
    out = []
    for r in rows:
        ctrl = "  `[" + r["control"] + "]`" if r.get("control") else ""
        seg = ["- **" + r.get("speaker", "?") + "** / `" + r.get("trigger", "?") + "`" + ctrl,
               "  > " + (r.get("text", "") or "")]
        hud = r.get("hud_line") or ""
        if hud:
            seg.append("  >")
            seg.append("  > `HUD (" + str(len(hud)) + " chars):` " + hud)
        out.append(chr(10).join(seg))
    return (chr(10) * 2).join(out)


def write_demos(results: list[dict]) -> Path:
    md = [
        "# Before / After - the Style Guide Agent in action",
        "",
        "**Goblin Siege** - Assignment #7. Generated by `gsstyle.py`; every number below is "
        "from the run recorded in `out/style/*.trace.json`.",
        "",
        "The loop is unattended: `python gsstyle.py --case all` runs "
        "Generator -> Evaluator -> Refiner -> Evaluator to termination with no human in it.",
        "",
    ]
    for r in results:
        md += [
            f"## {r['case'].title()} - {r['title']}",
            "",
            f"- **Violation class:** {r['violation_class']}",
            f"- **Input:** {'REAL banked content from out/*.csv' if r['source'] == 'banked' else 'generated from a deliberately off-brand brief'}",
            f"- **Score:** {r['first_score']}/10 -> **{r['final_score']}/10**  ({r['outcome']})",
            "",
        ]
        if r["brief"]:
            md += ["**The off-brand brief given to the Generator:**", "", f"> {r['brief']}", ""]
        md += ["### BEFORE", "", _fmt_rows(r["before"]), ""]
        first = r["history"][0]
        md += [
            "### EVALUATOR",
            "",
            f"**SCORE: {first['score']}/10**",
            "",
            f"**REASON:** {first['reason']}",
            "",
        ]
        fails = [p for p in first["per_rule"] if p["verdict"] == "FAIL"]
        if fails:
            md += ["| Rule | Score | Offending text | Required instead | Evidence |",
                   "|---|---|---|---|---|"]
            for p in fails:
                md.append(f"| `{p['rule_id']}` | {p['rule_score']}/10 | {p['quote'][:70]} | "
                          f"{p['required_instead'][:70]} | `{p['evidence_chunk_id']}` |")
            md.append("")
        if first["lint"]:
            md += ["**Deterministic lint (no API call):**", ""]
            for f in first["lint"]:
                md.append(f"- `{f['rule_id']}` on **{f['row']}** - \"{f['quote']}\" - {f['why']}")
            md.append("")
        md += ["### AFTER", "", _fmt_rows(r["after"]), "",
               f"Re-scored **{r['final_score']}/10** by the same evaluator on the following pass.",
               "", "---", ""]

    path = ROOT / "DEMOS.md"
    path.write_text("\n".join(md), encoding="utf-8")
    return path


# --------------------------------------------------------------------------- cli


def main() -> None:
    global PROVIDER, CACHE_MODE
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--case", default="all", help="tone | vocab | format | all")
    ap.add_argument("--list", action="store_true", help="print the rules and cases, no API calls")
    ap.add_argument("--dry-run", action="store_true",
                    help="retrieval + deterministic lint only, no API calls, no cost")
    ap.add_argument("--replay", action="store_true",
                    help="reproduce a recorded run from cache; never calls the model")
    ap.add_argument("--no-cache", action="store_true", help="ignore the cache entirely")
    ap.add_argument("--rebuild-demos", action="store_true",
                    help="regenerate DEMOS.md from saved traces, no API calls")
    ap.add_argument("--negative-control", action="store_true",
                    help="score known-good shipped barks; proves the evaluator can also say 10")
    ap.add_argument("--provider", default=PROVIDER, choices=["claude-cli", "api"],
                    help="claude-cli bills the Max subscription; api needs a valid key")
    ap.add_argument("--threshold", type=int, default=PASS_THRESHOLD)
    ap.add_argument("--rounds", type=int, default=MAX_REFINEMENTS,
                    help="max refinements before reporting UNRESOLVED")
    args = ap.parse_args()
    PROVIDER = args.provider
    CACHE_MODE = "replay" if args.replay else ("off" if args.no_cache else "record")

    if args.rebuild_demos:
        res = [json.loads((STYLE_OUT / f"{k}.trace.json").read_text(encoding="utf-8"))
               for k in CASES if (STYLE_OUT / f"{k}.trace.json").exists()]
        print("rebuilt " + str(write_demos(res)) + " from " + str(len(res)) + " trace(s)")
        return

    if args.list:
        print(f"STYLE GUIDE - {len(STYLE_RULES)} rules, "
              f"{len({r.kind for r in STYLE_RULES})} constraint types\n")
        for r in STYLE_RULES:
            print(f"  [{r.id}] {r.kind} (GDD {r.gdd})\n      {r.summary}\n")
        print(f"CASES - {len(CASES)}\n")
        for c in CASES.values():
            src = "real banked content" if c.banked else "generated off-brand"
            print(f"  {c.key:8s} {c.title}\n           violation: {c.violation}  [{src}]\n")
        return

    keys = list(CASES) if args.case == "all" else [args.case]
    for k in keys:
        if k not in CASES:
            sys.exit(f"unknown case '{k}' (have: {', '.join(CASES)}, or 'all')")

    if not args.dry_run and PROVIDER == "api":
        load_dotenv()

    chunks = load_corpus()
    retr = Retriever(chunks)
    print(f"knowledge base: {len(chunks)} chunks | provider: {PROVIDER} | cache: {CACHE_MODE}")

    if args.dry_run:
        print("\n--dry-run: retrieval + deterministic lint only, no API calls\n")
        for k in keys:
            c = CASES[k]
            got, _ = retr.multi_search(c.queries, k_per=4, k_total=16)
            print(f"  {c.key:8s} retrieved {len(got)} chunks")
            if c.banked:
                rows = load_banked(c)
                findings = style_lint(rows)
                print(f"           {len(rows)} banked row(s), {len(findings)} lint finding(s)")
                for f in findings:
                    print(f"             - {f['rule_id']} on {f['row']}: \"{f['quote']}\"")
        return

    if args.negative_control:
        # An evaluator that flags everything is as useless as one that flags nothing. Feed it
        # content that already passed the existing critic and confirm it scores high.
        rows = []
        with (OUT / "barks.csv").open(encoding="utf-8", newline="") as fh:
            for rec in csv.DictReader(fh):
                if "granary" in " ".join(rec.values()).lower():
                    continue
                rows.append({"name": rec["Name"], "speaker": rec.get("speaker", ""),
                             "trigger": rec.get("trigger", ""), "text": rec.get("line", ""),
                             "hud_line": "", "control": "",
                             "sources": (rec.get("sources", "") or "").split()})
                if len(rows) == 5:
                    break
        got, _ = retr.multi_search(CASES["tone"].queries, k_per=4, k_total=16)
        findings = style_lint(rows)
        ev = evaluate(rows, findings, render_context(got))
        print(f"\nNEGATIVE CONTROL - {len(rows)} shipped barks, {len(findings)} lint finding(s)")
        print(f"  SCORE {ev['score']}/10")
        print(f"  REASON: {ev['reason'][:400]}")
        print(f"\n{'PASS' if ev['score'] >= args.threshold else 'FAIL'} - the evaluator "
              f"{'can' if ev['score'] >= args.threshold else 'CANNOT'} recognise on-brand content")
        return

    t0 = time.time()
    results = [run_case(CASES[k], retr, args.threshold, args.rounds) for k in keys]
    for r in results:
        p = write_case(r)
        print(f"\nwrote {p.relative_to(ROOT)}")
    dp = write_demos(results)
    print(f"wrote {dp.name}")

    print(f"{chr(10)}done in {time.time() - t0:.0f}s - "
          f"{USAGE['calls']} live call(s), "
          f"{USAGE['cache_hits']} cache hit(s), cache={CACHE_MODE}")
    for r in results:
        print(f"  {r['case']:8s} {r['first_score']}/10 -> {r['final_score']}/10  {r['outcome']}")


if __name__ == "__main__":
    main()
