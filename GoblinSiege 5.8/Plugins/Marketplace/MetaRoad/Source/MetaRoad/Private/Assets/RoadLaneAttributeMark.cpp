/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Assets/RoadLaneAttributeMark.h"
#include "Assets/RoadMarkProfile.h"
#include "Textures/SlateIcon.h"
#include "Utils/DrawUtils.h"
#include "RoadSplineComponent.h"

#define LOCTEXT_NAMESPACE "RoadLaneAttributeMark"

FRoadLaneMark::FRoadLaneMark()
{
	// Store only the default preset path -- never load an asset in a struct constructor.
	// FInstancedStruct re-constructs a throwaway instance during package save; loading there is
	// fatal in UE 5.8 (StaticFindObjectFast) and makes serialization non-deterministic. The asset
	// is resolved lazily on the game thread (FMarksOp::PreloadProfiles) before the build reads it.
	Profile = FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Profiles/Marks/solid_150.solid_150"));
}

const TInstancedStruct<FRoadLaneMarkProfile>& FRoadLaneMark::GetProfile() const
{
	static TInstancedStruct<FRoadLaneMarkProfile> Dummy;
	if (ProfileSource != ERoadLaneMarkProfile::UsePreset)
	{
		return CustomProfile;
	}
	// On the game thread (editor draw / authoring) load synchronously; off the game thread (the mesh
	// build worker) only .Get() -- LoadSynchronous is unsafe there, and FMarksOp::PreloadProfiles has
	// already made the asset resident by then.
	const URoadMarkProfile* ResolvedProfile = IsInGameThread() ? Profile.LoadSynchronous() : Profile.Get();
	return ResolvedProfile ? ResolvedProfile->LaneMarkProfiles : Dummy;
}

TInstancedStruct<FRoadLaneMarkProfile>& FRoadLaneMark::GetProfile()
{
	static TInstancedStruct<FRoadLaneMarkProfile> Dummy;
	if (ProfileSource != ERoadLaneMarkProfile::UsePreset)
	{
		return CustomProfile;
	}
	URoadMarkProfile* ResolvedProfile = IsInGameThread() ? Profile.LoadSynchronous() : Profile.Get();
	return ResolvedProfile ? ResolvedProfile->LaneMarkProfiles : Dummy;
}

#if WITH_EDITOR

namespace Local
{
	struct FMarkDrawStyle
	{
		FColor Color;
		bool bIsBroken;

		FMarkDrawStyle operator | (const FMarkDrawStyle& Other) const
		{
			return FMarkDrawStyle(Color, bIsBroken | Other.bIsBroken);
		}
	};

	static FMarkDrawStyle GetStyle(const TInstancedStruct<FRoadLaneMarkProfile>& Profile)
	{
		if (auto* AsSolid = Profile.GetPtr<FRoadLaneMarkProfileSolid>())
		{
			return { AsSolid->VertexColor, false };
		}
		if (auto* AsBroked = Profile.GetPtr<FRoadLaneMarkProfileBroked>())
		{
			return { AsBroked->VertexColor, true };
		}
		if (auto* AsDouble = Profile.GetPtr<FRoadLaneMarkProfileDouble>())
		{
			return GetStyle(AsDouble->Left) | GetStyle(AsDouble->Right);
		}
		return { FMetaRoadColors::AccentColorHi, false };
	}
}

void URoadLaneAttributeMarkDescriptor::Draw(const FAttributeDrawParams& Params) const
{
	const auto& Section = Params.Spline->GetLaneSection(Params.SectionIndex);

	const auto& AttributeKey = Params.Attribute->Keys[Params.AttributeIndex];
	const auto* AttributeValue = AttributeKey.GetValuePtr<FRoadLaneMark>();
	const auto [S0, S1] = Params.GetRang();

	if (AttributeValue)
	{
		auto DrawStyle = Local::GetStyle(AttributeValue->GetProfile());

		// Override white color
		//DrawStyle.Color = GetKeyColor();

		if(Params.bIsAttributeSelected)
		{
			DrawStyle.Color = FMetaRoadColors::SelectedColor;
		}
		else if (Params.bLowAccent)
		{
			DrawStyle.Color = DrawUtils::MakeLowAccent(DrawStyle.Color).ToFColor(true);
		}

		FColor Color2 = DrawStyle.Color;
		if (DrawStyle.bIsBroken)
		{
			Color2.A = 0;
		}
		
		DrawUtils::DrawLaneBorder(Params.PDI, Params.Spline, Params.SectionIndex, Params.LaneIndex, S0, S1, DrawStyle.Color, Color2, SDPG_Foreground, 4.0, 0, true);
	}
	
}

#endif

#undef LOCTEXT_NAMESPACE