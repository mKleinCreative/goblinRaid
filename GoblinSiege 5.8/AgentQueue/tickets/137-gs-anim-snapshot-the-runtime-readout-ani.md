---
id: 137
title: GS.Anim.Snapshot: the runtime readout animation never had
agent: claude-animsnap
status: done
claimed: 2026-08-12T03:48Z
build: required
waiting_on:
evaluated: 2026-08-12T04:55:22Z
observed: 2026-08-12T04:54:46Z | GS.Anim.Snapshot printed 18 pawns with 0 in reference pose on the working locomotion, then 9 of 15 flagged *YES* when the goblin AnimBP was pointed at the dead 2D blendspace - exactly the 9 pawns on ThirdPerson_AnimBP_Gob_C, with humans on ABP_Human_C correctly reading no
scenario: PIE on L_CombatArena, 8 horn-summoned goblins plus a spawned militia patrol and the player, run twice: once on the known-good 1D blendspace and once on the known-dead 2D one
files: 
  - GoblinSiege 5.8/Source/GoblinSiege/Combat/GSAnimDebugCommands.cpp
---

## Goal

GS.Anim.Snapshot: the runtime readout animation never had

The companion to #136. #136 made the queue demand an observation; this builds the thing that makes
an observation possible for the one system that had no way to produce one.

## Generate

### The gap, measured

An inventory of `Source/GoblinSiege/` found **18 console variables and 26 console commands, none of
which report anything about animation**. Stronger than "no debug tooling": there is no
`UAnimInstance` subclass in the module, `find -iname "*anim*"` returns zero files, and no line of
code in it has ever printed a speed. Combat has `LogAI`, `LogDamage`, `CrowdWatch`, `CrowdStats`;
animation had nothing, so during #133 every defect had to be found by Michael looking at the screen.
He found all four.

The `GS.*` namespace even has the hole visible in it: there is no `GS.Anim.*`, no `GS.Pawn.*`, no
`GS.Move.*`.

### New file `Combat/GSAnimDebugCommands.cpp`

`GS.Anim.Snapshot [radius=5000]`, one row per live pawn:

`pawn | controller | speed | direction | rotation mode | REFPOSE | playing | blackboard target`

Three design choices, each aimed at a specific failure from #133:

**A SNAPSHOT, NOT A LOG.** `GS.Combat.LogAI` prints when a decision *changes*, which is the right
shape for decisions and the wrong one here: the failure being hunted is a pawn standing still in the
wrong pose, and a frozen goblin generates no events. An event stream is silent exactly when it is
needed.

**IT PRINTS EVERY PAWN, NOT ONE.** The most expensive mistake in #133 was a test path that could not
reach the change - the player pins `Direction` near zero and exercised one of five blendspace
columns. A per-pawn query would allow that again; a table containing player, defender and horde
makes coverage structural instead of remembered.

**THE `REFPOSE` COLUMN IS THE POINT.** Every visible defect in #133 - T-pose, then no locomotion at
all, then A-pose - was one underlying condition: the evaluated pose was the mesh's reference pose.
Nothing in the project could state that, so it could only be seen. `GSIsInReferencePose` compares
live bone-space rotations against `FReferenceSkeleton::GetRefBonePose`, whole chain, skipping bone 0
(many clips legitimately leave the root at identity) and ignoring translation (the human clips
animate translation on the Hips alone, so translation matches the reference pose on every other bone
even when everything is working). Sampling the whole chain rather than a chosen bone is deliberate:
a hand-picked bone is how #092 shipped a detector that could only ever answer yes.

`Direction` is computed with `UKismetAnimationLibrary::CalculateDirection` - the same call the anim
blueprints use - so the column is the number the graph actually sees rather than one invented here.
The rotation-mode column sits beside it because it is the *explanation*: `OrientToMove` pins
Direction, `CtrlYaw` does not.

