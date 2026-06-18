#include "Modules/ModuleManager.h"

// CricketAudio is a plain runtime module — no custom startup. Loaded at the Default
// phase, after the gameplay/physics/match modules whose events it reacts to.
IMPLEMENT_MODULE(FDefaultModuleImpl, CricketAudio);
