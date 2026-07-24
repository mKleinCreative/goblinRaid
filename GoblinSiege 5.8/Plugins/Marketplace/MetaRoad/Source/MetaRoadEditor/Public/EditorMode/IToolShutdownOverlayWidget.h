
/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/Interface.h"
#include "Toolkits/BaseToolkit.h"
#include "IToolShutdownOverlayWidget.generated.h"

class SWidget;

UINTERFACE(MinimalAPI)
class UToolShutdownOverlayWidget : public UInterface
{
	GENERATED_BODY()
};


class METAROADEDITOR_API IToolShutdownOverlayWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> MakeShutdownOverlayWidget(const TWeakPtr<FModeToolkit>& InWeakToolkit) = 0;
	virtual const FSlateBrush* GetToolIcon(const TWeakPtr<FModeToolkit>& InWeakToolkit);
};
