/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadDragDropHandler.h"

#include "Editor.h"
#include "EditorViewportClient.h"
#include "SceneView.h"
#include "ScopedTransaction.h"
#include "Engine/Blueprint.h"
#include "AssetRegistry/AssetData.h"
#include "UnrealClient.h"
#include "HitProxies.h"
#include "RoadSceneProxy/RoadSceneProxy.h"
#include "Slate/SceneViewport.h"
#include "Framework/Application/SlateApplication.h"

#include "MetaRoadEditorModule.h"
#include "ComponentVisualizers/RoadAttributeComponentVisualizer.h"

#include "RoadSplineComponent.h"
#include "MetaRoadTypes.h"
#include "RoadLaneAttribute.h"
#include "Assets/RoadLaneAttributeDescriptor.h"
#include "Assets/RoadLaneAttributeMark.h"
#include "Assets/RoadMarkProfile.h"
#include "Assets/RoadPolygonProfile.h"
#if METAROAD_PRO
#include "Assets/RoadLaneAttributePolygon.h"
#endif
#include "Assets/RoadProfile.h"

#define LOCTEXT_NAMESPACE "MetaRoadDragDrop"

namespace
{
	enum class EDropMode : uint8
	{
		Generic,	// any URoadLaneAttributeDescriptor subclass -> replace with a single key at SOffset 0
		Mark,		// URoadMarkProfile -> URoadLaneAttributeMarkDescriptor (replace), Profile set on the value
		Polygon,	// URoadPolygonProfile -> URoadLaneAttributePolygonDescriptor (add key at cursor SOffset)
		Profile,	// URoadProfile -> AssignToRoadSpline() on the road under the cursor (not lane/selection based)
	};

	struct FResolvedDrop
	{
		TSubclassOf<URoadLaneAttributeDescriptor> DescriptorClass;	// unused for Profile
		EDropMode Mode = EDropMode::Generic;
		UObject* ProfileAsset = nullptr;	// URoadMarkProfile* / URoadPolygonProfile* / URoadProfile*

		bool IsValid() const { return Mode == EDropMode::Profile ? ProfileAsset != nullptr : DescriptorClass != nullptr; }
	};

	/** Map a dropped object to the attribute action it represents. Returns an invalid result for
	 *  anything we don't handle (caller falls back to Super). */
	FResolvedDrop ResolveDrop(UObject* Dropped)
	{
		FResolvedDrop Result;
		if (!IsValid(Dropped))
		{
			return Result;
		}

		if (Dropped->IsA<URoadMarkProfile>())
		{
			Result.DescriptorClass = URoadLaneAttributeMarkDescriptor::StaticClass();
			Result.Mode = EDropMode::Mark;
			Result.ProfileAsset = Dropped;
			return Result;
		}

#if METAROAD_PRO
		if (Dropped->IsA<URoadPolygonProfile>())
		{
			Result.DescriptorClass = URoadLaneAttributePolygonDescriptor::StaticClass();
			Result.Mode = EDropMode::Polygon;
			Result.ProfileAsset = Dropped;
			return Result;
		}
#endif

		if (Dropped->IsA<URoadProfile>())
		{
			Result.Mode = EDropMode::Profile;
			Result.ProfileAsset = Dropped;
			return Result;
		}

		UClass* Candidate = nullptr;
		if (const UBlueprint* Blueprint = Cast<UBlueprint>(Dropped))
		{
			Candidate = Blueprint->GeneratedClass;
		}
		else if (UClass* AsClass = Cast<UClass>(Dropped))
		{
			Candidate = AsClass;
		}

		if (Candidate
			&& Candidate->IsChildOf(URoadLaneAttributeDescriptor::StaticClass())
			&& !Candidate->HasAnyClassFlags(CLASS_Abstract))
		{
			Result.DescriptorClass = Candidate;
			Result.Mode = EDropMode::Generic;
		}

		return Result;
	}

	FResolvedDrop ResolveDrop(const FAssetData& AssetData)
	{
		return ResolveDrop(AssetData.IsValid() ? AssetData.GetAsset() : nullptr);
	}

