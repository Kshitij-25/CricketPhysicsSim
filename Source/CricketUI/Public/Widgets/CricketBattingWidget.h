#pragma once

#include "CoreMinimal.h"
#include "Widgets/CricketHUDWidgetBase.h"
#include "CricketBattingWidget.generated.h"

/**
 * The Batting HUD: shot timing feedback, footwork selection, shot direction and
 * contact quality. Reads FCricketBattingVM only — a pure read-out of what the
 * batting motion model + collision solver already decided.
 */
UCLASS()
class CRICKETUI_API UCricketBattingWidget : public UCricketHUDWidgetBase
{
	GENERATED_BODY()

public:
	UCricketBattingWidget() { PanelTitle = TEXT("BATTING"); TitleColor = FLinearColor(0.55f, 1.0f, 0.6f); }

protected:
	virtual void BuildPanel() override;
	virtual void RefreshFromModel(const FCricketHUDModel& Model) override;
	virtual bool IsRelevant(const FCricketHUDModel& Model) const override { return Model.bHasBatting; }

private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> IntentLine;   // shot · footwork · direction
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SwingLine;    // live swing phase + bat speed
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TimingLine;   // timing verdict + error
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ContactLine;  // contact region + quality + exit
};
