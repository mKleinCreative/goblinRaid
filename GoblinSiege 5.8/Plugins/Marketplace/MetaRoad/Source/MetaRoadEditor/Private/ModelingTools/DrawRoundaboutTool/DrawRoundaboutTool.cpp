/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ModelingTools/DrawRoundaboutTool.h"
#include "AssetSelection.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "CoreGlobals.h"
#include "SceneView.h"
#include "MetaRoadViewMatricesCompat.h"
#include "Drawing/PreviewGeometryActor.h"
#include "Editor.h"
#include "EditorMode/MetaRoadEditorMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolsContext.h"
#include "MetaRoadModule.h"
#include "MetaRoadActor.h" // AMetaRoad (SpawnRoadActor return type)
#include "RoadSplineComponent.h"
#include "SceneManagement.h"
#include "Selection.h"
#include "Selection/ToolSelectionUtil.h"
#include "SPrimaryButton.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "ToolSceneQueriesUtil.h"
#include "Utils/RoadUtils.h"
#include "Widgets/Images/SImage.h"
#define LOCTEXT_NAMESPACE "UDrawRoundaboutTool"

// ---------------------------------------------------------------------------
// UDrawRoundaboutToolProperties
// ---------------------------------------------------------------------------

UDrawRoundaboutToolProperties::UDrawRoundaboutToolProperties()
{
	DrawProfile = TSoftObjectPtr<URoadProfile>(FSoftObjectPath(
		TEXT("/MetaRoad/MetaRoad/Profiles/RoadProfiles/2_Lanes+Borders_Coodirect.2_Lanes+Borders_Coodirect")))
		.LoadSynchronous();
}

// ---------------------------------------------------------------------------
// UDrawRoundaboutToolBuilder
// ---------------------------------------------------------------------------

bool UDrawRoundaboutToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return true;
}

UInteractiveTool* UDrawRoundaboutToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UDrawRoundaboutTool* Tool = NewObject<UDrawRoundaboutTool>(SceneState.ToolManager);
	Tool->SetWorld(SceneState.World);
	return Tool;
}

// ---------------------------------------------------------------------------
// UDrawRoundaboutTool
// ---------------------------------------------------------------------------

void UDrawRoundaboutTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

void UDrawRoundaboutTool::Setup()
{
	UInteractiveTool::Setup();

	Settings = NewObject<UDrawRoundaboutToolProperties>(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);

	SetToolDisplayName(LOCTEXT("ToolName", "Draw Roundabout"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("ToolDesc", "Click and drag to set the roundabout center and radius."),
		EToolMessageLevel::UserNotification);

	DragBehavior = NewObject<UClickDragInputBehavior>();
	DragBehavior->Initialize(this);
	AddInputBehavior(DragBehavior);

	CreatePreviewActor();
}

void UDrawRoundaboutTool::CreatePreviewActor()
{
	ITransaction* UndoState = GUndo;
	GUndo = nullptr;
	ON_SCOPE_EXIT{ GUndo = UndoState; };

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.ObjectFlags = RF_Transient;
	PreviewActor = TargetWorld->SpawnActor<APreviewGeometryActor>(
		FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);

	USceneComponent* Root = NewObject<USceneComponent>(PreviewActor);
	PreviewActor->AddOwnedComponent(Root);
	PreviewActor->SetRootComponent(Root);
	Root->RegisterComponent();

	WorkingSpline = RoadUtils::CreateSplineInActor(PreviewActor);
	WorkingSpline->bSplineHasBeenEdited = true;
}

void UDrawRoundaboutTool::Shutdown(EToolShutdownType ShutdownType)
{
	Settings->SaveProperties(this);

	if (ShutdownType == EToolShutdownType::Accept && bSplineReady && WorkingSpline.IsValid())
		GenerateAsset();

	if (WorkingSpline.IsValid()) WorkingSpline->DestroyComponent();
	if (PreviewActor)            PreviewActor->Destroy();

	PreviewActor  = nullptr;
	WorkingSpline = nullptr;

	Super::Shutdown(ShutdownType);
}

bool UDrawRoundaboutTool::CanAccept() const
{
	return bSplineReady && WorkingSpline.IsValid();
}

// ---------------------------------------------------------------------------
// Raycast
// ---------------------------------------------------------------------------

bool UDrawRoundaboutTool::Raycast(const FRay& WorldRay, FVector& OutLocation) const
{
	if (Settings->bHitWorld)
	{
		FHitResult Hit;
		if (ToolSceneQueriesUtil::FindNearestVisibleObjectHit(this, Hit, WorldRay))
		{
			OutLocation = Hit.ImpactPoint + FVector::UpVector * Settings->ClickOffset;
			return true;
		}
	}

	if (Settings->bHitGroundPlane && !FMath::IsNearlyZero(WorldRay.Direction.Z))
	{
		const double t = -WorldRay.Origin.Z / WorldRay.Direction.Z;
		if (t > 0.0)
		{
			OutLocation    = WorldRay.Origin + WorldRay.Direction * t;
			OutLocation.Z += Settings->ClickOffset;
			return true;
		}
	}

	return false;
}

// ---------------------------------------------------------------------------
// Input behaviors
// ---------------------------------------------------------------------------

FInputRayHit UDrawRoundaboutTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FVector HitLoc;
	if (Raycast(PressPos.WorldRay, HitLoc))
		return FInputRayHit(static_cast<float>((HitLoc - PressPos.WorldRay.Origin).Size()));
	return FInputRayHit();
}

void UDrawRoundaboutTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	FVector HitLoc;
	if (!Raycast(PressPos.WorldRay, HitLoc)) return;
	CircleCenter     = HitLoc;
	Settings->Radius = 0.0;
	bIsDragging      = true;
}

void UDrawRoundaboutTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	FVector HitLoc;
	if (!Raycast(DragPos.WorldRay, HitLoc)) return;
	Settings->Radius = FVector::Dist(CircleCenter, HitLoc);
	if (Settings->Radius > KINDA_SMALL_NUMBER)
	{
		// Rotate node 0 to follow the cursor: compute angle from center→cursor in XY plane.
		const FVector2D Dir = FVector2D(HitLoc.X - CircleCenter.X, HitLoc.Y - CircleCenter.Y).GetSafeNormal();
		Settings->Rotation  = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		RebuildCircleSpline();
	}
}

void UDrawRoundaboutTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	FVector HitLoc;
	if (Raycast(ReleasePos.WorldRay, HitLoc))
	{
		Settings->Radius = FVector::Dist(CircleCenter, HitLoc);
		if (Settings->Radius > KINDA_SMALL_NUMBER)
		{
			const FVector2D Dir = FVector2D(HitLoc.X - CircleCenter.X, HitLoc.Y - CircleCenter.Y).GetSafeNormal();
			Settings->Rotation  = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		}
	}

	if (bIsDragging && Settings->Radius > KINDA_SMALL_NUMBER)
	{
		RebuildCircleSpline();
		bSplineReady = true;
	}
	bIsDragging = false;
}

void UDrawRoundaboutTool::OnTerminateDragSequence()
{
	bIsDragging = false;
}

void UDrawRoundaboutTool::OnPropertyModified(UObject* /*PropertySet*/, FProperty* /*Property*/)
{
	if (bSplineReady && WorkingSpline.IsValid())
		RebuildCircleSpline();
}

// ---------------------------------------------------------------------------
// Circle spline geometry
// ---------------------------------------------------------------------------

