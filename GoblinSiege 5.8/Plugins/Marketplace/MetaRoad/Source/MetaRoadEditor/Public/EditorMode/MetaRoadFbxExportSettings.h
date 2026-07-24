/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MetaRoadFbxExportSettings.generated.h"

/** Where the geometry exported to FBX comes from. */
UENUM()
enum class EMetaRoadFbxMeshSource : uint8
{
	/** Export the meshes that were already baked (AMetaRoad::LastGeneratedActor). Requires a prior Bake. */
	GeneratedAssets UMETA(DisplayName = "Generated assets"),
	/** Regenerate the road meshes in RAM (no disk assets, no level changes) via IRoadMeshBuildHost, then export. */
	FromScratch     UMETA(DisplayName = "From scratch (in RAM)")
};

/** How the exported .fbx files are laid out on disk. */
UENUM()
enum class EMetaRoadFbxExportLayout : uint8
{
	/** One .fbx per road actor, containing all its mesh components. */
	PerActor     UMETA(DisplayName = "One file per road"),
	/** A single .fbx containing every exported road. */
	Combined     UMETA(DisplayName = "Single combined file"),
	/** One .fbx per static/spline mesh component. */
	PerComponent UMETA(DisplayName = "One file per mesh")
};

/** Origin/pivot used for the exported geometry. */
UENUM()
enum class EMetaRoadFbxExportPivot : uint8
{
	/** Geometry is centered around (0,0,0) relative to the source actor — best for reuse in other DCC tools. */
	ActorOrigin   UMETA(DisplayName = "Centered at road origin"),
	/** Geometry keeps its level world coordinates. */
	WorldPosition UMETA(DisplayName = "Keep world position")
};

/** Self-contained mirror of UnFbx's EFbxExportCompatibility (so the public header needn't include a UnrealEd one). */
UENUM()
enum class EMetaRoadFbxVersion : uint8
{
	Fbx2013 UMETA(DisplayName = "FBX 2013"),
	Fbx2014 UMETA(DisplayName = "FBX 2014"),
	Fbx2016 UMETA(DisplayName = "FBX 2016"),
	Fbx2018 UMETA(DisplayName = "FBX 2018"),
	Fbx2019 UMETA(DisplayName = "FBX 2019"),
	Fbx2020 UMETA(DisplayName = "FBX 2020")
};

/**
 * UMetaRoadFbxExportSettings
 *
 * Settings for the "FBX Export" action (shown in the "Bake" palette panel via an IDetailsView, next to Bake).
 * A single config-backed instance (the CDO, see Get()) edited in the panel and read by FRoadFbxExporter to
 * export the baked (or freshly generated) road meshes of the selected / all AMetaRoad actors to .fbx files.
 * Mirrors UMetaRoadBakeSettings in shape (config CDO + Get() + SaveConfig on edit).
 */
UCLASS(config = EditorPerProjectUserSettings)
class METAROADEDITOR_API UMetaRoadFbxExportSettings : public UObject
{
	GENERATED_BODY()

public:
	UMetaRoadFbxExportSettings();

	// ---- Source ----
	/** Generated assets = export the already-baked meshes (LastGeneratedActor); requires a prior Bake.
	 *  From scratch = regenerate the meshes in RAM (no disk assets / level changes) and export those. */
	UPROPERTY(EditAnywhere, config, Category = "Source")
	EMetaRoadFbxMeshSource MeshSource = EMetaRoadFbxMeshSource::GeneratedAssets;

	// ---- Output ----
	/** Filesystem folder the .fbx files are written to (outside the content tree). */
	UPROPERTY(EditAnywhere, config, Category = "Output")
	FDirectoryPath OutputDirectory;

	/** How the exported files are organized. */
	UPROPERTY(EditAnywhere, config, Category = "Output")
	EMetaRoadFbxExportLayout FileLayout = EMetaRoadFbxExportLayout::PerActor;

	/** Base file name used when FileLayout == Combined. */
	UPROPERTY(EditAnywhere, config, Category = "Output",
		meta = (EditCondition = "FileLayout == EMetaRoadFbxExportLayout::Combined", EditConditionHides))
	FString CombinedFileName = TEXT("MetaRoadExport");

	/** Optional prefix prepended to every generated file name. */
	UPROPERTY(EditAnywhere, config, Category = "Output")
	FString FileNamePrefix;

	/** If false, an export that would overwrite an existing file is skipped (with a warning). */
	UPROPERTY(EditAnywhere, config, Category = "Output")
	bool bOverwriteExisting = true;

	// ---- Content ----
	/** Include UStaticMeshComponents in the export. */
	UPROPERTY(EditAnywhere, config, Category = "Content")
	bool bExportStaticMeshes = true;

	/** Include USplineMeshComponents in the export. */
	UPROPERTY(EditAnywhere, config, Category = "Content")
	bool bExportSplineMeshes = true;

	/** Origin/pivot used for the exported geometry. */
	UPROPERTY(EditAnywhere, config, Category = "Content")
	EMetaRoadFbxExportPivot Pivot = EMetaRoadFbxExportPivot::ActorOrigin;

	// ---- FBX format (translated to UFbxExportOption at export time) ----
	UPROPERTY(EditAnywhere, config, Category = "FBX Format")
	EMetaRoadFbxVersion FbxVersion = EMetaRoadFbxVersion::Fbx2013;

	/** Export as ASCII text instead of binary (larger, slower; only for debugging). */
	UPROPERTY(EditAnywhere, config, Category = "FBX Format")
	bool bExportAsText = false;

	/** Use X (instead of -Y) as the forward axis. */
	UPROPERTY(EditAnywhere, config, Category = "FBX Format")
	bool bForceFrontXAxis = false;

	/** Export vertex colors. */
	UPROPERTY(EditAnywhere, config, Category = "FBX Format")
	bool bExportVertexColor = true;

	/** Export the mesh LODs. */
	UPROPERTY(EditAnywhere, config, Category = "FBX Format")
	bool bExportLOD = false;

	/** Export collision geometry (static meshes only). */
	UPROPERTY(EditAnywhere, config, Category = "FBX Format", meta = (EditCondition = "bExportStaticMeshes"))
	bool bExportCollision = false;

	// ---- After Export ----
	/** Reveal OutputDirectory in the OS file explorer when the export finishes. */
	UPROPERTY(EditAnywhere, config, Category = "After Export")
	bool bOpenFolderAfterExport = true;

	/** The single shared instance (CDO), persisted to the editor config. */
	static UMetaRoadFbxExportSettings* Get() { return GetMutableDefault<UMetaRoadFbxExportSettings>(); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
