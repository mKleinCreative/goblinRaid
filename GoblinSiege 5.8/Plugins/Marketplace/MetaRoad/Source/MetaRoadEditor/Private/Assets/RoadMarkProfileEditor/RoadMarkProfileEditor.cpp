/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMarkProfileEditor.h"
#include "Assets/RoadMarkProfile.h"
#include "Assets/RoadLaneAttributeMark.h"
#include "SSingleObjectDetailsPanel.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Docking/SDockTab.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
//#include "SPixelMappingSurface.h"

#define LOCTEXT_NAMESPACE "RoadMarkProfileEditor"

namespace ERoadMarkProfileEditorTabs
{
    // Tab identifiers
    static const FName DetailsID(TEXT("Details"));
    static const FName ViewportID(TEXT("Viewport"));
};

class SRoadMarkProfilePropertiesTabBody : public SSingleObjectDetailsPanel
{
public:
	SLATE_BEGIN_ARGS(SRoadMarkProfilePropertiesTabBody) {}
	SLATE_END_ARGS()

private:
	// Pointer back to owning sprite editor instance (the keeper of state)
	TWeakPtr<class FRoadMarkProfileEditor> RoadMarkProfileEditorPtr;
public:
	void Construct(const FArguments& InArgs, TSharedPtr<FRoadMarkProfileEditor> RoadMarkProfileEditor)
	{
		RoadMarkProfileEditorPtr = RoadMarkProfileEditor;

		SSingleObjectDetailsPanel::Construct(SSingleObjectDetailsPanel::FArguments().HostCommandList(RoadMarkProfileEditor->GetToolkitCommands()).HostTabManager(RoadMarkProfileEditor->GetTabManager()), /*bAutomaticallyObserveViaGetObjectToObserve=*/ true, /*bAllowSearch=*/ true);
	}

	// SSingleObjectDetailsPanel interface
	virtual UObject* GetObjectToObserve() const override
	{
		return RoadMarkProfileEditorPtr.Pin()->GetWorkingAsset();
	}

	virtual TSharedRef<SWidget> PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget) override
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				PropertyEditorWidget
			];
	}
	// End of SSingleObjectDetailsPanel interface
};

class SRoadMarkProfilePreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRoadMarkProfilePreview)
		: _RoadMarkProfile(nullptr)
		{
		}
		SLATE_ATTRIBUTE(URoadMarkProfile*, RoadMarkProfile)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		RoadMarkProfile = InArgs._RoadMarkProfile;
	}

	// SWidget interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		OnPaintBackground(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);
		++LayerId;

		LayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
		++LayerId;

		const auto LocalSize = AllottedGeometry.GetLocalSize();
		const float RoadWidth = 365 * 2;
		const float CurbWidth = 20;

		const FColor RoadColor(70, 70, 70);
		const FColor CurnColor(50, 50, 50);

		const FSlateBrush* WhiteBrush = FAppStyle::GetBrush(TEXT("WhiteBrush"));

		// Draw road
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(RoadWidth * CmToPixel, LocalSize.Y),
				FSlateLayoutTransform({ LocalSize.X * 0.5 - RoadWidth * CmToPixel * 0.5 , 0.0 })),
			WhiteBrush,
			ESlateDrawEffect::None,
			RoadColor
		);

		// Draw left curb
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(CurbWidth * CmToPixel, LocalSize.Y),
				FSlateLayoutTransform({ LocalSize.X * 0.5 - RoadWidth * CmToPixel * 0.5 - CurbWidth * CmToPixel , 0.0 })),
			WhiteBrush,
			ESlateDrawEffect::None,
			CurnColor
		);

		// Draw right curb
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(CurbWidth * CmToPixel, LocalSize.Y),
				FSlateLayoutTransform({ LocalSize.X * 0.5 + RoadWidth * CmToPixel * 0.5 , 0.0 })),
			WhiteBrush,
			ESlateDrawEffect::None,
			CurnColor
		);

		const float TapeLeftOffset = 50;
		const float TapeBottomOffset = 50;

		TArray<FVector2D> Points{ 
			{LocalSize.X * 0.5 - RoadWidth * CmToPixel * 0.5, LocalSize.Y - TapeBottomOffset - 10},
			{LocalSize.X * 0.5 - RoadWidth * CmToPixel * 0.5, LocalSize.Y - TapeBottomOffset},
			{LocalSize.X * 0.5, LocalSize.Y - TapeBottomOffset},
			{LocalSize.X * 0.5, LocalSize.Y - TapeBottomOffset - 10},
		};

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId, 
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			FLinearColor::White * 0.65f,
			false
		);

		const FString DrawString = FString::Printf(TEXT("%i cm"), (int)(RoadWidth * 0.5));
		const FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
		const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FVector2D TextSize = FontMeasureService->Measure(DrawString, SmallLayoutFont);

		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(FVector2D{ LocalSize.X * 0.5 - RoadWidth * CmToPixel * 0.25 - TextSize.X * 0.5, LocalSize.Y - TapeBottomOffset + 3 })),
			DrawString,
			SmallLayoutFont,
			ESlateDrawEffect::None,
			FLinearColor::White * 0.65f
		);

		FReadScopeLock Lock(ProfileRWLock);
		if (!DrawSolidOrBrokedLane(AllottedGeometry, OutDrawElements, LayerId, ProfileToDraw, 0.0))
		{
			if (auto* AsDouble = ProfileToDraw.GetPtr<FRoadLaneMarkProfileDouble>())
			{
				DrawSolidOrBrokedLane(AllottedGeometry, OutDrawElements, LayerId, AsDouble->Left,  -AsDouble->Gap * 0.5);
				DrawSolidOrBrokedLane(AllottedGeometry, OutDrawElements, LayerId, AsDouble->Right, +AsDouble->Gap * 0.5);
			}
		}

		return LayerId;
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		//SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		const float Scale = 1 / 2000.0f;

		auto LocalSize = AllottedGeometry.GetLocalSize();
		CmToPixel = LocalSize.Y * Scale * ZoomLevels[ZoomLevel];

		if (!RoadMarkProfile.IsSet())
		{
			return;
		}

		URoadMarkProfile* Profile = RoadMarkProfile.Get();
		if (!IsValid(Profile))
		{
			return;
		}

		FWriteScopeLock Lock(ProfileRWLock);
		ProfileToDraw = Profile->LaneMarkProfiles;

	}

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		const int32 ZoomLevelDelta = FMath::FloorToInt(MouseEvent.GetWheelDelta());
		ZoomLevel = FMath::Clamp(int32(ZoomLevel + ZoomLevelDelta), int32(0), ZoomLevels.Num() - 1);
		return FReply::Handled();
	}

	void DrawSolidLane(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, uint32 Layer, const FRoadLaneMarkProfileSolid& Profile, const float LeftOffset) const
	{
		const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteTexture");
		auto LocalSize = AllottedGeometry.GetLocalSize();

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			Layer,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(Profile.Width * CmToPixel, LocalSize.Y),
				FSlateLayoutTransform({ LocalSize.X * 0.5 + LeftOffset * CmToPixel, 0 })),
			WhiteBrush,
			ESlateDrawEffect::None,
			Profile.VertexColor
		);
	}

	void DrawBrokedLane(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, uint32 Layer, const FRoadLaneMarkProfileBroked& Profile, const float LeftOffset) const
	{
		const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteTexture");
		auto LocalSize = AllottedGeometry.GetLocalSize();

		int Num = int(LocalSize.Y / (Profile.Long * CmToPixel + Profile.Gap * CmToPixel) + 0.5);
		if (Num % 2)
		{
			++Num;
		}

		float YOffset = -((Profile.Long + Profile.Gap) * Num * CmToPixel - Profile.Gap * CmToPixel - LocalSize.Y) * 0.5;

		for(int i = 0; i < Num; ++i)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				Layer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(Profile.Width, Profile.Long) * CmToPixel,
					FSlateLayoutTransform({ LocalSize.X * 0.5 + LeftOffset * CmToPixel, (double)YOffset})),
				WhiteBrush,
				ESlateDrawEffect::None,
				Profile.VertexColor
			);
			YOffset += (Profile.Long + Profile.Gap) * CmToPixel;
		}

	}

	bool DrawSolidOrBrokedLane(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, uint32 Layer, const TInstancedStruct<FRoadLaneMarkProfile>& Profile, const float LeftOffset) const
	{
		if (auto* AsSolid = Profile.GetPtr<FRoadLaneMarkProfileSolid>())
		{
			DrawSolidLane(AllottedGeometry, OutDrawElements, Layer, *AsSolid, LeftOffset);
			return true;
		}
		else if (auto* AsBroked = Profile.GetPtr<FRoadLaneMarkProfileBroked>())
		{
			DrawBrokedLane(AllottedGeometry, OutDrawElements, Layer, *AsBroked, LeftOffset);
			return true;
		}
		return false;
	}

	void OnPaintBackground(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
	{
		const FSlateBrush* BackgroundImage = FAppStyle::GetBrush(TEXT("Graph.Panel.SolidBackground"));
		PaintBackgroundAsLines(BackgroundImage, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);
	}

	void PaintBackgroundAsLines(const FSlateBrush* BackgroundImage, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32& DrawLayerId) const
	{
		const bool bAntialias = false;

		const int32 RulePeriod = (int32)FAppStyle::GetFloat("Graph.Panel.GridRulePeriod"); //GetGraphRulePeriod();
		check(RulePeriod > 0);

		const FLinearColor RegularColor(FAppStyle::GetColor("Graph.Panel.GridLineColor"));
		const FLinearColor RuleColor(FAppStyle::GetColor("Graph.Panel.GridRuleColor"));
		const FLinearColor CenterColor(FAppStyle::GetColor("Graph.Panel.GridCenterColor"));
		const float GraphSmallestGridSize = 8.0f;
		const float RawZoomFactor = ZoomLevels[ZoomLevel]; // GetZoomAmount();
		const float NominalGridSize = 4 * 1; //GetGridSize() * GetGridScaleAmount();

		float ZoomFactor = RawZoomFactor;
		float Inflation = 1.0f;
		while (ZoomFactor * Inflation * NominalGridSize <= GraphSmallestGridSize)
		{
			Inflation *= 2.0f;
		}

		const float GridCellSize = NominalGridSize * ZoomFactor * Inflation;

		FVector2D GridOrigin{};
		FVector2D LocalGridOrigin = AllottedGeometry.AbsoluteToLocal(GridOrigin);

		float ImageOffsetX = LocalGridOrigin.X - ((GridCellSize * RulePeriod) * FMath::Max(FMath::CeilToInt(LocalGridOrigin.X / (GridCellSize * RulePeriod)), 0));
		float ImageOffsetY = LocalGridOrigin.Y - ((GridCellSize * RulePeriod) * FMath::Max(FMath::CeilToInt(LocalGridOrigin.Y / (GridCellSize * RulePeriod)), 0));

		// Fill the background
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			DrawLayerId,
			AllottedGeometry.ToPaintGeometry(),
			BackgroundImage
		);


		TArray<FVector2D> LinePoints;
		new (LinePoints)FVector2D(0.0f, 0.0f);
		new (LinePoints)FVector2D(0.0f, 0.0f);

		// Horizontal bars
		for (int32 GridIndex = 0; ImageOffsetY < AllottedGeometry.GetLocalSize().Y; ImageOffsetY += GridCellSize, ++GridIndex)
		{
			if (ImageOffsetY >= 0.0f)
			{
				const bool bIsRuleLine = (GridIndex % RulePeriod) == 0;
				const int32 Layer = bIsRuleLine ? (DrawLayerId + 1) : DrawLayerId;

				const FLinearColor* Color = bIsRuleLine ? &RuleColor : &RegularColor;
				if (FMath::IsNearlyEqual(LocalGridOrigin.Y, ImageOffsetY, 1.0f))
				{
					Color = &CenterColor;
				}

				LinePoints[0] = FVector2D(0.0f, ImageOffsetY);
				LinePoints[1] = FVector2D(AllottedGeometry.GetLocalSize().X, ImageOffsetY);

				FSlateDrawElement::MakeLines(
					OutDrawElements,
					Layer,
					AllottedGeometry.ToPaintGeometry(),
					LinePoints,
					ESlateDrawEffect::None,
					*Color,
					bAntialias);
			}
		}

		// Vertical bars
		for (int32 GridIndex = 0; ImageOffsetX < AllottedGeometry.GetLocalSize().X; ImageOffsetX += GridCellSize, ++GridIndex)
		{
			if (ImageOffsetX >= 0.0f)
			{
				const bool bIsRuleLine = (GridIndex % RulePeriod) == 0;
				const int32 Layer = bIsRuleLine ? (DrawLayerId + 1) : DrawLayerId;

				const FLinearColor* Color = bIsRuleLine ? &RuleColor : &RegularColor;
				if (FMath::IsNearlyEqual(LocalGridOrigin.X, ImageOffsetX, 1.0f))
				{
					Color = &CenterColor;
				}

				LinePoints[0] = FVector2D(ImageOffsetX, 0.0f);
				LinePoints[1] = FVector2D(ImageOffsetX, AllottedGeometry.GetLocalSize().Y);

				FSlateDrawElement::MakeLines(
					OutDrawElements,
					Layer,
					AllottedGeometry.ToPaintGeometry(),
					LinePoints,
					ESlateDrawEffect::None,
					*Color,
					bAntialias);
			}
		}
		

		DrawLayerId += 2;
	}