void UDrawRoundaboutTool::RebuildCircleSpline()
{
	URoadSplineComponent* Spline = WorkingSpline.Get();
	if (!Spline) return;

	const int32  N = FMath::Clamp(Settings->NumPoints, 2, 16);
	const double R = Settings->Radius;

	Spline->ClearSplinePoints(false);
	Spline->bSplineHasBeenEdited = true;

	// Hermite tangent length for a perfect circle: 4*tan(π/(2N))*R.
	// Derived the same way CalcTangentMultiplier() works in RoadSplineComponent.cpp —
	// it adjusts the tangent so the cubic midpoint (t=0.5) lies exactly on the circle.
	// Works for all N≥2 without special cases (N=2: tan(π/4)=1 → len=4R).
	const double TangentLen = 4.0 * FMath::Tan(UE_DOUBLE_PI / (2.0 * N)) * R;
	const double Dir        = Settings->bClockwise ? 1.0 : -1.0;

	for (int32 i = 0; i < N; ++i)
	{
		const double Angle = Dir * 2.0 * UE_DOUBLE_PI * i / N
		                   + FMath::DegreesToRadians(Settings->Rotation);
		const FVector Pos  = CircleCenter + FVector(R * FMath::Cos(Angle),
		                                            R * FMath::Sin(Angle), 0.0);

		// Tangent direction is the derivative of position w.r.t. angle, scaled by Dir.
		const FVector TangentDir = FVector(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0) * Dir;
		const FVector Tangent    = TangentDir * TangentLen;

		Spline->AddSplinePoint(Pos, ESplineCoordinateSpace::World, false);
		Spline->SetTangentsAtSplinePoint(i, Tangent, Tangent,
		                                 ESplineCoordinateSpace::World, false);
		Spline->SetSplinePointType(i, ESplinePointType::CurveCustomTangent, false);
	}

	Spline->SetClosedLoop(true, false);
	Spline->UpdateSpline();

	ApplyProfileToSpline(Spline);
}

void UDrawRoundaboutTool::ApplyProfileToSpline(URoadSplineComponent* Spline) const
{
	if (IsValid(Settings->DrawProfile))
		Settings->DrawProfile->AssignToRoadSpline(Spline);
	Spline->GetRoadLayout().LoopedRoadZone = Settings->LoopedRoadZone;
	Spline->MarkRenderStateDirty();
}

// ---------------------------------------------------------------------------
// Render — wireframe preview during drag
// ---------------------------------------------------------------------------

void UDrawRoundaboutTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (!bIsDragging || Settings->Radius <= KINDA_SMALL_NUMBER) return;

	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	const int32  N = FMath::Clamp(Settings->NumPoints, 2, 16);
	const double R = Settings->Radius;

	for (int32 i = 0; i < N; ++i)
	{
		const double A0 = 2.0 * UE_DOUBLE_PI * i / N;
		const double A1 = 2.0 * UE_DOUBLE_PI * (i + 1) / N;
		PDI->DrawLine(
			CircleCenter + FVector(R * FMath::Cos(A0), R * FMath::Sin(A0), 0.0),
			CircleCenter + FVector(R * FMath::Cos(A1), R * FMath::Sin(A1), 0.0),
			FLinearColor::Yellow, SDPG_Foreground, 1.5f);
	}
	PDI->DrawPoint(CircleCenter, FLinearColor(0.1f, 1.0f, 0.2f), 12.f, SDPG_Foreground);
}

// ---------------------------------------------------------------------------
// DrawHUD — 2D canvas overlay during drag
// ---------------------------------------------------------------------------

void UDrawRoundaboutTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
    if (!bIsDragging || Settings->Radius <= KINDA_SMALL_NUMBER || !Canvas) return;

    const FSceneView* View = RenderAPI->GetSceneView();
    if (!View) return;

    const FIntRect VR  = View->UnscaledViewRect;
    const float    DPI = Canvas->GetDPIScale();

    // ProjectWorldToScreen returns window-global physical pixels (includes VR.Min).
    // Subtract VR.Min → viewport-local physical px. Divide by DPI → canvas logical px.
    auto Project = [&](const FVector& WorldPos, FVector2D& OutPixel) -> bool
    {
        FVector2D ScreenPos;
        if (!FSceneView::ProjectWorldToScreen(WorldPos, VR, MetaRoad::ViewCompat::GetWorldToClip(View->ViewMatrices), ScreenPos))
            return false;
        OutPixel = (ScreenPos - FVector2D(VR.Min)) / DPI;
        return true;
    };

    const double RotRad = FMath::DegreesToRadians(Settings->Rotation);
    const FVector Tip = CircleCenter + FVector(
        Settings->Radius * FMath::Cos(RotRad),
        Settings->Radius * FMath::Sin(RotRad), 0.0);

    FVector2D CenterPx, TipPx;
    if (!Project(CircleCenter, CenterPx) || !Project(Tip, TipPx)) return;

    const FLinearColor LineColor(1.f, 0.85f, 0.f);

    FCanvasLineItem Shaft(CenterPx, TipPx);
    Shaft.SetColor(LineColor);
    Shaft.LineThickness = 1.5f;
    Canvas->DrawItem(Shaft);

    FCanvasBoxItem Dot(TipPx - FVector2D(3.f, 3.f), FVector2D(6.f, 6.f));
    Dot.SetColor(LineColor);
    Canvas->DrawItem(Dot);

    FCanvasTextItem TextItem(CenterPx + FVector2D(8.f, -16.f),
        FText::FromString(FString::Printf(TEXT("R: %.1f m"), Settings->Radius * 0.01)),
        GEngine->GetSmallFont(), FLinearColor::White);
    TextItem.EnableShadow(FLinearColor::Black, FVector2D(1.f, 1.f));
    Canvas->DrawItem(TextItem);
}

// ---------------------------------------------------------------------------
// GenerateAsset
// ---------------------------------------------------------------------------

void UDrawRoundaboutTool::GenerateAsset()
{
	GetToolManager()->BeginUndoTransaction(LOCTEXT("DrawRoundaboutTx", "Draw Roundabout"));

	auto CreateSplineOnActor = [this](AActor* TargetActor) -> URoadSplineComponent*
	{
		URoadSplineComponent* Out = RoadUtils::CreateSplineInActor(TargetActor, /*bTransact=*/ true);
		Out->GetRoadLayout() = WorkingSpline->GetRoadLayout();
		Out->GetRoadLayout().UpdateLayout(Out);
		RoadUtils::CopySplineToSpline(*WorkingSpline, *Out, /*bTransact=*/ true);
		Out->PostEditChange();
		return Out;
	};

	URoadSplineComponent* OutputSpline = nullptr;

	if (bShutdownReqAddToSelectedActor)
	{
		// "Add to Selected Actor" only targets AMetaRoad (splines on other actors are ignored).
		TArray<AMetaRoad*> Selected;
		GEditor->GetSelectedActors()->GetSelectedObjects<AMetaRoad>(Selected);
		if (Selected.Num() > 0)
			OutputSpline = CreateSplineOnActor(Selected[0]);
	}
	else if (Settings->bCreateBlueprint
	         && Settings->BlueprintToCreate.IsValid()
	         && Settings->BlueprintToCreate->GeneratedClass
	         && !Settings->BlueprintToCreate->GeneratedClass->HasAnyClassFlags(
	                CLASS_NotPlaceable | CLASS_Abstract))
	{
		if (AActor* NewActor = FActorFactoryAssetProxy::AddActorForAsset(
		        Settings->BlueprintToCreate.Get(), /*bSelectActors=*/ false))
		{
			OutputSpline = CreateSplineOnActor(NewActor);
			NewActor->RerunConstructionScripts();
		}
	}
	else
	{
		AActor* NewActor = RoadUtils::SpawnRoadActor(TargetWorld.Get(), FTransform(CircleCenter));
		OutputSpline = CreateSplineOnActor(NewActor);
	}

	if (OutputSpline)
		ToolSelectionUtil::SetNewActorSelection(GetToolManager(),
		                                        OutputSpline->GetAttachmentRootActor());

	GetToolManager()->EndUndoTransaction();
}

