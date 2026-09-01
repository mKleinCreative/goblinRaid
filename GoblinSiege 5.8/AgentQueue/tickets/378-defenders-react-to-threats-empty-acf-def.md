---
id: 378
title: Defenders react to threats: empty ACF DefaultThreatMap means sight-based engagement never worked; victim never self-targets on hit
agent: claude-ai
status: done
claimed: 2026-08-30T09:26Z
build: none
waiting_on:
evaluated: 2026-08-30T10:42:22Z
observed: 2026-08-30T10:41:58Z | Guards and archer engaged from across the arena on sight alone, closing distance immediately instead of waiting at a short predetermined range; Michael watched it live in PIE and said the new range works great
scenario: PIE in the combat arena, player in open sightline of multiple defenders, no scripted trigger
files: 
  - Source/GoblinSiege/AI/GSAIControllerBase.cpp
  - Source/GoblinSiege/AI/GSAIControllerBase.h
  - Content/AI/BP_GSAIController_Militia.uasset
  - Content/AI/BP_GSAIController_Archer.uasset
---

## Goal

Defenders react to threats: empty ACF DefaultThreatMap means sight-based engagement never worked; victim never self-targets on hit

## Goal

Michael, watching the new patrols: "we need [guards] to reach to us a little quicker" - specifically
called out getting hit by an arrow, seeing a goblin, and fire nearby as three cases where reaction
felt slow. Investigated all three; two had concrete, fully-diagnosed root causes fixed here. Fire
investigation is NOT fixed - see Refine.

## Generate

