/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadFbxExportHost.h"
#include "EditorMode/MetaRoadFbxExportSettings.h"
#include "EditorMode/MetaRoadFbxBuildHost.h"
#include "MetaRoadActor.h"
#include "MetaRoadModule.h" // LogMetaRoad

#include "FbxExporter.h"                  // UnFbx::FFbxExporter (private UnrealEd header; see Build.cs include path)
#include "Exporters/FbxExportOption.h"    // UFbxExportOption, EFbxExportCompatibility
#include "CinematicExporter.h"            // INodeNameAdapter

#include "Components/StaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Framework/Docking/TabManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadFbxExportHost)

#define LOCTEXT_NAMESPACE "MetaRoadFbxExportHost"

namespace
{
	// Node-name adapter so the FBX nodes use the actors' editor labels (like the engine level exporter).
	class FMetaRoadNodeNameAdapter : public INodeNameAdapter
	{
	public:
		virtual FString GetActorNodeName(const AActor* InActor) override
		{
			if (InActor)
			{
				const FString Label = InActor->GetActorLabel();
				return Label.IsEmpty() ? InActor->GetName() : Label;
			}
			return INodeNameAdapter::GetActorNodeName(InActor);
		}
	};

	EFbxExportCompatibility ToFbxCompatibility(EMetaRoadFbxVersion Version)
	{
		switch (Version)
		{
		case EMetaRoadFbxVersion::Fbx2014: return EFbxExportCompatibility::FBX_2014;
		case EMetaRoadFbxVersion::Fbx2016: return EFbxExportCompatibility::FBX_2016;
		case EMetaRoadFbxVersion::Fbx2018: return EFbxExportCompatibility::FBX_2018;
		case EMetaRoadFbxVersion::Fbx2019: return EFbxExportCompatibility::FBX_2019;
		case EMetaRoadFbxVersion::Fbx2020: return EFbxExportCompatibility::FBX_2020;
		case EMetaRoadFbxVersion::Fbx2013:
		default:                           return EFbxExportCompatibility::FBX_2013;
		}
	}
}

void UMetaRoadFbxExportHost::BeginExportAsync(UWorld* InWorld, const TArray<TWeakObjectPtr<AMetaRoad>>& InRoads, UMetaRoadFbxExportSettings* InSettings)
{
	World = InWorld;
	if (!InWorld || !InSettings)
	{
		return;
	}

	// Snapshot the settings (the panel edits the CDO; an export must be self-consistent across its ticks).
	MeshSource      = InSettings->MeshSource;
	Layout          = InSettings->FileLayout;
	Pivot           = InSettings->Pivot;
	OutputDir       = InSettings->OutputDirectory.Path;
	FilePrefix      = InSettings->FileNamePrefix;
	CombinedFileName= InSettings->CombinedFileName.IsEmpty() ? TEXT("MetaRoadExport") : InSettings->CombinedFileName;
	bOverwrite      = InSettings->bOverwriteExisting;
	bExportStatic   = InSettings->bExportStaticMeshes;
	bExportSpline   = InSettings->bExportSplineMeshes;
	bOpenFolder     = InSettings->bOpenFolderAfterExport;

	// Filter out stale entries.
	Roads.Reset();
	for (const TWeakObjectPtr<AMetaRoad>& Road : InRoads)
	{
		if (Road.IsValid())
		{
			Roads.Add(Road);
		}
	}

	// Build the FBX export options from the settings.
	Options = NewObject<UFbxExportOption>(this);
	Options->FbxExportCompatibility = ToFbxCompatibility(InSettings->FbxVersion);
	Options->bASCII           = InSettings->bExportAsText ? 1 : 0;
	Options->bForceFrontXAxis = InSettings->bForceFrontXAxis ? 1 : 0;
	Options->VertexColor      = InSettings->bExportVertexColor ? 1 : 0;
	Options->LevelOfDetail    = InSettings->bExportLOD ? 1 : 0;
	Options->Collision        = InSettings->bExportCollision ? 1 : 0;

	NumExported = NumSkippedNotBaked = NumSkippedOverwrite = NumFailed = 0;
	CurrentIndex = 0;
	bExporterInitialized = false;
	bCombinedDocOpen = false;

	FAsyncTaskNotificationConfig Config;
	Config.TitleText = LOCTEXT("ExportingFbx", "Exporting Road Meshes (FBX)");
	Config.ProgressText = LOCTEXT("ExportStarting", "Preparing export…");
	Config.bCanCancel = true;
	Config.ExpireDuration = 8.0f;
	Notification = MakeUnique<FAsyncTaskNotification>(Config);

	// Early outs: no roads, missing output folder, or a combined target that exists and must not be overwritten.
	if (Roads.Num() == 0)
	{
		AbortAndReport(LOCTEXT("ExportNothingTitle", "Nothing to export"),
			LOCTEXT("ExportNothingBody", "No Meta Road actors to export."));
		return;
	}
	if (OutputDir.IsEmpty())
	{
		AbortAndReport(LOCTEXT("ExportNoFolderTitle", "Export failed"),
			LOCTEXT("ExportNoFolderBody", "No output directory is set in the FBX Export settings."));
		return;
	}
	IFileManager::Get().MakeDirectory(*OutputDir, /*Tree=*/true);

	if (Layout == EMetaRoadFbxExportLayout::Combined)
	{
		CombinedPath = MakeFilePath(CombinedFileName);
		if (!bOverwrite && FPaths::FileExists(CombinedPath))
		{
			AbortAndReport(LOCTEXT("ExportExistsTitle", "Export skipped"),
				FText::Format(LOCTEXT("ExportExistsBody", "{0} already exists (overwrite is disabled)."), FText::FromString(CombinedPath)));
			return;
		}
	}

	if (MeshSource == EMetaRoadFbxMeshSource::FromScratch)
	{
		BuildHost = NewObject<UMetaRoadFbxBuildHost>(this);
	}

	bExporting = true;
}

