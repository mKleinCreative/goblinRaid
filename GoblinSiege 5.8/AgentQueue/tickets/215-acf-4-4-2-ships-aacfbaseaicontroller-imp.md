---
id: 215
title: ACF 4.4.2 ships AACFBaseAIController implementing only 2 of IACFEntityInterface's 4 methods - supply the other two or nothing deriving from it can link
agent: claude-acfentity
status: done
claimed: 2026-08-20T21:31Z
build: required
waiting_on:
evaluated: 2026-08-20T21:32:34Z
observed: UNOBSERVED 2026-08-20T21:32:34Z - Written, never compiled - this ticket own claim shuts the build gate. The DEFECT is evidenced from ACF source (two of four interface methods defined, interface cpp empty) and from the verbatim LNK2001 output; the FIX has not been near a linker.
scenario: none - never run
files: 
  - Source/GoblinSiege/AI/GSAIControllerBase.h
  - Source/GoblinSiege/AI/GSAIControllerBase.cpp
---

## Goal

ACF 4.4.2 ships AACFBaseAIController implementing only 2 of IACFEntityInterface's 4 methods - supply the other two or nothing deriving from it can link

## Generate

Two overrides on `AGSAIControllerBase`, supplying interface methods **ACF 4.4.2 does not ship**:

```cpp
virtual bool  IsEntityAlive_Implementation() const override;   // -> the possessed pawn's IsAlive()
virtual float GetEntityExtentRadius_Implementation() const override;  // -> scaled capsule radius
```

### The defect, established by reading ACF rather than guessing

`AACFBaseAIController` declares `public IACFEntityInterface` and defines **two of its four** methods:

| Method | In ACF |
|---|---|
| `GetEntityCombatTeam_Implementation` | defined (`ACFBaseAIController.cpp:59`) |
| `AssignTeamToEntity_Implementation` | defined (`ACFBaseAIController.cpp:91`) |
| `IsEntityAlive_Implementation` | **never overridden, never defined** |
| `GetEntityExtentRadius_Implementation` | **never overridden, never defined** |

And `AscentCoreInterfaces/Private/Interfaces/ACFEntityInterface.cpp` is **empty** apart from the
comment *"Add default functionality here for any IACFEntityInterface functions that are not pure
virtual."* So no default bodies exist either. ACF's own module links because nothing in it forces
those thunks into a linked translation unit; the moment we derive from the class, UHT emits **our**
interface thunks into **our** module and the linker has nothing to bind:

```
Module.GoblinSiege.1.cpp.obj : error LNK2001: unresolved external symbol
  "public: virtual bool __cdecl IACFEntityInterface::IsEntityAlive_Implementation(void)const "
```

**Implemented properly, not stubbed.** ACF's targeting and motion warp both read the extent radius,
and aliveness gates whether an entity is a legal target — returning `true`/`0.f` to silence a linker
would have been a lie that only surfaces as bad targeting later.

**The radius comes from the CAPSULE, deliberately.** #094 measured the goblin meshes at a fraction of
their capsules — the player goblin's head sits +40 in a 240 capsule — so mesh bounds would hand ACF a
number unrelated to what actually blocks, traces and collides. Everything else in this project already
reasons about the capsule.

## Evaluate

**A wrong fix was nearly applied and checking stopped it.** The obvious reading was #166's recorded
lesson — *"AscentCombatFramework must be named explicitly to LINK, not just include"* — which would
have meant adding `AscentCoreInterfaces` to `Build.cs`. Reading the module's `.cpp` first showed it is
empty, so that fix would have changed nothing and produced a second confusing build. **A remembered
lesson that fits the shape of a problem is not the same as a diagnosis of it.**

**NOT COMPILED, NOT RUN.** Written only; this ticket's own claim shuts the build gate.

**What this does not resolve:** whether Phase 1 is otherwise sound. The previous build got as far as
linking, so the reparent itself compiled — no member collisions, no `DoNotCreateDefaultSubobject`
failure, no missing include. Those risks are retired. Everything behavioural is still unproven, and
ACF swapping in `UCrowdFollowingComponent` remains the change most likely to be visible.

**Worth knowing for Phase 2:** `AACFCharacter` overrides both of these methods itself
(`ACFCharacter.h:348,360`), so pawns will not hit this. This is a controller-side defect only.

## Refine

Chose the capsule over `GetActorBounds` after considering both — bounds would include the mesh, and
on this project the mesh is exactly the thing that does not match the collision volume.

Wrote the ACF defect into the header rather than only this ticket, because the next person to derive
anything from an ACF base class will meet it again and the ticket will not be read.
