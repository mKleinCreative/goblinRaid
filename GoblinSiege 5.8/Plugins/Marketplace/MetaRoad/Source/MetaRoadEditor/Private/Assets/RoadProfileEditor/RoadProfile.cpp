#include "Assets/RoadProfile.h"
#include "RoadSplineComponent.h"

bool FRoadLaneAttributeProfile::AssignFrom(const TSoftClassPtr<URoadLaneAttributeDescriptor>& InDescriptor, const FRoadLaneAttribute& InAttribute)
{
	if (InAttribute.Keys.IsEmpty())
	{
		return false;
	}

	UClass* Class = InDescriptor.LoadSynchronous();
	if (!Class)
	{
		return false;
	}
	auto* DefObject = Class->GetDefaultObject<URoadLaneAttributeDescriptor>();
	check(DefObject);

	const auto& DefAttributeValueTemplate = DefObject->GetAttributeValueTemplate();
	if (!DefAttributeValueTemplate.IsValid())
	{
		return false;
	}

	if (DefAttributeValueTemplate.GetScriptStruct() != InAttribute.Keys[0].Value.GetScriptStruct())
	{
		return false;
	}

	AttributeDesctiptor = InDescriptor;
	AttributeValueTemplate = InAttribute.Keys[0].Value;

	return true;
}

FRoadLaneProfile::FRoadLaneProfile(const FRoadLane& FromLane)
{
	Width = 0;
	for (auto& Key : FromLane.Width.Keys)
	{
		Width = FMath::Max(Width, Key.Value);
	}
	if (Width < 1.0)
	{
		Width = MetaRoad::DefaultRoadLaneWidth;
	}

	RoadZone = FromLane.RoadZone;

	for (auto& [Key, Value] : FromLane.Attributes)
	{
		FRoadLaneAttributeProfile Attribute;
		if (Attribute.AssignFrom(Key, Value))
		{
			Attributes.Add(MoveTemp(Attribute));
		}
	}

	Direction = FromLane.Direction;
	bSkipProceduralGeneration = FromLane.bSkipProceduralGeneration;
}

void URoadProfile::AssignToRoadSpline(URoadSplineComponent* TargetSpline) const
{
	if (!IsValid(TargetSpline))
	{
		return;
	}

	TargetSpline->GetLaneSections().Empty();

	static auto CreateAttributes = [](const TSet<FRoadLaneAttributeProfile>& Src)
	{
		TMap<TSoftClassPtr<URoadLaneAttributeDescriptor>, FRoadLaneAttribute> NewAttributes;
		for (auto& Profile : Src)
		{
			if (Profile.IsProfileValid())
			{
				FRoadLaneAttribute NewAttribute;
				NewAttribute.SetScriptStruct(Profile.AttributeValueTemplate.GetScriptStruct());
				NewAttribute.UpdateOrAddTypedKey(0.0, Profile.AttributeValueTemplate.GetMemory(), Profile.AttributeValueTemplate.GetScriptStruct());
				NewAttributes.Add(Profile.AttributeDesctiptor, MoveTemp(NewAttribute));
			}
		}
		return NewAttributes;
	};

	static auto CreateLane = [](const FRoadLaneProfile& Src)
	{
		FRoadLane NewLane{};
		NewLane.Attributes = CreateAttributes(Src.Attributes);
		NewLane.Width.AddKey(0, Src.Width);
		NewLane.Width.Keys[0].InterpMode = ERichCurveInterpMode::RCIM_Cubic;
		NewLane.Width.Keys[0].TangentMode = ERichCurveTangentMode::RCTM_Auto;
		NewLane.bSkipProceduralGeneration = Src.bSkipProceduralGeneration;
		NewLane.Direction = Src.Direction;
		NewLane.RoadZone = Src.RoadZone;
		return NewLane;
	};

	auto& NewSection = TargetSpline->GetLaneSections().Add_GetRef({});
	for (auto& LaneProfile : Left)
	{
		NewSection.Left.Add(CreateLane(LaneProfile));
	}
	for (auto& LaneProfile : Right)
	{
		NewSection.Right.Add(CreateLane(LaneProfile));
	}
	NewSection.Attributes = CreateAttributes(CenterAttributes);
	TargetSpline->GetRoadLayout().Direction = Direction;
	TargetSpline->UpdateRoadLayout();
}

