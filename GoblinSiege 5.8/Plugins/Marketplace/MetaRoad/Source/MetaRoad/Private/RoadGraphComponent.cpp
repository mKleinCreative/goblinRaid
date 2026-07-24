/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadGraphComponent.h"

#if WITH_EDITOR
#include "ZoneGraphObjectCRC32.h"
#endif

URoadGraphDataComponent::FOnRegisterComponentChanged URoadGraphDataComponent::OnComponentRegistredDelegate{};
URoadGraphDataComponent::FOnRegisterComponentChanged URoadGraphDataComponent::OnComponentUnregistredDelegate{};

URoadGraphDataComponent::URoadGraphDataComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
uint32 URoadGraphDataComponent::GetShapeHash()
{
	FZoneGraphObjectCRC32 Archive;
	const uint32 ShapeHash = Archive.Crc32(const_cast<UObject*>((const UObject*)this), 0);

	//const FTransform Transform = GetComponentTransform();
	//const uint32 TransformHash = GetTypeHash(Transform);

	//return HashCombine(ShapeHash, TransformHash);

	return ShapeHash;


}
#endif

void URoadGraphDataComponent::OnRegister()
{
	Super::OnRegister();

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		OnComponentRegistredDelegate.Broadcast(this);
	}
}

void URoadGraphDataComponent::OnUnregister()
{
	Super::OnUnregister();

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		OnComponentUnregistredDelegate.Broadcast(this);
	}
}