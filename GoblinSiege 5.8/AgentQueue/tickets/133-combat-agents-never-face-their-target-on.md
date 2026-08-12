---
id: 133
title: Combat agents never face their target: one facing authority, plus the directional locomotion to make it read
agent: claude-facing
status: done
claimed: 2026-08-12T01:35Z
build: none
waiting_on: 
evaluated: 2026-08-12T02:38:01Z
files: 
  - GoblinSiege 5.8/Source/GoblinSiege/AI/GSAIControllerBase.h
  - GoblinSiege 5.8/Source/GoblinSiege/AI/GSAIControllerBase.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_Block.cpp
  - Content/Characters/Humans/ABP_Human.uasset
  - Content/Characters/ScoutV2/Animations/ThirdPerson_AnimBP_Gob.uasset
---

## Goal

Combat agents never face their target: one facing authority, plus the directional locomotion to make
it read

Michael, 2026-08-11, after #132 landed and the spacing "looked good": *"I want to work on keeping
track of where enemies are. Goblins are looking off to the side and not paying visual attention to
enemies right in front of them. Even if they might be doing it programmatically."* - i.e. the combat
logic is fine, the character does not LOOK at the man it is fighting.

**Session 1 (2026-08-12 01:35Z) built nothing** - it was investigation plus a plan, written down
because that session was about to be restarted to reconnect the VibeUE MCP server.

**Session 2 (2026-08-12, this one) picked the ticket up with the MCP tools connected** and delivered
sections A-D below, minus the head Look At. Michael directed the takeover; the `blocked` state was
only ever the missing tool. **It BUILDS CLEAN and has NEVER BEEN WATCHED RUN** - see Evaluate.

## Generate

### The diagnosis: there is no persistent facing, and two authorities fight over what there is

Facing a target is set by three BT nodes, each only while that node is running:

| node | when it faces | who runs it after #132 |
|---|---|---|
| `UBTTask_MenaceOrbit` | every tick | only agents WITHOUT a token - 2 of 6 |
| `UBTTask_MeleeAttack` | on `ExecuteTask` only | token holders, but see below |
| `UBTTask_Block` | while blocking | whoever is blocking |

Everything else falls through to `bOrientRotationToMovement`, which points the body **where it is
walking**. `BTTask_Block.h:121` states this outright and treats it as known: *"these pawns run
bOrientRotationToMovement with bUseControllerRotationYaw false, so a defender standing still to block
never turns at all."*

Two specific holes:

1. **`BTTask_MeleeAttack` never faces during cooldown.** The cooldown gate returns `Failed` at
   `.cpp:49-54`, which is *before* the facing block at `.cpp:117`. Its own comment says failing
   "keeps the defender repositioning between swings instead of standing frozen mid-cooldown" - so the
   agent spends the whole inter-swing window walking, with its body pointed along its velocity.
2. **The three nodes that DO face are fighting the movement component for the same yaw.** They call
   `SetActorRotation` in their tick; `bOrientRotationToMovement` rotates the actor toward its
   acceleration in `PhysicsRotation` in the same frame. Whichever runs later wins, and the tick order
   of controller / BT component / character movement is not guaranteed.

**That second point is worth a hard look before writing any code: #109 ("MenaceOrbit sets facing
instantly, up to 167 degrees in one frame") and #124 ("the melee facing snap turns 120 degrees in one
frame") may both be symptoms of this fight rather than independent bugs.** Both were fixed by
rate-limiting the deliberate turn, which would not stop the movement component undoing it. Verify
before assuming - this is a hypothesis, not a finding.

**#132 plausibly made it more visible, and that is this agent's own doing.** The separation steer
added in that ticket contributes lateral `AddMovementInput`, and lateral input under
`bOrientRotationToMovement` rotates the body sideways. Raising the token budget also doubled the
number of agents in the attack branch (2 -> 4 per victim), and the attack branch is the one with the
cooldown hole above.

### The asset situation, corrected

A first pass reported "no strafe blendspace and no aim offset exist". **That was wrong** - it searched
blendspace naming (`BS_`, `Blend`) rather than animation clips. Michael said the strafe animations
should exist; they do. Corrected inventory:

