#pragma once

#include "CoreMinimal.h"
#include "Widgets/CricketHUDWidgetBase.h"
#include "CricketReplayWidget.generated.h"

/**
 * The Replay UI: transport controls hint, a text timeline scrubber, the timeline
 * position and the playback speed. Reads FCricketReplayVM only; shown only while a
 * replay is active.
 */
UCLASS()
class CRICKETUI_API UCricketReplayWidget : public UCricketHUDWidgetBase
{
	GENERATED_BODY()

public:
	UCricketReplayWidget() { PanelTitle = TEXT("REPLAY"); TitleColor = FLinearColor(1.0f, 0.95f, 0.5f); }

protected:
	virtual void BuildPanel() override;
	virtual void RefreshFromModel(const FCricketHUDModel& Model) override;
	virtual bool IsRelevant(const FCricketHUDModel& Model) const override
	{
		return Model.bHasReplay && Model.Replay.bReplaying;
	}

private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusLine;    // play/pause + speed
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TimelineBar;   // ascii scrubber
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ControlsLine;  // key hints
};