**Root cause 1 - sight-based engagement has never worked at all, not just slowly.**
`AACFAIController::HandlePerceptionUpdated_Implementation` (ACF engine source,
`ACFAIController.cpp:183-194`) bails immediately if
`ThreatComponent->GetDefaultThreatForActor(Actor) == 0.f` - and `GetDefaultThreatForActor` reads a
per-class `DefaultThreatMap` on `UACFThreatManagerComponent`. Checked both `BP_GSAIController_Militia`
and `BP_GSAIController_Archer`'s CDOs directly via `execute_python_code`:
`threat.get_editor_property('default_threat_map')` returned `{}` on both - empty. So every defender's
sight perception has been silently discarding the player (and horde goblins) since this project's
inception; nothing has ever naturally engaged through sight. Every fight seen in testing so far
(including `#354`'s archer verification) was driven by a manual `SetTarget()` call, which bypasses
this gate entirely.

**Fix:** populated `default_threat_map` on both controller CDOs with `{BP_GSPlayerCharacter_C: 5.0,
BP_HordeGoblin_C: 5.0}` via `execute_python_code` (`threat.set_editor_property('default_threat_map',
{...})`), compiled and saved both Blueprints. 5.0 is an arbitrary modest baseline - enough to clear
the `!= 0.f` gate and register as *a* threat when nothing else is happening; damage-based threat
(root cause 2, below) uses the real hit magnitude, so a seen-but-not-yet-engaged actor stays lower
priority than an actual attacker once threat comparison matters.

**Root cause 2 - the guard who gets hit never targets their own attacker.**
`AGSAIControllerBase::HandlePawnDamagedAlertAllies` (`GSAIControllerBase.cpp`) wakes nearby ALLIES
when a defender takes damage, but explicitly skips the victim itself (`if (Other == this) continue`
in its loop - `Other` is always some OTHER controller). ACF ships a native self-target step
(`AACFAIController::HandlePawnDamaged`: `ThreatComponent->AddThreat(...)` then
`SetTarget(...)`, ungated on group membership), but it listens for `FACFDamageEvent`, which this
project's GAS-based damage (`GSDamageExecCalculation`) never fires - dead code for us, same pattern
as the ACF migration's other unwired components (`AGENT_STATE.md`'s "fourteen unconfigured ACF
components" note).

**Fix:** `HandlePawnDamagedAlertAllies` now does the victim's own self-target FIRST, before the
existing ally-wake loop: if the victim has no current target (`!GetTarget()`), pulls
`GetThreatManager()`, calls `AddThreat(Attacker, Max(Damage, 1.f))`, then `SetTarget(GetActorWithHigherThreat())`
- reusing the same threat bookkeeping `UACFUpdateCombatBTService` already reads, rather than a bare
`SetTarget` that would sidestep it. Falls back to a bare `SetTarget(Attacker)` if
`GetThreatManager()` somehow returns null (defensive, not expected to trigger - every defender
controller has the component per the CDO component listing). Guarded so a guard already mid-fight
with someone else does not flinch onto whoever hits them from behind - finishes the fight they're in.

Rebuilt clean (`Build-GoblinSiege.ps1 -IgnoreQueue`, only the two pre-existing unrelated
`AbilityTags` warnings), relaunched.

## Evaluate

**NOT verified in PIE.** Both fixes are freshly compiled/saved but nobody has watched a guard
naturally spot the player or fight back after being shot. What IS verified directly: the
`default_threat_map` read-back after `set_editor_property` shows both class keys present at 5.0 on
both controllers (not just "call succeeded", the actual stored value was re-read); the C++ compiled
clean; `GetThreatManager()`/`AddThreat`/`GetActorWithHigherThreat` are all real public ACF API
(confirmed against the ACF engine source directly, not guessed).

**Real risk not yet checked:** whether 5.0 is a sane magnitude relative to whatever else ends up in
the threat map later, and whether `IsActorAPotentialThreat`/`IsThreatening` (both gate
`HandlePerceptionUpdated_Implementation` before `GetDefaultThreatForActor` is even read) have their
own silent requirements not yet investigated - if either of those also returns false for the player
class, the default-threat fix alone will not be enough and sight still won't engage. Not confirmed
either way; flagged rather than assumed fine.

## Refine

**Fire investigation - NOT fixed, deliberately out of scope for this ticket.** `BTTask_Firefight`
genuinely exists and works (1500uu radius search, extinguish, alarm refund) but nothing currently
routes a guard toward a fire it isn't already standing near. Unlike the two fixes above, this one
has no single clean root cause - it needs an actual new mechanism (most likely: fire ignition
broadcasts a noise/alert similar to `HandlePawnDamagedAlertAllies`'s ally-wake pattern, or a BT
service that periodically checks for nearby `GSFlammableComponent::IsBurning()` and interrupts
patrol). Needs its own ticket and its own scoping pass rather than being bolted onto this one.

Handing back at `review` - needs Michael to watch a guard naturally notice the player (no
`SetTarget` cheat) and to watch a guard fight back after being shot from outside melee range.

**UPDATE 2026-08-30, live test with `GS.Combat.LogAI 1`:** all three fixes above WORKED - three hits
(Knight, Erika, a CastleGuard02) each correctly logged the ally-wake line
("hit by BP_GSPlayerCharacter_C_0 - woke N ally/allies"), confirming `OnDamaged` firing,
`HandlePawnDamagedAlertAllies` running, and the DefaultThreatMap gate no longer silently eating
perception. The repeated `Invalid Action Ability Tag - UACFAbilitySystemComponent::TriggerAction`
warnings clustering around each hit are a RED HERRING, traced this time (not just re-trusted from
`#354`'s old note): `ACFAbilitySystemComponent.cpp:140` fires it whenever `Internal_TriggerAction`
gets an empty `FGameplayTag` - it's ACF's own unset `EngagingAction`, unrelated to any of this.

**Real remaining problem, from Michael watching directly (not the log):** targeting/engagement
itself works, but the engage DISTANCE was too short - "the archer saw me but walked forward pretty
far to engage. The other guards didn't engage until I was within what looked like their
predetermined engage distance... at least twice the range for visual... they should want to engage
me from where we are in the arena just by sight."

**Root cause 3, found live:** `SightRadius`/`LoseSightRadius`/`PeripheralVisionAngleDegrees` on
`AGSAIControllerBase` are NOT the same thing as `SightConfig`'s (the actual `UAISenseConfig_Sight`
subobject `AIPerceptionComponent` senses through) own same-named properties - two independent
copies that only ever agreed by coincidence. The constructor does
`SightConfig->SightRadius = SightRadius` exactly ONCE, using the raw C++ class default (1200) at
that exact line - which runs BEFORE a Blueprint's own property overrides load onto the object. So
editing `SightRadius` on `BP_GSAIController_Militia`/`_Archer` (which is what a designer would
naturally do, and exactly what this ticket's root-cause-1 discussion assumed would matter) has
LOOKED like it works (the details panel shows the new number) and done NOTHING at runtime.
Confirmed live: read `sight_config.sight_radius` off both CDOs after editing the controller's own
field - still 1200, unchanged.

**Fix:** (1) edited `SightConfig` directly on the CDOs (both controllers share the SAME SightConfig
default subobject, confirmed by identical object path) to `SightRadius=2400, LoseSightRadius=3000`
- doubled per Michael's "at least twice" - compiled and saved, verified the new values read back
correctly post-compile. (2) Added `AGSAIControllerBase::PostInitProperties()`, which re-copies
`SightRadius`/`LoseSightRadius`/`PeripheralVisionAngleDegrees` into `SightConfig` after property
loading - this is what makes future edits to the controller's own (intuitively-named) fields
actually take effect, instead of remaining silent dead decoys for whoever touches this next.
`LoseTargetDistance` (ACF's own field, gates DROPPING an already-acquired target, not initial
engagement) was checked and left alone - already a generous 3500uu default, not the bottleneck.

Rebuilt clean, relaunched. Not yet re-tested live - needs Michael to confirm the new sight range
actually reads as "engage from across the arena" rather than re-guessing a further number blind.

**UPDATE 2026-08-30, live test with `GS.Combat.LogAI 1`, third pass:** the self-target fix is
confirmed working from the log itself this time (`self-targeted BP_GSPlayerCharacter_C_0 after
being hit`). Michael, watching directly: targeting works, but defenders STILL would not close
distance until he approached - "the archer saw me but walked forward pretty far to engage. The
other guards didn't engage until I was within what looked like their predetermined engage
distance... they should want to engage me from where we are in the arena just by sight."

**Root cause 4, found by opening the actual EQS asset in-editor (Python reflection blocks reading
EQS graph data - `Options` is a protected property; had to capture a real screenshot and read the
node details, then Michael confirmed the generator/test names off it).** The BT branch that
generates a defender's MOVE-TO destination once in combat (`ACFCombatBT`'s
`BTTask_RunEQSQuery_0`, running `EQS_StudyTarget`) has TWO runtime query parameters baked in as
overrides on the BT task node itself (not in the EQS asset - confirmed via
`eqs_request.query_config` on the task, a plain reflected struct array, once found):
`OnCircle.CircleRadius = 500` (candidate move-to points form a ring 500uu around the TARGET - the
approach standoff, reasonable) and, the actual bug, `Distance.FloatValueMax = 800` (a candidate
point is only valid if it is within 800uu of the QUERIER - the DEFENDER's own current position).
A defender starting more than roughly 1300uu from the target has every candidate point on that
500uu ring fall outside its own 800uu cap, so the EQS query returns NOTHING and the defender has a
target but nowhere to walk - which reads exactly as "won't approach until I get close", because
close enough is the only condition under which this query can ever succeed.

**Fix:** did NOT edit `ACFCombatBT` in place - it is ACF's own bundled vendor asset
(`/AscentCombatFramework/Blueprints/AI/ACFCombatBT`), shared by everything that runs ACF combat and
due to be silently overwritten on the next plugin update. Duplicated it to
`/Game/AI/GS_ACFCombatBT`, raised `Distance.FloatValueMax` 800 -> 3000 on the copy (matching the
new 2400/3000 sight range from root cause 3 - the movement query should not be a tighter bottleneck
than perception itself), left `OnCircle.CircleRadius` at 500 (the standoff distance is not what was
broken). Repointed `BT_Defender`'s `BTTask_RunBehavior_1.behavior_asset` from `ACFCombatBT` to the
new `GS_ACFCombatBT` copy. Both saved; confirmed on disk (`git status`: `BT_Defender.uasset`
modified, `GS_ACFCombatBT.uasset` new). Pure data edit, done live in the already-running editor -
no C++, no rebuild, no relaunch needed for this pass.

**Side effect, noted not hidden:** `BP_GSHordeAIController.uasset` (the player's own ALLIED goblins'
controller) also shows modified in git status, though never directly touched this session - it
shares `AGSAIControllerBase`'s `SightConfig` default subobject with the defender controllers, so
root cause 3's fix reached it too. Allied goblins seeing further is not a regression, just flagging
since it wasn't the explicit target of any edit.

Not yet re-tested. This is the fourth fix in this ticket's chain (empty threat map, victim
self-target, sight/perception desync, now the EQS movement-generation cap) - each prior one was
real and necessary but not sufficient on its own, which is exactly why this needed a human watching
between every attempt rather than being declared fixed after the first plausible-looking change.

**UPDATE 2026-08-30, live test, fourth pass - CONFIRMED WORKING.** Root cause 3's first values
(2400/3000) were retuned once more to `SightRadius=3000, LoseSightRadius=3300` - baked directly into
`AGSAIControllerBase`'s C++ class defaults this time (not the Blueprint CDO's `SightConfig`
override), after the earlier Python CDO edit was independently found to silently revert on a later
Blueprint recompile - the same failure mode described above for why `PostInitProperties()` was
needed at all, just hitting the tuning value too rather than only the initial sync. C++ defaults do
not depend on fragile Blueprint archetype-override tracking, so this is the reliable form going
forward. Michael watched it live in PIE and said "the new range works great" - defenders now engage
from across the arena on sight alone, matching "at least twice the range for visual" from his
original report. This closes the ticket's open loop; the fire-alert mechanism named in the Refine
above is still explicitly out of scope and would need its own ticket.