private:
	float CmToPixel = 1.0;
	int ZoomLevel = 3;
	const TStaticArray<float, 16> ZoomLevels = { 0.25f, 0.333f, 0.75f, 1.f, 1.25f, 1.5f, 1.75f, 2.f , 2.25f, 2.5f, 2.75f, 3.f, 3.25f, 3.5f, 3.75f, 4.f};

	TAttribute<URoadMarkProfile*> RoadMarkProfile;

	mutable FRWLock ProfileRWLock;
	TInstancedStruct<FRoadLaneMarkProfile> ProfileToDraw;

};

void FRoadMarkProfileEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(WorkingAsset);
}

void FRoadMarkProfileEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) 
{
    WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MarkProfileEditor", "Mark Profile Editor"));
    auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

    InTabManager->RegisterTabSpawner(ERoadMarkProfileEditorTabs::ViewportID, FOnSpawnTab::CreateSP(this, &FRoadMarkProfileEditor::SpawnTab_Viewport))
        .SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
        .SetGroup(WorkspaceMenuCategoryRef)
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

    InTabManager->RegisterTabSpawner(ERoadMarkProfileEditorTabs::DetailsID, FOnSpawnTab::CreateSP(this, &FRoadMarkProfileEditor::SpawnTab_Details))
        .SetDisplayName(LOCTEXT("DetailsTabLabel", "Details"))
        .SetGroup(WorkspaceMenuCategoryRef)
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FRoadMarkProfileEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
    FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

    InTabManager->UnregisterTabSpawner(ERoadMarkProfileEditorTabs::ViewportID);
    InTabManager->UnregisterTabSpawner(ERoadMarkProfileEditorTabs::DetailsID);
}

