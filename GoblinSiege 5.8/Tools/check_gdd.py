"""check_gdd.py - the GDD drift gate.

Run before a build, beside gsqueue's buildgate:

    python "GoblinSiege 5.8/Tools/check_gdd.py"          # exit 0 = no drift
    python "GoblinSiege 5.8/Tools/check_gdd.py" -v       # show every check

WHY THIS EXISTS
---------------
On 2026-08-14 the GDD dropped the granary. The level generator's evaluator went on enforcing it,
and every seed passed a check for a building the design no longer contained; the banked bark and
prompt text went on naming it too. Nothing failed, because nothing was comparing the design to the
things that consume it. That is the failure this file is built to catch, so its checks deliberately
run ACROSS the seam rather than inside the document:

  1. the parser contract      - the document still parses the way ca/gdd.py needs it to,
                                INCLUDING the case where a row parses but the status is corrupt
  2. features.json coverage   - the Code Architect is not blind to a system, and vice versa
  3. the required roster      - the GDD and the generator name the same three objectives
  4. banked player-facing text- shipped rows do not use vocabulary the GDD has retired

RULES ARE IMPORTED, NEVER RESTATED. The banned vocabulary comes from content-pipeline/gsstyle.py
and the roster from level-gen/gslevelgen/generate.py. A second copy of a rule is a second thing to
forget to update, which is the same bug one level up.

A CHECK THAT CANNOT RUN MUST NOT LOOK LIKE A CHECK THAT PASSED. If an import fails, the check
reports SKIPPED and the run is marked incomplete - it never quietly counts as a pass. (Same rule the
level-gen evaluator applies to buildable_ground, which needs the editor's heightfield.)
"""
from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]        # D:\goblinRaid
PROJECT_ROOT = Path(__file__).resolve().parents[1]     # ...\GoblinSiege 5.8
CA_ROOT = PROJECT_ROOT / "Tools" / "CodeArchitect"

GDD_PATH = PROJECT_ROOT / "docs" / "goblin-siege-gdd.md"

# Ids pinned by features.json. All 21 must survive any edit to the 12.1 table.
EXPECTED_IDS = [str(n) for n in range(1, 21)] + ["5b"]
EXPECTED_BLOCKS = list("ABCDEFGH")
EXPECTED_NEVER_CUT = 4      # a wrapped line silently truncated this to 3 until 2026-08-19.
                            # 5 -> 4 on 2026-08-24: ruling 56 (#285) cut the crouch-and-confirm
                            # stealth core, the only item ever removed from that line. If this
                            # number ever drops again, the ruling that did it must be named here -
                            # a Never-cut list that shrinks without a paper trail is worth nothing.

# Shipped, player-facing content. Drafts and traces are working files and are not gated.
BANKED_CSVS = ["barks.csv", "prompts.csv", "whispers.csv"]

# Only these columns are checked. `note` and `sources` are designer commentary and MUST be able to
# say "replaces the retired granary whisper" without tripping the gate - a ledger that cannot name
# what it superseded is not a ledger. `Name` and `trigger` are included because they are code
# contracts, not prose: a trigger tag still called FirstSight.Granary is a real defect.
PLAYER_FACING_COLUMNS = {"line", "overlord_bark", "hud_line", "Name", "trigger"}


class Report:
    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.failures: list[str] = []
        self.skipped: list[str] = []
        self.passed = 0

    def ok(self, name: str, detail: str = "") -> None:
        self.passed += 1
        if self.verbose:
            print(f"  PASS  {name}" + (f" - {detail}" if detail else ""))

    def fail(self, name: str, detail: str) -> None:
        self.failures.append(f"{name}: {detail}")
        print(f"  FAIL  {name}\n        {detail}")

    def skip(self, name: str, why: str) -> None:
        self.skipped.append(f"{name}: {why}")
        print(f"  SKIP  {name}\n        {why}")


# --------------------------------------------------------------------------- 1. parser contract

