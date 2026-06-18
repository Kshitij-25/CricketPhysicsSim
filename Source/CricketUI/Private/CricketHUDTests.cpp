// Headless automation tests for the HUD data-binding layer (the only part of the UI
// with logic to test — widgets are pure layout). Each test drives a real gameplay
// source and asserts the view-model the UI would display. No widgets, no RHI.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.UI; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"

#include "CricketHUDDataSource.h"
#include "CricketHUDTypes.h"
#include "CricketMatchEngine.h"
#include "CricketScoringTypes.h"
#include "CricketMatchTypes.h"
#include "CricketReplayTypes.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketPitchInteraction.h"
#include "CricketPhysicsConstants.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace CricketPhysics;

namespace
{
	using FOutcome = FCricketDeliveryOutcome;

	FCricketSquad MakeSquad(const FString& Team, const FString& Code)
	{
		FCricketSquad S; S.TeamName = Team; S.ShortCode = Code;
		for (int32 i = 0; i < 11; ++i) { S.PlayerNames.Add(FString::Printf(TEXT("%s%d"), *Code, i)); }
		return S;
	}

	// India batting first vs Australia, in FirstInnings (mirrors the match-engine tests).
	UCricketMatchEngine* NewTossed(int32 Overs = 20)
	{
		UCricketMatchEngine* E = NewObject<UCricketMatchEngine>();
		FCricketMatchRules R; R.OversPerInnings = Overs;
		E->ConfigureMatch(R, MakeSquad(TEXT("India"), TEXT("IND")), MakeSquad(TEXT("Australia"), TEXT("AUS")));
		E->StartMatch();
		E->PerformToss(0, /*bWinnerBatsFirst*/ true);
		return E;
	}
}

// 1. SCORE UPDATES — runs, overs, score/over strings and the run rate mirror the engine.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketUIScoreTest,
	"CricketSim.UI.ScoreUpdates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketUIScoreTest::RunTest(const FString&)
{
	UCricketMatchEngine* E = NewTossed();
	E->SetBowler(TEXT("AUS0"));

	E->ApplyDelivery(FOutcome::Runs(1));   // 1
	E->ApplyDelivery(FOutcome::Four());    // 5
	E->ApplyDelivery(FOutcome::Dot());     // 5
	E->ApplyDelivery(FOutcome::Six());     // 11, 4 balls

	const FCricketScoreboardVM VM = UCricketHUDDataSource::BuildScoreboard(*E);

	TestEqual(TEXT("Batting team"), VM.BattingTeam, FString(TEXT("India")));
	TestEqual(TEXT("Runs"), VM.Runs, 11);
	TestEqual(TEXT("Wickets"), VM.Wickets, 0);
	TestEqual(TEXT("Balls this over"), VM.BallsThisOver, 4);
	TestEqual(TEXT("Overs text"), VM.OversText, FString(TEXT("0.4")));
	TestEqual(TEXT("Score text"), VM.ScoreText, FString(TEXT("11/0")));
	TestEqual(TEXT("Ball number in over"), VM.BallNumberInOver, 5);
	TestTrue(TEXT("CRR matches the engine"), FMath::IsNearlyEqual(VM.CurrentRunRate, E->GetActiveInnings().RunRate(), 1e-6));
	TestFalse(TEXT("Not chasing in the first innings"), VM.bChasing);
	TestFalse(TEXT("Striker name is present"), VM.StrikerName.IsEmpty());
	return true;
}

// 2. WICKET UPDATES — a dismissal bumps the team wickets and credits the bowler.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketUIWicketTest,
	"CricketSim.UI.WicketUpdates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketUIWicketTest::RunTest(const FString&)
{
	UCricketMatchEngine* E = NewTossed();
	E->SetBowler(TEXT("AUS0"));

	E->ApplyDelivery(FOutcome::Runs(2));               // 2/0
	E->ApplyDelivery(FOutcome::Out(ECricketDismissal::Bowled));  // 2/1

	const FCricketScoreboardVM VM = UCricketHUDDataSource::BuildScoreboard(*E);

	TestEqual(TEXT("Wicket counted"), VM.Wickets, 1);
	TestEqual(TEXT("Score text"), VM.ScoreText, FString(TEXT("2/1")));
	TestEqual(TEXT("Bowler credited"), VM.BowlerWickets, 1);
	TestFalse(TEXT("Bowler figures present"), VM.BowlerFiguresText.IsEmpty());
	return true;
}

