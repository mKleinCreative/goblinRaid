/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "RoadProfileFactory.generated.h"

UCLASS()
class URoadProfileFactory : public UFactory 
{
    GENERATED_BODY()

public:
    URoadProfileFactory(const FObjectInitializer& ObjectInitializer);

public:
    virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
    virtual bool CanCreateNew() const override;
};