def check_parser_contract(rep: Report):
    """The document still parses the way ca/gdd.py needs it to.

    Structural only. gdd.py's own docstring is explicit that the status column is 'treated as a
    claim, never as truth', so nothing here reads the prose - it checks the shape the parser
    depends on, which is the part an ordinary edit can silently break.
    """
    if not GDD_PATH.exists():
        rep.fail("gdd_present", f"canonical GDD not found at {GDD_PATH}")
        return None

    sys.path.insert(0, str(CA_ROOT))
    try:
        from ca import gdd as gdd_mod           # noqa: E402
    except Exception as exc:                    # pragma: no cover
        rep.skip("parser_contract", f"cannot import ca.gdd ({exc}) - the contract went unchecked")
        return None

    model = gdd_mod.parse(GDD_PATH)

    if model.warnings:
        rep.fail("parse_clean", "; ".join(model.warnings))
    else:
        rep.ok("parse_clean")

    got = [s.sys_id for s in model.systems]
    missing = [i for i in EXPECTED_IDS if i not in got]
    extra = [i for i in got if i not in EXPECTED_IDS]
    if missing or extra:
        rep.fail("systems_inventory",
                 f"12.1 ids drifted. missing={missing or 'none'} unexpected={extra or 'none'}. "
                 "features.json pins these; a dropped row makes the Code Architect blind to that "
                 "system. If a row is genuinely retired, retire its features.json entry too.")
    else:
        rep.ok("systems_inventory", f"{len(got)} ids")

    missing_blocks = [b for b in EXPECTED_BLOCKS if b not in model.blocks]
    if missing_blocks:
        rep.fail("build_blocks",
                 f"12.2 blocks missing: {missing_blocks}. Block letters must be **bold** and within "
                 "A-H - the regex drops anything else silently, including a block 'I'.")
    else:
        rep.ok("build_blocks", f"{len(model.blocks)} blocks")

    if len(model.never_cut) != EXPECTED_NEVER_CUT:
        rep.fail("never_cut_unwrapped",
                 f"parsed {len(model.never_cut)} never-cut items, expected {EXPECTED_NEVER_CUT}: "
                 f"{model.never_cut}. That line must stay on ONE unwrapped line - the regex captures "
                 "to end-of-line only. Also check nothing above 12.3 restates the heading's literal "
                 "text, because the regex takes the first match in the file.")
    else:
        rep.ok("never_cut_unwrapped", f"{len(model.never_cut)} items")

    # A fourth column does NOT drop the row - established by testing the regex, not by reading it.
    # `| 14 | Lives | **BUILT** | extra |` still matches, and the trailing cell is swallowed into
    # the status string. So the id check above sees nothing wrong while the status quietly lies.
    # That is worse than a drop, because a drop is loud in the id list.
    polluted = [f"{s_.sys_id} ({s_.name})" for s_ in model.systems if "|" in s_.claimed_status]
    if polluted:
        rep.fail("status_column_clean",
                 f"status cell contains a stray '|' for: {', '.join(polluted)}. A fourth column in "
                 "12.1 still parses - the extra cell is absorbed into the status text rather than "
                 "dropping the row. Keep the table at three columns.")
    else:
        rep.ok("status_column_clean", f"{len(model.systems)} statuses")

    blank = [s_.sys_id for s_ in model.systems if not s_.claimed_status.strip()]
    if blank:
        rep.fail("status_not_blank",
                 f"empty status cell for id(s) {blank}. An empty cell parses fine and reads as a "
                 "system with no claim at all.")
    else:
        rep.ok("status_not_blank")

    return model


# ------------------------------------------------------------------- 2. features.json coverage

def check_feature_coverage(rep: Report, model):
    """Both-direction drift between the 12.1 table and the Code Architect's knowledge file."""
    if model is None:
        rep.skip("feature_coverage", "no parsed model - see parser_contract above")
        return

    import json
    sys.path.insert(0, str(CA_ROOT))
    try:
        from ca import gdd as gdd_mod
        features = json.loads((CA_ROOT / "features.json").read_text(encoding="utf-8"))
    except Exception as exc:
        rep.skip("feature_coverage", f"cannot load features.json ({exc}) - coverage went unchecked")
        return

    warnings = gdd_mod.coverage_check(model, features)
    if warnings:
        rep.fail("feature_coverage", " | ".join(warnings))
    else:
        rep.ok("feature_coverage", f"{len(features['features'])} feature entries")


# ----------------------------------------------------------------------- 3. the required roster

