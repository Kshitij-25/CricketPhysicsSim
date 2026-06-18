#include "Widgets/CricketBowlingWidget.h"
#include "Components/TextBlock.h"

void UCricketBowlingWidget::BuildPanel()
{
	TypeLine      = AddRow(13, FLinearColor::White);
	DialLine      = AddRow(12, FLinearColor(0.85f, 0.85f, 0.85f));
	PredictedLine = AddRow(12, FLinearColor(0.55f, 1.0f, 0.65f));
}

void UCricketBowlingWidget::RefreshFromModel(const FCricketHUDModel& Model)
{
	const FCricketBowlingVM& B = Model.Bowling;

	// Delivery type + length + line indicators.
	SetText(TypeLine, FString::Printf(TEXT("%s · %s · %s"),
		*B.DeliveryTypeText, *B.LengthText, *B.LineText));

	// Pace + the two intent dials and the resulting revs.
	SetText(DialLine, FString::Printf(TEXT("Pace %.0f km/h   Swing %.2f   Spin %.2f  (%.0f rpm)"),
		B.PaceKmh, B.SwingAmount01, B.SpinAmount01, B.SpinRPM));

	// Physics-predicted shape (only meaningful once a delivery has been generated).
	if (B.bHasDelivery)
	{
		SetText(PredictedLine, FString::Printf(TEXT("Pred: len %.1f m · line %+.2f m · swing %+.2f m · %s"),
			B.PredictedLengthM, B.PredictedLineM, B.FreeFlightSwingM, *B.RegimeText));
	}
	else
	{
		SetText(PredictedLine, TEXT("Pred: —"));
	}
}
