# HANDOFF — ranged combat, torch, and the outstanding code-review findings

*Written 2026-08-06 by `claude-ranged` for the single agent taking over everything.
Read `AGENT_STATE.md` first for project-wide memory; this file is the detail that would
otherwise only exist in closed tickets, which nothing loads at run start.*

---

## PART 1 — CODE-REVIEW FINDINGS — ✅ ALL NINE FIXED (closed 2026-08-07)

> **DO NOT WORK THIS LIST.** Every finding below is resolved. It is kept as the record of what was
> found and why, not as a queue.
>
> - **#036** closed four: finding 2 (`set -Status done` bypass), finding 3 (stale-Evaluate gate),
>   finding 9 (`evaluated` stamp truncating seconds), and reported finding 1 as **already gone** —
>   `git log -S PIECE_KEYS` shows #033's "a building is a merged house actor" rewrite deleted the
>   crashing block as a side effect, after the review was taken. #036 correctly declined to invent
>   a fix for a bug that no longer existed.
> - **#037** closed the five C++ ones: the adopt-radius diagonal, the silent completion freeze, the
>   lying BeginPlay diagnostic, `BuildingStatus` re-deriving the piece set, and "nearest" with no pawn.
>
> Verified independently on 2026-08-07 (#073) before this banner was written — not taken on trust
> from the tickets. Two of the fixes were confirmed by hitting them: `set -Id 070 -Status done` was
> refused with the message citing ticket 028, and `evaluated:` now stamps seconds.
>
> **This heading previously read "nobody has fixed these", and `AGENT_STATE.md` carried a banner at
> the top of the file — the first thing every agent reads at run start — repeating it. Both were
> wrong for a full day after the work landed.** Neither #036 nor #037 updated the two documents that
> advertise their own work as outstanding. If you close findings from a list, close the list.

From a `/code-review` over `origin/interact-framework...HEAD` plus the working tree, 2026-08-06.
Twelve findings. Three were `claude-ranged`'s own and were fixed in #030; the nine below were not,
and are now closed as above. Ordered by severity.

### HIGH — `tools/hamlet/gs_buildings.py:110` — the script crashes on any level

The guard `if m and any(...)` became `if not any(k in m for k in PIECE_KEYS)`. `mesh_of()` still
returns `None` for any actor with no `StaticMeshComponent` or no assigned mesh, and `k in None`
raises `TypeError: argument of type 'NoneType' is not iterable`.

The first PlayerStart / light / landscape / volume returned by `get_all_level_actors()` kills the
run before it clusters anything. Restore the `m and` half of the guard.

> Note: ticket #031 (`claude-raid`, open at time of writing) claims this same file. Coordinate
> rather than both editing it.

### MEDIUM — `gsqueue.ps1:564` — `set -Status done` bypasses every close check

`Invoke-Set` writes any `-Status` straight to the ticket, `done` included, so
`set -Id N -Status done` skips all of `Invoke-Done`: the G/E/R placeholder check, the new
`evaluated` check, and the late-file check.

**This is not hypothetical.** `AgentQueue/tickets/028-teleport-reported-success-while-teleport.md`
is on disk right now as `status: done` with an empty `evaluated:` and `<!-- REPLACE` still sitting
in its Generate section. It closed without review.

Fix: route `$Status -eq 'done'` through `Invoke-Done`, or refuse it in `Invoke-Set`.

### MEDIUM — `gsqueue.ps1:212` — the #024 stale-Evaluate gate is a no-op

`Join-Path $RepoRoot ($rel -replace '/','\')` with `$RepoRoot = D:\goblinRaid`, but tickets record
**project**-relative paths (`Source/GoblinSiege/...`, `Content/Maps/...`) whose files actually live
under `D:\goblinRaid\GoblinSiege 5.8\`. `Test-Path` fails, the loop `continue`s, the file is never
checked.

So the "did work continue after Evaluate?" guard silently passes for the majority of claimed files
— it fails **open**, which is the same shape as the bug #024 was written to stop.

### MEDIUM — `GSBuildingObjective.cpp:126` — adopt radius is counted twice

`EdgeDistance = Dist(PieceOrigin, Origin) - PieceExtent.Size()` subtracts the box **diagonal**
(√(x²+y²+z²)), overestimating the piece's reach by up to √3×. And `gs_buildings.py` already folds
that same diagonal into `adopt_radius = span*1.1`.

A roof piece with R≈700 is therefore adopted from ~700uu beyond the intended footprint — i.e. out
of the neighbouring house. The class header names over-large adopt radius as a known route to an
**unwinnable objective**, and it also inflates the completion denominator.

Related: because this proximity test now runs *before* the mesh-name filter, any actor whose bounds
envelope the building (landscape, `AInstancedFoliageActor`, blockout volumes) passes the gate
unconditionally. Adoption now rests entirely on `PieceNameFilters`.

### MEDIUM — `GSRaidDebugCommands.cpp:533` — `BuildingStatus` re-derives pieces, wrongly

It rebuilds the piece set as "any actor with a flammable within 2500uu of the building's
`GetActorLocation()`" instead of reading `Nearest`'s own adopted `Pieces`. That is measured
**pivot-to-pivot** — precisely the bug commit `270d107` on this branch fixed in `AdoptPieces` — and
2500 is unrelated to the building's real `AdoptRadius`.

On a tavern spanning >2500uu it under-counts; between two adjacent houses it counts the neighbour's
pieces. A diagnostic whose whole job is "prove char is being applied" reports numbers that do not
describe the building it names.

### MEDIUM — `GSBuildingObjective.cpp:438` — a building can freeze its completion silently

`if (Shell > 0) SetCompletion01(...)` means a building whose adopted pieces are **all**
interior-named never calls `SetCompletion01` at all — completion is frozen at its last value
forever, with no log line.

The pre-existing `InitialPieceCount == 0` path logs an `Error` precisely because this class of
silent dead objective has burned the project before. The `Shell == 0` case has no equivalent, and
it is reachable today: `InteriorNameFilters` contains `"Beam"`, so a detached roof-beam cluster
adopts pieces, counts >0, and scores nothing.

### LOW — `GSBuildingObjective.cpp:88` — the BeginPlay diagnostic lies

It still prints `needs CeilToInt(InitialPieceCount * CompletionThreshold01) burnt`, but
`RecomputeCompletion` now divides by `Shell`. On a kit that is ~28% interior this over-reports the
requirement by ~40% on every building — the one line a designer uses to sanity-check a house now
describes a denominator nothing uses.

### LOW — `GSRaidDebugCommands.cpp:513` — "nearest" building with no pawn

When `Pawn` is null, `D` is `0.f` for every building, so `D < BestSq` is true only on the first
iteration: the command reports on an arbitrary (iteration-order) building while the help text says
"nearest", with no "no player pawn" line. `BurnHere` directly above it bails out loudly in the same
situation; this should too.

### LOW — `gsqueue.ps1:568` — the `evaluated` stamp truncates seconds

Stamped `yyyy-MM-ddTHH:mmZ` but compared against `LastWriteTimeUtc`. Any file saved earlier in the
same minute as `set -Status review` — the normal case, since you save then stamp — has an mtime up
to 59s past the stamp and is flagged as "kept working after Evaluate", forcing `-Reaffirm` on a
clean close. Store full precision, or compare with a tolerance.

---

## PART 2 — STATE OF THE RANGED / TORCH WORK

### Built and compiled (in the DLL as of the 2026-08-05 13:25 build)

- **`UGSAimComponent`** (`Combat/`) — the one definition of where a ranged verb comes from and
  goes. `GetMuzzleTransform()` is shared by the arc preview and every spawn, so they cannot drift.
  Owns aim state (`EGSAimMode` None/Torch/Bow), the `PredictProjectilePath` prediction, the spline-
  mesh ribbon + landing decal, and `Server_SetAimRotation`.
- **`AGSArrowProjectile`**, **`UGSGA_BowShot`** — the bow half of sword⇄bow.
- Left-shoulder aim camera blend on `AGSPlayerCharacter` (arm 450→250, offset +55→−55, FOV 90→70,
  0.2s eased both ways).
- Torch/arrow no longer stick to their thrower; muzzle is yaw-derived so it cannot spawn inside the
  capsule when aiming steeply down.
- Weapon-swap now logs which of its gates refused instead of failing silently.

### Written but NEVER COMPILED (blocked on the build gate — tickets #023, #030)

Everything in #023 and #030. Notably: arc ribbon width now means world units, `MaxSimSeconds` 3→5,
torch speed 1400→2400, a Niagara flame on the in-flight torch, and the derived arc-segment pool.
**Nothing in either ticket has ever run.**

### Editor-side wiring fixed 2026-08-06 (#026, live now, no build needed)

`ArcMaterial` and `LandingDecalMaterial` were assigned to **each other's slots** — a decal-domain
material on the spline meshes and a surface material on the decal, which is why no reticle ever
appeared. Also set `BowShotAbilityClass=GSGA_BowShot`, `ranged_mesh=GS_Bow_Only`,
`quiver_mesh=GS_Quiver` on `DA_Weapon_Scout`.

### KNOWN WRONG, NOT FIXED

- **No torch throw animation, at all.** `AM_GS_ThrowTorch.uasset` is authored and referenced by
  **nothing** in C++. `UGSGA_TorchToss` contains no `PlayMontage` call. The throw is a 0.25s timer
  and a spawn.
- **The held torch is visible for 0.25s.** `TorchWindupSeconds` readies the prop, then `EndAbility`
  un-readies it the instant the projectile spawns. Michael's report "it's never in your hand" is
  this, *not* a missing socket or mesh — both are fine (verified: `GOB_Scout_v2` has `hand_l_torch`
  on `L_Hand`, and `held_torch_mesh=GS_Torch`).
- **`GSWeaponComponent.h` contains a STALE COMMENT** claiming the goblin skeleton has no weapon
  sockets. It has had `hand_l_torch`, `hand_r_weapon`, `hand_l_weapon` and `spine_quiver` for some
  time. That comment misled two of my diagnoses. Delete it.
- `UGSAimComponent` hardcodes `SetForwardAxis(ESplineMeshAxis::X)`. Any segment mesh whose long
  axis is not X renders wrong, with no property to fix it. Currently `1M_Cube` (symmetric, fine).
- ~~Arrows do not respect `RaceTag`~~ — **FALSE, and was false when this was written.** #038
  brought ranged in line with melee: `GSArrowProjectile.cpp` gates damage on
  `ShooterChar->IsHostileTo(OtherActor)`. Re-verified 2026-08-07 (#073) and again in #115.
  What arrows *do* is **STICK** in an allied body, dealing nothing and wasting the shot. That is
  intended: Michael's settled ruling of 2026-08-06, taken with the horde case (firing past your
  own line) explicitly on the table — see `AGENT_STATE.md` DECISIONS, *"do not re-litigate"*.
  An agent finding "every shot is eaten by a friendly" is looking at working behaviour.

---

## PART 3 — THE RADIAL WEAPON WHEEL (specified, not started)

Michael's design, with two decisions already settled — **do not re-litigate these:**

1. **Q opens a wheel; hold, drag a direction, release to commit.** Top = torch, bottom-left =
   sword, bottom-right = bow. Drag back to centre cancels. Uses accumulated mouse *delta*, so it
   does not depend on cursor position.
2. **Torch becomes a real held weapon.** Selecting it puts it in hand and the ATTACK button aims
   and throws it — one verb, "use what you're holding". `IA_ThrowTorch` retires.

**Still open:** who builds the widget. C++ should own the selection maths and expose
`BlueprintReadOnly` state + open/close/changed events; the UMG is unbuilt.

This work subsumes the two "known wrong" torch items above: making the torch a held weapon is what
makes it stay in his hand, and the throw ability wants `AM_GS_ThrowTorch` with the spawn on an
AnimNotify rather than a blind timer.

**Implementation note:** `bRangedMode` is a **bool** on `UGSWeaponComponent`. Three slots is not a
bigger toggle, it is a different type — expect an `EGSWeaponSlot` enum and to touch every
`IsInRangedMode()` caller.

**ACF is not the answer here.** `Plugins/Marketplace/AscentCombatFramework` has an `InventorySystem`
module, but ACF is **not enabled** in the uproject, is 47 modules, sits in a gitignored folder, and
ships its own `ACFAbilitySystemComponent` that would sit alongside this project's GAS layer. Adopting
it is a real architectural decision, not a way to get a 3-slot selector.

---

## PART 4 — GOTCHAS LEARNED THE HARD WAY (2026-08-05/06)

- **A successful tool call is not evidence.** Re-read the value off the asset. This caught the
  swapped materials and a build that reported "up to date" after running zero actions.
- **Compare source mtimes against `Binaries/Win64/UnrealEditor-GoblinSiege.dll` before believing
  anything is live.** A stale DLL and a working one look identical in the editor. Then compare the
  DLL's write time against the editor process `StartTime` — an editor launched before the build is
  running old code.
- **Live Coding fails for the whole module if *any* agent added a new `UCLASS`**, not just you.
  Check `git log` for new classes before assuming Ctrl+Alt+F11 will do.
- **`gsqueue.ps1` must stay pure ASCII.** An em-dash written as UTF-8 in a BOM-less file is read as
  CP1252, whose third byte is a smart quote, which terminates the string literal and takes the whole
  script down — and then *no* agent can claim, close or check the gate. It fails closed for everyone.
- **Stale comments are defects.** Two wrong diagnoses today traced to comments that were true when
  written and are not now.
- `USplineMeshComponent::SetStartScale` is a **multiplier on the mesh cross-section**, not a width.
