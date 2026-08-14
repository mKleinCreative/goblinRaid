---
id: 087
title: A blocked swing is turned aside: cancel the attack and open the attacker up
agent: claude-recoil
status: done
claimed: 2026-08-08T23:38Z
build: required
waiting_on:
evaluated: 2026-08-09T00:17:10Z
files: 
  - Source/GoblinSiege/Combat/GSGameplayTags.h
  - Source/GoblinSiege/Combat/GSGameplayTags.cpp
  - Source/GoblinSiege/Combat/GSDamageExecCalculation.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.h
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_Block.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_Block.cpp
  - Source/GoblinSiege/Combat/GSDebugCommands.cpp
---

## Goal

A blocked swing is turned aside: cancel the attack and open the attacker up

## Generate

**Michael's ruling, 2026-08-08:** a blocked light attack should open the attacker up, and a block
should cancel the attack. That replaces the guard break as the answer to "nobody is ever punished".

New tag `State.Recoil`. `AGSCharacterBase::NotifyAttackWasBlocked(Blocker, RecoilSeconds)` sets it,
cancels the attacker's in-flight swing by the `State.Attacking` tag, and plays a flinch toward the
blocker. `UGSDamageExecCalculation` calls it on `bBlocked` — the one place that knows a block
*resolved*, since the ability only knows it swung and the victim only knows it was guarding. Window
is `GS.Combat.RecoilSeconds`, default 0.6, live-tunable beside the other block cvars.

`State.Recoil` goes in `ActivationBlockedTags` on **both** `UGSGA_SwordLight` and `UGSGA_Block`: a
recoiling fighter can neither swing again nor hide behind their own guard. Dodging is deliberately
left open so a player who reads their own mistake still has one way out.

`UBTTask_MeleeAttack` bypasses its own cooldown when the TARGET is recoiling — an opening nobody
exploits is not an opening. Read off the target rather than off self, so the rule is "everyone
punishes an opening" rather than per-side bookkeeping. `UBTTask_Block` ends early when its target
starts recoiling, so the blocker drops the guard and goes to hit them instead of holding a shield
through the reward it just earned.

Also: the silent `State.Attacking` decline in `UBTTask_Block` now logs (it was
indistinguishable from the node never running, which cost an hour on 2026-08-08), and
`GS.Combat.Duel` derives spawn height from each class's own capsule instead of a flat 95uu that was
shorter than these capsules and left every combatant embedded in the floor.

## Evaluate

**VERIFIED IN PIE, 3v3 on `L_CombatArena`.** The full chain in four consecutive lines:

```
[GS.AI]     BP_CastleGuard01_C_0: block ACCEPTED (telegraph, p=0.55) vs BP_KnightDPelegrini_C_0
[GS.Damage] BP_KnightDPelegrini_C_0 -> BP_CastleGuard01_C_0  raw 25.0  BLOCKED - armor 6.0  = 0.0
[GS.AI]     BP_CastleGuard01_C_0: block LANDED - BP_KnightDPelegrini_C_0 is open, dropping guard
```

**It did not deadlock, which was the real risk.** 30 damage events, **4 blocked, 26 landed clean**;
one pair ran 75 → 56 → 37 → 18 → dead in ~7s. The fear with a mechanic that rewards blocking is
that both sides block forever; at a 0.55 read chance against a ~2s swing cadence, blocks stay
occasional and decisive rather than constant.

**A second thing was settled by accident and matters more than the ticket.** The combatants
**closed on their own this time** — spawned 600uu apart, they paired off and fought without being
teleported together. So the "pathing is broken" worry from #085 was never pathing: it was the Block
decorator observing an *invalid* blackboard key (before #086) and aborting the lower-priority
`MoveTo` branch. The arena navmesh is 7000x7000 over a 6000x6000 floor and was never implicated.
The spawn-height fix is still right, but it was not the cause either.

**Not verified / not claimed:**
- **The guard break still has not fired** (0 in 30). It is now largely superseded for AI — the
  recoil is the punish Michael asked for — but the branch remains dead code in practice and nobody
  has proven it *can* fire.
- **The player side is untested.** This changes player feel materially: your blocked swings now
  cancel and leave you open for 0.6s, and an AI blocking you will immediately punish it. That is the
  intended symmetry, but no human has swung into a guard yet.
- TTK ~7s is still under the 10-18s band the research recommends. Untouched deliberately — that is a
  damage/cadence dial, and it should be set by someone watching, not by me.

**Owed to `AGENT_STATE.md`:** a DECISION line — a blocked swing is turned aside, not merely reduced;
and the correction that the #085 pathing suspicion was a decorator-on-invalid-key artefact.

## Refine

**Deferred the ability cancel by one frame**, which is the only non-obvious line in the change. The
exec calc runs inside the blocked swing's own `ApplyGameplayEffectSpecToTarget`, which is inside
`DoSweep`'s loop over its overlap results — cancelling there re-enters `EndAbility`, clears the
timers and resets `CurrentStage` while that loop is still iterating. The loose tag is applied
immediately (so nothing new can start) and only the cancel waits for `SetTimerForNextTick`.

Used `SetLooseGameplayTagCount(tag, 1)` rather than the Add/Remove pair, same as the telegraph in
#083: two swings blocked in quick succession should refresh one window, not stack two that outlive
the fight.

**Deliberately left undone:** no damage bonus during the recoil window. The opening *is* the punish —
the target cannot block the counter, so it lands unmitigated — and a multiplier would be a second
knob doing the first one's job. Also left: the guard break question, the TTK dial, and any player-side
tuning, all of which want a human watching a fight first.
