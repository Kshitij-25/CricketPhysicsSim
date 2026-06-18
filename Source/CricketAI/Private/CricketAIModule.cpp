// CricketAI module bootstrap. The AI layer is plain runtime code (components +
// pure reasoning cores), so the module needs no special startup; this just
// registers it with the engine's module system.

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, CricketAI);
