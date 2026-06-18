#pragma once

#include "CoreMinimal.h"
#include "Widgets/CricketHUDWidgetBase.h"
#include "CricketBowlingWidget.generated.h"

/**
 * The Bowling HUD: delivery type, line indicator, length indicator, swing amount
 * and spin amount, plus the physics-predicted shape of the delivery. Reads
 * FCricketBowlingVM only.
 */
UCLASS()
class CRICKETUI_API UCricketBowlingWidget : public UCricketHUDWidgetBase
{
	GENERATED_BODY()

public:
	UCricketBowlingWidget() { PanelTitle = TEXT("BOWLING"); TitleColor = FLinearColor(1.0f, 0.7f, 0.4f); }

protected:
	virtual void BuildPanel() override;
	virtual void RefreshFromModel(const FCricketHUDModel& Model) override;
	virtual bool IsRelevant(const FCricketHUDModel& Model) const override { return Model.bHasBowling; }

private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TypeLine;       // movement · length · line
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DialLine;       // pace / swing / spin
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PredictedLine;  // predicted length/line/swing/regime
};
