#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Editor-only module (#393): houses the bulk fracture-generation tool. Kept separate from the
// GoblinSiege runtime module deliberately - FractureEngine/PlanarCut/DataflowCore are editor-time
// content-authoring dependencies, not something the shipped game needs to link against.
class FGoblinSiegeEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};
