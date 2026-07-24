/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "MetaRoadEditorModule.h"
#include "MetaRoadEditorStyle.h"
#include "RoadSplineComponent.h"
#include "MetaRoadActor.h"
#include "TileMapWindowComponent.h"
#include "RoadEditorCommands.h"
#include "LevelEditor.h"
#include "UnrealEdGlobals.h"
#include "Selection.h"
#include "Editor/UnrealEdEngine.h"
#include "MetaRoadModule.h"
#include "EngineUtils.h"
#include "Widgets/Input/SHyperlink.h"
#include "Utils/RoadUtils.h"
#include "SSceneOutliner.h"
#include "ActorPickingMode.h"
#include "ActorTreeItem.h"
#include "SceneOutlinerModule.h"
#include "Landscape.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "IRoadPolygon.h"
#include "ScopedTransaction.h"
#include "Utils/AssetUtils.h"
#include "Assets/RoadLaneAttributeDescriptor.h"

#define LOCTEXT_NAMESPACE "FMetaRoadEditorModule_BindCommands"

class SAttachRoadSpline : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAttachRoadSpline) {}
	SLATE_END_ARGS()

	TSet<TWeakObjectPtr<URoadSplineComponent>> Splines;
	TWeakObjectPtr<AActor> SelectedActor;
	TWeakPtr<SWindow> OuterWindow;

	// Constructs this widget with InArgs
	void Construct(const FArguments& InArgs, TSet<TWeakObjectPtr<URoadSplineComponent>> && InSplines, TWeakPtr<SWindow> InOuterWindow)
	{
		Splines = MoveTemp(InSplines);
		OuterWindow = InOuterWindow;

		FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");

		FSceneOutlinerInitializationOptions InitOptions;
		InitOptions.bShowHeaderRow = true;
		InitOptions.bFocusSearchBoxWhenOpened = true;
		//InitOptions.FilterBarOptions.bHasFilterBar = true;
		//InitOptions.FilterBarOptions.bUseSharedSettings = false;
		//InitOptions.bShowTransient = true;
		InitOptions.Filters->AddFilterPredicate<FActorTreeItem>(FActorTreeItem::FFilterPredicate::CreateStatic([](const AActor* InActor)
		{
				// Only AMetaRoad targets: per the AMetaRoad-only rule, a road spline moved onto any other actor
				// would be silently dropped from the build/preview/bake pipeline. Use "Create New Actor" otherwise.
				return ((InActor->GetWorld() && InActor->GetWorld()->WorldType != EWorldType::PIE) || GEditor->ObjectsThatExistInEditorWorld.Get(InActor))
					&& !InActor->HasAnyFlags(EObjectFlags::RF_Transient)
					&& InActor->IsA<AMetaRoad>();

		}), FSceneOutlinerFilter::EDefaultBehaviour::Pass);

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0)
			[
				SceneOutlinerModule.CreateActorPicker(
					InitOptions,
					FOnActorPicked::CreateLambda([&](AActor* Actor)
					{
						SelectedActor = Actor;
					})
				)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(0.0f, 3.0f, 1.0f, 0.0f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.5f)
				[
					SNew(SButton)
					.Text(LOCTEXT("AttachToActor", "Attach To"))
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([this]()
					{
						return SelectedActor.IsValid();
					})
					.OnClicked(this, &SAttachRoadSpline::OnMoveSplines)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.5f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateNewActor", "Create New Actor"))
					.HAlign(HAlign_Center)
					.OnClicked(this, &SAttachRoadSpline::OnCreateNewActor)
				]

			]
		];
	}

	virtual ~SAttachRoadSpline() {}

	bool MoveComponents(AActor* TargetActor)
	{
		if (!IsValid(TargetActor))
		{
			return false;
		}

		TArray<UActorComponent*> ComponentsToCopy;
		TArray<FTransform> ComponentsTransforms;
		for (auto& Spline : Splines)
		{
			if (Spline.IsValid() && FComponentEditorUtils::CanCopyComponent(Spline.Get()))
			{
				ComponentsTransforms.Add(Spline->GetComponentTransform());
				ComponentsToCopy.Add(Spline.Get());
			}
		}

		if (ComponentsToCopy.IsEmpty())
		{
			return false;
		}

		FString DestinationData;
		FComponentEditorUtils::CopyComponents(ComponentsToCopy, &DestinationData);

		UActorComponent* ComponentToSelect = nullptr;
		FComponentEditorUtils::DeleteComponents(ComponentsToCopy, ComponentToSelect);

		TArray<UActorComponent*> OutPastedComponents;
		FComponentEditorUtils::PasteComponents(OutPastedComponents, TargetActor, nullptr, &DestinationData);

		if (OutPastedComponents.Num() == ComponentsTransforms.Num())
		{
			for (int i = 0; i < OutPastedComponents.Num(); ++i)
			{
				if (auto* Spline = Cast<URoadSplineComponent>(OutPastedComponents[i]))
				{
					Spline->SetWorldTransform(ComponentsTransforms[i]);
				}
			}
		}

		if (OutPastedComponents.Num())
		{
			GEditor->SelectNone(true, true);
			GEditor->SelectActor(TargetActor, true, true, true, true);
			GEditor->SelectComponent(OutPastedComponents[0], true, true, true);
		}

		return true;
	}

	FReply OnMoveSplines()
	{
		const FScopedTransaction Transaction(LOCTEXT("AttachTo", "Attach To"));
		MoveComponents(SelectedActor.Get());
		CloseWindow();
		return FReply::Handled();
	}

	FReply OnCreateNewActor()
	{
		const FScopedTransaction Transaction(LOCTEXT("CreateAttachTo", "Create Actor & Attach"));

		URoadSplineComponent* FirstSpline = Splines.Num() ? Splines.begin()->Get() : nullptr;

		if (IsValid(FirstSpline))
		{
			AActor* NewActor = RoadUtils::SpawnRoadActor(FirstSpline->GetWorld(), FirstSpline->GetComponentTransform());
			check(NewActor);
			MoveComponents(NewActor);
		}

		CloseWindow();
		return FReply::Handled();
	}

	void CloseWindow()
	{
		if (OuterWindow.IsValid())
		{
			OuterWindow.Pin()->RequestDestroyWindow();
		}
	}

};

