/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

METAROAD_API DECLARE_LOG_CATEGORY_EXTERN(LogMetaRoad, Log, All);


class METAROAD_API FMetaRoadModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Plugin version, parsed from the .uplugin at startup. Read at runtime (e.g. RoadSplineComponent
	// serialization stamps the saving version), so it stays in the runtime module.
	static TTuple<int, int, int> GetVersion() { return { MajorVersion, MinorVersion, PatchVersion }; }
	static const FString& GetVersionStr() { return Version; }

private:
	static FString Version;
	static int MajorVersion;
	static int MinorVersion;
	static int PatchVersion;
};