**Goblin** (`Content/Characters/ScoutV2/Anims_Loco/`, `Anims_LocoSet/`): full directional set already
imported - `A_MX_standing_walk_forward/back/left/right_Gob`,
`A_MX_standing_run_forward/back/left/right_Gob`, `A_MX_Std_WalkF/B/L/R_Gob`, `A_MX_Std_RunF/B_Gob`,
plus turn-in-place `A_MX_standing_turn_90_left/right_Gob`. Also `ThirdPerson_IdleRun_2D_Gob`, an
existing 2D blendspace the first search missed because of the `_2D_` infix.

**Human** (`Content/Characters/Humans/Anims/`): `A_HU_Std_WalkF/B/L/R` (full 4-way walk) but
`A_HU_Std_RunF/B` only - **no strafe run**. That was the one real gap.

**Michael has since downloaded the fix** (`C:\Users\Michael\Downloads`, 2026-08-11 18:31-18:33):

- `Locomotion Pack.zip` (5.43 MB) - `castle_guard_01.fbx` (rig/mesh) plus `idle`, `walking`,
  `running`, `left strafe`, `right strafe`, `left strafe walking`, `right strafe walking`,
  `left turn 90`, `right turn 90`, `left turn`, `right turn`, `jump`.
- `Left Strafe.fbx` / `Right Strafe.fbx` - loose, 227,456 bytes each; the pack carries its own copies
  at a slightly different export size, so confirm which pair to import rather than importing both.

Both loose FBX verified as standard Mixamo rigs (`mixamorig:Hips`, binary FBX), same family as every
`A_MX_*` already in the project - so the established import/retarget pipeline from #112 and #119
(Mixamo source -> `SK_Human_Skeleton`) applies unchanged.

### The plan

Ordered so the thing that makes it look right lands in the same build as the thing that changes
behaviour. Squaring up to a target without directional locomotion is strictly worse than today: the
agent slides sideways playing a forward run.

**A. Import (editor + VibeUE tools).** The pack's animations onto the human skeleton, following the
existing naming (`A_MX_*` into `MixamoSource/Anims`, retargeted to `A_HU_Std_*`). Goblin needs
nothing - it already has its directional set. Confirm `castle_guard_01`'s proportions against
`SK_Human_Skeleton` before assuming a straight import rather than a retarget.

**B. Blendspaces.** `Direction` x `Speed` 2D blendspace per rig, `Direction` from
`CalculateDirection(Velocity, ActorRotation)`. Goblin may be able to extend
`ThirdPerson_IdleRun_2D_Gob` rather than adding a second asset - check what its axes actually are.

**C. AnimBP wiring.** Locomotion state drives the new blendspace in both `ABP_Human` and
`ThirdPerson_AnimBP_Gob`. Add a `Look At` bone control on the head/neck aimed at the current target,
which needs no new animation asset and is the literal "paying visual attention" Michael asked for.
Note `ABP_Human` already carries the UpperBody slot split from #129 - do not disturb it.

**The look-at must not be a hard lock.** See the prior-art section below: give it a priority order and
an interest decay, or the result is a goblin whose head is welded to the player, which reads as
robotic targeting rather than attention. That is a different wrong answer, not a lesser one.

**D. C++: one facing authority.** While a combat agent holds a target: `SetFocus(Target)` on the
controller, `bUseControllerDesiredRotation = true`, `bOrientRotationToMovement = false`; restore on
disengage. Then **remove** the `SetActorRotation` calls from `MenaceOrbit`, `MeleeAttack` and `Block`,
so exactly one thing decides yaw - the #108 lesson, applied to rotation instead of position. Turn rate
is already plumbed: `AGSCharacterBase::SetTurnRateRadPerSec` writes `CharacterMovementComponent::
RotationRate`, which is what `bUseControllerDesiredRotation` interpolates at. Put it behind a cvar
(`GS.Combat.FaceTarget 0|1`) so it A/Bs in one PIE session, as #131 and #132 both did.

The controller is the right home and already ticks as of #132.