	/** Active MetaRoad section/attribute visualizer, or null if the mode/visualizer isn't suitable.
	 *  Note: road editing is driven by the registered component visualizer + RoadSelectionMode on the
	 *  selected component; it does NOT require the dedicated "Meta Road" UEdMode to be active. */
	FRoadSectionComponentVisualizer* GetActiveSectionVisualizer()
	{
		if (!GEditor)
		{
			return nullptr;
		}

		FMetaRoadSelectionController& Module = FMetaRoadSelectionController::Get();
		const ERoadSelectionMode SelectionMode = Module.GetRoadSelectionMode();
		// Lane selection is only meaningful (and the visualizer only derives from the section
		// visualizer) in Section and Attribute modes.
		if (SelectionMode != ERoadSelectionMode::Section && SelectionMode != ERoadSelectionMode::Attribute)
		{
			return nullptr;
		}

		TSharedPtr<FComponentVisualizer> Visualizer = Module.GetComponentVisualizer();
		if (!Visualizer.IsValid())
		{
			return nullptr;
		}

		return StaticCastSharedPtr<FRoadSectionComponentVisualizer>(Visualizer).Get();
	}

	/** Resolve the drop target from the current selection. Returns true and fills the out params when a
	 *  valid lane is selected and the descriptor can be added to it (whether the cursor is actually over
	 *  that lane is hit-tested separately). */
	bool GetDropTarget(const FResolvedDrop& Resolved,
		URoadSplineComponent*& OutSpline, int32& OutSectionIndex, int32& OutLaneIndex,
		URoadLaneAttributeDescriptor*& OutDefaultObject)
	{
		OutSpline = nullptr;
		OutSectionIndex = INDEX_NONE;
		OutLaneIndex = INDEX_NONE;
		OutDefaultObject = Resolved.DescriptorClass ? Resolved.DescriptorClass->GetDefaultObject<URoadLaneAttributeDescriptor>() : nullptr;

		if (!OutDefaultObject)
		{
			return false;
		}

		FRoadSectionComponentVisualizer* Visualizer = GetActiveSectionVisualizer();
		if (!Visualizer)
		{
			return false;
		}

		URoadSectionComponentVisualizerSelectionState* State = Visualizer->GetSelectionState();
		if (!State || State->GetStateVerified() < ERoadSectionSelectionState::Section)
		{
			return false;
		}

		OutSpline = State->GetSelectedSpline();
		OutSectionIndex = State->GetSelectedSectionIndex();
		OutLaneIndex = State->GetSelectedLaneIndex();

		return OutSpline && OutDefaultObject->CanBeAddedTo(OutSpline, OutSectionIndex, OutLaneIndex);
	}

