/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RoadMeshBuild/IRoadMeshBuildHost.h"
#include "Misc/AsyncTaskNotification.h"
#include "MetaRoadBakeHost.generated.h"

class UWorld;
class UMaterialInterface;
class AMetaRoad;

/**
 * UMetaRoadBakeHost
 *
 * IRoadMeshBuildHost used by the Bake action to (re)generate road mesh assets/actors from the actors'
 * per-actor build settings, mirroring the former UTriangulateRoadTool::Shutdown(Accept).
 *
 * ASYNCHRONOUS: BeginBakeAsync() kicks the background mesh build; the owner ticks TickBake() each frame
 * (it harvests the background compute, updates a cook-style FAsyncTaskNotification, and supports Cancel).
 * When the build finishes, asset generation runs synchronously in one game-thread step: replace-on-
 * regenerate cleanup (Phase 1) + create the generated _Gen output actor and GenerateAssets (Phase 2). It
 * always uses each actor's committed (real) settings — not the Preset working copies.
 */
UCLASS(Transient)
class UMetaRoadBakeHost : public UObject, public IRoadMeshBuildHost
{
	GENERATED_BODY()

public:
	/** Kick the asynchronous bake of the given AMetaRoad actors. Drive it with TickBake() until it returns false. */
	void BeginBakeAsync(UWorld* InWorld, const TArray<TWeakObjectPtr<AMetaRoad>>& Actors);

	/** Advance the async bake one frame. Returns true while still running; false when finished or cancelled
	 *  (the owner should then drop this host). */
	bool TickBake(float DeltaTime);

	bool IsBaking() const { return bBaking; }

	/** True once the most recent bake ran to completion (assets generated), as opposed to being cancelled or
	 *  aborted. Read by the owner after TickBake() returns false to decide whether to exit the mode. */
	bool DidComplete() const { return bCompleted; }

	/** Delete the generated _Gen actor(s) and their mesh assets for the given road actors (the cleanup the
	 *  Bake replace-on-regenerate step does), without generating anything. Always clears (ignores the
	 *  bReplaceOnRegenerate flag). Returns the number of actors cleared. */
	static int32 ClearGenerated(const TArray<TWeakObjectPtr<AMetaRoad>>& Actors);

	// IRoadMeshBuildHost
	virtual UWorld* GetTargetWorld() const override { return World.Get(); }
	virtual void NotifyMeshUpdated() override {}
	virtual UMaterialInterface* GetWorkingMaterial() override { return nullptr; }

private:
	// Phase 2: create the generated _Gen output actor(s) + assets. Returns the number of source actors
	// generated; adds any actor whose asset writes failed to OutWriteFailedActors.
	// (Phase 1 replace-on-regenerate cleanup is inlined in the bake step: assets are disposed per CleanStrategy
	// before the undo transaction, then the old _Gen actor is destroyed inside it — see the .cpp.)
	int32 GenerateOutputs(TSet<AMetaRoad*>& OutWriteFailedActors);
	// Common teardown after the async bake finishes or is cancelled.
	void Finish();

	UPROPERTY()
	TWeakObjectPtr<UWorld> World;

	TUniquePtr<FAsyncTaskNotification> Notification;
	TArray<TWeakObjectPtr<AMetaRoad>> BakeActorsList;
	bool bBaking = false;
	bool bRebuildKicked = false;
	bool bCompleted = false; // ran to completion (not cancelled/aborted); not reset by Finish()
};
