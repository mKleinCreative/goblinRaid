 /*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "MetaRoadSubsystem.generated.h"

class URoadConnection;
class URoadSplineComponent;
class ULaneConnection;


UCLASS()
class METAROAD_API UMetaRoadSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Editor || WorldType == EWorldType::PIE || WorldType == EWorldType::Inactive || WorldType == EWorldType::EditorPreview;
	}

#if WITH_EDITOR
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;

public:
	/**
	 * Rebuilds inter-spline connections for a set of freshly pasted/duplicated splines.
	 * Builds a {ULaneConnection::Guid → ULaneConnection*} map from all lane LCs in `Splines`,
	 * then for each URoadConnection whose LaneConnectionGuid matches an entry in that map,
	 * calls ConnectTo() to restore the link. Only connections within the pasted set are restored
	 * (cross-paste links to non-pasted splines are intentionally left disconnected).
	 */
	static void EndCopySplineTransaction(const TArray<URoadSplineComponent*>& Splines);

	/** Called from URoadSplineComponent::OnRegister() when bWasImported is true.
	 *  Adds the spline to PendingSplinesToImport; connections are restored on the next Tick
	 *  after all components in the same paste operation have been registered. */
	void OnSplinePostEditImport(URoadSplineComponent* Spline);

	const TSet<TWeakObjectPtr<AActor>>& GetMovingActors() const { return MovingActors; }

private:
	/**
	 * Bound to FEditorDelegates::OnDuplicateActorsBegin and OnEditPasteActorsBegin.
	 * Calls URoadSplineComponent::RefreshConnectionGuids() on every spline in the world so that
	 * URoadConnection::LaneConnectionGuid values are populated before T3D serialization.
	 * This is the actor-duplicate equivalent of the PreDuplicate() call used on the binary-dup path.
	 */
	void OnDuplicateActorsBegin();
	void BeginObjectMovement(UObject& Object);
	void EndObjectMovement(TArray<AActor*>& Actors);

	TSet<TWeakObjectPtr<AActor>> MovingActors;

	/** Splines that have been pasted/duplicated and are waiting for EndCopySplineTransaction on the next Tick. */
	TSet<TWeakObjectPtr<URoadSplineComponent>> PendingSplinesToImport;

#endif // WITH_EDITOR
};
