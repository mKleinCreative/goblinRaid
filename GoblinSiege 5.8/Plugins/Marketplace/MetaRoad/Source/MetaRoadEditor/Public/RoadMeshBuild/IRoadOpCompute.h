
/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "BackgroundModelingComputeSource.h"
#include "MeshOpPreviewHelpers.h"
//#include "IRoadOpCompute.generated.h"

namespace MetaRoad
{
	/** ComponentTag stamped only on components whose UStaticMesh was CREATED by the bake (road surface
	 *  / decals / sidewalks / curbs / marks / lofting). The re-bake cleanup disposes of the mesh asset
	 *  ONLY for components carrying this tag, so user meshes merely REFERENCED via lane attributes
	 *  (URoadLaneAttributeSplineMeshDescriptor::StaticMesh, component templates) are never trashed or deleted. */
	inline const TCHAR* GeneratedAssetComponentTagName = TEXT("MetaRoadGeneratedAsset");
}

class METAROADEDITOR_API IRoadOpCompute
{

public:

	virtual ~IRoadOpCompute() = default;

	/** Shut down the background compute and write this layer's generated asset(s)/component(s) onto TargetActor.
	 *  @return false only when a write was attempted but failed (so the bake can surface the error); true when
	 *  it succeeded or there was legitimately nothing to write (disabled/empty layer). */
	virtual bool ShutdownAndGenerateAssets(AActor* TargetActor, const FTransform3d& ActorToWorld) = 0;

	/**
	 * Request that the current computation be canceled and a new one started
	 */
	virtual void InvalidateResult() = 0;

	/**
	 * Cancel the active computation without returning anything. Doesn't destroy the mesh.
	 */
	virtual void CancelCompute() = 0;

	/**
	 * Set the visibility of the Preview mesh
	 */
	virtual void SetVisibility(bool bVisible) = 0;

	/**
	 * Tick the background computation and Preview update.
	 * @warning this must be called regularly for the class to function properly
	 */
	virtual void Tick(float DeltaTime) = 0;

	/**
	 * Run the operator synchronously on the calling thread and apply the result to the preview mesh,
	 * bypassing the background-compute threading. Used by synchronous build paths (e.g. thumbnails).
	 * Default no-op for computes that don't produce a previewable surface mesh (spline-mesh attributes,
	 * road graph).
	 */
	virtual void ComputeSynchronous() {}

	/**
	 * Enable/disable wireframe overlay rendering
	 */
	virtual void EnableWireframe(bool bEnable) = 0;

	/**
	* Terminate any active computation without returning anything
	*/
	virtual void Cancel() = 0;

	/**
	 * @return true if current PreviewMesh result is valid (no update actively being computed) and that mesh has at least one triangle
	 */
	virtual bool HaveValidNonEmptyResult() const = 0;

	// Stored status of last compute, mainly so that we know when we should show the "busy" material.
	virtual UE::Geometry::EBackgroundComputeTaskStatus GetLastComputeStatus() const  = 0;

	virtual bool IsRoadAttribute() const = 0;

	virtual int GetNumVertices() const = 0;

	virtual int GetNumTriangles() const = 0;

	virtual UObject* AsObject() = 0;

	virtual TSet<FName>& GetRebuildTags() = 0;

	virtual const TSet<FName>& GetRebuildTags() const = 0;
	
};
