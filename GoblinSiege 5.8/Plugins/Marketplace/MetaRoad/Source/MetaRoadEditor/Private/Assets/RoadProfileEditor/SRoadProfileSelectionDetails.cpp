/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "SRoadProfileSelectionDetails.h"
#include "Assets/RoadProfile.h"
#include "RoadProfilePreviewBuilder.h"
#include "IStructureDetailsView.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Modules/ModuleManager.h"

void SRoadProfileSelectionDetails::Construct(const FArguments& InArgs, URoadProfile* InProfile, URoadProfilePreviewBuilder* InBuilder)
{
	Profile = InProfile;
	Builder = InBuilder;

	FPropertyEditorModule& PropModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Lane struct view (edits the selected FRoadLaneProfile in place).
	{
		FDetailsViewArgs Args;
		Args.bAllowSearch = false;
		Args.bShowOptions = false;
		LaneView = PropModule.CreateStructureDetailView(Args, FStructureDetailsViewArgs{}, nullptr);
		LaneView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SRoadProfileSelectionDetails::OnDetailsChanged);
	}

	// Profile object view: center -> Direction + CenterAttributes (filtered); nothing -> whole profile.
	{
		FDetailsViewArgs Args;
		Args.bAllowSearch = false;
		Args.bHideSelectionTip = true;
		Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		ProfileView = PropModule.CreateDetailView(Args);
		ProfileView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SRoadProfileSelectionDetails::IsProfilePropertyVisible));
		ProfileView->OnFinishedChangingProperties().AddSP(this, &SRoadProfileSelectionDetails::OnDetailsChanged);
		ProfileView->SetObject(Profile.Get());
	}

	ChildSlot
	[
		SAssignNew(Switcher, SWidgetSwitcher)
		+ SWidgetSwitcher::Slot()
		[
			LaneView->GetWidget().ToSharedRef()
		]
		+ SWidgetSwitcher::Slot()
		[
			ProfileView.ToSharedRef()
		]
	];

	if (Builder.IsValid())
	{
		SelectionHandle = Builder->OnSelectionChanged.AddSP(this, &SRoadProfileSelectionDetails::Refresh);
	}

	Refresh();
}

SRoadProfileSelectionDetails::~SRoadProfileSelectionDetails()
{
	if (Builder.IsValid())
	{
		Builder->OnSelectionChanged.Remove(SelectionHandle);
	}
}

void SRoadProfileSelectionDetails::Refresh()
{
	URoadProfile* P = Profile.Get();
	const int32 Idx = Builder.IsValid() ? Builder->GetSelectedLane() : URoadProfilePreviewBuilder::NoSelection;

	if (P && Idx != URoadProfilePreviewBuilder::NoSelection && Idx != MetaRoad::ZeroLaneIndex && P->GetLaneByIndex(Idx))
	{
		// A real lane — edit its FRoadLaneProfile in place.
		LaneScope = MakeShared<FStructOnScope>(FRoadLaneProfile::StaticStruct(), reinterpret_cast<uint8*>(P->GetLaneByIndex(Idx)));
		LaneView->SetStructureData(LaneScope);
		Switcher->SetActiveWidgetIndex(0);
	}
	else
	{
		// Center (filtered to road-level props) or nothing (whole profile).
		LaneScope.Reset();
		LaneView->SetStructureData(nullptr);
		ProfileView->ForceRefresh();
		Switcher->SetActiveWidgetIndex(1);
	}
}

bool SRoadProfileSelectionDetails::IsProfilePropertyVisible(const FPropertyAndParent& PropertyAndParent) const
{
	const int32 Idx = Builder.IsValid() ? Builder->GetSelectedLane() : URoadProfilePreviewBuilder::NoSelection;
	if (Idx != MetaRoad::ZeroLaneIndex)
	{
		return true; // nothing selected -> show the whole profile
	}

	// Center reference line -> only the road-level props.
	auto IsAllowed = [](FName Name)
	{
		return Name == GET_MEMBER_NAME_CHECKED(URoadProfile, Direction)
			|| Name == GET_MEMBER_NAME_CHECKED(URoadProfile, CenterAttributes);
	};

	if (IsAllowed(PropertyAndParent.Property.GetFName()))
	{
		return true;
	}
	for (const FProperty* Parent : PropertyAndParent.ParentProperties)
	{
		if (IsAllowed(Parent->GetFName()))
		{
			return true;
		}
	}
	return false;
}

void SRoadProfileSelectionDetails::OnDetailsChanged(const FPropertyChangedEvent& Event)
{
	if (Profile.IsValid())
	{
		// Mark dirty + fire the property-changed event for the profile: the editor's OnObjectPropertyChanged
		// handler rebuilds the preview, and UThumbnailManager re-renders the stored thumbnail on save. In-place
		// struct edits via FStructOnScope don't fire this for the owning profile on their own.
		Profile->MarkPackageDirty();
		FPropertyChangedEvent EmptyEvent(nullptr);
		FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(Profile.Get(), EmptyEvent);
	}
}
