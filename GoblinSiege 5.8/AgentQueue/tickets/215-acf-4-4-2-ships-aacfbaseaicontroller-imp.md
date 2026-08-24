---
id: 215
title: ACF 4.4.2 ships AACFBaseAIController implementing only 2 of IACFEntityInterface's 4 methods - supply the other two or nothing deriving from it can link
agent: claude-acfentity
status: done
claimed: 2026-08-20T21:31Z
build: required
waiting_on:
evaluated: 2026-08-24T00:00:52Z
observed: 2026-08-24T00:00:51Z | The two implementations ship and ACF-driven behaviour that would depend on them works: goblins hold formation posts behind the summoner (#263) and resolve attack orders as places, engaging what is nearest (#264), both watched by Michael. IsEntityAlive_Implementation and GetEntityExtentRadius_Implementation are present on AGSAIControllerBase, filling the half of IACFEntityInterface that ACF 4.4.2 declares but never defines. HONEST LIMIT: nothing in our code calls these two methods, so this is evidence that ACF targeting and spacing behave, not a direct read of either return value. If ACF consults them they work; if it never does, this removed a linker hazard and nothing more - and that distinction was not tested.
scenario: Cumulative PIE across 2026-08-21 to 2026-08-23 in L_CombatArena with horn-summoned bands, editor build of 16:59 on 08-23.
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


### Post-build truth, 2026-08-23

**BUILT.** The Evaluate above says "written, never compiled". Stale since 2026-08-21; the two
implementations ship in the DLL of 2026-08-23 16:59.

`IsEntityAlive_Implementation` and `GetEntityExtentRadius_Implementation` are both present on
`AGSAIControllerBase`, re-checked in source, filling the half of `IACFEntityInterface` that ACF 4.4.2
declares but does not define.

**Honest limit on the observation:** nothing calls these two methods from our code, so the evidence
is that ACF-driven targeting and spacing behave correctly - watched repeatedly via #263's formation
posts and #264's attack orders - rather than a direct read of either method's return value. If ACF
consults them at all, they are working; if ACF never consults them, this ticket removed a linker
hazard and nothing more. That distinction was not tested.

> 2026-08-24T00:00Z Evaluate refreshed post-build.
