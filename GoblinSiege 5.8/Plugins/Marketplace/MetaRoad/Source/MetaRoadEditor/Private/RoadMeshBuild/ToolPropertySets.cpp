/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ToolPropertySets.h"
#include "Engine/CollisionProfile.h"
#include "MetaRoadSettings.h"

URoadSurfaceToolProperties::URoadSurfaceToolProperties(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BodyInstance.SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

}

URoadDecalToolProperties::URoadDecalToolProperties(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}


URoadSidewalkToolProperties::URoadSidewalkToolProperties(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BodyInstance.SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
}

URoadCertbToolProperties::URoadCertbToolProperties(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BodyInstance.SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

}

URoadMarkToolProperties::URoadMarkToolProperties(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

ULoftingToolProperties::ULoftingToolProperties(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BodyInstance.SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
}

namespace
{
	TMap<FName, TObjectPtr<UMaterialInterface>> MakeZoneTypeMaterialsMap(
		const TMap<FRoadZoneType, TObjectPtr<UMaterialInterface>>& OverrideMaterials,
		bool bUseDecal = false)
	{
		auto& ZoneTypes = GetDefault<UMetaRoadSettings>()->RoadZoneTypes;
		TMap<FName, TObjectPtr<UMaterialInterface>> Ret;
		for (auto& ZoneType : ZoneTypes)
		{
			if (auto* Override = OverrideMaterials.Find(ZoneType.Key))
				Ret.Add(ZoneType.Key, *Override);
			else
				Ret.Add(ZoneType.Key, bUseDecal ? ZoneType.Value.DecalMaterial : ZoneType.Value.MeshMaterial);
		}
		return Ret;
	}
}

TMap<FName, TObjectPtr<UMaterialInterface>> URoadSurfaceToolProperties::GetMaterialsMap() const
{
	return MakeZoneTypeMaterialsMap(OverrideMaterials);
}

TMap<FName, TObjectPtr<UMaterialInterface>> URoadDecalToolProperties::GetMaterialsMap() const
{
	return MakeZoneTypeMaterialsMap(OverrideMaterials, /*bUseDecal=*/true);
}

TMap<FName, TObjectPtr<UMaterialInterface>> URoadSidewalkToolProperties::GetMaterialsMap() const
{
	return MakeZoneTypeMaterialsMap(OverrideMaterials);
}

TMap<FName, TObjectPtr<UMaterialInterface>> URoadCertbToolProperties::GetMaterialsMap() const
{
	return {};
}

TMap<FName, TObjectPtr<UMaterialInterface>> URoadMarkToolProperties::GetMaterialsMap() const
{
	return {};
}

TMap<FName, TObjectPtr<UMaterialInterface>> ULoftingToolProperties::GetMaterialsMap() const
{
	return {};
}