### Prior art: how Elder Scrolls does this

> **Added 2026-08-12, AFTER session 2 built and shipped the body-facing half.** It sits in this
> section because it is background for the plan above, not because it was available when the plan was
> written. Its live consequence is for the ONE piece session 2 deliberately left undone - the head
> `Look At` - so read it alongside "Deliberately left undone" at the foot of the ticket.

Skyrim is the only one of these with public documentation, via the Creation Kit wiki. Read it as two
separate lessons: one thing to copy for the remaining work, one thing that is a follow-up, and one
thing to deliberately not copy.

**COPY (the outstanding Look At): head tracking is a separate system from body facing, with priority
and decay.**

Skyrim's head tracking is an always-on SOCIAL system, not a combat one, and that decoupling is the
part worth taking - it is the same split as step C (Look At) versus step D (facing authority) above,
arrived at independently, which is mild evidence the split is right.

Its target selection is a priority list, not "the current enemy": the actor being spoken to, then
actors talking to other NPCs, then whoever is straightest in front. Crucially it has **interest
decay** - an NPC loses interest in a target over time and stops tracking it unless it is directly
ahead, and re-acquires if that actor starts talking or changes AI package. Tunable through INI:
`fUpdateDelayNewTargetSecondsMin` / `Max` (1.5 / 3.0), `fMinPathLookAtPointDist` (128),
`fMaxPathLookAtPointDist` (512).

Applied here: the Look At target should be a short priority list (current combat target > nearest
live hostile > whoever last hit me) with a re-evaluation delay of roughly 1.5-3s and a lapse when the
target sits outside the head's comfortable cone. A permanent weld to `TargetActor` is cheaper to
write and will read as lock-on.

**FOLLOW-UP (not this ticket): Group Offensive Mult.**

The one mechanism Skyrim has that this project does not. Per the CK wiki it "will override the
offensive mult... the more actors that are attacking the player, the less offensive they will be" -
i.e. a CONTINUOUS aggression scalar that falls as the gang grows, where #090/#132's token budget is a
DISCRETE permission gate. They are complementary: tokens decide who may swing, a group scalar decides
how hard the whole gang presses. Six engaged at full individual aggression still reads as a pile-on
even when only four hold tokens.

Cheapest version here: scale `DefaultAttackCooldownSeconds` and the menace feint interval by
`UGSEngagementComponent::GetEngagedCount()`. Worth its own ticket AFTER #132's numbers have been
watched - it is a pressure dial, and tuning it against an unmeasured baseline is how #105-#110 went.

**DO NOT COPY: Skyrim's crowd handling, because it does not have any.**

Vanilla has no attacker reservation at all - every enemy attacks at once, however it wants. It is the
single most-modded aspect of Skyrim combat, and the mods that fix it (*Wait Your Turn - Enemy
Circling Behaviour*, *Wait Your Turn Redux*, *Puppeteer*) are reimplementing the DOOM-style token
rationing #090 already built. Same for attack commitment: vanilla actors turn freely mid-swing, which
the *Attack Commitment* mod calls "one of the biggest problems with Skyrim's combat" and fixes by
clamping to 20 degrees of turn during an attack. This project solved that in #130 (swing commitment)
and via `FGSAttackGrant::bLocked`.

**A correction to the record.** Bethesda was cited in passing during #132's session as a reference
point for crowd rationing. It is the counter-example, not the model. #090's header already cites the
right lineage - DOOM 2016's token bank and Michael Dawe's Kingdoms of Amalur chapter (Game AI Pro
ch.28) - and nothing here changes that.

**Also noted, for whenever the archetype data gets revisited.** Skyrim keeps all of this in one
reusable data record per archetype (CSTY), referenced by many actors, rather than in the behaviour
logic. Its close-range fields are near one-to-one with dials this project has scattered across BT
node defaults: `Circle Mult` ~ `StrafeSpeedScale`, `Fallback Mult` ~ `BackOffSpeedScale`,
`Flank Distance` ~ `RingRadius`, plus `Stalk Time` (time flanking before committing) and
`Strafe Mult` (dodging projectiles), neither of which exists here. `FGSArchetypeDefinition` is the
natural home. Not a task - an observation about where these numbers want to live eventually, and #107
already ruled per-archetype budgets are "tuning, not repair" until the positional work is measured.

