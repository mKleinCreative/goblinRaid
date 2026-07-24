/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadSelectionController.h"
#include "Modules/ModuleManager.h"
#include "EditorMode/MetaRoadSelectionModel.h"
#include "EditorMode/MetaRoadEditorMode.h"
#include "ComponentVisualizers/RoadSplineComponentVisualizer.h"
#include "ComponentVisualizers/RoadSectionComponentVisualizer.h"
#include "ComponentVisualizers/RoadOffsetComponentVisualizer.h"
#include "ComponentVisualizers/RoadWidthComponentVisualizer.h"
#include "ComponentVisualizers/RoadAttributeComponentVisualizer.h"
#include "RoadSplineComponent.h"

#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorModeManager.h"
#include "Selection.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "PropertyEditorModule.h"
#include "Tools/UEdMode.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"

FMetaRoadSelectionController& FMetaRoadSelectionController::Get()
{
	static FMetaRoadSelectionController Instance;
	return Instance;
}

void FMetaRoadSelectionController::SetSplineEditorMode()
{
	if (!GUnrealEd)
	{
		return;
	}

	if (RoadSelectionMode != ERoadSelectionMode::Spline)
	{
		SelectionAttributeDescriptor = nullptr;
		RoadSelectionMode = ERoadSelectionMode::Spline;
		SetComponentVisualizer(MakeShared<FRoadSplineComponentVisualizer>());
	}
}

void FMetaRoadSelectionController::SetPresetEditorMode()
{
	if (!GUnrealEd)
	{
		return;
	}

	if (RoadSelectionMode != ERoadSelectionMode::Preset)
	{
		SelectionAttributeDescriptor = nullptr;
		RoadSelectionMode = ERoadSelectionMode::Preset;
		// Keep the default spline visualizer so lane/section lines still render; no spline editing is the
		// focus here — the Preset panel edits build settings against the live preview.
		SetComponentVisualizer(MakeShared<FRoadSplineComponentVisualizer>());
	}
}

void FMetaRoadSelectionController::SetBakeEditorMode()
{
	if (!GUnrealEd)
	{
		return;
	}

	if (RoadSelectionMode != ERoadSelectionMode::Bake)
	{
		SelectionAttributeDescriptor = nullptr;
		RoadSelectionMode = ERoadSelectionMode::Bake;
		// Keep the default spline visualizer so lane/section lines still render; the Bake panel only hosts
		// generate/clear actions (no spline editing).
		SetComponentVisualizer(MakeShared<FRoadSplineComponentVisualizer>());
	}
}

void FMetaRoadSelectionController::SetFbxExportEditorMode()
{
	if (!GUnrealEd)
	{
		return;
	}

	if (RoadSelectionMode != ERoadSelectionMode::FbxExport)
	{
		SelectionAttributeDescriptor = nullptr;
		RoadSelectionMode = ERoadSelectionMode::FbxExport;
		// Keep the default spline visualizer so lane/section lines still render; the FBX Export panel only hosts
		// export actions (no spline editing).
		SetComponentVisualizer(MakeShared<FRoadSplineComponentVisualizer>());
	}
}

void FMetaRoadSelectionController::SetVisibilityEditorMode()
{
	if (!GUnrealEd)
	{
		return;
	}

	if (RoadSelectionMode != ERoadSelectionMode::Visibility)
	{
		SelectionAttributeDescriptor = nullptr;
		RoadSelectionMode = ERoadSelectionMode::Visibility;
		// Keep the default spline visualizer; the Visibility panel only hosts editor visibility settings.
		SetComponentVisualizer(MakeShared<FRoadSplineComponentVisualizer>());
	}
}

void FMetaRoadSelectionController::SetSectionEditorMode()
{
	if (!GUnrealEd)
	{
		return;
	}

	if (RoadSelectionMode != ERoadSelectionMode::Section)
	{
		SelectionAttributeDescriptor = nullptr;
		RoadSelectionMode = ERoadSelectionMode::Section;
		SetComponentVisualizer(MakeShared<FRoadSectionComponentVisualizer>());
	}
}

void FMetaRoadSelectionController::SetOffsetEditorMode()
{
	if (!GUnrealEd)
	{
		return;
	}

	if (RoadSelectionMode != ERoadSelectionMode::Offset)
	{
		SelectionAttributeDescriptor = nullptr;
		RoadSelectionMode = ERoadSelectionMode::Offset;
		SetComponentVisualizer(MakeShared<FRoadOffsetComponentVisualizer>());
	}
}

void FMetaRoadSelectionController::SetWidthEditorMode()
{
	if (!GUnrealEd)
	{
		return;
	}

	if (RoadSelectionMode != ERoadSelectionMode::Width)
	{
		SelectionAttributeDescriptor = nullptr;
		RoadSelectionMode = ERoadSelectionMode::Width;
		SetComponentVisualizer(MakeShared<FRoadWidthComponentVisualizer>());
	}
}

void FMetaRoadSelectionController::SetAttributeEditorMode(const TSubclassOf<URoadLaneAttributeDescriptor>& AttributeDescriptor)
{
	if (!GUnrealEd)
	{
		return;
	}

	if (!IsValid(AttributeDescriptor.Get()))
	{
		// Can't enter attribute editing without a valid descriptor — fall back to the default
		// Spline visualizer so a valid visualizer is always registered.
		SetSplineEditorMode();
		return;
	}

	SelectionAttributeDescriptor = AttributeDescriptor;

	if (RoadSelectionMode != ERoadSelectionMode::Attribute)
	{
		RoadSelectionMode = ERoadSelectionMode::Attribute;
		SetComponentVisualizer(MakeShared<FRoadAttributeComponentVisualizer>());
	}

	const TSharedPtr<FRoadAttributeComponentVisualizer> AttributeVisualizer =
		StaticCastSharedPtr<FRoadAttributeComponentVisualizer>(ComponentVisualizer);
	AttributeVisualizer->SetActiveAttributeDescriptor(AttributeDescriptor);

	// Switching attributes keeps the selected section/lane but clears the (attribute-specific) key in place
	// (the visualizer is not recreated), so repaint the viewport to drop the stale attribute-key highlight.
	GEditor->RedrawLevelEditingViewports(true);
}

