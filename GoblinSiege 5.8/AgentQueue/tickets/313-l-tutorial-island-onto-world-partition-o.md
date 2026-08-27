---
id: 313
title: L_Tutorial_Island onto World Partition, objective actors always loaded
agent: claude-ui
status: done
claimed: 2026-08-26T01:58Z
build: required
waiting_on:
evaluated: 2026-08-26T17:59:28Z
observed: UNOBSERVED 2026-08-26T17:59:28Z - Parked by Michael after four failed WorldPartitionConvertCommandlet runs; engine asserts at save (UObjectArray.h:1083). Map verified byte-identical to backup and a pre-worldpartition git tag exists, so nothing was damaged. Nothing to observe - the conversion never produced a running map.
scenario: none - never run
files: 
  - Content/Maps/L_Tutorial_Island.umap
  - Content/Maps/__ExternalActors__
---

## Goal

L_Tutorial_Island onto World Partition, objective actors always loaded

## Generate

Michael asked what World Partition would look like here, then chose the "always loaded" approach
for the streaming problem. Parked BLOCKED after four conversion attempts - the map does not convert.

Done before parking:

- Map backed up OUTSIDE the repo and `git tag pre-worldpartition` at 84459e4.
- `WorldPartitionConvertCommandlet` run four times, three different invocation faults fixed, then
  two identical engine crashes.

**Why the map is 177 MB, measured rather than guessed.** It is not baked lighting - there is no
`_BuiltData.uasset` sibling and no `MapBuildDataRegistry` or `PrecomputedLightVolume` in the file.
It is foliage, stored inline as instance transforms. The commandlet enumerated it:

    SM_VillageWheat_01_FoliageType      91,510
    SM_VillageWheat_02_FoliageType      91,417
    SM_VillageWheat_Grass_FoliageType   91,930
    SM_VillageOak / Reeds x3 / Trees x3  ~3,500 combined

~275,000 wheat instances alone. That matches the raw byte profile: from 11 MB to 77 MB the file
runs ~53% zero bytes, which is what packed float transform arrays look like.

**Michael's own edits are ~0.5 MB of the 177.** The marketplace original, `Tutorial_Island.umap`,
is 176.5 MB. The map was born this size.

## Evaluate

**The conversion fails, and it is the map, not the environment.** Michael was mid-way through an
engine install during the first crash, which was a fair suspicion. It was tested rather than
assumed: the retry after the install settled produced the SAME assertion at the SAME stage, only
sooner because the cache was warm (landscape 10 s instead of 2:31).

    Assertion failed: Index >= 0  [UObjectArray.h:1083]

It dies immediately after `FixupSoftObjectPaths`, at the start of saving. Everything before that
succeeds: PartitionFoliage, PartitionLandscape, path fixup.

**Prime suspect:** a `SimpleConstructionScript` ensure fires during every conversion run -
`Node->ParentComponentOrVariableName != PendingParent->GetVariableName()`. That is a Blueprint in
the level with malformed component parenting, which would leave exactly the sort of dangling
object that asserts at save.

**Three invocation faults were fixed along the way, worth recording so nobody repeats them:**

1. PowerShell `--%` passes everything VERBATIM, so quotes inside it become literal characters. The
   run silently opened the editor instead of running the commandlet.
2. Passing a Windows path through the Bash tool into PowerShell strips the backslashes -
   `D:goblinRaidGoblinSiege 5.8MyProject.uproject`. Use the PowerShell tool directly.
3. **The commandlet's own error message is wrong in 5.8.** It says to "use the
   SaveFoliageTypeToContentFolder switch"; that switch is not parsed at all. The source
   (`WorldPartitionConvertCommandlet.cpp:689`) reads `Arguments.Find(TEXT("FoliageTypePath"))`, so
   the real form is `-FoliageTypePath=/Game/Foliage/Types`. Reading the source was the only way to
   find it.

**Nothing was damaged.** The map's MD5 is identical to the pre-conversion backup after all four
attempts, and no external actors were written.

## Refine

Parked at Michael's call: *"park it, close 313 as blocked"*. The push problem this was meant to
solve is an annoyance, not a blocker, and the demo matters more.

**The command that gets furthest, for whoever picks this up** (run it from the PowerShell tool, not
through bash - see fault 2 above):

    UnrealEditor-Cmd.exe "<project>.uproject" -run=WorldPartitionConvertCommandlet
      /Game/Maps/L_Tutorial_Island -AllowCommandletRendering -FoliageTypePath=/Game/Foliage/Types

**Next step is `-ReportOnly`**, which runs the analysis and writes nothing. It should name the
actor that asserts, turning an engine crash into one fixable Blueprint. Not run here - that was the
point at which Michael parked it.

**Left undone deliberately:** the `bIsSpatiallyLoaded = false` pass on objective actors, which is
the whole reason the ticket existed. It is meaningless until the map is actually World Partition.
The actors that will need it are everything carrying a `UGSTopplableComponent`, every
`AGSBurnObjectiveBase`, `AGSRunicSite` and `AGSRaidMarker`.

**The verification to run after any future conversion** - the raid director already logs exactly
what is needed, so this is a precise pass/fail rather than an impression:

    Raid starting in '...': 71 carriers across 5 types
    Monument 'BP_Statue_Warrior_C_0' counts toward Objective.Topple.Statue (1 needed).

If the carrier count comes back below 71, streaming ate objectives and the always-loaded pass
failed.

> 2026-08-26T02:20Z Map does not convert: engine asserts at save (UObjectArray.h:1083). Not the environment - retested after the engine install finished, identical crash. Suspect a Blueprint with malformed component parenting. Next step is -ReportOnly to name it. Map verified byte-identical to backup; nothing damaged.

> 2026-08-26T17:59Z Michael 2026-08-26: close it - the push problem it was solving is an annoyance, not a blocker.