// ---------------------------------------------------------------------------
// MakeShutdownOverlayWidget
// ---------------------------------------------------------------------------

TSharedRef<SWidget> UDrawRoundaboutTool::MakeShutdownOverlayWidget(
	const TWeakPtr<FModeToolkit>& InWeakToolkit)
{
	auto Toolkit = InWeakToolkit.Pin();
	check(Toolkit);

	TWeakObjectPtr<UDrawRoundaboutTool> WeakThis(this);

	auto ToolShutdownViewportOverlayWidget = SNew(SHorizontalBox)

	+SHorizontalBox::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(0.0f, 0.0f, 0.f, 15.f))
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
		.Padding(8.f)
		[
			SNew(SHorizontalBox)

			// Tool icon
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(SImage)
				.Image_Lambda([WeakThis, InWeakToolkit]() -> const FSlateBrush*
				{
					return WeakThis.IsValid() ? WeakThis->GetToolIcon(InWeakToolkit) : nullptr;
				})
			]
			// Tool name
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(STextBlock)
				.Text(Toolkit->GetActiveToolDisplayName())
			]
			// Create New Actor
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.0, 0.f, 2.f, 0.f))
			[
				SNew(SPrimaryButton)
				.Text(LOCTEXT("Overlay_NewActor", "Create New Actor"))
				.ToolTipText(LOCTEXT("Overlay_NewActor_Tooltip", "Create a new actor and draw a new RoadSplineComponent inside"))
				.OnClicked_Lambda([WeakThis, InWeakToolkit]()
				{
					if (WeakThis.IsValid()) WeakThis->bShutdownReqAddToSelectedActor = false;
					InWeakToolkit.Pin()->GetScriptableEditorMode()
						->GetInteractiveToolsContext(EToolsContextScope::EdMode)
						->EndTool(EToolShutdownType::Accept);
					return FReply::Handled();
				})
			]
			// Add to Selected Actor
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.0, 0.f, 2.f, 0.f))
			[
				SNew(SPrimaryButton)
				.Text(LOCTEXT("Overlay_AddToActor", "Add to Selected Actor"))
				.ToolTipText(LOCTEXT("Overlay_AddToActor_Tooltip", "Draw a new RoadSplineComponent inside a selected actor"))
				.OnClicked_Lambda([WeakThis, InWeakToolkit]()
				{
					if (WeakThis.IsValid()) WeakThis->bShutdownReqAddToSelectedActor = true;
					InWeakToolkit.Pin()->GetScriptableEditorMode()
						->GetInteractiveToolsContext(EToolsContextScope::EdMode)
						->EndTool(EToolShutdownType::Accept);
					return FReply::Handled();
				})
				.IsEnabled_Lambda([]()
				{
					// "Add to Selected" only targets AMetaRoad (Accept uses GetSelectedObjects<AMetaRoad>).
					TArray<AMetaRoad*> Sel;
					GEditor->GetSelectedActors()->GetSelectedObjects<AMetaRoad>(Sel);
					for (AMetaRoad* A : Sel)
						if (A->GetComponentByClass<URoadSplineComponent>()) return true;
					return false;
				})
			]
			// Cancel
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(2.0, 0.f, 0.f, 0.f))
			[
				SNew(SButton)
				.Text(LOCTEXT("Overlay_Cancel", "Cancel"))
				.ToolTipText(LOCTEXT("Overlay_Cancel_Tooltip", "Cancel the active Tool [Esc]"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([InWeakToolkit]()
				{
					InWeakToolkit.Pin()->GetScriptableEditorMode()
						->GetInteractiveToolsContext(EToolsContextScope::EdMode)
						->EndTool(EToolShutdownType::Cancel);
					return FReply::Handled();
				})
			]
		]
	];

	return ToolShutdownViewportOverlayWidget;
}

#undef LOCTEXT_NAMESPACE