bool UMetaRoadFbxExportHost::TickExport(float /*DeltaTime*/)
{
	if (!bExporting)
	{
		return false;
	}

	if (!World.IsValid())
	{
		AbortAndReport(LOCTEXT("ExportAbortedTitle", "Export aborted"), FText::GetEmpty());
		return false;
	}

	if (Notification.IsValid() && Notification->GetPromptAction() == EAsyncTaskNotificationPromptAction::Cancel)
	{
		AbortAndReport(LOCTEXT("ExportCancelledTitle", "Export cancelled"),
			FText::Format(LOCTEXT("ExportCancelledBody", "Exported {0} file(s) before cancelling."), NumExported));
		return false;
	}

	if (CurrentIndex >= Roads.Num())
	{
		FinalizeAndReport();
		return false;
	}

	ProcessNextRoad();
	return true;
}

void UMetaRoadFbxExportHost::ProcessNextRoad()
{
	const int32 Index = CurrentIndex++;
	AMetaRoad* Road = Roads[Index].Get();
	if (!Road)
	{
		return;
	}

	if (Notification.IsValid())
	{
		Notification->SetProgressText(FText::Format(
			LOCTEXT("ExportProgress", "Exporting {0} ({1}/{2})…"),
			FText::FromString(Road->GetActorLabel()), Index + 1, Roads.Num()));
	}

	// Resolve a source actor whose mesh components sit at their world positions.
	AActor* ScratchActor = nullptr;
	AActor* SourceActor = nullptr;
	if (MeshSource == EMetaRoadFbxMeshSource::FromScratch)
	{
		SourceActor = BuildHost ? BuildHost->BuildTransientExportActor(World.Get(), Road) : nullptr;
		ScratchActor = SourceActor;
		if (!SourceActor)
		{
			++NumFailed;
			UE_LOG(LogMetaRoad, Warning, TEXT("FBX export: failed to generate meshes for \"%s\"."), *Road->GetActorLabel());
			return;
		}
	}
	else
	{
		SourceActor = ResolveGeneratedSourceActor(Road);
		if (!SourceActor)
		{
			++NumSkippedNotBaked;
			UE_LOG(LogMetaRoad, Warning, TEXT("FBX export: \"%s\" has no baked output — skipped (bake it first, or use the From scratch source)."), *Road->GetActorLabel());
			return;
		}
	}

	TArray<UPrimitiveComponent*> Comps;
	GatherExportComponents(SourceActor, Comps);
	if (Comps.Num() == 0)
	{
		++NumSkippedNotBaked;
		UE_LOG(LogMetaRoad, Warning, TEXT("FBX export: \"%s\" has no exportable mesh components — skipped."), *Road->GetActorLabel());
		if (ScratchActor)
		{
			ScratchActor->Destroy();
		}
		return;
	}

	const FString BaseLabel = FilePrefix + Road->GetActorLabel();

	switch (Layout)
	{
	case EMetaRoadFbxExportLayout::Combined:
		ExportIntoCombinedDoc(SourceActor, Comps);
		break;

	case EMetaRoadFbxExportLayout::PerComponent:
		for (UPrimitiveComponent* Comp : Comps)
		{
			const FString CompPath = MakeFilePath(Road->GetActorLabel() + TEXT("_") + Comp->GetName());
			ExportToOwnFile(SourceActor, { Comp }, CompPath);
		}
		break;

	case EMetaRoadFbxExportLayout::PerActor:
	default:
		ExportToOwnFile(SourceActor, Comps, MakeFilePath(Road->GetActorLabel()));
		break;
	}

	// FromScratch: the throwaway actor's geometry is already copied into the FBX document(s) — drop it.
	if (ScratchActor)
	{
		ScratchActor->Destroy();
	}
}

