#include "CricketHUD.h"
#include "CricketHUDDataSource.h"
#include "Widgets/CricketHUDLayout.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

namespace
{
	TAutoConsoleVariable<int32> CVarUIWidgets(TEXT("cricket.UI.Widgets"), 1,
		TEXT("Player-facing UMG HUD (scoreboard/batting/bowling/replay). 0=off, 1=on"));
	TAutoConsoleVariable<int32> CVarUIPhysics(TEXT("cricket.UI.Debug.Physics"), 0,
		TEXT("Physics Overlay: ball speed/rpm/spin axis/seam/swing/bounce. 0=off, 1=on"));
	TAutoConsoleVariable<int32> CVarUIMatch(TEXT("cricket.UI.Debug.Match"), 0,
		TEXT("Match State Overlay: state machine + innings + chase. 0=off, 1=on"));
	TAutoConsoleVariable<int32> CVarUIDev(TEXT("cricket.UI.Debug.Dev"), 0,
		TEXT("Developer HUD: which gameplay sources the HUD resolved + model summary. 0=off, 1=on"));
}

ACricketHUD::ACricketHUD()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACricketHUD::BeginPlay()
{
	Super::BeginPlay();

	DataSource = NewObject<UCricketHUDDataSource>(this, TEXT("CricketHUDDataSource"));

	if (!LayoutClass) { LayoutClass = UCricketHUDLayout::StaticClass(); }
	if (APlayerController* PC = GetOwningPlayerController())
	{
		Layout = CreateWidget<UCricketHUDLayout>(PC, LayoutClass);
		if (Layout) { Layout->AddToViewport(0); }
	}
}

void ACricketHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Layout) { Layout->RemoveFromParent(); Layout = nullptr; }
	Super::EndPlay(Reason);
}

void ACricketHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!DataSource) { return; }
	DataSource->ResolveSources(GetWorld());
	Model = DataSource->BuildModel();

	const bool bShowWidgets = CVarUIWidgets.GetValueOnGameThread() != 0;
	if (Layout)
	{
		Layout->SetVisibility(bShowWidgets ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowWidgets) { Layout->UpdateFromModel(Model); }
	}
}

void ACricketHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) { return; }

	if (CVarUIPhysics.GetValueOnGameThread() != 0) { DrawPhysicsOverlay(); }
	if (CVarUIMatch.GetValueOnGameThread()   != 0) { DrawMatchOverlay(); }
	if (CVarUIDev.GetValueOnGameThread()     != 0) { DrawDevOverlay(); }
}

void ACricketHUD::DrawLine(const FString& Text, const FLinearColor& Color, float& InOutY, float X)
{
	if (!Canvas) { return; }
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	FCanvasTextItem Item(FVector2D(X, InOutY), FText::FromString(Text), Font ? Font : GEngine->GetSmallFont(), Color);
	Item.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Item);
	InOutY += 18.f;
}

void ACricketHUD::DrawPhysicsOverlay()
{
	if (!Model.bHasPhysics) { return; }
	const FCricketPhysicsVM& P = Model.Physics;

	// Anchor on the right half so it doesn't fight the bowling panel.
	float Y = 80.f;
	const float X = Canvas->ClipX * 0.62f;
	auto Line = [&](const FString& S, const FLinearColor& C) { DrawLine(S, C, Y, X); };

	Line(TEXT("=== PHYSICS OVERLAY ==="), FLinearColor(0.5f, 0.9f, 1.0f));
	Line(FString::Printf(TEXT("In flight: %s   Speed %.1f km/h   Spin %.0f rpm"),
		P.bInFlight ? TEXT("yes") : TEXT("no"), P.SpeedKmh, P.SpinRPM), FLinearColor::White);
	Line(FString::Printf(TEXT("Spin axis (%.2f, %.2f, %.2f)"), P.SpinAxis.X, P.SpinAxis.Y, P.SpinAxis.Z),
		FLinearColor(1.0f, 0.5f, 1.0f));
	Line(FString::Printf(TEXT("Seam normal (%.2f, %.2f, %.2f)  stability %.2f"),
		P.SeamNormal.X, P.SeamNormal.Y, P.SeamNormal.Z, P.SeamStability), FLinearColor(1.0f, 0.75f, 0.4f));
	Line(FString::Printf(TEXT("Aero: drag %.2f N  magnus %.2f N  swing %.2f N"),
		P.DragForceN, P.MagnusForceN, P.SwingForceN), FLinearColor(0.7f, 1.0f, 0.7f));
	Line(FString::Printf(TEXT("Regime %.2f (0=conv,1=rev)   Re %.0f"), P.ReverseRegime, P.ReynoldsNumber),
		FLinearColor(0.8f, 0.8f, 0.8f));
	if (P.bHasBounce)
	{
		Line(FString::Printf(TEXT("Bounce #%d: angle %.1f°  apex %.2f m  turn %.2f  seam %.2f  pace kept %.0f%%"),
			P.BounceCount, P.LastBounceAngleDeg, P.LastBounceHeightM, P.LastTurnMS, P.LastSeamDeviationMS,
			P.LastSpeedRetainedFrac * 100.0), FLinearColor(1.0f, 0.9f, 0.5f));
	}
}

