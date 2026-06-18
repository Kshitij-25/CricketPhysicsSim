#include "Modules/ModuleManager.h"

// CricketUI is a plain runtime module — no custom startup. It is loaded at the
// Default phase, after the gameplay/physics/match modules it consumes.
IMPLEMENT_MODULE(FDefaultModuleImpl, CricketUI);
