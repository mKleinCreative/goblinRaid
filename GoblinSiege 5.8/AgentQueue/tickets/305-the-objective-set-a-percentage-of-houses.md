---
id: 305
title: The objective set: a percentage of houses, the statue counts, the mill does not
agent: claude-warren
status: done
claimed: 2026-08-25T06:36Z
build: required
waiting_on: Michael: burn 27 houses + market + field + statue and confirm the portal opens.
evaluated: 2026-08-25T06:49:21Z
observed: UNOBSERVED 2026-08-25T07:02:49Z - Each of the four behaviours was seen working in-engine and logged - houses tally 20/27 instead of completing on the first, the market still needs one, the statue satisfies by toppling, the mill no longer gates - but nobody has had all four true at once, so the portal actually opening on objectives-complete is still inferred rather than watched. Michael has played this build and reported two gaps from it (the statue missing from the HUD list, three windmills that cannot be burned), which are now #306.
scenario: none - never run
files: 
  - Source/GoblinSiege/Raid/GSRaidDirector.h
  - Source/GoblinSiege/Raid/GSRaidDirector.cpp
  - Source/GoblinSiege/Destruction/GSTopplableComponent.h
  - Source/GoblinSiege/Destruction/GSTopplableComponent.cpp
  - Content/Maps/L_Tutorial_Island.umap
  - docs/decisions-ledger.md
  - Config/DefaultGame.ini
  - Config/DefaultGameplayTags.ini
---

## Goal

Michael: *"The objective we need to complete, is to burn down a percentage of houses, the market
stalls, the Statue and the field. Everything else is optional, but the prompt to leave doesn't come
back unless you've completed those missions, or you've ran out of lives."*

Three gaps between that sentence and the code. Rulings 63-66.

## Generate

**1. A type completes at a FRACTION, not at the first carrier.** `FGSObjectiveTypeBucket` gains
`CompletedCount` and `RequiredCount`; `TypeCompletionFraction` (config, keyed by type tag) resolves the
count at registration. **Supersedes ruling Q-32**, which demoted every sibling on the first burn - right
for "the mill", wrong for 67 houses. `RequiredCount` is recomputed on every registration, because
carriers arrive one at a time and a fraction resolved on the first would demand `ceil(0.4 x 1) = 1`.

**2. The mill drops out.** `OptionalTypes` - a type that registers, lists, burns and scores but never
enters `RequiredTypes`.

**3. The statue counts.** `UGSTopplableComponent::ObjectiveTypeTag`, and the director sweeps
monuments **separately** at BeginPlay because a statue is not burned, it is pulled over.
`AGSObjective_ToppleStatue` looks like the answer and is not - it hard-casts to
`AGSDestructibleObjective`, which completes by BURNING, so its total stayed 0 and its condition was
true on the first broadcast.

`HandleMonumentToppled` **recounts rather than increments**: `FGSOnToppled` carries the TOPPLER, not
the monument, so the broadcast cannot say which statue it came from, and a blind `++` would
double-count two monuments or miscount after `GS.Topple.ResetAll`.

New tag `Objective.Topple.Statue`, set on `GS_Seal_Warrior`.

## Evaluate

**Build succeeded.** Runtime, PIE on `L_Tutorial_Island`:

```
Monument 'BP_Statue_Warrior_C_0' counts toward Objective.Topple.Statue (1 needed).
Raid starting: 71 carriers across 4 types           <- was 5; the mill dropped out
Objective.Topple.Statue satisfied - 1 of 1 monument(s) cast down.
'GSBuildingObjective_253' burned: Objective.Burn.House now 20/27.
'GSMarketObjective_0'     burned: Objective.Burn.Market now 1/1. TYPE SATISFIED.
types 2/4  objectives complete=no  raid ended=no
```

**All four behaviours confirmed in one run:** houses tally toward 27 instead of completing on the
first, single-carrier types are untouched (market 1/1), the statue satisfies by toppling, and the mill
no longer gates. **The portal correctly stayed shut** with 20/27 houses and the field unburnt.

**THE CONFIG DID NOT PARSE, AND THIS IS THE part worth keeping.** `TypeCompletionFraction` and
`OptionalTypes` were first written into `DefaultGame.ini`. A `TMap` keyed by `FGameplayTag` and a
`TSet` of them are both finicky to express in config, and **the failure is silent**: the section loads,
the properties stay empty, and the raid quietly keeps the old rules. The only symptom was
`4 types` still reading `5`. Caught by reading the CDO back - `type_completion_fraction: {}` - rather
than by trusting the file. Seeded in the constructor instead, in native tags that cannot be mistyped;
the properties stay `Config`, so a working ini override still wins.

**NOT ESTABLISHED: nobody has completed a raid.** Every type has been seen to satisfy, but no run has
had all four true at once, so the portal opening on objectives-complete is still only inferred from
`bOpenOnObjectivesComplete` having always worked.

## Refine

**One number, calibrated rather than guessed.** 0.4 of 67 houses is 27, and #304's fire spread
completes 23 from one well-placed torch - so 40% needs a second fire or the outliers. It rewards
choosing where to light rather than counting doors.

**Deliberately not done:** the *prompt* to leave. Michael's sentence says "the prompt to leave doesn't
come back unless..." and nothing renders one - the portal simply opens. Also untouched: the
out-of-lives path he named as the other way the prompt returns.

**Not fixed, and it caused this ruling:** two village houses are each split into two objectives, one
per storey - `GS_Building_39`/`52` and `GS_Building_17`/`53`. A percentage absorbs the oddity rather
than fixing it; the split itself is still there.
