/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "MetaRoadSettings.h"
#include "Engine/Texture.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Styling/StyleColors.h"
#include "EngineUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#endif

#define LOCTEXT_NAMESPACE "UMetaRoadSettings"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadSettings)

#if WITH_EDITOR
static void RedrawAllRoadSplines()
{
	if (GEditor)
	{
		for (TActorIterator<AActor> It(GEditor->GetEditorWorldContext().World()); It; ++It)
		{
			TArray<URoadSplineComponent*> Components;
			It->GetComponents(Components);
			for (auto* Component : Components)
			{
				Component->MarkRenderStateDirty();
			}
		}
	}
}
#endif

UMetaRoadSettings::UMetaRoadSettings(const FObjectInitializer& ObjectInitializer)
#if WITH_EDITOR
	: Super(ObjectInitializer)
#endif
{
#if WITH_EDITOR
	// The preview dynamic materials now live on UMetaRoadEditorSettings. Here we only seed the per-zone
	// EditorMaterial with the shared editor material paths (same style as FRoadZoneTypeDetails() default ctor).
	const FLinearColor NormalDrive(FColor(0, 96, 153));
#endif

	RoadZoneTypes.Add(ERoadZoneTypes::Driving.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		NormalDrive, 
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLane.M_DirectLane"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/MI_DriveSurface.MI_DriveSurface"))).LoadSynchronous(),
		nullptr,
		0,
		false,
		TEXT("Describes a 'normal' drivable road that is not one of the other types")
#endif
	});

	RoadZoneTypes.Add(ERoadZoneTypes::Sidewalk.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		FLinearColor(FColor(33, 82, 53)),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLane.M_DirectLane"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/M_Sidewolk.M_Sidewolk"))).LoadSynchronous(),
		nullptr,
		1,
		false,
		TEXT("Describes a lane on which pedestrians can walk")
#endif
	});

	RoadZoneTypes.Add(ERoadZoneTypes::Shoulder.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		FLinearColor(0.0f, 0.049f, 0.13f),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLaneGrid.M_DirectLaneGrid"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/MI_DriveSurface.MI_DriveSurface"))).LoadSynchronous(),
		nullptr,
		1,
		false,
		TEXT("Describes a soft border at the edge of the road")
#endif
	});

	RoadZoneTypes.Add(ERoadZoneTypes::Border.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		FLinearColor(0.0f, 0.049f, 0.13f),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLaneGrid.M_DirectLaneGrid"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/MI_DriveSurface.MI_DriveSurface"))).LoadSynchronous(),
		nullptr,
		1,
		false,
		TEXT("Describes a hard border at the edge of the road. It has the same height as the drivable lane")
#endif
	});

	RoadZoneTypes.Add(ERoadZoneTypes::Stop.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		FLinearColor(0.0f, 0.049f, 0.13f),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLaneGrid.M_DirectLaneGrid"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/MI_DriveSurface.MI_DriveSurface"))).LoadSynchronous(),
		nullptr,
		1,
		false,
		TEXT("Hard shoulder on motorways for emergency stops")
#endif
	});

	RoadZoneTypes.Add(ERoadZoneTypes::Median.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		FLinearColor(0.0f, 0.049f, 0.13f),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLaneGrid.M_DirectLaneGrid"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/MI_DriveSurface.MI_DriveSurface"))).LoadSynchronous(),
		nullptr,
		0,
		false,
		TEXT("Describes a lane that sits between driving lanes that lead in opposite directions. It is typically used to separate traffic in towns on large roads")
#endif
	});

	RoadZoneTypes.Add(ERoadZoneTypes::Parking.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		FLinearColor(0.13f, 0.068f, 0.123f),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLane.M_DirectLane"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/MI_DriveSurface.MI_DriveSurface"))).LoadSynchronous(),
		nullptr,
		1,
		false,
		TEXT("Describes a lane with parking spaces")
#endif
	});

	RoadZoneTypes.Add(ERoadZoneTypes::Biking.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		FLinearColor(0.254f, 0.231f, 0.0f),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLane.M_DirectLane"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/MI_Biking.MI_Biking"))).LoadSynchronous(),
		nullptr,
		1,
		false,
		TEXT("Describes a lane that is reserved for cyclists")
#endif
	});

	RoadZoneTypes.Add(ERoadZoneTypes::Restricted.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		FLinearColor(0.0f, 0.0f, 0.224f),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLaneGrid.M_DirectLaneGrid"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/MI_Restricted.MI_Restricted"))).LoadSynchronous(),
		nullptr,
		1,
		false,
		TEXT("Describes a lane on which cars should not drive. The lane has the same height as drivable lanes. Typically, the lane is separated with lines and often contains dotted lines as well")
#endif
	});

	RoadZoneTypes.Add(ERoadZoneTypes::Marking.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		FLinearColor(1.0, 1.0, 1.0),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLane.M_DirectLane"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/M_SurfaceMark.M_SurfaceMark"))).LoadSynchronous(),
		nullptr,
		3,
		true,
		TEXT("Marking zones on asphalt created by a separate Looped Road Spine. Typically used for arrows")
#endif
	});

	RoadZoneTypes.Add(ERoadZoneTypes::Tram.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		FLinearColor(0.092f, 0.019f, 0.0f),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLane.M_DirectLane"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/MI_Tram.MI_Tram"))).LoadSynchronous(),
		nullptr,
		3,
		false,
		TEXT("")
#endif
	});

	RoadZoneTypes.Add(ERoadZoneTypes::Rail.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		FLinearColor(0.092f, 0.019f, 0.0f),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLane.M_DirectLane"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/MI_Tram.MI_Tram"))).LoadSynchronous(),
		nullptr,
		3,
		false,
		TEXT("")
#endif
	});

	RoadZoneTypes.Add(ERoadZoneTypes::Bus.GetName(), FRoadZoneTypeDetails{
#if WITH_EDITOR
		FLinearColor(0.043f, 0.034f, 0.023f),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/EditorAssets/Materials/M_DirectLane.M_DirectLane"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Materials/MI_Bus.MI_Bus"))).LoadSynchronous(),
		nullptr,
		3,
		false,
		TEXT("")
#endif

	});

#if WITH_EDITOR
	RedrawAllRoadSplines();
#endif
}


#if WITH_EDITOR

FText UMetaRoadSettings::GetSectionText() const
{
	return LOCTEXT("MetaRoadEditorSettings_Section", "MetaRoad");
}

FText UMetaRoadSettings::GetSectionDescription() const
{
	return LOCTEXT("MetaRoadEditorSettings_Description", "MetaRoad Settings");
}

void UMetaRoadSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	FProperty* Property = PropertyChangedEvent.Property;
	FProperty* MemberProperty = nullptr;
	if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode())
	{
		MemberProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
	}

	if (MemberProperty && Property)
	{
		if (MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaRoadSettings, RoadZoneTypes))
		{
			if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(FRoadZoneTypeDetails, EditorColor))
			{
				for (auto& It : RoadZoneTypes)
				{
					It.Value.UpdateEditorColor();
				}
			}
			else
			{
				for (auto& It : RoadZoneTypes)
				{
					It.Value.ResetEditorMaterial();
				}
			}

			RedrawAllRoadSplines();

			//MetaRoadDelegates::OnRoadZoneTypesChanged.Broadcast();
			//GEditor->RedrawLevelEditingViewports(true);
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
