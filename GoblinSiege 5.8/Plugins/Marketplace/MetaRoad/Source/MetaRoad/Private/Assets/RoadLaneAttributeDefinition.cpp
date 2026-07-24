/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Assets/RoadLaneAttributeDescriptor.h"
#include "Textures/SlateIcon.h"
#include "Utils/DrawUtils.h"
#include "RoadSplineComponent.h"
#include "MetaRoadSettings.h"
#include "PrimitiveDrawInterface.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Styling/SlateIconFinder.h"
#endif

#define LOCTEXT_NAMESPACE "RoadAttributeProfile"

#if WITH_EDITOR

static void DrawLaneBorder(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, int SectionIndex, int LaneIndex, const FRoadLaneAttribute& Attribute, double S0, double S1, const FColor& Color1, const FColor& Color2, uint8 DepthPriorityGroup, float Thickness, float DepthBias, bool bScreenSpace)
{
	const auto& Section = SplineComp->GetLaneSection(SectionIndex);

	double StartS, EndS;
	if (LaneIndex == MetaRoad::ZeroLaneIndex)
	{
		StartS = Section.SOffset;
		EndS = Section.SOffsetEnd_Cashed;
	}
	else
	{
		auto& Lane = Section.GetLaneByIndex(LaneIndex);
		StartS = Lane.GetStartOffset();
		EndS = Lane.GetEndOffset();
	}

	const int NumPointPerSegmaent = GetDefault<UMetaRoadSettings>()->NumPointPerSegmaent;
	const int NumPointPerSection = GetDefault<UMetaRoadSettings>()->NumPointPerSection;

	TInstancedStruct<FRoadLaneAttributeValue> TempAttribute;

	TArray<FSplinePositionLinearApproximation> Points;
	SplineComp->BuildLinearApproximation(
		Points,
		[&](double S)
		{
			Attribute.Evaluate(S - StartS, TempAttribute);
			return SplineComp->EvalAttributeAnchorROffset(SectionIndex, LaneIndex, S, TempAttribute.Get());
		},
		StartS, EndS, NumPointPerSegmaent, NumPointPerSection, ESplineCoordinateSpace::World);

	if (!FMath::IsNearlyEqual(S0, StartS) || !FMath::IsNearlyEqual(S1, EndS))
	{
		DrawUtils::TrimPoints(SplineComp->SplineCurves.ReparamTable.Eval(S0, 0.0f), SplineComp->SplineCurves.ReparamTable.Eval(S1, 0.0f), Points);
	}

	for (int32 StepIdx = 1; StepIdx < Points.Num(); StepIdx++)
	{
		if (StepIdx % 2)
		{
			PDI->DrawTranslucentLine(Points[StepIdx - 1].Position, Points[StepIdx].Position, Color1, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
		}
		else
		{
			PDI->DrawTranslucentLine(Points[StepIdx - 1].Position, Points[StepIdx].Position, Color2, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
		}
	}
}

#endif // WITH_EDITOR

URoadLaneAttributeDescriptor::FOnAttributeBeginDestroyDelegate URoadLaneAttributeDescriptor::OnAttributeBeginDestroy;

void URoadLaneAttributeDescriptor::BeginDestroy()
{
	OnAttributeBeginDestroy.Broadcast(this);
	Super::BeginDestroy();
}

TConstStructView<FRoadLaneAttributeValue> URoadLaneAttributeDescriptor::GetAttributeValueTemplate() const 
{ 
	static FRoadLaneAttributeValue Dummy; 
	return Dummy; 
}

#if WITH_EDITOR

TPair<double, double> URoadLaneAttributeDescriptor::FAttributeDrawParams::GetRang() const
{
	const auto& Section = Spline->GetLaneSection(SectionIndex);
	const auto& AttributeKey = Attribute->Keys[AttributeIndex];

	double S0, S1;

	if (LaneIndex == MetaRoad::ZeroLaneIndex)
	{
		S0 = Section.SOffset + AttributeKey.SOffset;
		S1 = (AttributeIndex < (Attribute->Keys.Num() - 1)
			? Section.SOffset + Attribute->Keys[AttributeIndex + 1].SOffset
			: Section.SOffsetEnd_Cashed);
	}
	else
	{
		const auto& Lane = Section.GetLaneByIndex(LaneIndex);
		S0 = Lane.GetStartOffset() + Attribute->Keys[AttributeIndex].SOffset;
		S1 = (AttributeIndex < (Attribute->Keys.Num() - 1)
			? Section.SOffset + Attribute->Keys[AttributeIndex + 1].SOffset
			: Lane.GetEndOffset());
	}

	return { S0, S1 };
}

FText URoadLaneAttributeDescriptor::GetDisplayName() const 
{ 
	if (!LabelOverride.IsEmpty())
	{
		return LabelOverride;
	}

	if (!GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		if (const UPackage* Package = GetPackage())
		{
			const FString FileName = FPaths::GetBaseFilename(Package->GetLoadedPath().GetPackageName());
			if (!FileName.IsEmpty())
			{
				return FText::FromString(*FileName);
			}
		}
	}

	return GetClass()->GetDisplayNameText();
}

FText URoadLaneAttributeDescriptor::GetAssetDisplayName() const
{
	return GetDisplayName(); // LOCTEXT("RoadLaneAttributeDescriptor_AssetDisplayName", "Attribute Profile");
}
 
FText URoadLaneAttributeDescriptor::GetToolTip() const 
{ 
	return ToolTipOverride.IsEmpty() ? GetClass()->GetToolTipText() : ToolTipOverride;
}

FSlateIcon URoadLaneAttributeDescriptor::GetIcon() const 
{ 
	return FSlateIconFinder::FindIconForClass(GetClass());
}

bool URoadLaneAttributeDescriptor::CanBeAdded() const 
{ 
	return IsInBlueprint(); 
}

void URoadLaneAttributeDescriptor::Draw(const FAttributeDrawParams& Params) const
{

	const auto& Section = Params.Spline->GetLaneSection(Params.SectionIndex);
	const auto& AttributeKey = Params.Attribute->Keys[Params.AttributeIndex];
	const auto [S0, S1] = Params.GetRang();

	FColor Color = GetDefaultColor();

	if (Params.bIsAttributeSelected)
	{
		Color = FMetaRoadColors::SelectedColor;
	}
	else if (Params.bLowAccent)
	{
		Color = DrawUtils::MakeLowAccent(Color).ToFColor(true);
	}

	DrawLaneBorder(Params.PDI, Params.Spline, Params.SectionIndex, Params.LaneIndex, *Params.Attribute, S0, S1, Color, Color, SDPG_Foreground, 4.0, 0, true);
}

FColor URoadLaneAttributeDescriptor::GetDefaultColor() const
{
	return FMetaRoadColors::AccentColorHi;
}

void URoadLaneAttributeDescriptor::DrawKey(const FAttributeDrawParams& Params) const
{
	const auto& AttributeKey = Params.Attribute->Keys[Params.AttributeIndex];
	const auto [S0, S1] = Params.GetRang();
	const auto* Value = AttributeKey.GetValuePtr<FRoadLaneAttributeValue>();

	const float GrabHandleSize = 14.0f;// +GetDefault<ULevelEditorViewportSettings>()->SelectedSplinePointSizeAdjustment;

	const FColor Color = Params.bIsAttributeSelected ? FMetaRoadColors::SelectedColor : GetDefaultColor();

	FVector Pos;
	if (Value)
	{
		Pos = Params.Spline->EvalAttributeAnchorPosition(Params.SectionIndex, Params.LaneIndex, S0, *Value, ESplineCoordinateSpace::World).Location;
	}
	else
	{
		// No value: fall back to lane centre (Alpha is ignored for ZeroLaneIndex -> R=0).
		Pos = Params.Spline->EvalLanePoistion(Params.SectionIndex, Params.LaneIndex, S0, 0.5, ESplineCoordinateSpace::World);
	}
	Params.PDI->DrawPoint(Pos, Color, GrabHandleSize, SDPG_Foreground);
}

#endif

#undef LOCTEXT_NAMESPACE