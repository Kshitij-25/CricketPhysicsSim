#include "Widgets/CricketBattingWidget.h"
#include "Components/TextBlock.h"

namespace
{
	/** Timing verdict colour: green at perfect, amber early/late, red at the extremes. */
	FLinearColor TimingColor(ECricketTimingQuality Q)
	{
		switch (Q)
		{
		case ECricketTimingQuality::Perfect: return FLinearColor(0.45f, 1.0f, 0.45f);
		case ECricketTimingQuality::Early:
		case ECricketTimingQuality::Late:    return FLinearColor(1.0f, 0.85f, 0.35f);
		default:                             return FLinearColor(1.0f, 0.40f, 0.35f);
		}
	}
}

void UCricketBattingWidget::BuildPanel()
{
	IntentLine  = AddRow(13, FLinearColor::White);
	SwingLine   = AddRow(12, FLinearColor(0.75f, 0.75f, 0.75f));
	TimingLine  = AddRow(15, FLinearColor::White);
	ContactLine = AddRow(12, FLinearColor(0.85f, 0.85f, 0.85f));
}

void UCricketBattingWidget::RefreshFromModel(const FCricketHUDModel& Model)
{
	const FCricketBattingVM& B = Model.Batting;

	SetText(IntentLine, FString::Printf(TEXT("%s · %s · %s"),
		*B.ShotText, *B.FootworkText, *B.DirectionText));

	SetText(SwingLine, B.bSwinging
		? FString::Printf(TEXT("%s  (bat %.1f m/s)"), *B.SwingPhaseText, B.BatSpeedMS)
		: FString(TEXT("Ready")));

	if (B.bHasPlayed)
	{
		if (TimingLine) { TimingLine->SetColorAndOpacity(FSlateColor(TimingColor(B.Timing))); }
		SetText(TimingLine, FString::Printf(TEXT("Timing: %s  (%+.0f ms)"), *B.TimingText, B.TimingErrorMs));

		if (B.bMadeContact)
		{
			SetText(ContactLine, FString::Printf(TEXT("%s%s · Q %.2f · exit %.0f km/h @ %.0f°"),
				*B.ContactText, B.bIsEdge ? TEXT(" (EDGE)") : TEXT(""),
				B.ContactQuality, B.ExitSpeedKmh, B.LaunchAngleDeg));
		}
		else
		{
			SetText(ContactLine, TEXT("Missed"));
		}
	}
	else
	{
		SetText(TimingLine, TEXT("Timing: —"));
		SetText(ContactLine, TEXT(""));
	}
}
