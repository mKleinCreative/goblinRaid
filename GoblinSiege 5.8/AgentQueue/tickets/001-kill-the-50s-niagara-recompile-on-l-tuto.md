---
id: 001
title: Kill the 50s Niagara recompile on L_Tutorial_Island load
agent: claude-perf
status: review
claimed: 2026-08-05T18:38Z
build: none
waiting_on:
files: 
  - Content/PortalVFXEnhanced/VFX/Systems/PortalsV2/N_Portal4_V2.uasset
  - Content/PortalVFXEnhanced/VFX/Systems/Elemental/N_Portal4Elemental.uasset
---

## Goal

Kill the 50s Niagara recompile on L_Tutorial_Island load

## Generate

No C++, no build. Two marketplace Niagara systems were loaded and re-saved so the compiled
data this session already paid for is serialised into the asset instead of being recomputed
every load:

- `/Game/PortalVFXEnhanced/VFX/Systems/PortalsV2/N_Portal4_V2`
- `/Game/PortalVFXEnhanced/VFX/Systems/Elemental/N_Portal4Elemental`

Both are placed in `L_Tutorial_Island` as loose actors (`N_Portal4_V2`, `N_Portal4Elemental`),
found via `EditorActorSubsystem.get_all_level_actors()`.

Method: `EditorAssetSubsystem.load_asset` -> `modify(True)` -> `save_loaded_asset(only_if_is_dirty=False)`.
Backups of all five N_Portal4* assets at `D:\goblinRaid\PortalVFX_Backup_20260805\`.

## Evaluate

**Verified by evidence:**

- The cost is real and recurring, not a one-time import. Per-log counts of
  `Compiling System NiagaraSystem .*Portal`: backup-18.12.10 = 22 compiles / 320.9s;
  MyProject_2 (Michael's PIE session) = 2 compiles / 48.5s; sessions that never loaded the
  level = 0. A one-time import cost would appear once, not in every session that opens the map.
- Individual costs from MyProject_2.log: `N_Portal4_V2 took 24.265083 sec`,
  `N_Portal4Elemental took 24.203548 sec`. The other three systems in the project total ~1.2s,
  so these two ARE the load hitch.
- The save changed the assets substantially: 3031003 -> 1270805 bytes and 3076565 -> 1303231
  bytes, i.e. ~58% smaller. Consistent with stale duplicate data being dropped and the current
  compiled state written cleanly.
- Frame timing (editor viewport, PIE not running): game_thread 16.9ms, render 1.88ms,
  gpu 0.34ms, verdict GameThread-bound, "clear" confidence. The GPU is idle. Any fix aimed at
  resolution or graphics settings would be aimed at the wrong thread.

**Written but NOT verified:** that the recompile is actually gone. Proving it needs a fresh
editor process loading the level and the log showing zero `Compiling System NiagaraSystem
.*Portal` lines. This session cannot prove it - the systems are already compiled in memory here,
so a level reload would be silent whether or not the fix worked. **This is the one claim to
distrust until a restart is done.**

**Touched outside the goal:** nothing. No C++, no build, no queue-conflicting files.

**Owes AGENT_STATE.md** a FAILED/gotcha line: marketplace Niagara systems imported into this
project recompile on every load until re-saved once; check
`grep "Compiling System NiagaraSystem" Saved/Logs/*.log` before blaming gameplay code for a
load hitch. Not yet written - the entry should be added once the restart confirms it.

## Refine

Considered and deliberately NOT done:

- **Setting `bBakeOutRapidIteration = True`** on both systems. It is the other standard lever for
  Niagara compile cost, but it bakes rapid-iteration parameters and can change behaviour of VFX I
  did not author and cannot visually verify. Re-saving is the reversible half of the fix; if the
  restart shows the recompile persists, this is the next thing to try.
- **Deleting or unloading the two portal actors from L_Tutorial_Island.** That would remove the
  cost outright and is tempting, but they are presumably placed for the runic site / portal work
  in progress. That is Michael's content decision, not mine.

Left undone on purpose: the second, separate problem this investigation surfaced - the editor is
game-thread bound at 16.9ms with an idle GPU *before PIE even starts*. That is not the load hitch
and does not belong to this ticket; it needs a PIE-time `stat dumpframe` reading and its own
ticket, and it is the more likely cause of the fight being hard to read once the map is up.
