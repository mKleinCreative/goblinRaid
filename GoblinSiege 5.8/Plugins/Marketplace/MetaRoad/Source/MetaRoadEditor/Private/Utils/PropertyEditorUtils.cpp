/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Utils/PropertyEditorUtils.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

void PropertyEditorUtils::AddTextRow(IDetailChildrenBuilder& ChildrenBuilder, const FText& Search, const FText & Name, const FText& Value)
{
	ChildrenBuilder.AddCustomRow(Search)
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
			.Text(Name)
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
			.Text(Value)
		];
}

void PropertyEditorUtils::AddTextRow(IDetailChildrenBuilder& ChildrenBuilder, const FText& Search, const FText& Name, TFunction<FText()>&& Value)
{
	ChildrenBuilder.AddCustomRow(Search)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(Name)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Text_Lambda(MoveTemp(Value))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}