Copied verbatim from the house idioms: headerless registration file, `FAutoConsoleCommandWithWorldAndArgs`,
`DEFINE_LOG_CATEGORY_STATIC`, on-screen message alongside the log, and `GSAnimGameWorld` re-resolving
the world through `GEngine->GetWorldContexts()` - the editor console is not the PIE world, and four
attempts at `GS.Raid.GotoActor` printed nothing before that was understood (#027).

## Evaluate

**BUILD SUCCEEDED in 00:52** (fourth attempt - three real compile failures first, below). No new
warnings.

**The claimed file list grew by one:** `GoblinSiege.Build.cs`, for the `AnimGraphRuntime` private
dependency. Claimed retroactively; the queue held no other agent.

### It was validated against a KNOWN-BAD case, not a known-good one

This is the part that matters, and it is the thing #092 failed to do when it shipped a headshot
detector that could only ever answer yes. Same command, same scenario, two states:

| goblin locomotion source | pawns in reference pose |
|---|---|
| the working 1D `ThirdPerson_IdleRun_2D_Gob` | **0 of 18** |
| the known-dead `BS_GS_Locomotion_Gob` | **9 of 15**, flagged `*YES*` |

Those 9 are exactly the pawns running `ThirdPerson_AnimBP_Gob_C` (8 horn-summoned goblins + the
player); every human on `ABP_Human_C` correctly read `no` in the same table. The dead blendspace was
then unwired again and the goblin restored to the 1D asset, verified by reading the graph
connections back.

**A second finding falls straight out of that run, and it is not a small one: the 2D blendspace is
now MEASURED dead rather than theorised dead.** Across four passes in #133 I could only infer that,
and inferred it wrongly three times on the way. The instrument settled it in one command.

### What else the first run proved, for free

- **#135's tick fix works, observed rather than argued.** Every `BP_HordeGoblin_C_*` reported
  rotation mode `DesiredRot` and a live blackboard target. `bUseControllerDesiredRotation` is set
  only by `AGSAIControllerBase::TickFacing`, which cannot run unless the controller ticks - so that
  row is proof the #135 boolean took effect on a horn-summoned goblin.
- **RC3, the "one column out of five" claim, is now data.** In one table: the player reads
  `Direction 0.0 / OrientToMove`, while the eight horde goblins read -138.1, -106.8, -97.8, -76.7,
  -59.5, -46.2, 107.8 and 121.6. The player structurally cannot exercise what they exercise. That
  argument was previously reasoning; it is now a printout.

### Three compile failures worth recording

1. `Kismet/KismetAnimationLibrary.h` - **wrong path.** Despite the class prefix, and despite every
   sibling Kismet library living under `Kismet/`, this header sits directly in
   `AnimGraphRuntime/Public/`. Found by locating the file rather than guessing a second time.
2. The same error again after adding the module - because the module was never the problem.
3. `GetBoneSpaceTransforms()` is **non-const and returns by value**, so the helper takes a non-const
   component and copies the pose.

### What is NOT verified

- **`GSRefPoseToleranceRad` (0.02 rad) has only been tested against a total collapse.** A pose that
  is subtly wrong - one limb stuck, a blend at 5% weight - is not what this was proven on, and could
  read `no` while looking wrong. It answers "is this pawn animating at all", not "is this pawn
  animating correctly", and should not be quoted as the latter.
- **Cost is unmeasured.** It walks every bone of every pawn in range. Fine for a command typed by
  hand; it is not a per-frame tool and there is no measurement saying it could be one.
- **Only exercised on `L_CombatArena`**, with the two rigs this project ships.

**Owed to `AGENT_STATE.md`:** a DECISION line - *`GS.Anim.Snapshot` is the animation instrument; its
`REFPOSE` column detects a pose that evaluated to nothing, and it prints every pawn so a
player-only test cannot hide an AI-only failure* - plus the `KismetAnimationLibrary.h` path gotcha.

## Refine

**Deliberately left undone:**

- **Not added to the `GSDebugToggles[]` table** in `GSDebugCommands.cpp`. That table silences
  *continuous* overlays and logs so `GS.PlayerView 1` gives a clean screen; this command prints once
  when typed and then stops, so there is nothing for `GS.PlayerView` to quieten. If it ever grows a
  per-frame mode, it belongs in the table that day - two cvars were previously missed and kept
  talking.
- **No cvar-gated continuous variant.** Tempting, and premature: nobody has yet used the one-shot
  form in anger, and a per-frame readout across every pawn is real cost on the class that the horde
  design exists to keep cheap.
- **The blackboard target column reads `TargetActor` by literal name**, matching every other consumer
  in the project. A pawn whose tree uses a different key will show `-`, which is honest but could
  read as "no target".
