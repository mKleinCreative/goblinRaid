#include "GoblinSiege.h"
#include "Modules/ModuleManager.h"

// NOTE: this was IMPLEMENT_PRIMARY_GAME_MODULE in the standalone GoblinSiege project. MyProject
// owns the primary game module now, so GoblinSiege registers as a plain runtime module.
IMPLEMENT_MODULE(FDefaultGameModuleImpl, GoblinSiege);
