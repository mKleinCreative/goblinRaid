"""Offline verification suite — no network, no editor. Run: python -m pytest tests/ -q
Uses the real staged repo snapshot when present (CA_TEST_PROJECT_ROOT), else builds a
minimal synthetic tree exercising the same paths."""
import json
import os
import sys
from pathlib import Path

import pytest

TOOL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(TOOL))

from ca.config import Config
from ca.perception import scan
from ca.scorer import detect_and_score, load_features, pick
from ca import gdd as gdd_mod
from ca import editor as editor_mod
from ca import state as state_mod
from ca.blackboard import Blackboard

SNAPSHOT = Path(os.environ.get(
    "CA_TEST_PROJECT_ROOT",
    "/mnt/user-data/uploads/goblinRaid/GoblinSiege 5.8"))

needs_snapshot = pytest.mark.skipif(not SNAPSHOT.exists(), reason="repo snapshot not staged")


@pytest.fixture(scope="module")
def cfg(tmp_path_factory):
    out = tmp_path_factory.mktemp("out")
    c = Config(project_root=SNAPSHOT, provider="fixture")
    c.out_dir = out
    c.state_path = out / "AGENT_STATE.md"
    return c


@pytest.fixture(scope="module")
def report(cfg):
    return scan(cfg)


@needs_snapshot
def test_perception_finds_wired_classes(report):
    assert report.class_status("UGSFlammableComponent") == "wired"
    assert report.class_status("AGSFieldFireObjective") == "wired"
    assert report.class_status("AGSPlayerCharacter") == "wired"


@needs_snapshot
def test_perception_flags_stubs_and_missing(report):
    # GSHordeGoblin.cpp is 225B — scaffold by any measure
    assert report.class_status("AGSHordeGoblin") in ("stubbed", "header-only")
    # The interact framework does not exist anywhere in Source
    for cls in ("UGSInteractableComponent", "UGSInteractionComponent", "UGSGA_Interact", "UGSCarryComponent"):
        assert not report.has_class(cls), f"{cls} unexpectedly present — update features.json expectations"


@needs_snapshot
def test_gdd_parses_systems_inventory(cfg):
    model = gdd_mod.parse(cfg.gdd_path)
    assert len(model.systems) >= 15, model.warnings
    ids = {s.sys_id for s in model.systems}
    assert {"8", "6", "5b"} <= ids
    assert model.blocks.get("A") and "interact" in model.blocks["A"].lower()


@needs_snapshot
def test_scorer_ranks_interact_framework_first(cfg, report):
    ranked = detect_and_score(load_features(cfg.features_path), report)
    top = pick(ranked)
    assert top is not None
    assert top.key == "interact_framework", (
        f"expected interact_framework, got {top.key} — ranking: "
        + ", ".join(f"{g.key}:{g.utility}" for g in ranked[:5]))
    # requires_editor features must never be pickable
    for g in ranked:
        if g.requires_editor:
            assert not g.eligible


@needs_snapshot
def test_blackboard_orders_scores_before_generation(cfg, report, tmp_path):
    bb = Blackboard(tmp_path, "test-run")
    ranked = detect_and_score(load_features(cfg.features_path), report)
    bb.scored(ranked)
    bb.issued("generate:x", "fixture", "sys", "user")
    bb.generated({"a.h": "x"}, "", tmp_path / "staging")
    kinds = [json.loads(l)["kind"] for l in (bb.jsonl_path).read_text().splitlines()]
    assert kinds.index("scored") < kinds.index("issued") < kinds.index("generated")


def test_fixture_payload_is_valid_json():
    p = TOOL / "fixtures" / "generation_interact_framework.json"
    payload = json.loads(json.loads(p.read_text())["response"])
    assert len(payload["files"]) == 8
    assert all(rel.startswith("Source/GoblinSiege/") for rel in payload["files"])
    assert "GSGameplayTags" in payload["notes"]  # patch instructions present


def test_editor_commands_are_quoted_and_correct():
    cmd = editor_mod.build_command()
    assert cmd[2].endswith("Build.bat")
    assert any("MyProject.uproject" in c for c in cmd)
    launch = editor_mod.launch_command()
    assert launch[0].endswith("UnrealEditor.exe") and launch[1].endswith(".uproject")
    # off-Windows these must be safe no-ops, not crashes
    assert editor_mod.editor_running() is False
    assert editor_mod.compiling_now() is False


def test_state_file_roundtrip(tmp_path):
    class G:  # minimal Gap stand-in
        def __init__(s, key, name, u, elig, ed):
            s.key, s.name, s.utility, s.eligible, s.requires_editor = key, name, u, elig, ed
            s.block, s.missing, s.stubbed = "A", ["X"], []
    p = tmp_path / "AGENT_STATE.md"
    state_mod.update_after_run(p, "r1", [G("a", "Alpha", 9.0, True, False)],
                               ["STAGED Alpha"], ["boom failed"], "picked Alpha")
    text = p.read_text()
    assert "## NEXT" in text and "Alpha" in text and "STAGED Alpha" in text and "boom failed" in text
    # second run rewrites NEXT, appends RUNS
    state_mod.update_after_run(p, "r2", [G("b", "Beta", 5.0, True, False)], [], [], "picked Beta")
    text = p.read_text()
    assert "Beta" in text and text.count("**r") == 2
    assert "Alpha" not in text.split("## NEXT")[1].split("## FAILED")[0]  # NEXT rewritten, not appended