static TSharedRef<SWindow> CreateAboutWindow()
{
	FText Version = FText::Format(LOCTEXT("AboutWindow_Version", "Version {0} {1}"),
		FText::FromString(FMetaRoadModule::GetVersionStr()),
		METAROAD_PRO ? LOCTEXT("AboutWindow_VersionPostfixPro", "Pro") : LOCTEXT("AboutWindow_VersionPostfixFree", "Free")
	);

	auto AboutWindow = SNew(SWindow)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(640, 300))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.Title(FText::FromString("About MetaRoad")) 
		[
			SNew(SBox)
			.Padding(12.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(64)
						.HeightOverride(64)
						[
							SNew(SImage)
							.Image(FMetaRoadEditorStyle::Get().GetBrush("MetaRoadLogo.Image"))
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AboutWindow_Name", "MetaRoad"))
						.TextStyle(&FMetaRoadEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaRoadLogo.Text"))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(.1f, .1f, .1f))
					.Padding(1)
				]
				+ SVerticalBox::Slot()
				[
					SNew(SBox)
					.Padding(12.f)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0, 10.0, 0.0, 2.0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AboutWindow_Name", "MetaRoad Plugin"))
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2.0)
						[
							SNew(STextBlock)
							.Text(Version)
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2.0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AboutWindow_copyright", "(c) 2026 Ivan Zhukov. All Rights Reserved."))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2.0)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AboutWindow_copyrightText", "This product is licensed under the "))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SHyperlink)
								.Text(LOCTEXT("AboutWindow_copyrightLink", "Fab End User License Agreement"))
								.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
								.OnNavigate_Lambda([]()
								{
										FPlatformProcess::LaunchURL(TEXT("https://www.fab.com/eula"), nullptr, nullptr);
								})
							]

						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 12, 0, 2)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0, 0, 4, 0)
							[
								SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.Documentation"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SHyperlink)
								.Text(LOCTEXT("AboutWindow_docLink", "Documentation"))
								.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
								.OnNavigate_Lambda([]()
								{
										FPlatformProcess::LaunchURL(TEXT("https://unrealdrive.readthedocs.io/en/latest/index.html"), nullptr, nullptr);
								})
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2.0)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0, 0, 4, 0)
							[
								SNew(SImage)
								.Image(FMetaRoadEditorStyle::Get().GetBrush("Icons.YouTube"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SHyperlink)
								.Text(LOCTEXT("AboutWindow_youTube", "YouTube"))
								.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
								.OnNavigate_Lambda([]()
								{
										FPlatformProcess::LaunchURL(TEXT("https://www.youtube.com/@MetaRoadPlugin"), nullptr, nullptr);
								})
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2.0)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0, 0, 4, 0)
							[
								SNew(SImage)
								.Image(FMetaRoadEditorStyle::Get().GetBrush("Icons.Discord"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SHyperlink)
								.Text(LOCTEXT("AboutWindow_discord", "Discord"))
								.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
								.OnNavigate_Lambda([]()
								{
										FPlatformProcess::LaunchURL(TEXT("https://discord.gg/eMt8rMZTes"), nullptr, nullptr);
								})
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2.0)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0, 0, 4, 0)
							[
								SNew(SImage)
								.Image(FMetaRoadEditorStyle::Get().GetBrush("Icons.Email"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SHyperlink)
								.Text(FText::FromString(TEXT("ivzhuk7@gmail.com")))
								.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
								.OnNavigate_Lambda([]()
								{
										FPlatformProcess::LaunchURL(TEXT("ivzhuk7@gmail.com"), nullptr, nullptr);
								})
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1)
			]
		];

	return AboutWindow;
}