	/** True cursor position in viewport pixels. The MouseX/MouseY delivered to the drop handler come from
	 *  the drag event and sit ~20px below the real cursor; recompute from the live OS cursor + the render
	 *  surface geometry, exactly like FSceneViewport::UpdateCachedCursorPos. The level-viewport drop path
	 *  always uses an FSceneViewport, so the cast is safe here. */
	FIntPoint GetCursorViewportPixel(FViewport* Viewport)
	{
		const FGeometry& Geo = static_cast<FSceneViewport*>(Viewport)->GetCachedGeometry();
		const FVector2D Local = Geo.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos()) * Geo.Scale;
		return FIntPoint(FMath::RoundToInt(Local.X), FMath::RoundToInt(Local.Y));
	}

	/** True when the cursor is over ANY selected lane (primary or a member of the multi-selection set)
	 *  of the given spline. Used so a bulk attribute drop can be released over any highlighted lane.
	 *  When non-null, OutSection/OutLane receive the lane under the cursor (needed by positional drops). */
	bool IsCursorOnAnySelectedLane(FViewport* Viewport, const URoadSplineComponent* Spline,
		const URoadSectionComponentVisualizerSelectionState* State,
		int32* OutSection = nullptr, int32* OutLane = nullptr)
	{
		if (!State)
		{
			return false;
		}
		const FIntPoint Pixel = GetCursorViewportPixel(Viewport);
		HHitProxy* HitProxy = Viewport->GetHitProxy(Pixel.X, Pixel.Y);
		if (HitProxy && HitProxy->IsA(HRoadLaneVisProxy::StaticGetType()))
		{
			const HRoadLaneVisProxy* LaneProxy = static_cast<const HRoadLaneVisProxy*>(HitProxy);
			if (LaneProxy->Component.Get() != Spline)
			{
				return false;
			}
			const bool bInSelection =
				(LaneProxy->SectionIndex == State->GetSelectedSectionIndex() && LaneProxy->LaneIndex == State->GetSelectedLaneIndex())
				|| State->IsLaneInSelection(LaneProxy->SectionIndex, LaneProxy->LaneIndex);
			if (bInSelection)
			{
				if (OutSection) { *OutSection = LaneProxy->SectionIndex; }
				if (OutLane) { *OutLane = LaneProxy->LaneIndex; }
			}
			return bInSelection;
		}
		return false;
	}

	/** The road spline under the cursor (any of its hit proxies — surface, reference line, keys — derive
	 *  from HRoadSplineVisProxy). Cursor-targeted, works for any visible road regardless of selection/mode. */
	URoadSplineComponent* GetRoadSplineUnderCursor(FViewport* Viewport)
	{
		const FIntPoint Pixel = GetCursorViewportPixel(Viewport);
		HHitProxy* HitProxy = Viewport->GetHitProxy(Pixel.X, Pixel.Y);
		if (HitProxy && HitProxy->IsA(HRoadSplineVisProxy::StaticGetType()))
		{
			const HRoadSplineVisProxy* RoadProxy = static_cast<const HRoadSplineVisProxy*>(HitProxy);
			return const_cast<URoadSplineComponent*>(Cast<URoadSplineComponent>(RoadProxy->Component.Get()));
		}
		return nullptr;
	}

	/** Apply a URoadProfile to a road spline (rebuilds its layout) under a transaction. */
	void ApplyRoadProfile(const URoadProfile* Profile, URoadSplineComponent* Spline)
	{
		if (!Profile || !Spline)
		{
			return;
		}

		const FScopedTransaction Transaction(LOCTEXT("DropRoadProfile", "Apply Road Profile"));
		Spline->Modify();
		Profile->AssignToRoadSpline(Spline);	// empties + rebuilds the lane layout + UpdateRoadLayout()
		Spline->MarkRoadStateDirty();
		Spline->MarkRenderStateDirty();			// rebuild the road scene proxy (lane geometry changed)
		GEditor->RedrawLevelEditingViewports(true);
	}

	/** Key SOffset (relative to the section) nearest the cursor along the selected lane — used by Polygon
	 *  drops. Builds a scene view for the cursor ray (mirrors HandleInputDelta), so call it only on drop. */
	double GetCursorSOffsetOnLane(FViewport* Viewport, URoadSplineComponent* Spline, int32 SectionIndex, int32 LaneIndex)
	{
		const FRoadLaneSection& Section = Spline->GetLaneSection(SectionIndex);
		const auto Rang = Spline->GetLaneRang(SectionIndex, LaneIndex);

		FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(Viewport->GetClient());
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags));
		FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
		if (!View)
		{
			return Rang.StartS - Section.SOffset;
		}

		const FIntPoint Pixel = GetCursorViewportPixel(Viewport);
		FViewportCursorLocation Cursor(View, ViewportClient, Pixel.X, Pixel.Y);
		const FVector RayOrigin = Cursor.GetOrigin();
		const FVector RayDir = Cursor.GetDirection();
		const float Key = Spline->KeyAtRayHit(Rang.StartS, Rang.EndS, RayOrigin, RayOrigin + RayDir * 50000.0f);
		const double ClosestS = FMath::Clamp((double)Spline->GetDistanceAlongSplineAtSplineInputKey(Key), Rang.StartS, Rang.EndS);
		return ClosestS - Section.SOffset;
	}

	/** Perform the actual drop: add the attribute key, switch to Attribute mode and select the key.
	 *  Returns true if the drop was applied. */
	bool ApplyDrop(const FResolvedDrop& Resolved, URoadSplineComponent* Spline,
		int32 SectionIndex, int32 LaneIndex, double PolygonSOffset)
	{
		FRoadSectionComponentVisualizer* Visualizer = GetActiveSectionVisualizer();
		if (!Visualizer)
		{
			return false;
		}

		const FScopedTransaction Transaction(LOCTEXT("DropRoadAttribute", "Drop Road Attribute"));

		int32 KeyIndex = INDEX_NONE;
		switch (Resolved.Mode)
		{
		case EDropMode::Mark:
		{
			FRoadLaneMark Value = GetDefault<URoadLaneAttributeMarkDescriptor>()->AttributeValueTemplate;
			Value.ProfileSource = ERoadLaneMarkProfile::UsePreset;
			Value.Profile = Cast<URoadMarkProfile>(Resolved.ProfileAsset);
			KeyIndex = Visualizer->AddAttributeKeyToAllSelectedLanes(Resolved.DescriptorClass, Value, 0.0, /*bReplaceExisting*/ true);
			break;
		}
#if METAROAD_PRO
		case EDropMode::Polygon:
		{
			FRoadLaneAttributePolygonValue Value = GetDefault<URoadLaneAttributePolygonDescriptor>()->AttributeValueTemplate;
			Value.Profile = Cast<URoadPolygonProfile>(Resolved.ProfileAsset);
			// Polygon is positional (keyed at a specific SOffset on a specific lane): target ONLY the lane
			// the drop landed on, never the whole multi-selection.
			KeyIndex = Visualizer->AddAttributeKeyToLane(SectionIndex, LaneIndex, Resolved.DescriptorClass, Value, PolygonSOffset, /*bReplaceExisting*/ false);
			break;
		}
#endif
		default:
			KeyIndex = Visualizer->AddAttributeKeyToAllSelectedLanes(Resolved.DescriptorClass, TConstStructView<FRoadLaneAttributeValue>(), 0.0, /*bReplaceExisting*/ true);
			break;
		}

		if (KeyIndex == INDEX_NONE)
		{
			return false;
		}

		// Switch to Attribute mode for this descriptor (this may recreate the visualizer) and select
		// the newly added key on the (now active) attribute visualizer.
		FMetaRoadSelectionController::Get().SetAttributeEditorMode(Resolved.DescriptorClass);
		if (FRoadSectionComponentVisualizer* ActiveVisualizer = GetActiveSectionVisualizer())
		{
			static_cast<FRoadAttributeComponentVisualizer*>(ActiveVisualizer)->SelectAttributeKey(Spline, SectionIndex, LaneIndex, Resolved.DescriptorClass, KeyIndex);
		}

		return true;
	}
}

