---
id: 398
title: Fix headless GC_ regeneration crash (IsFullyLoaded)
agent: claude-fracture-fix
status: review
claimed: 2026-09-01T04:03Z
build: success
waiting_on:
evaluated: 2026-09-01T04:08:53Z
observed:
scenario:
files: 
  - Source/GoblinSiegeEditor/GSFractureToolsLibrary.cpp
  - Source/GoblinSiegeEditor/GSFractureToolsLibrary.h
---

## Goal

Fix headless GC_ regeneration crash (IsFullyLoaded)

## Generate

Edited `Source/GoblinSiegeEditor/GSFractureToolsLibrary.cpp`, `GenerateFractureAsset` only (`.h`
untouched - no signature change). Root cause per prior research: `CreatePackage(*PackageName)` on a
name that already has a `.uasset` on disk hands back (or constructs) a `UPackage` whose
`bHasBeenFullyLoaded` flag was never set by a completed `LoadPackage`/`LoadObject` call -
`UPackage::IsFullyLoaded()` then falls back to "does a file with this name exist on disk", finds one,
and `SavePackage`'s pre-save validation fatally errors with "cannot be saved as it has only been
partially loaded" (`SavePackage2.cpp`). Pre-deleting the asset in Python before calling the tool does
not fix this - it just changes which residual/racing `UPackage` gets found, and reproduced the
identical crash on the identical asset in two of the three prior attempts.

Fix: when `bOverwriteExisting=true`, `LoadObject<UGeometryCollection>` the existing asset FIRST
(a real completed load - one of the two call sites that actually flips
`MarkAsFullyLoaded`/`bHasBeenFullyLoaded`). If that succeeds, reuse that object and its
already-fully-loaded outer `UPackage` in place - `SetGeometryCollection` a fresh
`FGeometryCollection` onto it and fall through into the existing AppendStaticMesh / UniformFracture /
AutoCluster / CreateNonOverlappingConvexHullData / SavePackage pipeline unchanged. Only when
`LoadObject` finds nothing (genuinely new asset, or `bOverwriteExisting=false` already returned
above) does the code fall back to the original `CreatePackage` + `NewObject` path - so the working
42-new-assets flow from #393 is untouched byte-for-byte in its code path, and `bOverwriteExisting=false`
behavior (return existing unchanged) is untouched (still the first early-return, unmodified).

No delete, no `collect_garbage()`, no source-control-routed file removal, no GC-timing dependency -
the whole problem was going through `CreatePackage` on an already-on-disk name at all, not a race
around deleting it first. Added an inline comment at the change site (matching the file's existing
explain-the-why-and-cite-the-failure comment style) documenting the `IsFullyLoaded()` mechanism and
citing the 2026-09 headless regeneration crash.

Diff shape (see file for full context, `GSFractureToolsLibrary.cpp` lines ~49-105):
```cpp
const FString ObjectPath = PackageName + TEXT(".") + AssetName;

if (!bOverwriteExisting)
{
    if (UGeometryCollection* Existing = LoadObject<UGeometryCollection>(nullptr, *ObjectPath))
    {
        return Existing;
    }
}

// [comment explaining IsFullyLoaded/CreatePackage trap - see file]

UPackage* Package = nullptr;
UGeometryCollection* NewAsset = nullptr;
if (bOverwriteExisting)
{
    NewAsset = LoadObject<UGeometryCollection>(nullptr, *ObjectPath);
}

if (NewAsset)
{
    Package = NewAsset->GetOutermost();
}
else
{
    Package = CreatePackage(*PackageName);
    NewAsset = NewObject<UGeometryCollection>(Package, FName(*AssetName), RF_Public | RF_Standalone);
}
NewAsset->SetGeometryCollection(MakeShared<FGeometryCollection, ESPMode::ThreadSafe>());
// ... rest of function (AppendStaticMesh onward) unchanged
```