void FMetaRoadEditorModule::BindCommands()
{
	auto& Commands = FRoadEditorCommands::Get();

	CommandList->MapAction(
		Commands.TileMapWindowVisibility,
		FExecuteAction::CreateLambda([this]() 
		{
			FMetaRoadEditorModule::SetIsTileRendersVisibleInEditor(!FMetaRoadEditorModule::IsTileRendersVisibleInEditor());
			
			for (TActorIterator<AActor> It(GEditor->GetEditorWorldContext().World()); It; ++It)
			{
				TArray<UTileMapWindowComponent*> Components;
				It->GetComponents(Components);
				for (auto* Component : Components)
				{
					Component->MarkRenderStateDirty();
				}
			}
			GEditor->RedrawLevelEditingViewports(true);

		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return FMetaRoadEditorModule::IsTileRendersVisibleInEditor();
		})
	);

	CommandList->MapAction(
		Commands.HideSelectedSpline,
		FExecuteAction::CreateLambda([this]() 
		{
			for (FSelectionIterator It(GEditor->GetSelectedComponentIterator()); It; ++It)
			{
				if (auto* SplineComp = Cast<URoadSplineComponent>(*It))
				{
					SplineComp->SetVisibility(false);
				}
			}
		})
	);

	CommandList->MapAction(
		Commands.UnhideAllSpline,
		FExecuteAction::CreateLambda([this]() 
		{
			for (TActorIterator<AActor> It(GEditor->GetEditorWorldContext().World()); It; ++It)
			{
				TArray<URoadSplineComponent*> Components;
				It->GetComponents(Components);
				for (auto* Component : Components)
				{
					Component->SetVisibility(true);
				}
			}
		})
	);

	CommandList->MapAction(
		Commands.FitWidth,
		FExecuteAction::CreateLambda([this]() 
		{
			for (FSelectionIterator It(GEditor->GetSelectedComponentIterator()); It; ++It)
			{
				if (auto* SplineComp = Cast<URoadSplineComponent>(*It))
				{
					const FScopedTransaction Transaction(LOCTEXT("FitLanesWidth", "Fit Lanes Width"));
					SplineComp->Modify();
					RoadUtils::FitLanesWidthToBeginConnection(SplineComp, SplineComp->GetPredecessorConnection()->GetOuterConnection());
					RoadUtils::FitLanesWidthToEndConnection(SplineComp, SplineComp->GetSuccessorConnection()->GetOuterConnection());

					SplineComp->MarkRenderStateDirty();
					SplineComp->UpdateRoadLayout();
					SplineComp->UpdateMagicTransform();
					SplineComp->UpdateLandscape();

					GEditor->RedrawLevelEditingViewports(true);
				}
			}
		})
	);

	CommandList->MapAction(
		Commands.AttachTo,
		FExecuteAction::CreateLambda([this]()
		{
			TSet<TWeakObjectPtr<URoadSplineComponent>> Components;
			for (FSelectionIterator It(GEditor->GetSelectedComponentIterator()); It; ++It)
			{
				if (auto* Component = Cast<URoadSplineComponent>(*It))
				{
					Components.Add(Component);
				}
			}

			if (!Components.Num())
			{
				return;
			}

			auto AttachToWindow = SNew(SWindow)
				.SizingRule(ESizingRule::UserSized)
				.ClientSize(FVector2D(640, 300))
				.AutoCenter(EAutoCenter::PreferredWorkArea)
				.Title(LOCTEXT("AttachToWindow", "Attach Selected Splines(s) to"));

			AttachToWindow->SetContent(
				SNew(SBox)
				.MaxDesiredHeight(400.0f)
				.WidthOverride(300.0f)
				[
					SNew(SAttachRoadSpline, MoveTemp(Components), AttachToWindow.ToWeakPtr())
				]
			);

			GEditor->EditorAddModalWindow(AttachToWindow);
		})
	);
		
	CommandList->MapAction(
		Commands.RoadSplineMode,
		FExecuteAction::CreateLambda([this]()
		{
			FMetaRoadSelectionController::Get().DeactivateActiveTool();
			FMetaRoadSelectionController::Get().SetSplineEditorMode();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return !FMetaRoadSelectionController::Get().IsInteractiveToolActive() && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Spline;
		})
	);

	CommandList->MapAction(
		Commands.RoadSectionMode,
		FExecuteAction::CreateLambda([this]()
		{
			FMetaRoadSelectionController::Get().DeactivateActiveTool();
			FMetaRoadSelectionController::Get().SetSectionEditorMode();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return !FMetaRoadSelectionController::Get().IsInteractiveToolActive() && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Section && !IsValid(FMetaRoadSelectionController::Get().GetSelectedAttributeDescriptor().Get());
		})
	);

	CommandList->MapAction(
		Commands.RoadOffsetMode,
		FExecuteAction::CreateLambda([this]()
		{
			FMetaRoadSelectionController::Get().DeactivateActiveTool();
			FMetaRoadSelectionController::Get().SetOffsetEditorMode();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return !FMetaRoadSelectionController::Get().IsInteractiveToolActive() && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Offset;
		})
	);

	CommandList->MapAction(
		Commands.RoadLaneWidthMode,
		FExecuteAction::CreateLambda([this]()
		{
			FMetaRoadSelectionController::Get().DeactivateActiveTool();
			FMetaRoadSelectionController::Get().SetWidthEditorMode();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return !FMetaRoadSelectionController::Get().IsInteractiveToolActive() && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Width;
		})
	);

	CommandList->MapAction(
		Commands.RoadAttributeMode,
		FExecuteAction::CreateLambda([this]()
		{
			FMetaRoadSelectionController::Get().DeactivateActiveTool();
			// Enter attribute editing. Keep the current descriptor if one is selected,
			// otherwise pick the first available one (the tree below is the real chooser).
			TSubclassOf<URoadLaneAttributeDescriptor> Descriptor = FMetaRoadSelectionController::Get().GetSelectedAttributeDescriptor();
			if (!IsValid(Descriptor.Get()))
			{
				for (UClass* Class : AssetUtils::GetAllClassesOfSubClass(URoadLaneAttributeDescriptor::StaticClass()))
				{
					if (Class)
					{
						Descriptor = Class;
						break;
					}
				}
			}
			if (IsValid(Descriptor.Get()))
			{
				FMetaRoadSelectionController::Get().SetAttributeEditorMode(Descriptor);
			}
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return !FMetaRoadSelectionController::Get().IsInteractiveToolActive() && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Attribute;
		})
	);

	CommandList->MapAction(
		Commands.RoadPresetMode,
		FExecuteAction::CreateLambda([this]()
		{
			FMetaRoadSelectionController::Get().DeactivateActiveTool();
			FMetaRoadSelectionController::Get().SetPresetEditorMode();
		}),
		FCanExecuteAction::CreateLambda([]()
		{
			// Active only when at least one selected actor owns a road spline.
			if (GEditor)
			{
				if (USelection* Selection = GEditor->GetSelectedActors())
				{
					for (FSelectionIterator It(*Selection); It; ++It)
					{
						if (AMetaRoad* Road = Cast<AMetaRoad>(*It))
						{
							if (Road->FindComponentByClass<URoadSplineComponent>())
							{
								return true;
							}
						}
					}
				}
			}
			return false;
		}),
		FIsActionChecked::CreateLambda([this]()
		{
			return !FMetaRoadSelectionController::Get().IsInteractiveToolActive() && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Preset;
		})
	);

	CommandList->MapAction(
		Commands.RoadBakeMode,
		FExecuteAction::CreateLambda([this]()
		{
			FMetaRoadSelectionController::Get().DeactivateActiveTool();
			FMetaRoadSelectionController::Get().SetBakeEditorMode();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return !FMetaRoadSelectionController::Get().IsInteractiveToolActive() && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Bake;
		})
	);

	CommandList->MapAction(
		Commands.RoadFbxExportMode,
		FExecuteAction::CreateLambda([this]()
		{
			FMetaRoadSelectionController::Get().DeactivateActiveTool();
			FMetaRoadSelectionController::Get().SetFbxExportEditorMode();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return !FMetaRoadSelectionController::Get().IsInteractiveToolActive() && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::FbxExport;
		})
	);

	CommandList->MapAction(
		Commands.RoadVisibilityMode,
		FExecuteAction::CreateLambda([this]()
		{
			FMetaRoadSelectionController::Get().DeactivateActiveTool();
			FMetaRoadSelectionController::Get().SetVisibilityEditorMode();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return !FMetaRoadSelectionController::Get().IsInteractiveToolActive() && FMetaRoadSelectionController::Get().GetRoadSelectionMode() == ERoadSelectionMode::Visibility;
		})
	);


	CommandList->MapAction(
		Commands.About,
		FExecuteAction::CreateLambda([this]() 
		{
			GEditor->EditorAddModalWindow(CreateAboutWindow());
		})
	);
}




#undef LOCTEXT_NAMESPACE