bool UMetaRoadLevelEditorDragDropHandler::PreviewDropObjectsAtCoordinates(
	int32 MouseX, int32 MouseY, UWorld* World, FViewport* Viewport, const FAssetData& AssetData)
{
	const FResolvedDrop Resolved = ResolveDrop(AssetData);
	if (!Resolved.IsValid())
	{
		// Not one of our assets — let the engine drive the cursor/placement.
		return Super::PreviewDropObjectsAtCoordinates(MouseX, MouseY, World, Viewport, AssetData);
	}

	// URoadProfile: cursor-targeted onto any road, no selection/mode/lane required.
	if (Resolved.Mode == EDropMode::Profile)
	{
		const bool bOnRoad = GetRoadSplineUnderCursor(Viewport) != nullptr;
		bCanDrop = bOnRoad;
		HintText = bOnRoad
			? FText::Format(LOCTEXT("MetaRoadCanDropProfile", "Apply road profile {0}"), FText::FromString(Resolved.ProfileAsset->GetName()))
			: LOCTEXT("MetaRoadProfileNeedsRoad", "Drop onto a road to apply the profile");
		return false;
	}

	// Our asset: drive the cursor ourselves. The drop is only allowed when a valid lane is selected,
	// the attribute can be added to it, AND the cursor is over that lane. Returning false signals we
	// handled the preview (bCanDrop / HintText drive the cursor).
	URoadSplineComponent* Spline = nullptr;
	int32 SectionIndex = INDEX_NONE, LaneIndex = INDEX_NONE;
	URoadLaneAttributeDescriptor* DefaultObject = nullptr;
	const bool bTargetOk = GetDropTarget(Resolved, Spline, SectionIndex, LaneIndex, DefaultObject);

	FRoadSectionComponentVisualizer* PreviewVisualizer = GetActiveSectionVisualizer();
	const URoadSectionComponentVisualizerSelectionState* PreviewState = PreviewVisualizer ? PreviewVisualizer->GetSelectionState() : nullptr;
	const bool bOnLane = bTargetOk && IsCursorOnAnySelectedLane(Viewport, Spline, PreviewState);

	bCanDrop = bTargetOk && bOnLane;
	if (!bTargetOk)
	{
		HintText = LOCTEXT("MetaRoadCannotDrop", "This attribute can't be added to the selected lane");
	}
	else if (!bOnLane)
	{
		HintText = LOCTEXT("MetaRoadMoveToLane", "Move the cursor over a selected lane to add the attribute");
	}
	else
	{
		HintText = FText::Format(LOCTEXT("MetaRoadCanDrop", "Add {0} to the selected lane(s)"), DefaultObject->GetDisplayName());
	}

	return false;
}