AActor* UMetaRoadFbxExportHost::ResolveGeneratedSourceActor(AMetaRoad* Road) const
{
	if (!Road)
	{
		return nullptr;
	}
	// The baked geometry lives on the generated _Gen actor.
	if (IsValid(Road->LastGeneratedActor))
	{
		return Road->LastGeneratedActor;
	}
	return nullptr;
}

void UMetaRoadFbxExportHost::GatherExportComponents(AActor* Source, TArray<UPrimitiveComponent*>& OutComps) const
{
	if (!Source)
	{
		return;
	}
	// GetComponents<UStaticMeshComponent> also returns USplineMeshComponents (a subclass) — branch by type.
	TArray<UStaticMeshComponent*> MeshComps;
	Source->GetComponents<UStaticMeshComponent>(MeshComps);
	for (UStaticMeshComponent* Comp : MeshComps)
	{
		if (!Comp || !Comp->GetStaticMesh())
		{
			continue;
		}
		const bool bIsSpline = Comp->IsA<USplineMeshComponent>();
		if (bIsSpline ? bExportSpline : bExportStatic)
		{
			OutComps.Add(Comp);
		}
	}
}

void UMetaRoadFbxExportHost::EnsureExporterInitialized()
{
	if (!bExporterInitialized)
	{
		UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
		Exporter->SetExportOptionsOverride(Options);
		bExporterInitialized = true;
	}
}

AActor* UMetaRoadFbxExportHost::BuildProxyActor(const TArray<UPrimitiveComponent*>& Comps, const FTransform& SourceXf, bool bForceWorldPivot)
{
	UWorld* W = World.Get();
	if (!W)
	{
		return nullptr;
	}

	// Centered → proxy at identity (geometry ends up around (0,0,0) relative to the source origin);
	// world → proxy at the source transform (geometry keeps its world position). Combined forces world so the
	// roads in one file keep their relative layout.
	const bool bWorld = bForceWorldPivot || (Pivot == EMetaRoadFbxExportPivot::WorldPosition);
	const FTransform SpawnXf = bWorld ? SourceXf : FTransform::Identity;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.bTemporaryEditorActor = true;
	SpawnParams.bHideFromSceneOutliner = true;
	AActor* Proxy = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!Proxy)
	{
		return nullptr;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Proxy, TEXT("Root"));
	Proxy->SetRootComponent(Root);
	Root->RegisterComponent();
	Proxy->SetActorTransform(SpawnXf);
	// Freeze the root Static AFTER positioning: the baked mesh components are Static, so the root must be Static
	// too or AttachTo aborts ("cannot attach a static component to a non-static parent"); positioning while still
	// Movable avoids a "static component moved" warning in a PIE world.
	Root->SetMobility(EComponentMobility::Static);

	for (UPrimitiveComponent* Comp : Comps)
	{
		UStaticMeshComponent* Src = Cast<UStaticMeshComponent>(Comp);
		if (!Src)
		{
			continue;
		}
		// DuplicateObject preserves the dynamic type, so a USplineMeshComponent stays a spline mesh.
		UStaticMeshComponent* Dup = DuplicateObject<UStaticMeshComponent>(Src, Proxy);
		if (!Dup)
		{
			continue;
		}
		Proxy->AddInstanceComponent(Dup);
		Dup->SetupAttachment(Root);
		// Same arrangement around the proxy root as the source had around its own actor. Set before
		// RegisterComponent so positioning a Static component doesn't warn.
		Dup->SetRelativeTransform(Src->GetComponentTransform().GetRelativeTransform(SourceXf));
		Dup->RegisterComponent();
	}

	return Proxy;
}

bool UMetaRoadFbxExportHost::ExportToOwnFile(AActor* SourceActor, const TArray<UPrimitiveComponent*>& Comps, const FString& FilePath)
{
	if (!bOverwrite && FPaths::FileExists(FilePath))
	{
		++NumSkippedOverwrite;
		return false;
	}

	AActor* Proxy = BuildProxyActor(Comps, SourceActor->GetActorTransform(), /*bForceWorldPivot=*/false);
	if (!Proxy)
	{
		++NumFailed;
		return false;
	}

	EnsureExporterInitialized();
	UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
	FMetaRoadNodeNameAdapter Adapter;
	Exporter->CreateDocument();
	Exporter->ExportActor(Proxy, /*bExportComponents=*/true, Adapter, /*bSaveAnimSeq=*/false);
	Exporter->WriteToFile(*FilePath);
	Exporter->CloseDocument();

	Proxy->Destroy();

	++NumExported;
	return true;
}

