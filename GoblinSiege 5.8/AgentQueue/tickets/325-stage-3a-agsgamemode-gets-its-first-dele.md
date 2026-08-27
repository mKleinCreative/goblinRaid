---
id: 325
title: Stage 3a - AGSGameMode gets its first delegate: OnCharacterKilled, so a defender death is reportable at all
agent: claude-corruption
status: done
claimed: 2026-08-26T19:49Z
build: required
waiting_on: NOT COMPILED - gate closed by 317, 319, 320. Inert until stage 3b subscribes, so its honest observation comes with 3b. Needs editor-closed build (new delegate = UHT reflection, Live Coding cannot register it).
evaluated: 2026-08-26T19:51:57Z
observed: 2026-08-27T21:53:20Z | Killed defenders in PIE and the death bus carried them: the corruption subsystem received 3 kills through AGSGameMode::OnCharacterKilled and separated them into 2 soldiers and 1 civilian, which is the first time an AI defender death has been reported to anything in this project
scenario: PIE in L_Tutorial_Island after the 14:31 build, killed two guards and a peasant, then GS.Corruption.Dump
files: 
  - Source/GoblinSiege/Core/GSGameMode.h
  - Source/GoblinSiege/Core/GSGameMode.cpp
---

## Goal

Stage 3a - AGSGameMode gets its first delegate: OnCharacterKilled, so a defender death is reportable at all

## Generate

Stage 3 of the corruption plan, split in two. **This is 3a: the signal.** 3b (corruption consuming
it) is blocked behind #320, which still holds `GSCorruptionSubsystem.h/.cpp`.

**`Core/GSGameMode.h`** - `FGSOnCharacterKilled(AGSCharacterBase* Victim, FGameplayTag VictimRaceTag,
FVector Location)` and a `BlueprintAssignable` instance. **AGSGameMode's first delegate ever.**

**`Core/GSGameMode.cpp`** - broadcast as the first statement of `HandleGoblinDeath`, **above both
early returns**. That placement is the entire correctness argument: a defender whose controller is
already destroyed returns on the `!Controller` line, and every AI defender returns on the missing
`AGSPlayerState` line, so a broadcast placed after either one reports player deaths only.

No new `UCLASS`, but a new `DECLARE_DYNAMIC_MULTICAST_DELEGATE` is UHT reflection, so Live Coding
cannot register it - editor-closed build required.

## Evaluate

**NOT COMPILED.** Gate closed by #317 (stale), #319, #320.

**Why this is a separate ticket from the rest of stage 3, and why that is not just queue mechanics.**
The delegate lands in the middle of the ACF migration's blast radius. #223 rewrote the death path -
`AGSCharacterBase` now derives `AACFCharacter`, ACF's `UACFDamageHandlerComponent::OnOwnerDeath`
drives `HandleDeath`, and ACF's own `IsAlive()` is documented as answering "alive" for a corpse.
Isolating the broadcast means that if a death now fires twice, or fires for the wrong actor, the
ticket that changed the death path is the only suspect. Bundled with the corruption maths it would
have been guesswork.

**What I deliberately did NOT do:** name the killer. That means reading ACF's damage handler, whose
`StatisticsComp` is documented as present-but-uninitialised in this project (#223), and the global
corruption model does not need it. `Location` is carried instead - free at the broadcast, impossible
to recover afterwards, and the one field a future zoned model or a per-corpse ash decal would need.

**Adversarially:**

- **Nothing subscribes yet.** This ships an inert delegate. That is the intended shape of a split,
  but it means the ticket cannot be observed by watching corruption move - only by watching the
  broadcast fire, which needs a temporary listener or a log line I have not added. **I should
  probably have added a Verbose log line inside the broadcast guard** so 3a is observable on its
  own; as written, its only honest observation is via 3b.
- **Single-fire is inherited, not proven.** I rely on `AGSCharacterBase::HandleDeath` guarding on
  `bIsDead` before calling in. I read that guard, I have not watched it hold under ACF's death path.
- **`GetRaceTag()` on a corpse is assumed stable.** If ACF's death handling clears or reinitialises
  attributes before this runs, the tag could read invalid and every kill would classify as human by
  the fallback rule. Unverified.
- **The civilian discriminator is decided but not built.** `DA_Race_Human` carries four archetype
  rows - Militia, Archer, Knight, **Civilian** - and `BP_PeasantMan` is the civilian pawn, so
  `AGSEnemyCharacter::GetArchetypeRowName()` is the discriminator, read as a data key rather than a
  class-name match. `RoleTag` exists on the archetype and has no consumers; using it would be
  cleaner but needs a `DA_Race_Human` edit I cannot make without editor MCP in this session.

**Owes `AGENT_STATE.md`:** a DECISIONS line - *"AGSGameMode::OnCharacterKilled is the death bus;
broadcast above both early returns or it reports player deaths only"*.

**A queue fault in this ticket, and it was mine.** I chained `claim` with
`set -Id 321 -Status active` in one command, hardcoding the id I expected the claim to return. The
claim actually returned **#325**, so I set **#321 - claude-ui's live "horde readout" ticket - to
active**. I could not recover its prior status (tickets are untracked in git), and I have left it
alone rather than guessing a value to restore, since changing another agent's ticket twice is worse
than once. Almost certainly a no-op, because an agent that has claimed a ticket sets it active
anyway - but "almost certainly" is not a thing I get to conclude about someone else's work.
**Never hardcode a ticket id; read it from the claim output.**

## Refine

**Changed from my own review:** the broadcast was first written after the `!Controller` guard,
where it read more naturally beside the PlayerState logic. That would have silently excluded every
AI defender - the entire population this signal exists to count - and it would have looked correct
in review. Moved above both guards, with the reasoning written at the call site rather than left
implicit.

**Deliberately left undone:**

- **Stage 3b** - the kill counter, ruling 62's civilian weighting, and the ceiling moving from 0.80
  to 1.00. Blocked on #320 closing.
- **The kill soft-knee re-size.** I tried to count the defender roster from the .umap files and got
  string-occurrence counts, which are not instance counts. I am not sizing it from a number I know
  to be unreliable; it stays a cvar until someone can count it in the editor or watch a full raid.
- **A Verbose log line on the broadcast**, per the first adversarial bullet - it would make 3a
  observable alone. Left out only because the file is claimed and I would rather raise it than
  quietly widen the ticket.
