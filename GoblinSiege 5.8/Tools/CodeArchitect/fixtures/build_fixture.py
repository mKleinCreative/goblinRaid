#!/usr/bin/env python3
"""Assemble fixtures/generation_interact_framework.json from the C++ files in
fixtures/src_interact/. The fixture is the recorded generation used when no
ANTHROPIC_API_KEY is available (cloud demo runs, offline tests) — same pattern
as Bark Foundry's fixture provider. Re-run after editing the C++."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SRC = ROOT / "src_interact"

NOTES = """### Patch instructions for EXISTING files (apply by hand or next supervised session)

1. **`Source/GoblinSiege/Combat/GSGameplayTags.h`** — add inside `namespace GSTags`:
   ```cpp
   /** Hold-E channel in flight (UGSGA_Interact ActivationOwnedTags). Noise scaling, HUD and
    *  takedown eligibility query this rather than reaching into the interaction component. */
   UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Interacting);
   /** Cargo on the shoulder (UGSCarryComponent). Attack abilities add this to their
    *  ActivationBlockedTags; chickens never raise it (design doc §9 one-handed fighting). */
   UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Carrying);
   // Slice interact verbs (design doc §4) — data on UGSInteractableComponent::VerbTag.
   UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Loot);
   UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Takedown);
   UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_FoulWell);
   UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Extract);
   ```
2. **`Source/GoblinSiege/Combat/GSGameplayTags.cpp`** — add matching definitions:
   ```cpp
   UE_DEFINE_GAMEPLAY_TAG(State_Interacting, "State.Interacting");
   UE_DEFINE_GAMEPLAY_TAG(State_Carrying,    "State.Carrying");
   UE_DEFINE_GAMEPLAY_TAG(Interact_Loot,     "Interact.Loot");
   UE_DEFINE_GAMEPLAY_TAG(Interact_Takedown, "Interact.Takedown");
   UE_DEFINE_GAMEPLAY_TAG(Interact_FoulWell, "Interact.FoulWell");
   UE_DEFINE_GAMEPLAY_TAG(Interact_Extract,  "Interact.Extract");
   ```
3. **`AGSPlayerCharacter`** — constructor: `CreateDefaultSubobject<UGSInteractionComponent>(TEXT("Interaction"))`
   and `CreateDefaultSubobject<UGSCarryComponent>(TEXT("Carry"))`; grant `UGSGA_Interact` alongside the
   existing dodge/block grants; bind the Interact input action (hold-E): press ->
   `AbilitySystemComponent->TryActivateAbilityByClass(UGSGA_Interact::StaticClass())`, release -> the
   ability's `InputReleased` via the input id used for the other abilities.
4. **Input (editor-side, supervised):** add `IA_Interact` to `IMC_GSDefault` bound to E (hold).
5. **Skeleton (editor-side, supervised):** add `CarrySocket` to the goblin skeleton (spine/shoulder).
6. **Build:** new UCLASS types — FULL editor-closed build (`architect.py --build-and-relaunch`);
   Live Coding cannot register them.

Exit test (Block A, status doc): stand on the island as the Scout, hold E on a test interactable
(a `UGSInteractableComponent` with VerbTag Interact.Loot on any prop), watch the channel bar fill,
and confirm it aborts on damage, on release, and on walking away."""

DECISIONS = [
    "Verbs are gameplay tags (Interact.*), not subclasses — same extensibility argument as Objective.Burn.*: a new verb is data, not a recompile.",
    "The channel lives in UGSInteractionComponent, not the ability: the HUD bar and the abort rules need a per-frame owner; UGSGA_Interact stays a thin GAS gate holding State.Interacting and composing with dodge/block via ActivationBlockedTags, exactly like the existing abilities.",
    "Damage-abort reuses AGSCharacterBase::OnHealthChanged (negative Delta) — the delegate exists precisely so listeners can hear damage without touching GAS internals; no new plumbing.",
    "Takedown's 120-degree behind-cone is NOT in the framework: the victim-side interactable proxy checks it in CanInteract, keeping the framework verb-agnostic (stealth lands in Block F without reopening this code).",
    "Carry speed penalty is multiplicative and inverted on drop rather than an absolute MaxWalkSpeed write, so it cannot fight the GAS MoveSpeed attribute or a weapon's speed identity.",
    "Replication: only bAvailable and CarriedActor replicate — the cheap root state, per the project's single-player-slice / co-op-ready posture (Q-36 pattern).",
]

files = {}
for f in sorted(SRC.rglob("*")):
    if f.suffix in (".h", ".cpp"):
        files[str(f.relative_to(SRC)).replace("\\", "/")] = f.read_text()

payload = {"files": files, "notes": NOTES, "decisions": DECISIONS}
out = ROOT / "generation_interact_framework.json"
out.write_text(json.dumps({"response": json.dumps(payload)}, indent=1))
print(f"wrote {out} ({out.stat().st_size} bytes, {len(files)} files)")