void UMetaRoadFbxExportHost::ExportIntoCombinedDoc(AActor* SourceActor, const TArray<UPrimitiveComponent*>& Comps)
{
	// Combined keeps every road's world position so the relative layout is preserved in the single file.
	AActor* Proxy = BuildProxyActor(Comps, SourceActor->GetActorTransform(), /*bForceWorldPivot=*/true);
	if (!Proxy)
	{
		++NumFailed;
		return;
	}

	EnsureExporterInitialized();
	UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
	if (!bCombinedDocOpen)
	{
		Exporter->CreateDocument();
		bCombinedDocOpen = true;
	}

	FMetaRoadNodeNameAdapter Adapter;
	Exporter->ExportActor(Proxy, /*bExportComponents=*/true, Adapter, /*bSaveAnimSeq=*/false);

	Proxy->Destroy();
	++NumCombinedRoads;
}

FString UMetaRoadFbxExportHost::MakeFilePath(const FString& BaseName) const
{
	const FString Sanitized = FPaths::MakeValidFileName(BaseName, TEXT('_'));
	return OutputDir / (Sanitized + TEXT(".fbx"));
}

void UMetaRoadFbxExportHost::FinalizeAndReport()
{
	// Close the combined document (write it once now that every road has been added).
	if (bCombinedDocOpen)
	{
		UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
		Exporter->WriteToFile(*CombinedPath);
		Exporter->CloseDocument();
		bCombinedDocOpen = false;
		if (NumCombinedRoads > 0)
		{
			++NumExported; // the single combined file
		}
	}

	if (Notification.IsValid())
	{
		TArray<FText> Lines;
		if (NumExported > 0)         { Lines.Add(FText::Format(LOCTEXT("StatExported", "{0} file(s) exported"), NumExported)); }
		if (NumCombinedRoads > 0)    { Lines.Add(FText::Format(LOCTEXT("StatCombinedRoads", "{0} road(s) combined"), NumCombinedRoads)); }
		if (NumSkippedNotBaked > 0)  { Lines.Add(FText::Format(LOCTEXT("StatNotBaked", "{0} road(s) skipped — not baked"), NumSkippedNotBaked)); }
		if (NumSkippedOverwrite > 0) { Lines.Add(FText::Format(LOCTEXT("StatOverwrite", "{0} skipped — file exists"), NumSkippedOverwrite)); }
		if (NumFailed > 0)           { Lines.Add(FText::Format(LOCTEXT("StatFailed", "{0} failed"), NumFailed)); }
		const FText Body = Lines.Num() > 0 ? FText::Join(FText::FromString(TEXT("\n")), Lines) : LOCTEXT("StatNone", "Nothing was exported.");

		const bool bSuccess = (NumFailed == 0);

		// Set the title + body first, then the hyperlink, then SetComplete(bool) — the text-taking SetComplete
		// overload would rebuild the state and clear the hyperlink (mirrors UMetaRoadBakeHost).
		Notification->SetTitleText(bSuccess
			? LOCTEXT("ExportDoneTitle", "FBX export complete")
			: LOCTEXT("ExportDoneErrTitle", "FBX export finished with errors"), /*bClearProgressText=*/false);
		Notification->SetProgressText(Body);

		if (NumExported > 0)
		{
			const FString FolderToOpen = OutputDir;
			Notification->SetHyperlink(
				FSimpleDelegate::CreateLambda([FolderToOpen]() { FPlatformProcess::ExploreFolder(*FolderToOpen); }),
				LOCTEXT("ShowInExplorer", "Show in Explorer"));
		}

		Notification->SetComplete(bSuccess);
	}

	if (bOpenFolder && NumExported > 0)
	{
		FPlatformProcess::ExploreFolder(*OutputDir);
	}

	Finish();
}

void UMetaRoadFbxExportHost::AbortAndReport(const FText& Title, const FText& Body)
{
	if (bCombinedDocOpen)
	{
		UnFbx::FFbxExporter::GetInstance()->CloseDocument();
		bCombinedDocOpen = false;
	}
	if (Notification.IsValid())
	{
		Notification->SetComplete(Title, Body, /*bSuccess=*/false);
	}
	Finish();
}

void UMetaRoadFbxExportHost::Finish()
{
	if (bExporterInitialized)
	{
		UnFbx::FFbxExporter::GetInstance()->SetExportOptionsOverride(nullptr);
		UnFbx::FFbxExporter::DeleteInstance();
		bExporterInitialized = false;
	}
	Notification.Reset();
	Roads.Reset();
	Options = nullptr;
	BuildHost = nullptr;
	World = nullptr;
	bExporting = false;
}

#undef LOCTEXT_NAMESPACE