bool UMetaRoadLevelEditorDragDropHandler::PreDropObjectsAtCoordinates(
	int32 MouseX, int32 MouseY, UWorld* World, FViewport* Viewport,
	const TArray<UObject*>& DroppedObjects, TArray<AActor*>& OutNewActors)
{
	if (DroppedObjects.Num() > 0)
	{
		const FResolvedDrop Resolved = ResolveDrop(DroppedObjects[0]);

		// URoadProfile: apply to the road under the cursor (any visible road, any mode).
		if (Resolved.IsValid() && Resolved.Mode == EDropMode::Profile)
		{
			if (URoadSplineComponent* RoadSpline = GetRoadSplineUnderCursor(Viewport))
			{
				ApplyRoadProfile(Cast<URoadProfile>(Resolved.ProfileAsset), RoadSpline);
				return false;
			}
		}

		URoadSplineComponent* Spline = nullptr;
		int32 SectionIndex = INDEX_NONE, LaneIndex = INDEX_NONE;
		URoadLaneAttributeDescriptor* DefaultObject = nullptr;
		FRoadSectionComponentVisualizer* DropVisualizer = GetActiveSectionVisualizer();
		const URoadSectionComponentVisualizerSelectionState* DropState = DropVisualizer ? DropVisualizer->GetSelectionState() : nullptr;
		int32 CursorSection = INDEX_NONE, CursorLane = INDEX_NONE;
		if (Resolved.IsValid() && GetDropTarget(Resolved, Spline, SectionIndex, LaneIndex, DefaultObject)
			&& IsCursorOnAnySelectedLane(Viewport, Spline, DropState, &CursorSection, &CursorLane))
		{
			if (Resolved.Mode == EDropMode::Polygon)
			{
				// Positional drop: retarget to the lane under the cursor and key it at the cursor SOffset.
				SectionIndex = CursorSection;
				LaneIndex = CursorLane;
				const double SOffset = GetCursorSOffsetOnLane(Viewport, Spline, SectionIndex, LaneIndex);
				ApplyDrop(Resolved, Spline, SectionIndex, LaneIndex, SOffset);
			}
			else
			{
				// Non-positional drops apply to the whole selection, keyed at SOffset 0 (lane start).
				ApplyDrop(Resolved, Spline, SectionIndex, LaneIndex, 0.0);
			}
			// Return false to suppress default actor placement (we don't create any actor).
			return false;
		}
	}

	return Super::PreDropObjectsAtCoordinates(MouseX, MouseY, World, Viewport, DroppedObjects, OutNewActors);
}

#undef LOCTEXT_NAMESPACE
