
/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "EditorMode/IToolShutdownOverlayWidget.h"
#include "ToolHostCustomizationAPI.h"
#include "ToolShutdownOverlayWidgetDefault.generated.h"

class SWidget;

UINTERFACE(MinimalAPI)
class UToolShutdownOverlayWidgetDefault : public UToolShutdownOverlayWidget
{
	GENERATED_BODY()
};


class METAROADEDITOR_API IToolShutdownOverlayWidgetDefault: public IToolShutdownOverlayWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> MakeShutdownOverlayWidget(const TWeakPtr<FModeToolkit>& InWeakToolkit);

	//virtual bool RequestAcceptCancelButtonOverride(IToolHostCustomizationAPI::FAcceptCancelButtonOverrideParams& Params);
	//virtual bool RequestCompleteButtonOverride(IToolHostCustomizationAPI::FCompleteButtonOverrideParams& Params);
	//virtual void ClearButtonOverrides();

private:
	TOptional<IToolHostCustomizationAPI::FAcceptCancelButtonOverrideParams> AcceptCancelButtonParams;
	TOptional<IToolHostCustomizationAPI::FCompleteButtonOverrideParams> CompleteButtonParams;
	bool bCurrentOverrideButtonsWereClicked = false;

	FReply HandleAcceptCancelClick(bool bAccept);
	FReply HandleCompleteClick();

	TWeakPtr<FModeToolkit> WeakToolkit;
};
