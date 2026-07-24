/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "MetaRoadModule.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"


#define LOCTEXT_NAMESPACE "FMetaRoadModule"

FString FMetaRoadModule::Version{};

int FMetaRoadModule::MajorVersion = 0;
int FMetaRoadModule::MinorVersion = 0;
int FMetaRoadModule::PatchVersion = 0;



void FMetaRoadModule::StartupModule()
{
	TSharedPtr<IPlugin> PluginPtr = IPluginManager::Get().FindPlugin(TEXT("MetaRoad"));
	check(PluginPtr.IsValid());
	Version = PluginPtr->GetDescriptor().VersionName;

	TArray<FString> OutArray;
	Version.ParseIntoArray(OutArray, TEXT("."), true);

	if (OutArray.Num() > 0)
	{
		MajorVersion = FCString::Atoi(*OutArray[1]);
	}
	if (OutArray.Num() > 1)
	{
		MinorVersion = FCString::Atoi(*OutArray[2]);
	}
	if (OutArray.Num() > 2)
	{
		PatchVersion = FCString::Atoi(*OutArray[0]);
	}

	UE_LOG(LogMetaRoad, Log, TEXT("MetaRoad version: %s"), *Version);
}

void FMetaRoadModule::ShutdownModule()
{
}


#undef LOCTEXT_NAMESPACE
	
DEFINE_LOG_CATEGORY(LogMetaRoad);

IMPLEMENT_MODULE(FMetaRoadModule, MetaRoad)