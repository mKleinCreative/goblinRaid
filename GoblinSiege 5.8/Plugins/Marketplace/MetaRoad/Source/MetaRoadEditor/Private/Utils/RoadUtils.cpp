/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */


#include "Utils/RoadUtils.h"
#include "EngineUtils.h"
#include "MetaRoadSettings.h"
#include "MetaRoadEditorSettings.h"
#include "MetaRoadActor.h"
#include "PrimitiveDrawInterface.h"
#include "DynamicMeshBuilder.h"
#include "SceneView.h"
#include "MetaRoadViewMatricesCompat.h"
#include "Materials/MaterialInterface.h"
#include "Editor/EditorEngine.h"
#include "RoadSplineComponent.h"
#include "Kismet2/ComponentEditorUtils.h"

namespace RoadUtils
{

void DrawTriangle(class FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B, const FVector& C, const FColor& VertexColor, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriorityGroup)
{
	FVector2f UVs[4] =
	{
		FVector2f(0,0),
		FVector2f(0,1),
		FVector2f(1,1),
		FVector2f(1,0),
	};

	FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());

	FVector3f Normal = FVector3f(0, 0, 1);
	FVector3f Tangent = FVector3f(1, 0, 0);

	MeshBuilder.AddVertex(FDynamicMeshVertex((FVector3f)A, Tangent, Normal, UVs[0], VertexColor));
	MeshBuilder.AddVertex(FDynamicMeshVertex((FVector3f)B, Tangent, Normal, UVs[1], VertexColor));
	MeshBuilder.AddVertex(FDynamicMeshVertex((FVector3f)C, Tangent, Normal, UVs[2], VertexColor));

	MeshBuilder.AddTriangle(0, 1, 2);
	MeshBuilder.Draw(PDI, FMatrix::Identity, MaterialRenderProxy, DepthPriorityGroup, false, false);

	//PDI->DrawLine(A, B, FColor::Yellow, DepthPriorityGroup, 1.f);
	//PDI->DrawLine(A, C, FColor::Yellow, DepthPriorityGroup, 1.f);
	//PDI->DrawLine(B, C, FColor::Yellow, DepthPriorityGroup, 1.f);
}

void DrawRoadLaneConnection(float Size, bool bIsSuccessor, bool bReverse, const FTransform & InTransform, const FColor& VertexColor, class FPrimitiveDrawInterface* PDI, const FSceneView* View, uint8 DepthPriorityGroup)
{
	static const float Width = Size;
	static const float Height = Size;

	const float Shift = bIsSuccessor ? -Width : 0.0;

	FVector A{ Shift , -Height / 2.0, 0.0 };
	FVector B{ Shift , +Height / 2.0, 0.0 };
	FVector C{ Width + Shift, 0.0, 0.0 };

	FTransform Transform = InTransform;

	if (bReverse)
	{
		Transform.SetRotation(MetaRoad::InvertRotation(InTransform.GetRotation()));
	}

	const double ViewScale = View->WorldToScreen(Transform.GetLocation()).W * (4.0f / View->UnscaledViewRect.Width() / MetaRoad::ViewCompat::GetViewToClip(View->ViewMatrices).M[0][0]);

	auto* Mat = GetDefault<UMetaRoadEditorSettings>()->UIVertexColorMaterial.Get();
	check(Mat);

	DrawTriangle(
		PDI,
		Transform.TransformPosition(A * ViewScale),
		Transform.TransformPosition(B * ViewScale),
		Transform.TransformPosition(C * ViewScale),
		VertexColor,
		Mat->GetRenderProxy(),
		DepthPriorityGroup);
}

