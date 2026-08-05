"""Provider abstraction — anthropic | claude-cli | fixture (+ auto resolution).

anthropic   direct SDK calls; needs ANTHROPIC_API_KEY = separate Claude-Platform
            pay-as-you-go billing (NOT included in a claude.ai Max subscription).
claude-cli  shells out to headless Claude Code (`claude -p`), which draws from
            Michael's MAX SUBSCRIPTION usage limits instead of API credits —
            the right default on doomsday, where Claude Code is installed and
            logged in (support.claude.com article 15036540: Agent SDK and
            `claude -p` usage draws from subscription limits).
auto        anthropic if a key+SDK exist, else claude-cli if the CLI is on
            PATH, else fixture.

Mirrors Bark Foundry's pattern: a missing key or SDK falls back rather than
failing, retries with backoff on transient errors, tolerant JSON extraction.
"""
from __future__ import annotations

import json
import os
import random
import re
import shutil
import subprocess
import time
from pathlib import Path

FIXTURE_DIR = Path(__file__).resolve().parent.parent / "fixtures"


class ProviderError(RuntimeError):
    pass


def extract_json(text: str) -> dict:
    """Tolerate code fences and prose wrappers around a JSON object.

    Brace counting is NOT safe here: the values are C++ source, so one unbalanced
    `}` inside a string literal or comment closes the object early and the slice
    ends mid-string (how live-002 died). Let the real decoder find the extent.
    """
    if not text or not text.strip():
        raise ProviderError("empty model output")
    candidates = []
    m = re.search(r"```(?:json)?\s*(\{.*\})\s*```", text, re.DOTALL)
    if m:
        candidates.append(m.group(1))
    candidates.append(text)

    dec = json.JSONDecoder()
    err: Exception | None = None
    fallback: dict | None = None
    for cand in candidates:
        idx, tries = cand.find("{"), 0
        while idx != -1 and tries < 64:  # bounded: `{` is everywhere in C++ payloads
            try:
                obj, _ = dec.raw_decode(cand[idx:])
                if isinstance(obj, dict):
                    if "files" in obj or "notes" in obj:  # the output contract
                        return obj
                    if fallback is None and tries == 0:
                        fallback = obj
            except json.JSONDecodeError as e:
                err = e
            idx, tries = cand.find("{", idx + 1), tries + 1
    if fallback is not None:
        return fallback
    if "{" not in text:
        raise ProviderError("no JSON object in model output")
    raise ProviderError(f"no parseable JSON object in model output ({err})")


def _cli_path() -> str | None:
    hit = shutil.which("claude") or shutil.which("claude.cmd")
    if hit:
        return hit
    # Standard install spots, in case the shell's PATH hasn't picked them up
    # (native installer -> ~/.local/bin; npm global -> %APPDATA%/npm).
    home = Path.home()
    for cand in (home / ".local" / "bin" / "claude.exe",
                 home / ".local" / "bin" / "claude",
                 Path(os.environ.get("APPDATA", "")) / "npm" / "claude.cmd"):
        if cand.exists():
            return str(cand)
    return None


class Provider:
    def __init__(self, kind: str, model: str):
        self.kind = kind
        self.model = model
        self._client = None
        if kind == "auto":
            if os.environ.get("ANTHROPIC_API_KEY"):
                kind = self.kind = "anthropic"
            elif _cli_path():
                self.kind = "claude-cli"
            else:
                self.kind = "fixture"
        if kind == "anthropic":
            try:
                import anthropic  # type: ignore
                if not os.environ.get("ANTHROPIC_API_KEY"):
                    raise ProviderError("ANTHROPIC_API_KEY not set")
                self._client = anthropic.Anthropic()
            except (ImportError, ProviderError):
                # deliberate non-failure path: subscription CLI first, fixture last
                self.kind = "claude-cli" if _cli_path() else "fixture"
        if kind == "claude-cli" and not _cli_path():
            self.kind = "fixture"

    def describe(self, fixture_name: str = "") -> str:
        """What actually served this call — the blackboard must not claim 'fixture'
        for a live claude-cli run (live-002 audit trail did exactly that)."""
        if self.kind == "anthropic":
            return f"anthropic:{self.model}"
        if self.kind == "claude-cli":
            return f"claude-cli:{os.environ.get('CA_CLI_MODEL') or 'default'}"
        return f"fixture:{fixture_name}"

    def complete(self, system: str, user: str, fixture_name: str, max_tokens: int = 16000) -> str:
        if self.kind == "claude-cli":
            # Headless Claude Code: billed against the Max subscription, not API credits.
            # Prompt over stdin (Windows arg-length limits); plain text out; no session reuse.
            cmd = [_cli_path(), "-p", "--output-format", "text",
                   "--append-system-prompt", system]
            if os.environ.get("CA_CLI_MODEL"):
                cmd += ["--model", os.environ["CA_CLI_MODEL"]]
            res = subprocess.run(cmd, input=user, capture_output=True, text=True,
                                 encoding="utf-8", errors="replace", timeout=900)
            if res.returncode != 0:
                raise ProviderError(f"claude -p failed ({res.returncode}): {res.stderr[:500]}")
            if not res.stdout.strip():
                raise ProviderError(f"claude -p returned empty stdout (stderr: {res.stderr[:300]!r})")
            return res.stdout
        if self.kind == "fixture":
            p = FIXTURE_DIR / f"{fixture_name}.json"
            if not p.exists():
                raise ProviderError(f"fixture provider active but no fixture at {p}")
            return json.loads(p.read_text(encoding="utf-8"))["response"]
        last = None
        for attempt in range(4):
            try:
                msg = self._client.messages.create(
                    model=self.model, max_tokens=max_tokens,
                    system=system, messages=[{"role": "user", "content": user}])
                return "".join(b.text for b in msg.content if getattr(b, "type", "") == "text")
            except Exception as e:  # transient classification, Bark Foundry style
                name = type(e).__name__
                if name in ("RateLimitError", "APIConnectionError", "InternalServerError", "APITimeoutError"):
                    last = e
                    time.sleep((2 ** attempt) + random.random())
                    continue
                raise
        raise ProviderError(f"exhausted retries: {last}")
