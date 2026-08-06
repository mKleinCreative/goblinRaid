"""Code generation for the top-scored eligible gap.

The one LLM stage. Context passed to the model is real perception output —
the actual headers the new code must sit beside — plus the GDD's relevant
constraints. Output contract: JSON {"files": {relpath: content}, "notes": str,
"decisions": [str]}. Everything is STAGED under out/runs/<id>/staging/ and
diff-able; nothing touches Source/ until Michael reviews (the human gate is
post-run review, per the 2026-08-04 ruling — same place Bark Foundry put it).
"""
from __future__ import annotations

from pathlib import Path

from .blackboard import Blackboard
from .config import Config
from .llm import Provider, ProviderError, extract_json
from .perception import CodebaseReport
from .scorer import Gap

SYSTEM_PROMPT = """You are the Code Architect for Goblin Siege, an Unreal Engine 5.8 C++ project
(module GoblinSiege, API macro GOBLINSIEGE_API). You write production C++ for review by the
project owner. House rules, non-negotiable:
- Class prefix GS (AGSFoo actors, UGSFoo components/objects). Canonical player class name is SCOUT.
- GAS is the combat backbone: abilities derive from UGameplayAbility, attributes live on
  UGSAttributeSetBase, native gameplay tags are declared in GSGameplayTags.h/.cpp.
- Never rewrite existing files wholesale. If an existing file needs an addition (a tag, an include,
  a component on the character), emit it in "notes" as an exact, minimal patch instruction instead.
- Multiplayer posture: single-player slice, co-op-ready — replicate the cheap root state
  (bIsX flags) but do not build prediction.
- Comments explain design intent tersely; no boilerplate comment noise.
Return ONLY a JSON object: {"files": {"Source/GoblinSiege/<path>": "<content>", ...},
"notes": "<markdown notes incl. exact patch instructions for existing files>",
"decisions": ["<why choices were made>", ...]}"""


def build_user_prompt(gap: Gap, rep: CodebaseReport, cfg: Config, context_files: dict[str, str]) -> str:
    parts = [
        f"## Feature to build\n{gap.name} (feature key `{gap.key}`, GDD system {gap.gdd_system}, block {gap.block})",
        f"Missing evidence to satisfy: {', '.join(gap.missing)}",
        f"Stubbed evidence to flesh out: {', '.join(gap.stubbed) or 'none'}",
        "## Design constraints (from the GDD)",
        gap_design_brief(gap),
        "## Existing code the new files must sit beside (verbatim headers)",
    ]
    for rel, content in context_files.items():
        parts.append(f"### {rel}\n```cpp\n{content}\n```")
    parts.append(
        "## Task\nWrite the missing classes as complete .h/.cpp pairs under Source/GoblinSiege/. "
        "Follow the existing include style (module-relative includes as seen in the headers above). "
        "List every edit needed to EXISTING files (tags, character wiring, Build.cs) in notes as "
        "exact patch instructions — do not emit modified copies of existing files."
    )
    return "\n\n".join(parts)


def gap_design_brief(gap: Gap) -> str:
    """Feature-specific design constraints, curated from the GDD. Kept in code so
    the prompt is reproducible; extend per feature as they come up in the queue."""
    briefs = {
        "interact_framework": (
            "- Hold-E channel framework; the slice verbs are loot / takedown / foul-well / extract, "
            "plus a carry state (GDD §4). Verbs are data (gameplay tags), not subclasses.\n"
            "- Channel: configurable duration; progress 0..1 exposed for the HUD channel bar.\n"
            "- Abort rules (stealth spec): taking damage aborts; releasing the key aborts; leaving "
            "range or breaking facing aborts. Interruptible always.\n"
            "- Eligibility: range + facing cone from the interactor; interactable advertises verb tag, "
            "channel seconds, and whether it is currently available.\n"
            "- On complete: interactable fires its effect (delegate + BlueprintNativeEvent), "
            "e.g. loot grants pouch, extract banks — those systems hook in later; the framework "
            "only owns the channel lifecycle.\n"
            "- GAS integration: UGSGA_Interact activates on the Interact input, applies a "
            "State.Interacting tag while channeling, and is cancelled by damage "
            "(listen for the existing damage flow via attribute change or a gameplay event).\n"
            "- Carry: UGSCarryComponent owns carried-object state (slows movement, blocks attack "
            "abilities via State.Carrying tag); pick-up/put-down route through the same channel."
        ),
    }
    return briefs.get(gap.key, "- Follow the GDD sections referenced by this feature's name.")


def select_context_files(gap: Gap, rep: CodebaseReport, cfg: Config, budget_bytes: int = 60000) -> dict[str, str]:
    """Pick the headers the generated code must integrate with."""
    wanted = {
        "interact_framework": [
            "Characters/GSCharacterBase.h", "Characters/GSPlayerCharacter.h",
            "Combat/GSGameplayTags.h", "Weapons/Abilities/GSGA_DodgeRoll.h",
            "Attributes/GSAttributeSetBase.h",
        ],
    }.get(gap.key, ["Characters/GSCharacterBase.h", "Combat/GSGameplayTags.h"])
    out: dict[str, str] = {}
    used = 0
    for rel in wanted:
        p = cfg.source_dir / rel
        if p.exists():
            t = p.read_text(encoding="utf-8", errors="ignore")
            if used + len(t) > budget_bytes:
                continue
            out[f"Source/GoblinSiege/{rel}"] = t
            used += len(t)
    return out


def generate(gap: Gap, rep: CodebaseReport, cfg: Config, bb: Blackboard, provider: Provider) -> dict:
    ctx = select_context_files(gap, rep, cfg)
    user = build_user_prompt(gap, rep, cfg, ctx)
    # Blackboard order is the contract: ISSUED is logged before the call,
    # GENERATED before anything lands outside the staging dir.
    bb.issued(f"generate:{gap.key}", provider.describe(f"generation_{gap.key}"), SYSTEM_PROMPT, user)
    raw = provider.complete(SYSTEM_PROMPT, user, fixture_name=f"generation_{gap.key}")
    # Keep the raw response BEFORE parsing: a parse failure with no artifact is
    # undiagnosable after the fact (live-002 lost both failures that way).
    raw_path = bb.dir / f"raw_{gap.key}.txt"
    raw_path.write_text(raw, encoding="utf-8")
    bb.line(f"\n*raw model output:* `{raw_path.name}` ({len(raw)} chars)")
    try:
        result = extract_json(raw)
    except ProviderError as e:
        raise ProviderError(f"{e} — raw output kept at {raw_path}") from e
    files = result.get("files", {})
    staging = bb.dir / "staging"
    bb.generated(files, result.get("notes", ""), staging)
    for rel, content in files.items():
        dest = staging / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(content, encoding="utf-8")
    (staging / "NOTES.md").parent.mkdir(parents=True, exist_ok=True)
    (staging / "NOTES.md").write_text(
        f"# {gap.name} — generated {bb.run_id}\n\n{result.get('notes','')}\n\n"
        + "\n".join(f"- {d}" for d in result.get("decisions", [])), encoding="utf-8")
    return result