def check_roster(rep: Report):
    """The GDD and the level generator must name the same three required objectives.

    This is the granary check, generalised. The generator's REQUIRED_KINDS is what actually decides
    what gets placed in a hamlet; if it and the design disagree, the map is built to a design nobody
    is reading.
    """
    text = GDD_PATH.read_text(encoding="utf-8", errors="ignore")
    m = re.search(r"The\s+three\s+are\s+\*\*([^*]+)\*\*", text)
    if not m:
        rep.fail("roster_declared",
                 "could not find the roster sentence ('The three are **A / B / C**') in 1. Vision. "
                 "That sentence is what this check reads; keep it, or update this check with it.")
        return

    declared = {w.strip().lower() for w in re.split(r"[^A-Za-z]+", m.group(1)) if w.strip()}
    rep.ok("roster_declared", ", ".join(sorted(declared)))

    sys.path.insert(0, str(REPO_ROOT / "level-gen"))
    try:
        from gslevelgen.generate import REQUIRED_KINDS
    except Exception as exc:
        rep.skip("roster_matches_generator",
                 f"cannot import gslevelgen.generate ({exc}) - the roster went unchecked against the "
                 "generator, which is exactly the seam the granary slipped through")
        return

    gen = {k.lower() for k in REQUIRED_KINDS}
    if declared != gen:
        rep.fail("roster_matches_generator",
                 f"GDD says {sorted(declared)}, generator REQUIRED_KINDS says {sorted(gen)}. "
                 "One of them is enforcing a design the other has changed. Fix whichever is stale - "
                 "and check the banked prompt text too, it names objectives by hand.")
    else:
        rep.ok("roster_matches_generator", ", ".join(sorted(gen)))


# ------------------------------------------------------- 4. banked player-facing text vocabulary

def check_banked_vocabulary(rep: Report):
    """Shipped bark/prompt/whisper rows must not use vocabulary the GDD has retired.

    Drafts and traces are working files and are deliberately not gated - only the .csv rows that
    would actually be imported into a DataTable.
    """
    sys.path.insert(0, str(REPO_ROOT / "content-pipeline"))
    try:
        import gsstyle
        banned = gsstyle.BANNED_TERMS
    except Exception as exc:
        rep.skip("banked_vocabulary",
                 f"cannot import gsstyle ({exc}) - banked text went unchecked")
        return

    out_dir = REPO_ROOT / "content-pipeline" / "out"
    hits: list[str] = []
    checked = 0

    for name in BANKED_CSVS:
        path = out_dir / name
        if not path.exists():
            continue
        checked += 1
        with path.open(encoding="utf-8", newline="") as fh:
            for lineno, rec in enumerate(csv.DictReader(fh), start=2):
                for col, val in rec.items():
                    if col not in PLAYER_FACING_COLUMNS or not val:
                        continue
                    low = val.lower()
                    for term, why in banned.items():
                        if term in low:
                            hits.append(f"{name}:{lineno} column '{col}' uses '{term}' - {why}")

    if not checked:
        rep.skip("banked_vocabulary",
                 f"none of {BANKED_CSVS} found under {out_dir} - nothing was checked")
        return

    if hits:
        shown = hits[:8]
        more = f" (+{len(hits) - len(shown)} more)" if len(hits) > len(shown) else ""
        rep.fail("banked_vocabulary",
                 "\n        ".join(shown) + more +
                 "\n        Regenerate with content-pipeline/gsstyle.py rather than hand-editing; "
                 "it already rewrites these rows and #194 made the runs reproducible.")
    else:
        rep.ok("banked_vocabulary", f"{checked} file(s) clean")


# ------------------------------------------------------------------------------------- driver

def main() -> int:
    ap = argparse.ArgumentParser(description="GDD drift gate")
    ap.add_argument("-v", "--verbose", action="store_true", help="show passing checks too")
    args = ap.parse_args()

    print(f"check_gdd: {GDD_PATH}")
    rep = Report(args.verbose)

    model = check_parser_contract(rep)
    check_feature_coverage(rep, model)
    check_roster(rep)
    check_banked_vocabulary(rep)

    print()
    if rep.failures:
        print(f"DRIFT: {len(rep.failures)} check(s) failed, {rep.passed} passed, "
              f"{len(rep.skipped)} skipped.")
        return 1
    if rep.skipped:
        # Incomplete is not the same as clean, and must not exit 0.
        print(f"INCOMPLETE: {rep.passed} passed, {len(rep.skipped)} could not run. "
              "A check that cannot run is not a check that passed.")
        return 2
    print(f"CLEAN: {rep.passed} checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