// 3. OVER TRANSITIONS — six legal balls roll the over; the next ball reads "1.1".
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketUIOverTest,
	"CricketSim.UI.OverTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketUIOverTest::RunTest(const FString&)
{
	UCricketMatchEngine* E = NewTossed();
	E->SetBowler(TEXT("AUS0"));
	for (int32 i = 0; i < 6; ++i) { E->ApplyDelivery(FOutcome::Dot()); }

	FCricketScoreboardVM VM = UCricketHUDDataSource::BuildScoreboard(*E);
	TestEqual(TEXT("Over rolled"), VM.CompletedOvers, 1);
	TestEqual(TEXT("Fresh over"), VM.BallsThisOver, 0);
	TestEqual(TEXT("Overs text after over"), VM.OversText, FString(TEXT("1.0")));

	// New over needs a (different, legal) bowler before the next ball.
	TestTrue(TEXT("Needs a bowler"), E->NeedsBowler());
	E->SetBowler(TEXT("AUS1"));
	E->ApplyDelivery(FOutcome::Runs(1));

	VM = UCricketHUDDataSource::BuildScoreboard(*E);
	TestEqual(TEXT("Still one completed over"), VM.CompletedOvers, 1);
	TestEqual(TEXT("One ball into the new over"), VM.BallsThisOver, 1);
	TestEqual(TEXT("Overs text mid-over"), VM.OversText, FString(TEXT("1.1")));
	return true;
}

// 4. REPLAY CONTROLS — speed, pause and the timeline cursor flow into the VM.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketUIReplayTest,
	"CricketSim.UI.ReplayControls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketUIReplayTest::RunTest(const FString&)
{
	// A one-second clip at 60 Hz.
	FCricketReplayClip Clip;
	for (int32 i = 0; i <= 60; ++i) { FCricketReplayFrame F; F.Time = i / 60.0; Clip.AddFrame(F); }

	FCricketReplayPlayer P;
	P.Start(Clip);

	auto VMOf = [&]() {
		return UCricketHUDDataSource::BuildReplay(P.bPlaying, P.bPaused, P.Rate,
			P.NormalizedTime(), Clip.NumFrames(), Clip.Duration(), Clip.Events.Num());
	};

	FCricketReplayVM VM = VMOf();
	TestTrue(TEXT("Replaying"), VM.bReplaying);
	TestEqual(TEXT("Frame count"), VM.FrameCount, 61);
	TestTrue(TEXT("Duration ~1s"), FMath::IsNearlyEqual(VM.DurationSec, 1.0, 1e-6));
	TestEqual(TEXT("Default speed text"), VM.RateText, FString(TEXT("1.00x")));
	TestTrue(TEXT("Cursor at the start"), FMath::IsNearlyEqual(VM.NormalizedTime, 0.0, 1e-6));

	// Slow-mo + advance: 0.25x for 0.5s wall-clock = 0.125s of clip = 12.5%.
	P.SetRate(0.25);
	P.Advance(0.5);
	VM = VMOf();
	TestEqual(TEXT("Slow-mo speed text"), VM.RateText, FString(TEXT("0.25x")));
	TestTrue(TEXT("Cursor advanced to 12.5%"), FMath::IsNearlyEqual(VM.NormalizedTime, 0.125, 1e-3));

	// Pause + frame-step.
	P.TogglePause();
	VM = VMOf();
	TestTrue(TEXT("Paused"), VM.bPaused);
	const double BeforeStep = VM.NormalizedTime;
	P.StepFrames(Clip, 12);
	VM = VMOf();
	TestTrue(TEXT("Frame-step moved the cursor forward"), VM.NormalizedTime > BeforeStep);
	return true;
}

// 5. PHYSICS OVERLAY ACCURACY — the overlay mirrors the live ball state exactly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketUIPhysicsTest,
	"CricketSim.UI.PhysicsOverlayAccuracy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketUIPhysicsTest::RunTest(const FString&)
{
	UCricketBallPhysicsComponent* Ball = NewObject<UCricketBallPhysicsComponent>();

	const double SpeedMS = 38.0;                 // ~136.8 km/h
	const double Rpm = 1800.0;
	const FVector Vel(SpeedMS, 0.0, 0.0);
	const FVector SpinAxis(0.0, -1.0, 0.0);      // leg-spin-like axis
	const FVector AngVel = SpinAxis * RpmToRadS(Rpm);
	const FVector Seam(0.0, 1.0, 0.0);

	Ball->Release(FVector(0, 0, 100), Vel, AngVel, Seam);

	const FCricketBounceReport NoBounce;
	const FCricketPhysicsVM VM = UCricketHUDDataSource::BuildPhysics(*Ball, /*bHasBounce*/ false, 0, NoBounce);

	TestTrue(TEXT("In flight"), VM.bInFlight);
	TestTrue(TEXT("Speed mirrors the state (km/h)"), FMath::IsNearlyEqual(VM.SpeedKmh, MsToKmh(SpeedMS), 0.1));
	TestTrue(TEXT("RPM mirrors the state"), FMath::IsNearlyEqual(VM.SpinRPM, Rpm, 0.5));
	TestTrue(TEXT("Spin axis mirrors the state"), (VM.SpinAxis - SpinAxis).IsNearlyZero(1e-3));
	TestTrue(TEXT("Seam normal mirrors the state"), (VM.SeamNormal - Seam).IsNearlyZero(1e-3));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