Confirmed `UnrealEditor.exe` was NOT running before editing/building (`tasklist //FI "IMAGENAME eq
UnrealEditor.exe"` -> "No tasks are running which match the specified criteria."). Built
`MyProjectEditor` (Development, Win64) via the documented `Build.bat` invocation - **Result:
Succeeded**, only `GSFractureToolsLibrary.cpp` recompiled + `GoblinSiegeEditor` relinked, 227.95s
total.

## Evaluate

**Verified: compiles clean.** `[1/4] Compile GSFractureToolsLibrary.cpp` -> link -> `Result:
Succeeded`, no warnings reported in the build output. This is a real signal (the code is
syntactically/type valid, `GetOutermost()` and `LoadObject<UGeometryCollection>` are both legitimate
calls) but it is NOT proof the fix works - it is proof the fix compiles. Per this project's own
"verification means runtime, not read-back" lesson, that distinction matters here specifically
because this whole bug was invisible to every static check the first three attempts already passed
(their code also compiled, and attempt 2 even logged a false "REGENERATED: 84" success).

**NOT verified - and this is the important gap:** I have not actually run this against the real 84
existing `GC_` assets in a headless `-ExecutePythonScript` pass. I have no evidence yet that:
1. `LoadObject<UGeometryCollection>(nullptr, *ObjectPath)` genuinely returns non-null and
   fully-loaded for these specific on-disk assets in the headless environment (vs. e.g. returning
   null because of some headless-only load-order issue, which would silently fall through to the
   CreatePackage path and reproduce the original crash).
2. `SavePackage` on the reused-in-place `Package`/`NewAsset` actually succeeds this time and doesn't
   hit some OTHER guard I haven't hit yet (e.g. a check on the object's own flags/GC state after
   `SetGeometryCollection` clears its collection but the object itself retains stale
   collection-derived caches elsewhere - `RebuildRenderData`/`InitializeMaterials` are called
   afterward same as before, but I did not trace whether resetting `GeometryCollection` on an
   already-loaded object leaves any other stale per-instance state that a freshly-`NewObject`'d one
   never had).
3. `FAssetRegistryModule::AssetCreated(NewAsset)` (unchanged, still called unconditionally) is
   correct/harmless to call on an asset that already existed in the registry, not just on a genuinely
   new one - I did not verify this either way, only that it was already being called unconditionally
   before my change so this is not a regression I introduced, but it's untested for the reuse path
   specifically.
4. The resulting on-disk `.uasset` after a headless overwrite run actually has the new
   `NumVoronoiCells=3` piece count (not just "didn't crash") - i.e. the exact false-positive shape of
   attempt 2 (clean exit, unchanged files) needs to be explicitly ruled out again for this fix, not
   assumed absent just because the mechanism is different this time.

I touched nothing outside `GenerateFractureAsset` - `BulkGenerateMissingBuildingFractures` (task item
2's "if relevant") was read and left alone: it only ever calls `GenerateFractureAsset` with
`bOverwriteExisting=false` and only after confirming via `FPackageName::DoesPackageExist` that no
asset exists yet, so it never enters the changed code path and needs no change.

DECISION for AGENT_STATE.md: implemented and compiles; runtime behavior against the real 84-asset
headless regeneration is UNVERIFIED and is exactly what the next verification step must run before
this ticket goes to `done`.

## Refine

Nothing changed after self-evaluation - the gaps above are runtime-verification gaps, not defects I
can find by re-reading the code, and the task instructions explicitly reserve runtime verification
for a later step and told me to leave this ticket in `review`, not `done`. What I'd flag for that
step specifically, beyond the general "run it and check": watch for `LoadObject` returning null on a
subset of the 84 (would silently and safely fall back to the CreatePackage path and might reproduce
the original crash on exactly those) rather than assuming an all-or-nothing result: log/print
`ObjectPath` alongside a non-null-check before/after `LoadObject` in the Python driver for this run so
a partial failure is diagnosable asset-by-asset instead of stopping the whole 84-asset batch cold
again.
