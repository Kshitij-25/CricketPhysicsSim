#pragma once

#include "CoreMinimal.h"
#include "CricketGameMode.h"
#include "CricketHUDGameMode.generated.h"

/**
 * ACricketHUDGameMode — ACricketGameMode + the HUD wiring.
 *
 * The base game mode (in CricketSim) must not know about the UI layer, or the
 * one-directional module graph would cycle. So the HUD is attached here, in the UI
 * module, by setting HUDClass to ACricketHUD. Selecting this as the default game
 * mode (Config/DefaultEngine.ini) makes the HUD appear in every scene with no
 * level edits.
 */
UCLASS()
class CRICKETUI_API ACricketHUDGameMode : public ACricketGameMode
{
	GENERATED_BODY()

public:
	ACricketHUDGameMode();
};
