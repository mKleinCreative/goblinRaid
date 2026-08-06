---
id: 014
title: Invisible meshes still present in L_Tutorial_Island
agent: claude-raid
status: queued
claimed: 2026-08-06T00:35Z
build: none
waiting_on:
files: 
  - DIAGNOSIS-ONLY-no-files-claimed-yet
---

## Goal

Invisible meshes still present in L_Tutorial_Island

Reported by Michael: there are still invisible meshes in the map. Filed with a read-only audit
attached so the fix does not start from a guess. **No files edited, nothing changed.**

## Generate

Read-only audit of the open editor world (no PIE, no writes, safe with the build gate shut).
Scanned 9,056 actors / 8,344 StaticMeshComponents for the five ways a mesh renders as nothing:
null StaticMesh, `hidden_in_game`, `visible=false`, ~0 scale, and zero-extent bounds.

**Result: 11 real offenders, all the same cause — a StaticMeshActor whose StaticMesh is null.**

| label | location | extent | collision |
|---|---|---|---|
| `House_2x1_L7_Detailed` | (-1824, 52930, **-1610**) | 0 | QUERY_AND_PHYSICS |
| `House_1x15` | (-4079, 49549, **-955**) | 0 | QUERY_AND_PHYSICS |
| `WaterMill_Closed` | (-2000, 64127, **-1560**) | 0 | QUERY_AND_PHYSICS |
| `House_2x1_T9` | (-18137, 74261, 1719) | 0 | QUERY_AND_PHYSICS |
| `House_1x3_10` | (-10654, 47870, -100) | 0 | QUERY_AND_PHYSICS |
| `Innbase` | (-12924, 58007, 1148) | 0 | QUERY_AND_PHYSICS |
| `Bridge1` | (5769, 54298, **-1861**) | 0 | QUERY_AND_PHYSICS |
| `Bridge3` | (1949, 61935, **-1304**) | 0 | QUERY_AND_PHYSICS |
| `Bridge4` | (-5162, 40834, **-1162**) | 0 | QUERY_AND_PHYSICS |
| `QuestBoard1` | (1834, 59329, **-1642**) | 0 | QUERY_AND_PHYSICS |
| `Square fence mid2` | (-12575, 55119, 801) | 0 | QUERY_AND_PHYSICS |

Other categories came back effectively clean: `visible=false` 1 (`SM_WarriorStatue2`), ~0 scale 0,
zero-extent bounds on an *assigned* mesh 0.

**Three hits are expected and are NOT bugs** — excluded from the count above:
- `GS_RunicSite` — its `PortalMesh` is deliberately empty; the placed `SM_Portal4` is the visual.
- `GS_Windmill` (listed twice, two mesh-less components) — a logic-only `AGSMillObjective` sitting
  on the existing `SM_WIndmill_Base` art.
- `CameraActor` — `hidden_in_game` is normal.

## Evaluate

**Correcting my own first pass.** The audit initially flagged these as "invisible but solid" —
walls you cannot see. **That is wrong and I am not leaving it in the ticket.** Every one has
**extent 0**: with no StaticMesh there is no collision geometry, so `QUERY_AND_PHYSICS` on the
component collides with nothing. They are empty shells, not invisible walls, and they do not block
the player. Anyone acting on the first framing would go hunting for phantom collision that is not
there.

**What the evidence does support:** these are leftovers from a *different* kit. The names
(`House_2x1_L7_Detailed`, `Innbase`, `WaterMill_Closed`, `House_1x15`) match nothing in the
Dreamscape set the hamlet is actually built from — that kit uses `SM_House_Floor_5x4_*`,
`SM_House_Wall_5x4_*`, `SM_MERGED_House_*`. So the most likely story is an earlier blockout or an
asset pack that was removed or migrated, leaving the actors behind with dead mesh references.

**Seven of the eleven sit at negative Z**, well below the landscape. That is consistent with an
abandoned blockout layer under the map — and it lines up with an observation from the raid-loop
work: a PIE pawn once spawned at z=-1671 "standing on Plane2" with Landscape *above* it. Worth
checking whether `Plane2` and these actors are the same leftover layer.

**Not established:** whether they are safe to delete. They are referenced by nothing I checked, and
I did not run a reference search. Michael may also be keeping them as placement markers.

**Owes AGENT_STATE.md** a FAILED/gotcha line only if the cause turns out to be a broken migration
rather than deliberate leftovers — do not write one on the strength of this audit alone.

## Refine

Changed in response to the above: dropped the "invisible but solid" claim entirely and replaced it
with the extent-0 finding that disproves it.

Deliberately left undone:
- **No deletions.** The fix touches `Content/Maps/L_Tutorial_Island.umap`, held by ticket 009, and
  deleting eleven actors on the strength of an audit is Michael's call, not mine.
- **No reference search.** Worth doing before deletion, to catch anything pointing at them.
- The suggested fix, when someone takes it: confirm with Michael, then delete the eleven, and
  re-run this audit as the check. The script is at
  `scratchpad/invisaudit.py` + `ghostlist.py` and is read-only.
