/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "RoadMeshBuild/ToolPropertySets.h"         // UTriangulateRoadToolProperties + layer property-set classes
#include "RoadMeshBuild/MetaRoadBuildSettingsBase.h"
#include "MetaRoadActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadBuildSettings)

#define LOCTEXT_NAMESPACE "MetaRoadBuildSettings"

UMetaRoadBuildSettingsBase* UMetaRoadBuildSettings::FindPropertySet(const TSubclassOf<UMetaRoadBuildSettingsBase>& PropertySetClass) const
{
	for (const TObjectPtr<UMetaRoadBuildSettingsBase>& Existing : PropertySets)
	{
		if (Existing && Existing->GetClass() == PropertySetClass.Get())
		{
			return Existing;
		}
	}
	return nullptr;
}

UMetaRoadBuildSettingsBase* UMetaRoadBuildSettings::FindOrCreatePropertySet(const TSubclassOf<UMetaRoadBuildSettingsBase>& PropertySetClass)
{
	if (UMetaRoadBuildSettingsBase* Existing = FindPropertySet(PropertySetClass))
	{
		return Existing;
	}

	UMetaRoadBuildSettingsBase* NewProps = NewObject<UMetaRoadBuildSettingsBase>(this, PropertySetClass);
	PropertySets.Add(NewProps);
	return NewProps;
}

const TArray<FMetaRoadBuildPropertySetInfo>& UMetaRoadBuildSettings::GetBuildPropertySetInfos()
{
	// Single source of truth (class + Details section + factory key), in pipeline order. PRO gating
	// mirrors FMetaRoadEditorModule::RegisterRoadComputeFactories(). Triangulation is first (no factory).
	static const TArray<FMetaRoadBuildPropertySetInfo> Infos = []()
	{
		TArray<FMetaRoadBuildPropertySetInfo> Result;
		Result.Add({ NAME_None,           UTriangulateRoadToolProperties::StaticClass(), LOCTEXT("Sec_Triangulation", "Triangulation") });
		Result.Add({ "RoadSurface",       URoadSurfaceToolProperties::StaticClass(),     LOCTEXT("Sec_DriveSurface", "Drive Surface") });
		Result.Add({ "RoadDecals",        URoadDecalToolProperties::StaticClass(),       LOCTEXT("Sec_Decals", "Decals") });
		Result.Add({ "RoadSidewalks",     URoadSidewalkToolProperties::StaticClass(),    LOCTEXT("Sec_Sidewalks", "Sidewalks") });
		Result.Add({ "RoadCurbs",         URoadCertbToolProperties::StaticClass(),       LOCTEXT("Sec_Curbs", "Curbs") });
		Result.Add({ "RoadMarks",         URoadMarkToolProperties::StaticClass(),        LOCTEXT("Sec_Marks", "Marks") });
#if METAROAD_PRO
		Result.Add({ "RoadLofting",       ULoftingToolProperties::StaticClass(),         LOCTEXT("Sec_Lofting", "Lofting") });
#endif
		Result.Add({ "RoadSplineMeshes",  URoadAttributesToolProperties::StaticClass(),  LOCTEXT("Sec_SplineMeshes", "Spline Meshes") });
		Result.Add({ "RoadGraph",         URoadGraphToolProperties::StaticClass(),       LOCTEXT("Sec_Graph", "Graph") });
		return Result;
	}();
	return Infos;
}

void UMetaRoadBuildSettings::EnsureDefaultPropertySets()
{
	for (const FMetaRoadBuildPropertySetInfo& Info : GetBuildPropertySetInfos())
	{
		if (Info.PropsClass)
		{
			FindOrCreatePropertySet(Info.PropsClass);
		}
	}
}

void UMetaRoadBuildSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	// Migrate the old dedicated triangulation field into the unified PropertySets array.
	if (TriangulateProperties_DEPRECATED)
	{
		if (!FindPropertySet(UTriangulateRoadToolProperties::StaticClass()))
		{
			PropertySets.Add(TriangulateProperties_DEPRECATED);
		}
		TriangulateProperties_DEPRECATED = nullptr;
	}
#endif
}

UMetaRoadBuildSettings* UMetaRoadBuildSettings::GetForActor(AMetaRoad* Road, bool bCreateIfMissing)
{
	if (!Road)
	{
		return nullptr;
	}

	UMetaRoadBuildSettings* Settings = Cast<UMetaRoadBuildSettings>(Road->BuildSettings);
	if (!Settings && bCreateIfMissing)
	{
		Settings = NewObject<UMetaRoadBuildSettings>(Road);
		Road->BuildSettings = Settings;
		// Intentionally not marking the package dirty here: merely previewing/selecting a road should not
		// flag the level for save. Editing a setting in the Details panel dirties it via PostEditChange.
	}
	return Settings;
}

#undef LOCTEXT_NAMESPACE