void ACricketHUD::DrawMatchOverlay()
{
	if (!Model.bHasScoreboard) { return; }
	const FCricketScoreboardVM& S = Model.Scoreboard;

	float Y = 80.f;
	auto Line = [&](const FString& Str, const FLinearColor& C) { DrawLine(Str, C, Y); };

	Line(TEXT("=== MATCH STATE OVERLAY ==="), FLinearColor(0.5f, 0.9f, 1.0f));
	Line(FString::Printf(TEXT("State: %s   Phase: %s"), *S.MatchStateText, *S.PhaseText), FLinearColor::White);
	Line(FString::Printf(TEXT("%s %s (%s/%d)   Extras %d   CRR %.2f"),
		*S.BattingTeam, *S.ScoreText, *S.OversText, S.TotalOvers, S.Extras, S.CurrentRunRate), FLinearColor(0.6f, 1.0f, 0.6f));
	if (S.bChasing)
	{
		Line(FString::Printf(TEXT("Chase: target %d  need %d off %d  RRR %.2f"),
			S.Target, S.RunsRequired, S.BallsRemaining, S.RequiredRunRate), FLinearColor(0.5f, 0.85f, 1.0f));
	}
	if (S.bMatchComplete)
	{
		Line(FString::Printf(TEXT("Result: %s"), *S.ResultSummary), FLinearColor(1.0f, 0.95f, 0.4f));
	}
}

void ACricketHUD::DrawDevOverlay()
{
	float Y = 80.f;
	const float X = Canvas->ClipX * 0.30f;
	auto Line = [&](const FString& S, const FLinearColor& C) { DrawLine(S, C, Y, X); };

	Line(TEXT("=== DEVELOPER HUD ==="), FLinearColor(0.5f, 0.9f, 1.0f));
	Line(FString::Printf(TEXT("Sources  score:%s  bat:%s  bowl:%s  replay:%s  physics:%s"),
		Model.bHasScoreboard ? TEXT("Y") : TEXT("-"),
		Model.bHasBatting    ? TEXT("Y") : TEXT("-"),
		Model.bHasBowling    ? TEXT("Y") : TEXT("-"),
		Model.bHasReplay     ? TEXT("Y") : TEXT("-"),
		Model.bHasPhysics    ? TEXT("Y") : TEXT("-")), FLinearColor::White);
	if (Model.bHasPhysics)
	{
		Line(FString::Printf(TEXT("Ball: %.1f km/h  %.0f rpm  bounces %d"),
			Model.Physics.SpeedKmh, Model.Physics.SpinRPM, Model.Physics.BounceCount), FLinearColor(0.8f, 0.8f, 0.8f));
	}
	if (Model.bHasReplay)
	{
		Line(FString::Printf(TEXT("Replay: %s  %s  t=%.0f%%"),
			Model.Replay.bReplaying ? TEXT("active") : TEXT("idle"), *Model.Replay.RateText,
			Model.Replay.NormalizedTime * 100.0), FLinearColor(0.8f, 0.8f, 0.8f));
	}
}
