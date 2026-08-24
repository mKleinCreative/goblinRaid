---
id: 294
title: Uriel is replaced by a Knight in the prototype levels
agent: claude-warren
status: done
claimed: 2026-08-24T22:54Z
build: none
waiting_on:
evaluated: 2026-08-24T22:56:19Z
observed: UNOBSERVED 2026-08-24T22:56:43Z - Nothing to watch: the asset was placed in no level, referenced by no asset and spawned by no code, so its removal changes no runtime behaviour. The deletion itself was verified on disk (ls) rather than from the delete API's return value, which lied twice.
scenario: none - never run
files: 
  - Content/Maps/Test/L_CombatArena.umap
  - Content/Blueprints/Adversaries/BP_UrielAPlotexia.uasset
  - docs/decisions-ledger.md
---

## Goal

Michael: *"just replace him with a knight. Don't worry about the boss in the demo. We NEED to get
moving on the actual demo."*

Raised from #292, which found `BP_UrielAPlotexia` had **no `CharacterInitDataAsset` at all** - the same
defect #275 found on the peasant, so his ACF stats would never initialise.

## Generate

**Rulings 60 and 61**, superseding ruling 55.

55 parked Uriel in demo scope and told everyone to leave him alone, which was right while the demo was
undefined. **Ruling 58 has since defined it** as a straight raid - horn, horde, burn, bank, extract -
and there is no boss in that. `BP_KnightDPelegrini` already exists, is armoured, moved onto ACF in
#292 and was signed off in play; a second armoured human would be work with nothing behind it.

**`BP_UrielAPlotexia` deleted.** Established first, not assumed:

- placed in **no level** - checked all 90 worlds in the project by asset reference, not by opening the
  two levels somebody remembers;
- referenced by **no asset** - registry referencers, hard and soft, empty;
- spawned from **no code** - no mention anywhere in `Source/` or `Config/`.

So he was dormant AND broken, which is the combination worth removing: an asset that looks usable and
silently is not costs whoever finds it next a session.

## Evaluate

**Deleted, and the deletion was VERIFIED ON DISK rather than from a return value** - which is the whole
story of this ticket.

`EditorAssetSubsystem.delete_asset` returned **True** and the file was still there.
`delete_loaded_asset` returned **True** and the file was still there. Two different APIs, both
reporting success, neither doing anything - and `does_asset_exist` agreed with the file system rather
than with the return value both times.

That is #287's "saved both was a lie" wearing different clothes, and it is why the check was
`ls` on the directory rather than the API's own opinion. The delete that worked was `git rm` with the
editor closed, and the asset is **recoverable from git** if he is ever wanted back.

`check_gdd` clean, 10 checks passed, after the ledger edit.

**Not established:** nothing observable changed in play, because nothing referenced him. There is no
runtime behaviour to watch here - this ticket removes a dormant asset and writes down a decision.

## Refine

**Stopped after two failed delete APIs rather than trying a third.** QUEUE.md rule 3 sets the limit at
three passes on a failing check; two API guesses returning a confident True was enough to say the
approach was wrong rather than the arguments, and `git rm` is a different mechanism rather than a
third variation of the same one.

**The ruling says what it does NOT decide.** "No bespoke boss character" is not "no climactic
encounter" - a Knight can be a boss. Written into the ledger explicitly, because a future session
reading only the headline would otherwise take it as a design cut it is not.

**Deliberately not done:** no Knight was placed anywhere to "replace" him. He was placed nowhere, so
there is no position to fill; `Patrol_Knight_1` already went into `L_CombatArena` in #292 and is the
Knight that exists. Placing a second one to satisfy the word "replace" would be inventing content
nobody asked for.