Sources: [Combat Style](https://ck.uesp.net/wiki/Combat_Style),
[CSTY record format](https://en.uesp.net/wiki/Skyrim_Mod:Mod_File_Format/CSTY),
[Combat AI Uncapped](https://www.nexusmods.com/skyrimspecialedition/mods/102200),
[Wait Your Turn](https://www.nexusmods.com/skyrimspecialedition/mods/65091),
[Attack Commitment](https://www.nexusmods.com/skyrimspecialedition/mods/3325),
[Skyrim INI/HeadTracking](https://stepmodifications.org/wiki/Guide:Skyrim_INI/HeadTracking).

### Open questions for whoever picks this up

1. **Does the player want the same treatment?** He runs `bOrientRotationToMovement` with an aim-state
   override (`UpdateRotationMode`, `GSPlayerCharacter.cpp:1081`). Out of scope as written, and
   changing it is a feel decision, not a bug fix.
2. **Turn-in-place.** The pack has `left/right turn 90`. Once the body squares up and holds, a
   stationary agent tracking a circling enemy will foot-slide without it. Probably a follow-up.
3. **Archers.** `UBTTask_RangedAttack` already calls `SetFocus` with `EAIFocusPriority::Gameplay`
   (`.cpp:77, 119`). A controller-level focus must not fight it - check the priority levels, and
   prefer letting the ranged task keep the higher one.

---

# Session 2 - what was actually built

## Generate (session 2)

**The claimed file list grew by three.** `BTTask_MeleeAttack.h` (a new tolerance property),
`Content/Characters/Humans/Anims/A_HU_Std_RunL|RunR.uasset` and the two new blendspaces. The queue
held only this ticket throughout, so nothing was blocked.

### 0. The diagnosis above is WRONG on the one fact the whole plan rested on

Session 1 wrote, quoting `BTTask_Block.h:121`: *"these pawns run `bOrientRotationToMovement` with
`bUseControllerRotationYaw` false."* Read off the CDOs, both are inverted:

| pawn | `bOrientRotationToMovement` | `bUseControllerRotationYaw` | MaxWalkSpeed |
|---|---|---|---|
| BP_CastleGuard01 | **false** | **true** | 1210 |
| BP_ErikaArcher | **false** | **true** | 1023 |
| BP_GSPlayerCharacter | true | false | 470 |

That inverts the mechanism, and it makes the fix *more* certain rather than less.
`bUseControllerRotationYaw` makes `APawn::FaceRotation` **assign** the pawn's yaw from the control
rotation every frame - so the rate-limited `SetActorRotation` in all three BT nodes was being
overwritten in the same frame it ran, and since nothing outside `UBTTask_RangedAttack` ever called
`SetFocus`, the control rotation pointed wherever path following last left it. A defender looked
along its last path while fighting something in front of it. That is the complaint, exactly.

It also means **#109 and #124 were probably chasing this**: both fixed a snap by rate-limiting a
deliberate turn, which cannot help when the thing undoing the turn is an un-interpolated
`FaceRotation`. Stated as the likely explanation, still not proven - `GS.Combat.FaceTarget 0` is how
anyone falsifies it.

### 1. The human strafe-run clips - and the collapse Michael caught

**Michael stopped this mid-flight:** *"right out of the gate it looks wrong, the spine looks like
it's collapsing inside the body again."* He was right, and it was mine.

Session 1's plan said to import the Locomotion Pack and to *"confirm `castle_guard_01`'s proportions
against `SK_Human_Skeleton` before assuming a straight import rather than a retarget."* I imported
first. Measured against the known-good `A_HU_Std_WalkL`, every bone in the import came out at
**exactly 0.600x**: Spine 24.44 -> 14.66, Neck 44.51 -> 26.70, LeftLeg 59.87 -> 35.90, LeftFoot
67.66 -> 40.58. The pack is exported from a character 60% the size of `SK_Human_Skeleton`, and

**every bone on `SK_Human_Skeleton` is set to `Animation` translation retargeting**, so those
translations are applied verbatim and the skeleton telescopes inward. The existing `A_HU_*` set only
ever worked because it happens to be exported at matching scale. The trap was armed for whoever
imported next.

Two fixes were tested and one was **disproven by measurement**: `import_uniform_scale = 1/0.6` moved
only the Hips (159.3 -> 149.1) and left every child bone at 0.600. Recorded because it is the
obvious-looking fix and it does not work.

**The real answer was that no import was needed at all.** `A_MX_standing_run_left` and
`A_MX_standing_run_right` have been sitting in `/Game/Characters/MixamoSource/Anims/` the whole time
on the XBot skeleton, alongside the `IK_MixamoXBot -> IK_Human` retargeter `RTG_MixamoToHuman` built
by #112/#119. Retargeted through it, the result measures **1.000x on every limb bone** against the
existing set. The Downloads pack was a red herring; the source was already in the project.

New: `A_HU_Std_RunL`, `A_HU_Std_RunR`. Nothing was imported from `C:\Users\Michael\Downloads`, and
`SK_Human_Skeleton` was **not** modified - the retargeter handles proportions, so the shared-skeleton
edit (and its blast radius across every human animation) was avoided entirely.

### 2. Two 2D blendspaces

`BS_GS_Locomotion_Hu` and `BS_GS_Locomotion_Gob`. Direction (-180..180, wrapping) x Speed, 12 samples
each: idle at all four bearings, then a walk row and a run row at B/L/F/R.

**Session 1's open question about `ThirdPerson_IdleRun_2D_Gob` is answered: it is a `BlendSpace1D`**,
despite the `_2D_` in its name. It could not be extended; the goblin needed a new asset like the human.

Each sample carries a `rate_scale` = row speed / that clip's own measured travel, so the row is
coherent instead of four clips at four different foot speeds. The goblin clips are **in place** (zero
travel), so their natural speeds were taken from the XBot sources they were retargeted from - the
goblin rig measures within 0.5% of XBot (hip 90.4 vs 90.9), so that carries across unscaled.

Human rows 160 / 420 uu/s; goblin rows 130 / 310 uu/s.

### 3. AnimBP wiring

Both AnimBPs compile `BS_UP_TO_DATE`.

- **`ABP_Human`**: new `HU_Direction` float, set from `CalculateDirection(GetVelocity, GetActorRotation)`
  appended to `BlueprintUpdateAnimation`. The blendspace player replaces the Idle/Walk/Run state
  machine as the source of `Slot 'DefaultSlot'`. **The #129 UpperBody split is untouched** - the
  layered-blend-per-bone and both slots wire exactly as before.
- **`ThirdPerson_AnimBP_Gob`**: it already *had* a `Direction` variable and **nothing ever set it** -
  a ThirdPerson-template leftover. Now driven the same way. The 1D blendspace on the `Idle/Run` state
  was swapped for the 2D one and its X pin re-pointed from Speed to Direction.
  - The `Set Direction` node went in **before** the `Cast To BP_GSPlayerCharacter` in that graph, and
    that ordering is load-bearing: the cast fails on a horde goblin, so anything wired after it never
    executes for an AI pawn - which is every pawn this ticket is about.

**Neither state machine was deleted.** `ABP_Human`'s Locomotion machine is still in the graph, merely
unwired, so reverting this is one reconnect.

### 4. The facing authority

`AGSAIControllerBase::TickFacing`, behind `GS.Combat.FaceTarget` (default 1), on the tick #132 added.
While the blackboard `TargetActor` is set and both parties are alive: `SetFocus(Target,
EAIFocusPriority::Gameplay)`, `bUseControllerDesiredRotation = true`, `bUseControllerRotationYaw =
false`, `bOrientRotationToMovement = false`. Released on disengage, on death and on unpossess.

- **Gameplay priority, not Move.** Move looks tidier and is wrong: path following parks its own focus
  there (`AAIController::SetMoveFocus`), so a combat focus at Move is overwritten by every MoveTo -
  the bug being fixed, one layer down. Session 1's open question 3 (does this fight the archer?) is
  answered: `UBTTask_RangedAttack` focuses **the same blackboard actor at the same priority**, so both
  ask for the same thing and last-writer-wins writes the same value.
- **The rotation mode is captured off the pawn, not assumed** - precisely because assuming those two
  flags is the mistake session 1 made.
- **The three `SetActorRotation` calls were NOT removed**, against the letter of session 1's plan.
  They are gated behind `!IsFacingAuthorityEnabled()`. Deleting them would make `GS.Combat.FaceTarget 0`
  a deadlock rather than a comparison: `UBTTask_MeleeAttack` refuses to swing until it faces its
  target, and #089 records what a refusing node that stops making progress does (stands at 130uu, zero
  velocity, never swings). #131 and #132 both shipped behind a switch for the same reason.
- `UBTTask_MeleeAttack` gains `FacingToleranceDegrees` (25). Under controller facing it judges and no
  longer turns. It needs a **tolerance** rather than the old exact-equality test because
  `bUseControllerDesiredRotation` approaches its goal asymptotically and never arrives, and against a
  moving target never settles - exact equality would refuse every swing forever.

## Evaluate

**BUILD SUCCEEDED in 02:16**, editor closed, `-IgnoreQueue` on Michael's explicit instruction (this
was the only open ticket). No new warnings - the two C4996 `AbilityTags` deprecations in
`GSGA_Block.cpp` and `GSGA_Interact.cpp` are pre-existing and untouched, exactly as #132 recorded.

**NOTHING HAS BEEN WATCHED. NO FIGHT HAS RUN.** A compile proves the code is well-formed and nothing
else. Given `AGENT_STATE`'s FAILED entry - three consecutive fixes shipped unwatched, all three
wrong - that is the headline, not a footnote.

**Verification recipe, one PIE session:**

```
GS.Combat.LogAI 1
GS.Combat.Duel 4                 # or horn a warband onto a patrol
GS.Combat.FaceTarget 0           # A/B the whole change, live
GS.Combat.FaceTarget 1
GS.Combat.CrowdWatch 20 4        # #132's spacing must not have regressed
```

The thing to watch first is whether a guard **on cooldown between swings** now keeps its body on its
enemy - that was the specific hole (`BTTask_MeleeAttack` returns Failed at the cooldown gate long
before it ever reached the old facing code), and it is the clearest single tell that the controller
authority is doing its job.

**What is verified, and by what evidence:**

- The 0.600x collapse and the 1.000x retarget: **measured**, bone by bone, both directions. This is
  the one claim here I would defend without watching it.
- `import_uniform_scale` does not fix it: **measured**, and it is the fix most people would reach for.
- The CDO rotation flags: **read off the CDOs**, not inferred. This is what makes the whole mechanism
  claim solid.
- `ThirdPerson_IdleRun_2D_Gob` is 1D: **read off the asset.**
- Goblin `Direction` was never set: **read off the graph** - zero connections mention it.
- Both AnimBPs compile and both blendspaces read back with 12 correct samples: **read back after
  writing**, per the rule that a successful call is not proof.

**What is arithmetic or design, and has never been seen:**

- Every row speed and every `rate_scale`. The walk/run crossover points (160/420 human, 130/310
  goblin) are picks, not measurements of what looks right.
- `FacingToleranceDegrees` 25 is reasoned from the swing trace, not measured against a real swing.
- That the facing change looks *good* rather than merely correct. `bUseControllerDesiredRotation`
  interpolating at `RotationRate` is the intent; whether a guard turning at 360 deg/s reads as
  menacing or as a turret is Michael's eye.
- The #109/#124 explanation stays a hypothesis on purpose.

**A real problem this work uncovered and did NOT fix:** `BP_CastleGuard01` has MaxWalkSpeed **1210**
against a run row of 420 uu/s - the animation is ~2.9x too slow for the movement, so a charging guard
must foot-slide badly however good the blendspace is. This is pre-existing (equally true of the single
`A_HU_Std_RunF` before today) and out of this ticket's scope, but it is very likely part of what
"the animations look wrong" means, and no blendspace can fix it. It is a speed/animation balance
decision for Michael.

**Not done from session 1's plan: the head `Look At` bone control** (section C). The body now squares
up, which is the substantive half of "paying visual attention"; the head-tracking refinement was left
rather than piled on top of a change nobody has watched yet.

**What this owes `AGENT_STATE.md` when it closes** (nothing reads old tickets at run start):

- **FAILED / gotcha:** *`SK_Human_Skeleton` has every bone on `Animation` translation retargeting, so
  a Mixamo FBX exported from a differently-proportioned character imports as a COLLAPSED skeleton -
  measured at exactly 0.600x on every bone from the Locomotion Pack. `import_uniform_scale` does NOT
  fix it (it moves only the Hips). Import human animation through `MixamoSource` +
  `RTG_MixamoToHuman` instead, which measures 1.000x. Michael spotted it by eye in seconds; the
  ticket had told the agent to check proportions first and it did not.*
- **DECISION:** *Combat facing has ONE authority as of #133 - `AGSAIControllerBase::TickFacing`
  (`SetFocus` + `bUseControllerDesiredRotation`), behind `GS.Combat.FaceTarget`. The three BT nodes
  keep their turn code only as the switch-off path. Defender CDOs ship
  `bOrientRotationToMovement=false` / `bUseControllerRotationYaw=true` - the OPPOSITE of what
  `BTTask_Block.h:121` claims, and that stale comment should be corrected wherever it is repeated.*
- **NEXT / open:** *`BP_CastleGuard01` MaxWalkSpeed 1210 against a ~420 uu/s run animation. No
  blendspace can absorb a 2.9x mismatch; this is a speed-vs-animation balance decision.*

## FINAL STATE AS SHIPPED - read this before the narrative below

The blendspace work landed **asymmetrically**, and the ticket closes that way deliberately:

| rig | locomotion source at close | status |
|---|---|---|
| **Goblin** (`ThirdPerson_AnimBP_Gob`) | `BS_GS_Locomotion_Gob`, Direction->X, Speed->Y | **NEW - Michael confirmed it looks good** |
| **Human** (`ABP_Human`) | its ORIGINAL Idle/Walk/Run state machine | **REVERTED - unchanged from before this ticket** |

`BS_GS_Locomotion_Hu` exists, is correct by every check available, and is **not wired into anything**.
Its blendspace-player node sits in `ABP_Human`'s AnimGraph with X/Y bound to `HU_Direction`/`HU_Speed`
but its Pose output goes nowhere; the state machine feeds `Slot 'DefaultSlot'` exactly as it always
did. `A_HU_Std_RunL`/`RunR` and `HU_Direction` are likewise built, correct, and currently unused.

**Wiring the human is a two-line job for whoever picks this up** - disconnect the state machine from
`Slot 'DefaultSlot'.Source` and connect the blendspace player's Pose there instead - but it was left
undone on purpose. The goblin was rewired first as a single-rig test after two broken passes, it was
confirmed good, and shipping the human on the same pattern without a second confirmation is the exact
habit that produced those two passes.

The C++ facing authority applies to **both** rigs and is live for both.

## Refine

**Changed in response to MICHAEL WATCHING IT RUN - the first pass of both blendspaces was broken.**
Reported symptom: *"a lot of the animations broke... blends weren't successfully implemented"*, and on
the two-part question, **T-pose/reference pose AND wrong-clip-for-the-motion**. That pairing is what
identified it: a pace error cannot produce a T-pose, but a **hole in the sample hull** produces both.

Three construction errors, all mine, all in the first pass:

1. **The direction axis had a gap.** Samples sat at only `-180, -90, 0, +90`, with `wrap_input=True`
   trusted to close the arc from +90 back round to +180. It does not close the convex hull, so any
   agent moving **backward-right** was outside every sample and evaluated to the reference pose. Fixed
   by placing an explicit `+180` column (duplicating the `-180` clips) and turning `wrap_input` off:
   the hull is now a full rectangle, every row `[-180,-90,0,90,180]` COMPLETE, verified by read-back.
2. **Double speed correction.** Per-sample `rate_scale` AND `axis_to_scale_animation = BSA_Y` were
   both set, so playback was scaled twice. The asset that demonstrably worked
   (`ThirdPerson_IdleRun_2D_Gob`) uses axis scaling alone with `rate_scale` 1.000 on every sample.
   Now matched: all rates back to 1.0, pace from the axis.
3. **Dropped content and a truncated axis.** `A_MX_Sprint_Gob` (in `Anims_Climb`, not `Anims_Loco`)
   sat at speed 763 in the old asset and was simply missing from mine, whose axis stopped at 470. The
   human axis stopped at 500 against a guard MaxWalkSpeed of 1210. Both axes now cover the real range
   (human 0..1210, goblin 0..800) and the sprint row is restored.
4. **THE ONE THAT ACTUALLY KILLED IT: the blend surface was never generated.** After fixes 1-3 the
   answer was *"there's no locomotion animations for goblins or humans right now"* - worse, not
   better. `UBlendSpace` builds its grid/triangulation inside `PostEditChangeProperty`, and UE
   Python's `set_editor_property` was not firing it for these arrays. **The samples were written and
   the blend surface never existed**, so the runtime had nothing to interpolate and every agent fell
   through to the reference pose. Fixed by re-writing `sample_data`, `blend_parameters` and
   `axis_to_scale_animation` with `notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS`, plus
   `target_weight_interpolation_speed_per_sec` 5.0 to match the working asset.

   **This is the important entry.** It is invisible to every verification this project's tooling can
   perform: the samples read back perfectly, the skeletons match, the AnimBP compiles `BS_UP_TO_DATE`,
   and the blendspace still does nothing. Any future agent creating a BlendSpace from Python must pass
   `notify_mode=ALWAYS` or the asset is cosmetically perfect and functionally dead.

**The lesson, which is the same one AGENT_STATE keeps recording:** I verified the first pass by
reading the samples back and declared it correct. Reading back proves the array was written; it says
nothing about whether the hull covers the input space. **The only thing that found this was Michael
looking at it** - the second time in one session.

Both blendspaces were rebuilt **in place** rather than deleted and recreated, so the blendspace-player
references inside the two AnimBPs survived; both recompile `BS_UP_TO_DATE` with X/Y/Pose connected.

**Changed in response to my own review:**

- The whole import approach was abandoned after Michael's report and replaced with the retarget, once
  measurement showed the pack was 0.6 scale. The `_MixamoStaging/_StrafeCandidates` staging folder and
  the five `Anims/_StrafeTest` probe assets were deleted; `git status` shows only intended files.
- `SK_Human_Skeleton`'s translation retargeting was **considered and deliberately not changed**, even
  though I proved it would be a no-op for every existing clip (limb translation spread is exactly
  0.0000 across walk, run, attack and idle - the limbs are rigid, only Hips translates). Michael chose
  the retarget route, and it turned out to need no skeleton edit at all.
- The BT nodes keep their old turn code behind the switch instead of being deleted, reversing session
  1's "remove the `SetActorRotation` calls" - see Generate 4.

**Deliberately left undone:**

- The head Look At, and turn-in-place (session 1 open question 2). Both are refinements on top of a
  change that has not been watched once.
  - **Before building the Look At, read "Prior art: how Elder Scrolls does this" above** (added
    2026-08-12). The short version: Skyrim keeps head tracking as a separate always-on system with a
    target PRIORITY LIST and INTEREST DECAY, not a weld to the current enemy. A permanent lock onto
    `TargetActor` is cheaper to write and reads as robotic targeting - a different wrong answer, not a
    lesser one. Now that the body squares up via `TickFacing`, a head that also never looks away would
    double the effect.
- The player's rotation mode (open question 1) - a feel decision, still out of scope.
- `AttackRange`, the token budget and every #132 spacing dial: untouched, so if the crowd changes
  today it is because of facing, not because two tickets moved at once.

> 2026-08-12T02:06Z Built clean 02:16. Never watched.
