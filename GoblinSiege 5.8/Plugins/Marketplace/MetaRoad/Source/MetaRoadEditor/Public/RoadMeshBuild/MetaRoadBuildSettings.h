/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "MetaRoadBuildSettings.generated.h"

class UTriangulateRoadToolProperties;
class UMetaRoadBuildSettingsBase;
class AMetaRoad;

/**
 * One build property-set "slot" — the single source of truth for which property sets a road's build
 * settings contain, their Details section label, and (for layers) the matching RoadComputeFactories key.
 * Triangulation is the first entry (Key == NAME_None — it drives the base op, not a layer factory).
 */
struct FMetaRoadBuildPropertySetInfo
{
	FName Key;                                              // matches RoadComputeFactories key; NAME_None for Triangulation
	TSubclassOf<UMetaRoadBuildSettingsBase> PropsClass;
	FText SectionName;                                      // Details category label
};

/**
 * UMetaRoadBuildSettings
 *
 * Per-actor road-mesh build settings holder, owned by an AMetaRoad (editor-only). Lives in the
 * editor module so it can reference the editor-only property-set types that AMetaRoad (runtime
 * module) cannot. Stored on the actor via an editor-only UObject* property, so settings serialize
 * with the level in the editor but are stripped from cooked/shipping builds.
 *
 * Mirrors the IRoadMeshBuildHost SetupPropertySet/GetTriangulationProperties pattern, but the
 * instances are owned per-actor instead of per-host — the build pipeline sources them from the
 * scope's target actor (preview, bake, profile preview).
 */
UCLASS(EditInlineNew)
class METAROADEDITOR_API UMetaRoadBuildSettings : public UObject
{
	GENERATED_BODY()

public:
	/** All build property sets for this road, keyed by class — triangulation params and every layer set,
	 *  stored uniformly (see GetBuildPropertySetInfos() for the canonical schema). */
	UPROPERTY(EditAnywhere, Instanced, Category = "Build Settings")
	TArray<TObjectPtr<UMetaRoadBuildSettingsBase>> PropertySets;

	/** Find an existing property set of the given class (no creation — safe on any thread). */
	UMetaRoadBuildSettingsBase* FindPropertySet(const TSubclassOf<UMetaRoadBuildSettingsBase>& PropertySetClass) const;

	/** Find-or-create a property set of the given class (game thread only — allocates a UObject). */
	UMetaRoadBuildSettingsBase* FindOrCreatePropertySet(const TSubclassOf<UMetaRoadBuildSettingsBase>& PropertySetClass);

	template <typename T> T* Find() const { return Cast<T>(FindPropertySet(T::StaticClass())); }
	template <typename T> T* FindOrCreate() { return Cast<T>(FindOrCreatePropertySet(T::StaticClass())); }

	/** Create any missing property sets from the canonical schema so all sections are present (e.g. for the
	 *  actor Details, or before a background build reads them — call on the game thread). */
	void EnsureDefaultPropertySets();

	/** Single source of truth for the build property sets: class, Details section label and matching
	 *  RoadComputeFactories key, in pipeline order (triangulation first). PRO gating mirrors
	 *  FMetaRoadEditorModule::RegisterRoadComputeFactories(). Keep in sync when adding a new operator. */
	static const TArray<FMetaRoadBuildPropertySetInfo>& GetBuildPropertySetInfos();

	/** Get (and optionally create) the build settings holder owned by the given AMetaRoad actor.
	 *  Returns null when Actor is null or (bCreateIfMissing is false and none exists). */
	static UMetaRoadBuildSettings* GetForActor(AMetaRoad* Actor, bool bCreateIfMissing);

	// UObject
	virtual void PostLoad() override;

private:
#if WITH_EDITORONLY_DATA
	/** Deprecated: triangulation params used to live in a dedicated field; migrated into PropertySets in
	 *  PostLoad() after storage unification. */
	UPROPERTY()
	TObjectPtr<UTriangulateRoadToolProperties> TriangulateProperties_DEPRECATED;
#endif
};
