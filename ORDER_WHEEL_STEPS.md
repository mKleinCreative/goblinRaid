# Order wheel — the editor work, step by step

> ### ⚠️ Read this first — the ground moved under this document
>
> When it was written, `BT_HordeGoblin` was NOT using `BB_HordeGoblin` at all. It was running on
> `ACFAIBB` (marketplace content), which has none of our keys. Adding keys to `BB_HordeGoblin` would
> have done **nothing**, and that is why you could not find anything this document described.
>
> **Already fixed for you (2026-08-12):**
> - `BB_HordeGoblin` now has `ACFAIBB` as its **Parent**, so it inherits all twelve ACF keys.
>   Its three duplicate definitions (`SelfActor`, `TargetActor`, `TargetLocation`) were removed.
> - `BT_HordeGoblin` now points at `BB_HordeGoblin`. **16 keys are visible to the tree.**
> - Two selectors that had silently rebound to `SelfActor` were repointed:
>   "Follow the summoner" `MoveTo` → `FollowTarget` (it was moving the goblin to its own position),
>   and the "Target Is Winding Up" decorator → `TargetIsAttacking` (it was always true).
>
> **This changes how the horde behaves**, independently of the order wheel. Follow and Block have
> never actually run. Watch for that in the next PIE pass rather than assuming it is an improvement.
>
> Everything below is now correct as written.

Both assets are already open. Do them in this order: the tree cannot reference keys that do not exist yet.

Nothing here blocks **Attack** or **Follow** — those two already work with no blackboard or tree change.
This unblocks **Hold**, **Loot** and **Smash**.

---

## STEP 1 — `BB_HordeGoblin`: add five keys

Leave the existing seven alone. Add these:

| # | Key name | Type | Extra setting |
|---|---|---|---|
| 1 | `OrderVerb` | **Enum** | Enum Type = **`EGSHordeOrder`** |
| 2 | `OrderSubject` | **Object** | Base Class = `Actor` |
| 3 | `OrderLocation` | **Vector** | — |
| 4 | `DeliveryLocation` | **Vector** | — |
| 5 | `CargoActor` | **Object** | Base Class = `Actor` |

⚠️ **Names must match exactly.** They are the `FName` defaults on `AGSHordeAIController`. A typo is a
silent no-write — nothing errors, the key just never gets filled.

⚠️ On `OrderVerb`, make sure you pick **Enum**, not *Enum Class* / *Name*, and that the Enum Type
dropdown lands on `EGSHordeOrder` (not `EGSHordeState`, which is the existing `HordeState` key).

**Save the blackboard before opening the tree.**

---

## STEP 2 — `BT_HordeGoblin`: root Selector goes from 5 children to 8

The existing five stay exactly as they are, in the same relative order. You are **inserting three new
branches between the melee branch and the chase branch**.

Final left-to-right order under the root Selector:

```
1. Block                    (existing)
2. MeleeAttack              (existing)
3. Smash the ordered prop   ← NEW
4. Courier the cargo        ← NEW
5. Hold the ground          ← NEW
6. MoveTo TargetLocation    (existing — was 3rd)
7. MoveTo FollowTarget      (existing — was 4th)
8. Wait                     (existing — was 5th)
```

Root keeps its `BTService_AcquireTarget` with `bSelectTarget = false`. Do not touch it.

### Branch 3 — Sequence, "Smash the ordered prop"

**Two decorators**, both on the Sequence:

- `Blackboard` → `OrderVerb` **Is Equal To** `Attack`, Observer aborts = **Both**
- `Is BBEntry Of Class` → key `OrderSubject`, Class `GSCharacterBase`, **Inverse Condition = TRUE**

> The second decorator is load-bearing, not a nicety. Without it an Attack order on a *living guard*
> also enters this branch, and its `MoveTo` walks the goblin in on the raw actor location — bypassing
> the ring-slot standoff that #107/#108 exist to maintain. Inverted, the branch only ever runs for
> non-character subjects, which is exactly what "smash" means.

Children:

1. `MoveTo` — Blackboard Key **`OrderSubject`**, Acceptable Radius **140**
2. `Smash Ordered Target (Break)` — set its `TargetKey` to **`OrderSubject`**

### Branch 4 — Sequence, "Courier the cargo"

Decorator: `Blackboard` → `OrderVerb` **Is Equal To** `Loot`, Observer aborts = **Both**

One child, a **Selector**, with two Sequences under it:

**"Fetch"** — decorator `Blackboard` → `CargoActor` **Is Not Set**
1. `MoveTo` — Blackboard Key **`OrderSubject`**, Acceptable Radius **120**
2. `Pick Up Cargo (StartCarry)` — `CargoSourceKey` = `OrderSubject`, `CargoKey` = `CargoActor`

**"Haul home"** — decorator `Blackboard` → `CargoActor` **Is Set**
1. `MoveTo` — Blackboard Key **`DeliveryLocation`**, Acceptable Radius **200**
2. `Deliver Cargo (NotifyCourierDelivered)` — `CargoKey` = `CargoActor`,
   `DeliveryLocationKey` = `DeliveryLocation`

### Branch 5 — Sequence, "Hold the ground"

Decorator: `Blackboard` → `OrderVerb` **Is Equal To** `Hold`, Observer aborts = **Both**

1. `MoveTo` — Blackboard Key **`OrderLocation`**, Acceptable Radius **150**
2. `Wait` — Wait Time **1.0**, Random Deviation **0.3**

> The Wait is **not** padding. Without it `MoveTo` returns Succeeded instantly once the goblin is
> already standing there, the Selector immediately re-runs it, and you get a fresh path request every
> single tick against a point it is already on.

### Why branches 3–5 sit above the chase branch

Branch 6 (`MoveTo TargetLocation`) now sits **below** Hold and Loot. That ordering is the whole design
of Hold: a held goblin still fights back where it stands, but never abandons its post to chase.

The three new task nodes are compiled and in the node picker as:
**"Smash Ordered Target (Break)"**, **"Pick Up Cargo (StartCarry)"**, **"Deliver Cargo (NotifyCourierDelivered)"**.

---

## STEP 3 — `BP_LootSack` (only needed to test Loot)

Actor Blueprint in `Content/GoblinSiege/Test/`:

- a StaticMesh component (any prop mesh)
- a **`GSInteractable`** component with **`bIsCarryable` ticked** and `VerbTag = Interact.Carry`

No C++ needed — that component is already `BlueprintSpawnableComponent` and already carries the flag.
Place two or three in `L_CombatArena`.

## STEP 4 — a breakable prop (only needed to test Smash)

Any placed prop in `L_CombatArena` run through `UGSRaidLibrary::MakeActorBreakable` — say the word and
I'll do this one from Python, it's already `BlueprintCallable`.

## STEP 5 — lay out the wheel labels (cosmetic, any time)

`WBP_HordeOrderWheel` exists with all five `TextBlock`s and their text set, but they are stacked at the
origin — I could not set canvas slots from Python. Drag them to:

```
            ATTACK  (up)
   LOOT                    HOLD
  (left)   <subject>      (right)
            FOLLOW  (down)
```

---

## Then tell me, and I will:

1. Close nothing — **you close the editor**, I run the build (~9 min; the four C++ fixes are written
   but uncompiled, so until then `R` still reproduces the bug you saw).
2. You reopen, and we verify in `L_CombatArena` — `GS.Horde.SpawnTest 3`, then **`GS.Horde.Order Attack`
   before touching `R` at all**, so "the goblins ignore me" can only have one cause.