void FMetaRoadSelectionController::RegisterDefaultVisualizer()
{
	// Ensure the default Spline visualizer is registered. This is the persistent outside-the-mode visualizer
	// (registered at module startup and restored on mode exit); SetSplineEditorMode early-outs if it is already
	// the active sub-mode, so a road-spline point selection made outside the mode survives entering/leaving it.
	SetSplineEditorMode();
}

void FMetaRoadSelectionController::EndSelection()
{
	if (!GUnrealEd)
	{
		return;
	}

	// Reset the mode and suppress the selection broadcast during teardown (mirror SetComponentVisualizer): the
	// visualizer dtor's ResetSelection -> SetCurrentSelectedSpline would otherwise re-enter the Selection panel
	// while the visualizer is already null.
	RoadSelectionMode = ERoadSelectionMode::None;
	SelectionAttributeDescriptor = nullptr;
	{
		TGuardValue<bool> SwapGuard(bUpdatingComponentVisualizer, true);
		GUnrealEd->UnregisterComponentVisualizer(URoadSplineComponent::StaticClass()->GetFName());
		ComponentVisualizer.Reset();
	}

	// Release the selection model (its state objects are no longer borrowed by any live visualizer).
	SelectionModel.Reset();
}

void FMetaRoadSelectionController::DeactivateActiveTool()
{
	UEdMode* EdMode = GLevelEditorModeTools().GetActiveScriptableMode(UMetaRoadEditorMode::EM_MetaRoadEditorModeId);
	if (!EdMode)
	{
		return;
	}

	auto* Context = EdMode->GetInteractiveToolsContext(EToolsContextScope::EdMode);
	if (Context && Context->ToolManager && Context->ToolManager->HasAnyActiveTool())
	{
		Context->EndTool(EToolShutdownType::Cancel);
	}
}

void FMetaRoadSelectionController::SetCurrentSelectedSpline(URoadSplineComponent* InSpline)
{
	if (CurrentSelectedSpline.Get() == InSpline) // resolved-pointer compare → idempotent
	{
		return;
	}
	CurrentSelectedSpline = InSpline;
	if (!bUpdatingComponentVisualizer) // suppressed while swapping visualizers (see SetComponentVisualizer / EndSelection)
	{
		OnSelectedSplineChangedDelegate.Broadcast();
	}
}

UMetaRoadSelectionModel* FMetaRoadSelectionController::GetSelectionModel()
{
	if (!SelectionModel.IsValid())
	{
		SelectionModel.Reset(NewObject<UMetaRoadSelectionModel>(GetTransientPackage(), NAME_None, RF_Transient));
	}
	return SelectionModel.Get();
}

URoadSplineComponent* FMetaRoadSelectionController::GetSelectedSplineForActiveMode()
{
	UMetaRoadSelectionModel* Model = GetSelectionModel();
	switch (RoadSelectionMode)
	{
	case ERoadSelectionMode::Section:
	case ERoadSelectionMode::Width:
	case ERoadSelectionMode::Attribute:
		return Model->GetSectionState()->GetSelectedSpline();
	case ERoadSelectionMode::Offset:
		return Model->GetOffsetState()->GetSelectedSpline();
	case ERoadSelectionMode::Spline:
	{
		const FComponentPropertyPath Path = Model->GetSplineState()->GetSplinePropertyPath();
		return Path.IsValid() ? Cast<URoadSplineComponent>(Path.GetComponent()) : nullptr;
	}
	default:
		return nullptr; // Preset/Bake/FbxExport/Visibility/None — no spline element selection
	}
}

void FMetaRoadSelectionController::SetComponentVisualizer(TSharedRef<FComponentVisualizer> Visualizer)
{
	// Suppress OnSelectedSplineChanged for the whole swap. During the swap RoadSelectionMode is already the NEW
	// sub-mode but ComponentVisualizer is still the OLD/being-destroyed one; a teardown push (ResetSelection ->
	// SetCurrentSelectedSpline) that broadcast now would re-enter the Selection panel and construct the Offset/
	// Spline detail builder against the wrong/null visualizer (its ctor asserts). The sub-mode-change Tick
	// re-points the panel after the swap (reading the live selection from the model).
	TGuardValue<bool> SwapGuard(bUpdatingComponentVisualizer, true);
	GUnrealEd->UnregisterComponentVisualizer(URoadSplineComponent::StaticClass()->GetFName());
	ComponentVisualizer.Reset();
	ComponentVisualizer = Visualizer;
	GUnrealEd->RegisterComponentVisualizer(URoadSplineComponent::StaticClass()->GetFName(), ComponentVisualizer);
	ComponentVisualizer->OnRegister();

	FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor").NotifyCustomizationModuleChanged();

	const bool bComponentSelectionChanged = GEditor->GetSelectedComponentCount() > 0;
	USelection* Selection = bComponentSelectionChanged ? GEditor->GetSelectedComponents() : GEditor->GetSelectedActors();
	if (UTypedElementSelectionSet* SelectionSet = Selection->GetElementSelectionSet())
	{
		SelectionSet->OnChanged().Broadcast(SelectionSet);
	}
	GEditor->NoteSelectionChange();
}