TSet<TWeakObjectPtr<const ULaneConnection>> CaptureConnections(
	const URoadConnection* SrcConnection, 
	const FViewCameraState& CameraState, 
	double MaxViewDistance, 
	double MaxOrthoWidth, 
	TFunction<bool(const ULaneConnection*)> IsConnectionAllowed)
{

	TSet<TWeakObjectPtr<const ULaneConnection>> ObservedConnections;

	if (CameraState.bIsOrthographic && CameraState.OrthoWorldCoordinateWidth > MaxOrthoWidth)
	{
		return {};
	}


	auto TryAddConnection = [&](ULaneConnection* ProbeConnection)
	{
		check(ProbeConnection);
		if (!IsConnectionAllowed(ProbeConnection))
		{
			return false;
		}

		if (!SrcConnection->CanConnectTo(ProbeConnection))
		{
			return false;
		}

		FTransform Transform = ProbeConnection->EvalTransform(0.5, ESplineCoordinateSpace::World);
		if (!CameraState.bIsOrthographic)
		{
			if ((Transform.GetLocation() - CameraState.ViewPosition).Length() > MaxViewDistance)
			{
				return false;
			}
		}

		FVector2D ScreenPos;
		if (!FSceneView::ProjectWorldToScreen(Transform.GetLocation(), CameraState.ViewRect, CameraState.ViewToProj, ScreenPos))
		{
			return false;
		}

		if (ScreenPos.X < CameraState.ViewRect.Min.X || ScreenPos.X > CameraState.ViewRect.Max.X || ScreenPos.Y < CameraState.ViewRect.Min.Y || ScreenPos.Y > CameraState.ViewRect.Max.Y)
		{
			return false;
		}

		ObservedConnections.Emplace(ProbeConnection);
		return true;
	};


	for (TActorIterator<AMetaRoad> It(SrcConnection->GetWorld()); It; ++It)
	{
		It->ForEachComponent<URoadSplineComponent>(true, [&](const URoadSplineComponent* Comp)
		{
			for (auto& Section : Comp->GetLaneSections())
			{
				for (auto& Lane : Section.Left)
				{
					TryAddConnection(Lane.PredecessorConnection);
					TryAddConnection(Lane.SuccessorConnection);
				}
				for (auto& Lane : Section.Right)
				{
					TryAddConnection(Lane.PredecessorConnection);
					TryAddConnection(Lane.SuccessorConnection);
				}
			}
		});
	}

	//UE_LOG(LogMetaRoad, Log, TEXT("Captured %i connections"), NumCaptured);

	return ObservedConnections;
}


	void FitLanesWidthToEndConnection(URoadSplineComponent* TargetSpline, const ULaneConnection* EndLaneConnection)
	{
		if (!EndLaneConnection)
		{
			return;
		}

		if (TargetSpline->GetRoadLayout().Sections.Num() == 0)
		{
			return;
		}

		auto GetEndWidth = [EndLaneConnection](int Shfit, float& OutWidth)
		{
			check(Shfit != 0);

			const auto& BaseLayout = EndLaneConnection->GetOwnedRoadSpline()->GetRoadLayout();
			const auto& BaseLane = EndLaneConnection->GetOwnedRoadLane();
			const auto& [LeftLanes, RightLanes] = BaseLayout.GetLeftRighLanes(BaseLane.IsForwardLane() ? BaseLane.GetStartSectionIndex() : BaseLane.GetEndSectionIndex());
			const int BaseLaneIndex = BaseLane.GetLaneIndex();

			if (!BaseLane.IsForwardLane())
			{
				Shfit = -Shfit;
			}

			const int LaneIndex = Shfit > 0 
				? GetRightOf(BaseLaneIndex, Shfit - 1)
				: GetLeftOf(BaseLaneIndex, -Shfit - 1);

			if (LaneIndex == 0)
			{
				return false;
			}
			if (LaneIndex > 0 && LaneIndex > RightLanes.Num())
			{
				return false;
			}
			if (LaneIndex < 0 && -LaneIndex > LeftLanes.Num())
			{
				return false;
			}

			const auto& Lane = LaneIndex > 0 ? RightLanes[LaneIndex - 1] : LeftLanes[-LaneIndex - 1];

			const double SOffset = BaseLane.IsForwardLane()
				? Lane.GetStartOffset() 
				: Lane.GetEndOffset() - Lane.GetStartOffset();

			OutWidth = Lane.Width.Eval(SOffset);
			return true;
		};

		const auto& [LeftSection, RightSection] = TargetSpline->GetRoadLayout().GetLeftRighSections(TargetSpline->GetRoadLayout().Sections.Num() - 1);

		for (int LaneIndex = -LeftSection.Left.Num(); LaneIndex <= RightSection.Right.Num(); ++LaneIndex)
		{
			if (LaneIndex == MetaRoad::ZeroLaneIndex)
			{
				continue;
			}
			auto& Lane = LaneIndex > 0 ? RightSection.Right[LaneIndex - 1] : LeftSection.Left[-LaneIndex - 1];
			check(Lane.Width.Keys.Num() > 0);

			float DesireWidth;
			if (GetEndWidth(LaneIndex, DesireWidth))
			{
				float SOffset = Lane.GetEndOffset() - Lane.GetStartOffset();
				const float LastKeyWidth = Lane.Width.Eval(SOffset);
				if (FMath::Abs(LastKeyWidth - DesireWidth) > UE_KINDA_SMALL_NUMBER)
				{
					if (Lane.Width.GetNumKeys() > 1)
					{
						auto& Key = Lane.Width.Keys.Last();
						Key.Value = DesireWidth;
						Key.Time = SOffset;
						Key.InterpMode = ERichCurveInterpMode::RCIM_Cubic;
						Key.TangentMode = ERichCurveTangentMode::RCTM_Auto;

					}
					else
					{
						Lane.Width.AddKey(SOffset, DesireWidth);
						Lane.Width.Keys[1].InterpMode = ERichCurveInterpMode::RCIM_Cubic;
						Lane.Width.Keys[1].TangentMode = ERichCurveTangentMode::RCTM_Auto;
					}
				}
			}
		}
	}

	void FitLanesWidthToBeginConnection(URoadSplineComponent* TargetSpline, const ULaneConnection* EndLaneConnection)
	{
		if (!EndLaneConnection)
		{
			return;
		}

		if (TargetSpline->GetRoadLayout().Sections.Num() == 0)
		{
			return;
		}

		auto GetBeginWidth = [EndLaneConnection](int Shfit, float& OutWidth)
		{
			check(Shfit != 0);

			const auto& BaseLayout = EndLaneConnection->GetOwnedRoadSpline()->GetRoadLayout();
			const auto& BaseLane = EndLaneConnection->GetOwnedRoadLane();
			const auto& [LeftLanes, RightLanes] = BaseLayout.GetLeftRighLanes(BaseLane.IsForwardLane() ? BaseLane.GetEndSectionIndex() : BaseLane.GetStartSectionIndex());
			const int BaseLaneIndex = BaseLane.GetLaneIndex();

			if (!BaseLane.IsForwardLane())
			{
				Shfit = -Shfit;
			}

			const int LaneIndex = Shfit > 0 
				? GetRightOf(BaseLaneIndex, Shfit - 1)
				: GetLeftOf(BaseLaneIndex, -Shfit - 1);

			if (LaneIndex == 0)
			{
				return false;
			}
			if (LaneIndex > 0 && LaneIndex > RightLanes.Num())
			{
				return false;
			}
			if (LaneIndex < 0 && -LaneIndex > LeftLanes.Num())
			{
				return false;
			}

			const auto& Lane = LaneIndex > 0 ? RightLanes[LaneIndex - 1] : LeftLanes[-LaneIndex - 1];

			const double SOffset = BaseLane.IsForwardLane()
				? Lane.GetEndOffset() - Lane.GetStartOffset() 
				: Lane.GetStartOffset();

			OutWidth = Lane.Width.Eval(SOffset);
			return true;
		};

		const auto& [LeftSection, RightSection] = TargetSpline->GetRoadLayout().GetLeftRighSections(0);

		for (int LaneIndex = -LeftSection.Left.Num(); LaneIndex <= RightSection.Right.Num(); ++LaneIndex)
		{
			if (LaneIndex == MetaRoad::ZeroLaneIndex)
			{
				continue;
			}
			auto& Lane = LaneIndex > 0 ? RightSection.Right[LaneIndex - 1] : LeftSection.Left[-LaneIndex - 1];
			check(Lane.Width.Keys.Num() > 0);

			float DesireWidth;
			if (GetBeginWidth(LaneIndex, DesireWidth))
			{
				const float FirstKeyWidth = Lane.Width.Eval(0);
				if (FMath::Abs(FirstKeyWidth - DesireWidth) > UE_KINDA_SMALL_NUMBER)
				{
					if (Lane.Width.GetNumKeys())
					{
						auto& Key = Lane.Width.Keys[0];
						Key.Value = DesireWidth;
						Key.Time = 0;
						Key.InterpMode = ERichCurveInterpMode::RCIM_Cubic;
						Key.TangentMode = ERichCurveTangentMode::RCTM_Auto;

					}
				}
			}
		}
	}

	AMetaRoad* SpawnRoadActor(UWorld* World, const FTransform& Tranform)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = TEXT("Spline");
		SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		SpawnParams.OverrideLevel = World->GetCurrentLevel();
		AMetaRoad* NewActor = World->SpawnActor<AMetaRoad>(AMetaRoad::StaticClass(), Tranform, SpawnParams);
		FActorLabelUtilities::SetActorLabelUnique(NewActor, TEXT("RoadActor"));
		return NewActor;
	}

	URoadSplineComponent* CreateSplineInActor(AActor* Actor, bool bTransact, bool bSetAsRoot)
	{
		if (!ensure(Actor)) return nullptr;
		if (bTransact) Actor->Modify();

		FName Name = *FComponentEditorUtils::GenerateValidVariableName(
			URoadSplineComponent::StaticClass(), Actor);
		URoadSplineComponent* Spline = NewObject<URoadSplineComponent>(
			Actor, URoadSplineComponent::StaticClass(), Name,
			bTransact ? RF_Transactional : RF_NoFlags);

		if (bSetAsRoot) Actor->SetRootComponent(Spline);
		else            Spline->SetupAttachment(Actor->GetRootComponent());

		Spline->OnComponentCreated();
		Actor->AddInstanceComponent(Spline);
		Spline->RegisterComponent();
		Spline->ResetRelativeTransform();
		Actor->PostEditChange();
		return Spline;
	}

	void CopySplineToSpline(const URoadSplineComponent& Source,
	                        URoadSplineComponent& Destination, bool bTransact)
	{
		if (bTransact) Destination.Modify();

		Destination.ClearSplinePoints();
		Destination.bSplineHasBeenEdited = true;

		const int32 N = Source.GetNumberOfSplinePoints();
		for (int32 i = 0; i < N; ++i)
		{
			Destination.AddSplinePoint(
				Source.GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World),
				ESplineCoordinateSpace::World, false);
			Destination.SetUpVectorAtSplinePoint(i,
				Source.GetUpVectorAtSplinePoint(i, ESplineCoordinateSpace::World),
				ESplineCoordinateSpace::World, false);
			Destination.SetTangentsAtSplinePoint(i,
				Source.GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::World),
				Source.GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::World),
				ESplineCoordinateSpace::World, false);
			Destination.SetSplinePointType(i, Source.GetSplinePointType(i), false);
		}
		Destination.SetClosedLoop(Source.IsClosedLoop(), false);
		Destination.UpdateSpline();
	}

} // RoadUtils