void FRoadMarkProfileEditor::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UObject* InObject) 
{
	TArray<UObject*> ObjectsToEdit;
    ObjectsToEdit.Add(InObject);
    
    WorkingAsset = Cast<URoadMarkProfile>(InObject);

	// Default layout
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_FlipbookEditor_Layout_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.8f)
					->SetHideTabWell(true)
					->AddTab(ERoadMarkProfileEditorTabs::ViewportID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->AddTab(ERoadMarkProfileEditorTabs::DetailsID, ETabState::OpenedTab)
				)
			)
		);

	InitAssetEditor(Mode, InitToolkitHost, TEXT("RoadMarkProfileApp"), StandaloneDefaultLayout, /*bCreateDefaultStandaloneMenu=*/ true, /*bCreateDefaultToolbar=*/ true, ObjectsToEdit);
}

TSharedRef<SDockTab> FRoadMarkProfileEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTab_Title", "Viewport"))
		[
			//SNew(STextBlock)
			//.Text(LOCTEXT("HelloWorld", "Hello World"))
			//SNew(SPixelMappingSurface)
			//[
				SNew(SRoadMarkProfilePreview)
				.RoadMarkProfile(WorkingAsset)
			//]
		];
}

TSharedRef<SDockTab> FRoadMarkProfileEditor::SpawnTab_Details(const FSpawnTabArgs& Args)
{
    FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab_Title", "Details"))
		[
			SNew(SRoadMarkProfilePropertiesTabBody, SharedThis(this))
		];
}

FName FRoadMarkProfileEditor::GetToolkitFName() const
{ 
    return FName(TEXT("RoadMarkProfileEditor")); 
}

FText FRoadMarkProfileEditor::GetBaseToolkitName() const 
{ 
    return LOCTEXT("FRoadMarkProfileEditorLable", "Road Mark Profile Editor");
}

FString FRoadMarkProfileEditor::GetWorldCentricTabPrefix() const 
{ 
    return TEXT("RoadMarkProfileEditor"); 
}

FLinearColor FRoadMarkProfileEditor::GetWorldCentricTabColorScale() const
{ 
    return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f); 
}

#undef LOCTEXT_NAMESPACE