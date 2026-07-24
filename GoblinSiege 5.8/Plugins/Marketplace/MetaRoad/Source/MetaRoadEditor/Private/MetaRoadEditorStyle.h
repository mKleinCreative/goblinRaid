/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Styling/SlateStyle.h"


class FMetaRoadEditorStyle
    : public FSlateStyleSet
{
public:
	static FMetaRoadEditorStyle& Get();

protected:
	friend class FMetaRoadEditorModule;

	static void Register();
	static void Unregister();

private:
	FMetaRoadEditorStyle();
};
