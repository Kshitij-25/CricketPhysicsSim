#include "Widgets/CricketScoreboardWidget.h"
#include "Components/TextBlock.h"

void UCricketScoreboardWidget::BuildPanel()
{
	ScoreLine      = AddRow(20, FLinearColor::White);
	RateLine       = AddRow(12, FLinearColor(0.75f, 0.75f, 0.75f));
	StrikerLine    = AddRow(13, FLinearColor(0.55f, 1.0f, 0.55f));
	NonStrikerLine = AddRow(13, FLinearColor(0.80f, 0.80f, 0.80f));
	BowlerLine     = AddRow(13, FLinearColor(1.0f, 0.85f, 0.45f));
	ChaseLine      = AddRow(13, FLinearColor(0.45f, 0.85f, 1.0f));
}

void UCricketScoreboardWidget::RefreshFromModel(const FCricketHUDModel& Model)
{
	const FCricketScoreboardVM& S = Model.Scoreboard;

	SetText(ScoreLine, FString::Printf(TEXT("%s  %s  (%s/%d ov)"),
		S.BattingTeam.IsEmpty() ? TEXT("—") : *S.BattingTeam, *S.ScoreText, *S.OversText, S.TotalOvers));

	SetText(RateLine, FString::Printf(TEXT("CRR %.2f   Ball %d/6   %s"),
		S.CurrentRunRate, S.BallNumberInOver, *S.PhaseText));

	// Batters at the crease (* marks the striker).
	SetText(StrikerLine, S.StrikerName.IsEmpty() ? TEXT("") :
		FString::Printf(TEXT("%s*  %d (%d)"), *S.StrikerName, S.StrikerRuns, S.StrikerBalls));
	SetText(NonStrikerLine, S.NonStrikerName.IsEmpty() ? TEXT("") :
		FString::Printf(TEXT("%s   %d (%d)"), *S.NonStrikerName, S.NonStrikerRuns, S.NonStrikerBalls));

	SetText(BowlerLine, S.BowlerName.IsEmpty() ? TEXT("") :
		FString::Printf(TEXT("Bowling: %s  %s"), *S.BowlerName, *S.BowlerFiguresText));

	// The bottom line is context-sensitive: result, chase maths, or target at the break.
	if (S.bMatchComplete && !S.ResultSummary.IsEmpty())
	{
		SetText(ChaseLine, FString::Printf(TEXT(">>> %s"), *S.ResultSummary));
	}
	else if (S.bChasing)
	{
		SetText(ChaseLine, FString::Printf(TEXT("Target %d   Need %d off %d   RRR %.2f"),
			S.Target, S.RunsRequired, S.BallsRemaining, S.RequiredRunRate));
	}
	else if (S.MatchState == ECricketMatchState::InningsBreak)
	{
		SetText(ChaseLine, FString::Printf(TEXT("Innings break — target %d"), S.Target));
	}
	else
	{
		SetText(ChaseLine, TEXT(""));
	}
}
