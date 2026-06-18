#pragma once

#include "CoreMinimal.h"
#include "Widgets/CricketHUDWidgetBase.h"
#include "CricketScoreboardWidget.generated.h"

/**
 * The always-on gameplay HUD + Match Information: score/wickets/overs, current &
 * required run rate, the two batters, the current bowler, ball count, and the
 * chase/result line. Reads FCricketScoreboardVM only.
 */
UCLASS()
class CRICKETUI_API UCricketScoreboardWidget : public UCricketHUDWidgetBase
{
	GENERATED_BODY()

public:
	UCricketScoreboardWidget() { PanelTitle = TEXT("SCORE"); }

protected:
	virtual void BuildPanel() override;
	virtual void RefreshFromModel(const FCricketHUDModel& Model) override;
	virtual bool IsRelevant(const FCricketHUDModel& Model) const override { return Model.bHasScoreboard; }

private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ScoreLine;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RateLine;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StrikerLine;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NonStrikerLine;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BowlerLine;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ChaseLine;
};
