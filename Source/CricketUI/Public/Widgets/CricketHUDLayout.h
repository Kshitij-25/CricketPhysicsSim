#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "CricketHUDTypes.h"
#include "CricketHUDLayout.generated.h"

class UCanvasPanel;
class UCricketScoreboardWidget;
class UCricketBattingWidget;
class UCricketBowlingWidget;
class UCricketReplayWidget;

/**
 * UCricketHUDLayout — the root HUD container and the top of the widget hierarchy.
 *
 *   Layout (CanvasPanel)
 *     ├── Scoreboard   (top-centre)   — gameplay HUD + match information
 *     ├── Bowling      (top-left)     — delivery type / line / length / swing / spin
 *     ├── Batting      (bottom-left)  — timing / footwork / direction / contact
 *     └── Replay       (bottom-centre)— transport / timeline / speed (during replay)
 *
 * It owns no logic beyond placement and fan-out: UpdateFromModel forwards the one
 * snapshot to every child, and each child shows or hides itself from its relevance
 * flag. The panel classes are swappable (TSubclassOf) so a designer can drop in
 * reskinned Widget Blueprints without touching this class.
 */
UCLASS()
class CRICKETUI_API UCricketHUDLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Fan the per-frame snapshot out to every panel. */
	void UpdateFromModel(const FCricketHUDModel& Model);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Classes") TSubclassOf<UCricketScoreboardWidget> ScoreboardClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Classes") TSubclassOf<UCricketBattingWidget>    BattingClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Classes") TSubclassOf<UCricketBowlingWidget>    BowlingClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Classes") TSubclassOf<UCricketReplayWidget>     ReplayClass;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;

private:
	void EnsurePanels();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UCricketScoreboardWidget> Scoreboard;
	UPROPERTY(Transient) TObjectPtr<UCricketBattingWidget>    Batting;
	UPROPERTY(Transient) TObjectPtr<UCricketBowlingWidget>    Bowling;
	UPROPERTY(Transient) TObjectPtr<UCricketReplayWidget>     Replay;
};