bool URoadProfile::AssignFromRoadSection(const FRoadLayout& Layout, int SectionIndex)
{
	if (!Layout.Sections.IsValidIndex(SectionIndex))
	{
		return false;
	}

	Left.Empty();
	Right.Empty();
	CenterAttributes.Empty();

	const auto& [SrcLeft, SrcRight] = Layout.GetLeftRighLanes(SectionIndex);

	for (auto& It : SrcLeft)
	{
		Left.Emplace(It);
	}

	for (auto& It : SrcRight)
	{
		Right.Emplace(It);
	}

	for (auto& [Key, Value] : Layout.Sections[SectionIndex].Attributes)
	{
		FRoadLaneAttributeProfile Attribute;
		if (Attribute.AssignFrom(Key, Value))
		{
			CenterAttributes.Add(MoveTemp(Attribute));
		}
	}

	Direction = Layout.Direction;

	return true;
}

FRoadLaneProfile* URoadProfile::GetLaneByIndex(int32 LaneIndex)
{
	if (LaneIndex > 0)
	{
		return Right.IsValidIndex(LaneIndex - 1) ? &Right[LaneIndex - 1] : nullptr;
	}
	if (LaneIndex < 0)
	{
		return Left.IsValidIndex(-LaneIndex - 1) ? &Left[-LaneIndex - 1] : nullptr;
	}
	return nullptr; // center / ZeroLaneIndex
}

const FRoadLaneProfile* URoadProfile::GetLaneByIndex(int32 LaneIndex) const
{
	return const_cast<URoadProfile*>(this)->GetLaneByIndex(LaneIndex);
}

int32 URoadProfile::AddLane(int32 LaneIndex, bool bOnLeft)
{
	// Mirrors FRoadSectionComponentVisualizer::OnAddLane, but on FRoadLaneProfile (Left[]/Right[]).
	FRoadLaneProfile NewLane{};
	if (const FRoadLaneProfile* Selected = GetLaneByIndex(LaneIndex))
	{
		NewLane.RoadZone = Selected->RoadZone;
		NewLane.Direction = Selected->Direction;
		if (Selected->Width > UE_KINDA_SMALL_NUMBER)
		{
			NewLane.Width = Selected->Width;
		}
	}
	else
	{
		// Adding from the center reference line: a default driving lane.
		NewLane.RoadZone.InitializeAs<FRoadZoneDriving>();
		NewLane.Width = MetaRoad::DefaultRoadLaneWidth;
	}

	int32 NewSelectedLaneIndex = MetaRoad::ZeroLaneIndex;

	if (LaneIndex == MetaRoad::ZeroLaneIndex)
	{
		if (bOnLeft)
		{
			Left.Insert(MoveTemp(NewLane), 0);
			NewSelectedLaneIndex = -1;
		}
		else
		{
			Right.Insert(MoveTemp(NewLane), 0);
			NewSelectedLaneIndex = 1;
		}
	}
	else if (LaneIndex > 0)
	{
		Right.Insert(MoveTemp(NewLane), LaneIndex - 1 + (bOnLeft ? 0 : 1));
		NewSelectedLaneIndex = LaneIndex + (bOnLeft ? 0 : 1);
	}
	else // LaneIndex < 0
	{
		Left.Insert(MoveTemp(NewLane), -LaneIndex - 1 + (bOnLeft ? 1 : 0));
		NewSelectedLaneIndex = LaneIndex - (bOnLeft ? 1 : 0);
	}

	return NewSelectedLaneIndex;
}

int32 URoadProfile::DeleteLane(int32 LaneIndex)
{
	if (LaneIndex > 0 && Right.IsValidIndex(LaneIndex - 1))
	{
		Right.RemoveAt(LaneIndex - 1);
	}
	else if (LaneIndex < 0 && Left.IsValidIndex(-LaneIndex - 1))
	{
		Left.RemoveAt(-LaneIndex - 1);
	}
	return MetaRoad::ZeroLaneIndex;
}

void URoadProfile::ReverseLane(int32 LaneIndex)
{
	if (FRoadLaneProfile* Lane = GetLaneByIndex(LaneIndex))
	{
		Lane->Direction = (Lane->Direction == ERoadLaneDirection::Default)
			? ERoadLaneDirection::Invert
			: ERoadLaneDirection::Default;
	}
}