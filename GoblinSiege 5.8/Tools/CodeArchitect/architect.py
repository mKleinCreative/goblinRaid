#!/usr/bin/env python3
"""Code Architect — goal-oriented coding agent for Goblin Siege (Assignment #5).

receive -> scan -> diff -> score -> build -> remember

  receive   GDD parsed for the feature inventory (gdd.py)
  scan      live-tree perception, never doc trust (perception.py — shared module)
  diff      evidence check per feature (scorer.detect_and_score)
  score     utility ranking with requires_editor + dependency gates
  build     LLM codegen for the top eligible gap, STAGED for review (generator.py)
  remember  AGENT_STATE.md rewritten (state.py); blackboard logs every step first

Usage:
  python architect.py --project-root "D:\\goblinRaid\\GoblinSiege 5.8"            # full run
  python architect.py --project-root ... --scan-only                               # perception + ranking only
  python architect.py --project-root ... --ensure-editor                           # reopen editor if closed
  python architect.py --project-root ... --provider fixture                        # offline/demo
"""
from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from ca import editor as editor_mod
from ca import gdd as gdd_mod
from ca import state as state_mod
from ca.blackboard import Blackboard
from ca.config import Config
from ca.generator import generate
from ca.llm import Provider
from ca.perception import scan
from ca.scorer import detect_and_score, load_features, pick


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-root", required=True)
    ap.add_argument("--provider", default="auto", choices=["auto", "anthropic", "claude-cli", "fixture"],
                    help="auto = API key if present, else headless Claude Code (`claude -p`, "
                         "billed to the Max subscription), else fixture")
    ap.add_argument("--scan-only", action="store_true")
    ap.add_argument("--ensure-editor", action="store_true",
                    help="reopen the Unreal editor if it is not running, then exit")
    ap.add_argument("--build-and-relaunch", action="store_true",
                    help="run the editor-closed build then relaunch (BuildAndLaunchGame.ps1 path)")
    ap.add_argument("--run-id", default=time.strftime("%Y%m%d-%H%M%S"))
    args = ap.parse_args(argv)

    if args.ensure_editor:
        ok = editor_mod.ensure_editor(wait_seconds=60)
        print(f"[editor] running: {ok}")
        return 0 if ok else 1
    if args.build_and_relaunch:
        return editor_mod.build_and_relaunch()

    cfg = Config(project_root=args.project_root, provider=args.provider, run_id=args.run_id)
    bb = Blackboard(cfg.out_dir, args.run_id)

    # RECEIVE — parse the GDD, cross-check the knowledge file
    model = gdd_mod.parse(cfg.gdd_path)
    features = load_features(cfg.features_path)
    drift = gdd_mod.coverage_check(model, features)
    bb.section("WHAT IT RECEIVED")
    bb.line(f"- GDD: `{cfg.gdd_path}` — parsed **{len(model.systems)}** systems from §12.1, "
            f"{len(model.blocks)} blocks from §12.2")
    for w in model.warnings + drift:
        bb.line(f"- WARNING: {w}")
    if drift:
        bb.promote("GDD/features.json drift", "\n".join(f"- {w}" for w in drift))

    # SCAN
    rep = scan(cfg)
    bb.perceived(rep)

    # DIFF + SCORE
    ranked = detect_and_score(features, rep)
    bb.scored(ranked)
    open_gaps = [g for g in ranked if g.missing or g.stubbed]
    print(f"[architect] {len(open_gaps)} open gaps; top 5:")
    for g in open_gaps[:5]:
        print(f"   u={g.utility:<6} [{'E' if g.eligible else ' '}] {g.name}")

    if args.scan_only:
        state_mod.update_after_run(cfg.state_path, args.run_id, ranked, [],
                                   [], f"scan-only: {len(open_gaps)} open gaps")
        print(f"[architect] blackboard: {bb.md_path}")
        return 0

    # PICK + BUILD
    target = pick(ranked)
    if target is None:
        bb.line("\n**No eligible gap — everything open is editor-gated or blocked.**")
        bb.promote("No headless work available",
                   "All remaining gaps need a supervised editor session or a prerequisite. "
                   "See the ranking table.")
        state_mod.update_after_run(cfg.state_path, args.run_id, ranked, [], [],
                                   "no eligible gap")
        return 0

    bb.section("WHAT IT PICKED")
    bb.line(f"**{target.name}** — utility {target.utility}. {target.rationale}")
    provider = Provider(cfg.provider, cfg.model)
    if provider.kind != cfg.provider:
        bb.line(f"\n> NOTE: provider '{cfg.provider}' resolved to '{provider.kind}'"
                + (" — claude-cli bills the Max subscription, not API credits." if provider.kind == "claude-cli" else
                   " — fixture replay; no live generation." if provider.kind == "fixture" else ""))
    failed: list[str] = []
    built: list[str] = []
    try:
        result = generate(target, rep, cfg, bb, provider)
        built.append(f"STAGED {target.name}: {len(result.get('files', {}))} files at "
                     f"out/runs/{args.run_id}/staging/ — awaiting Michael's review, then an "
                     f"editor-closed full build (new UCLASS types; Live Coding cannot register them)")
        bb.promote(
            f"Review staged code: {target.name}",
            f"{len(result.get('files', {}))} generated files staged under "
            f"`out/runs/{args.run_id}/staging/`. Read every file before it goes into Source/ — "
            "\"matches patterns is not the same as matches design intent.\" Notes and patch "
            "instructions for existing files are in NOTES.md alongside the staged code.")
    except Exception as e:  # log the failure so the next run doesn't repeat it
        failed.append(f"generation of {target.key} failed: {e}")
        bb.line(f"\n**GENERATION FAILED:** {e}")

    state_mod.update_after_run(
        cfg.state_path, args.run_id, ranked, built, failed,
        f"picked '{target.name}' (u={target.utility}); {'ok' if not failed else 'FAILED'}")
    print(f"[architect] blackboard: {bb.md_path}")
    print(f"[architect] state:      {cfg.state_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
