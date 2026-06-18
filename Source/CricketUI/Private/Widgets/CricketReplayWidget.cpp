#include "Widgets/CricketReplayWidget.h"
#include "Components/TextBlock.h"

namespace
{
	/** A fixed-width text scrubber: "[=====O--------] 42%". */
	FString TimelineBarText(double Normalized, int32 Cells = 24)
	{
		const int32 Head = FMath::Clamp(FMath::RoundToInt(Normalized * Cells), 0, Cells);
		FString Bar = TEXT("[");
		for (int32 i = 0; i < Cells; ++i)
		{
			Bar += (i == Head) ? TEXT("O") : (i < Head ? TEXT("=") : TEXT("-"));
		}
		Bar += FString::Printf(TEXT("] %3d%%"), FMath::RoundToInt(Normalized * 100.0));
		return Bar;
	}
}

void UCricketReplayWidget::BuildPanel()
{
	StatusLine   = AddRow(14, FLinearColor::White);
	TimelineBar  = AddRow(12, FLinearColor(0.85f, 0.85f, 0.85f));
	ControlsLine = AddRow(11, FLinearColor(0.6f, 0.6f, 0.6f));
}

void UCricketReplayWidget::RefreshFromModel(const FCricketHUDModel& Model)
{
	const FCricketReplayVM& R = Model.Replay;

	SetText(StatusLine, FString::Printf(TEXT("%s  %s   %s"),
		R.bPaused ? TEXT("II PAUSED") : TEXT("> PLAYING"), *R.RateText, *R.TimelineText));

	SetText(TimelineBar, FString::Printf(TEXT("%s   (%d frames, %d events)"),
		*TimelineBarText(R.NormalizedTime), R.FrameCount, R.EventCount));

	SetText(ControlsLine, TEXT("P pause  [ ] speed  , . step"));
}
