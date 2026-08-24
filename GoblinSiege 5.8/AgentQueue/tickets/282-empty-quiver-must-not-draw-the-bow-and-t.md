---
id: 282
title: Empty quiver must not draw the bow, and the starting quiver drops to 15
agent: claude-warren
status: review
claimed: 2026-08-24T16:35Z
build: required
waiting_on: "Michael: five seconds. With the bow out and the count at 0, press fire - no draw, no aim arc, nothing. I have no way to inject a button press from here, so the press itself is unwatched; everything it depends on is verified."
evaluated: 2026-08-24T17:25:24Z
observed:
scenario:
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Content/Data/Characters/DA_Char_Player.uasset
---

## Goal

Michael, having played #281: *"the count works, but you shouldn't be able to engage firing an arrow
when you have 0 arrows. Make the initial count 15 as well."*

The gate was in the wrong place. `CanActivateAbility` refuses the **loose**, but the draw and the aim
arc start on the **press**, and the ability does not activate until the **release**. So an empty bow
drew, aimed, swept the timing bar and then did nothing - which reads as a broken bow, not an empty one.

## Generate

**The draw is now gated on ammo**, in `AGSPlayerCharacter::Input_AttackPressed`'s ranged branch,
before `AimComponent->BeginAim` and before `BowTimingComponent->BeginDraw`. It also cancels a draw
already in flight, because the last arrow can be spent mid-sweep.

**It is NOT the cooldown gate that was removed from this exact spot, and it must not become one.**
The comment there records why that one was wrong: a 1.5s recovery is *transient*, so nothing
appearing for a moment reads as the bow being broken. An empty quiver is the opposite - a
**persistent** state the player can now see on the HUD - so refusing to draw reads as "I have no
arrows" rather than as a fault. #281 is what makes this legible: without the count on screen, not
drawing would be just as mysterious as drawing and not firing.

**One rule, one place.** `GetAmmoItemClass` was promoted to a public static pair on `UGSGA_BowShot`:

- `GetAmmoItemClassFor(const AActor*)` - which item this avatar's bow spends, or null for free shots
- `HasAmmoFor(const AActor*)` - can it loose one right now, as far as ammo goes

Three callers now share them - the ability (refusal and consume), `AGSPlayerCharacter` (whether to
draw at all) and `UGSPlayerHUDWidget` (what number to show). The HUD previously carried its own copy
of the weapon lookup; a HUD that answers "which item is ammo" independently is a HUD that can show a
number the bow does not spend. `HasAmmoFor` returns **true** for a bow that spends nothing, so free
shots are never blocked for want of ammo.

**`DA_Char_Player` starting arrows 30 -> 15.**

## Evaluate

**Build succeeded in 28s**, editor closed, only the two pre-existing `C4996` warnings.

**Runtime, PIE on `L_CombatArena`:**

| check | result |
|---|---|
| starting quiver | **15** |
| player with 15 arrows, activate | **True** |
| player drained to 0, activate | **False** |
| Erika at 0 arrows, activate | **True** - her bow spends nothing |

The last two matter because `CanActivateAbility` was **refactored** onto the shared predicate in this
ticket. Re-running both confirms the refactor preserved the behaviour #280 established, rather than
assuming a pure rename.

**The gate's placement was verified by reading the emitted control flow**, not by trusting the edit:
the `HasAmmoFor` check and its `return` sit above `BeginAim`, which sits above `BeginDraw`.

**NOT WATCHED, and it is the actual fix: nobody has pressed fire at zero arrows.** There is no input
injection available from here - `IA_Attack`'s mapping has no bound key to replay,
`APlayerController` exposes no key-injection method in this build, and `Input_AttackPressed` is not a
`UFUNCTION`. I stopped rather than stack a fifth workaround, as with #281's widget.

What that leaves unproven is one `if` with an early return. Everything it depends on is verified:
the predicate is correct at 15 and at 0, the branch is ordered correctly, and the ability still
refuses. **What to look at: bow out, count at 0, press fire - there should be no draw and no aim arc
at all.** Then walk over a bundle and confirm it draws again.

## Refine

**The obvious fix was the wrong one and is worth recording.** The instinct is to move the refusal
"earlier" - into `ActivateAbility`, or to have `CanActivateAbility` do more. Neither would have
helped: the ability is not involved on the press at all. The draw and the arc are started directly by
the character's input handler, and the ability only enters the picture on release. **A gate inside
the ability can never stop the bow from drawing**, which is why this had to move to the character
even though every other ammo rule lives in the ability.

**Consolidating the HUD was not in the request** and was done anyway, because this ticket created the
second copy's replacement: once `GetAmmoItemClassFor` existed as a public static, leaving the HUD's
private duplicate in place would have been choosing to keep a drift risk.

**Deliberately not done:** any audible or visual "empty" feedback on the refused press. The HUD count
reading 0 is the feedback, and it is already on screen - adding a click or a flash on top is a design
call, not a bug fix, and Michael has not asked for one.
