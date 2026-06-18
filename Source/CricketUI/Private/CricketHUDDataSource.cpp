#include "CricketHUDDataSource.h"

#include "EngineUtils.h"                       // TActorIterator
#include "CricketPhysicsConstants.h"           // MsToKmh / RadSToRpm / unit helpers

#include "CricketMatchRunner.h"                // ACricketMatchRunner::GetEngine
#include "CricketMatchEngine.h"
#include "CricketBattingComponent.h"
#include "CricketBowlingComponent.h"
#include "CricketReplayComponent.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketReplayTypes.h"

using namespace CricketPhysics;

namespace
{
	/** Display-name text for any UENUM value (matches the project's debug readouts). */
	FString EnumText(const UEnum* E, int64 V)
	{
		return E ? E->GetDisplayNameTextByValue(V).ToString() : FString(TEXT("?"));
	}

	/** Coarse innings phase from overs vs the powerplay / death thresholds. */
	ECricketHUDPhase PhaseFromState(ECricketMatchState State, int32 CompletedOvers, int32 TotalOvers, int32 PowerplayOvers)
	{
		switch (State)
		{
		case ECricketMatchState::PreMatch:
		case ECricketMatchState::Toss:          return ECricketHUDPhase::PreMatch;
		case ECricketMatchState::InningsBreak:  return ECricketHUDPhase::Break;
		case ECricketMatchState::MatchComplete: return ECricketHUDPhase::Complete;
		default: break;
		}
		if (CompletedOvers < PowerplayOvers)        { return ECricketHUDPhase::Powerplay; }
		if (CompletedOvers >= TotalOvers - 4)        { return ECricketHUDPhase::Death; }
		return ECricketHUDPhase::Middle;
	}

	/** Coarse leg/straight/off label from the fine aim (handedness mirrors the sign). */
	FString DirectionLabel(double AimYawDeg, bool bRightHanded)
	{
		const double Off = bRightHanded ? AimYawDeg : -AimYawDeg;   // + opens to the off side for the striker
		if (Off > 10.0)  { return TEXT("Off side"); }
		if (Off < -10.0) { return TEXT("Leg side"); }
		return TEXT("Straight");
	}
}

// ---------------------------------------------------------------------------
// Source discovery
// ---------------------------------------------------------------------------

void UCricketHUDDataSource::ResolveSources(UWorld* World)
{
	if (!World) { return; }

	// The Match Engine lives on the self-driving match runner pawn.
	if (!MatchEngine.IsValid())
	{
		for (TActorIterator<ACricketMatchRunner> It(World); It; ++It)
		{
			if (UCricketMatchEngine* E = It->GetEngine()) { MatchEngine = E; break; }
		}
	}

	// The batting/bowling/replay controllers and the ball physics are components on
	// the playable pawns / the ball actor. Find them by class so the HUD is agnostic
	// to which pawn class carries them.
	const bool bNeedComponents =
		!Batting.IsValid() || !Bowling.IsValid() || !Replay.IsValid() || !BallPhysics.IsValid();

	if (bNeedComponents)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!Batting.IsValid())  { if (auto* C = A->FindComponentByClass<UCricketBattingComponent>())     { Batting = C; } }
			if (!Bowling.IsValid())  { if (auto* C = A->FindComponentByClass<UCricketBowlingComponent>())     { Bowling = C; } }
			if (!Replay.IsValid())   { if (auto* C = A->FindComponentByClass<UCricketReplayComponent>())      { Replay = C; } }
			if (!BallPhysics.IsValid()) { if (auto* C = A->FindComponentByClass<UCricketBallPhysicsComponent>()) { BallPhysics = C; } }
		}
	}

	RebindBounceDelegate(BallPhysics.Get());
}

void UCricketHUDDataSource::RebindBounceDelegate(UCricketBallPhysicsComponent* NewBall)
{
	if (NewBall == SubscribedBall.Get()) { return; }

	if (UCricketBallPhysicsComponent* Old = SubscribedBall.Get())
	{
		Old->OnBounce.RemoveDynamic(this, &UCricketHUDDataSource::HandleBounce);
	}
	if (NewBall)
	{
		NewBall->OnBounce.AddDynamic(this, &UCricketHUDDataSource::HandleBounce);
	}
	SubscribedBall = NewBall;
}

void UCricketHUDDataSource::HandleBounce(FCricketBounceReport Report)
{
	LastBounce = Report;
	bHasBounce = true;
	++BounceCount;
}

// ---------------------------------------------------------------------------
// Model assembly
// ---------------------------------------------------------------------------

FCricketHUDModel UCricketHUDDataSource::BuildModel() const
{
	FCricketHUDModel M;

	if (const UCricketMatchEngine* E = MatchEngine.Get())
	{
		M.bHasScoreboard = true;
		M.Scoreboard = BuildScoreboard(*E);
	}
	if (const UCricketBattingComponent* B = Batting.Get())
	{
		M.bHasBatting = true;
		M.Batting = BuildBatting(*B);
	}
	if (const UCricketBowlingComponent* Bo = Bowling.Get())
	{
		M.bHasBowling = true;
		M.Bowling = BuildBowling(*Bo);
	}
	if (const UCricketReplayComponent* R = Replay.Get())
	{
		M.bHasReplay = true;
		M.Replay = BuildReplayFromComponent(*R);
	}
	if (const UCricketBallPhysicsComponent* P = BallPhysics.Get())
	{
		M.bHasPhysics = true;
		M.Physics = BuildPhysics(*P, bHasBounce, BounceCount, LastBounce);
	}
	return M;
}

// ---------------------------------------------------------------------------
// Pure builders
// ---------------------------------------------------------------------------

FCricketScoreboardVM UCricketHUDDataSource::BuildScoreboard(const UCricketMatchEngine& Engine)
{
	FCricketScoreboardVM VM;

	const FCricketInningsScorecard& C = Engine.GetActiveInnings();
	const FCricketMatchRules& Rules = Engine.GetRules();
	const ECricketMatchState State = Engine.GetMatchState();

	VM.MatchState     = State;
	VM.MatchStateText = EnumText(StaticEnum<ECricketMatchState>(), (int64)State);
	VM.TotalOvers     = Rules.OversPerInnings;

	VM.BattingTeam    = C.BattingTeam;
	VM.BowlingTeam    = C.BowlingTeam;
	VM.Runs           = C.Totals.Runs;
	VM.Wickets        = C.Totals.Wickets;
	VM.Extras         = C.Totals.Extras;
	VM.CompletedOvers = C.CompletedOvers();
	VM.BallsThisOver  = C.BallsThisOver();
	VM.BallNumberInOver = FMath::Clamp(VM.BallsThisOver + 1, 1, FMath::Max(1, Rules.BallsPerOver));
	VM.OversText      = FString::Printf(TEXT("%d.%d"), VM.CompletedOvers, VM.BallsThisOver);
	VM.ScoreText      = FString::Printf(TEXT("%d/%d"), VM.Runs, VM.Wickets);
	VM.CurrentRunRate = C.RunRate();
	VM.RequiredRunRate = Engine.RequiredRunRate();

	// Batters at the crease.
	VM.StrikerName    = Engine.GetStrikerName();
	VM.NonStrikerName = Engine.GetNonStrikerName();
	if (C.Batters.IsValidIndex(C.StrikerIndex))
	{
		VM.StrikerRuns  = C.Batters[C.StrikerIndex].Runs;
		VM.StrikerBalls = C.Batters[C.StrikerIndex].Balls;
	}
	if (C.Batters.IsValidIndex(C.NonStrikerIndex))
	{
		VM.NonStrikerRuns  = C.Batters[C.NonStrikerIndex].Runs;
		VM.NonStrikerBalls = C.Batters[C.NonStrikerIndex].Balls;
	}

	// Current bowler.
	VM.BowlerName = Engine.GetBowlerName();
	if (C.Bowlers.IsValidIndex(C.CurrentBowler))
	{
		const FCricketBowlerStats& Bw = C.Bowlers[C.CurrentBowler];
		VM.BowlerWickets       = Bw.Wickets;
		VM.BowlerRunsConceded  = Bw.RunsConceded;
		VM.BowlerFiguresText   = FString::Printf(TEXT("%d.%d-%d-%d-%d"),
			Bw.CompletedOvers(), Bw.BallsInPartOver(), Bw.Maidens, Bw.RunsConceded, Bw.Wickets);
	}

	// Match information / chase.
	VM.bChasing       = (State == ECricketMatchState::SecondInnings);
	VM.Target         = Engine.GetTarget();
	VM.RunsRequired   = Engine.RunsRequired();
	VM.BallsRemaining = Engine.BallsRemaining();
	VM.bMatchComplete = (State == ECricketMatchState::MatchComplete);
	VM.ResultSummary  = Engine.GetResult().Summary;

	VM.Phase     = PhaseFromState(State, VM.CompletedOvers, VM.TotalOvers, Rules.PowerplayOvers);
	VM.PhaseText = EnumText(StaticEnum<ECricketHUDPhase>(), (int64)VM.Phase);

	return VM;
}

FCricketBattingVM UCricketHUDDataSource::BuildBatting(const UCricketBattingComponent& Batting)
{
	FCricketBattingVM VM;

	const FCricketBattingInput& In = Batting.GetInput();
	VM.ShotType      = In.ShotType;
	VM.ShotText      = EnumText(StaticEnum<ECricketShotType>(), (int64)In.ShotType);
	VM.Footwork      = In.Footwork;
	VM.FootworkText  = EnumText(StaticEnum<ECricketFootwork>(), (int64)In.Footwork);
	VM.AimYawDeg     = In.AimYawDeg;
	VM.DirectionText = DirectionLabel(In.AimYawDeg, In.bRightHanded);

	VM.bSwinging      = Batting.IsSwinging();
	VM.SwingPhase     = Batting.GetSwingPhase();
	VM.SwingPhaseText = EnumText(StaticEnum<ECricketSwingPhase>(), (int64)VM.SwingPhase);
	VM.BatSpeedMS     = Batting.GetCurrentBatSpeedMS();

	VM.bHasPlayed = Batting.HasPlayed();
	if (VM.bHasPlayed)
	{
		const FCricketBatImpactReport& R = Batting.GetLastReport();
		const FCricketTimingResult& T    = Batting.GetLastTiming();

		VM.bMadeContact   = R.bMadeContact;
		VM.Timing         = T.Quality;
		VM.TimingText     = EnumText(StaticEnum<ECricketTimingQuality>(), (int64)T.Quality);
		VM.TimingErrorMs  = T.TimingErrorSec * 1000.0;
		VM.TimingNormalized = T.Normalized;

		VM.ContactRegion  = R.Region;
		VM.ContactText    = EnumText(StaticEnum<ECricketContactRegion>(), (int64)R.Region);
		VM.bIsEdge        = R.bIsEdge;
		VM.ContactQuality = R.Quality;
		VM.ExitSpeedKmh   = MsToKmh(R.ExitSpeedMS);
		VM.LaunchAngleDeg = R.LaunchAngleDeg;
	}

	return VM;
}

FCricketBowlingVM UCricketHUDDataSource::BuildBowling(const UCricketBowlingComponent& Bowling)
{
	FCricketBowlingVM VM;

	const FCricketBowlingIntent& In = Bowling.GetIntent();
	VM.Movement         = In.Movement;
	VM.DeliveryTypeText = EnumText(StaticEnum<ECricketMovement>(), (int64)In.Movement);
	VM.Line             = In.Line;
	VM.LineText         = EnumText(StaticEnum<ECricketLine>(), (int64)In.Line);
	VM.Length           = In.Length;
	VM.LengthText       = EnumText(StaticEnum<ECricketLength>(), (int64)In.Length);
	VM.SwingAmount01    = In.SwingAmount;
	VM.SpinAmount01     = In.SpinAmount;

	const FCricketReleaseParameters& P = Bowling.GetLastReleaseParams();
	const FCricketDeliveryDiagnostics& D = Bowling.GetLastDiagnostics();
	VM.PaceKmh  = MsToKmh(P.ReleaseSpeedMS);
	VM.SpinRPM  = P.SpinRateRPM;

	VM.bHasDelivery     = D.bAimConverged || D.PredictedLengthM > KINDA_SMALL_NUMBER;
	VM.PredictedLengthM = D.PredictedLengthM;
	VM.PredictedLineM   = D.PredictedLineAtPitchM;
	VM.FreeFlightSwingM = D.FreeFlightSwingM;
	VM.Regime           = D.Regime;
	VM.RegimeText       = EnumText(StaticEnum<ECricketSwingRegime>(), (int64)D.Regime);

	return VM;
}

FCricketReplayVM UCricketHUDDataSource::BuildReplay(bool bReplaying, bool bPaused, double Rate,
	double NormalizedTime, int32 FrameCount, double DurationSec, int32 EventCount)
{
	FCricketReplayVM VM;
	VM.bReplaying     = bReplaying;
	VM.bPaused        = bPaused;
	VM.Rate           = Rate;
	VM.RateText       = FString::Printf(TEXT("%.2fx"), Rate);
	VM.NormalizedTime = FMath::Clamp(NormalizedTime, 0.0, 1.0);
	VM.FrameCount     = FrameCount;
	VM.DurationSec    = DurationSec;
	VM.EventCount     = EventCount;
	VM.TimelineText   = FString::Printf(TEXT("%.1fs / %.1fs"), VM.NormalizedTime * DurationSec, DurationSec);
	return VM;
}

FCricketReplayVM UCricketHUDDataSource::BuildReplayFromComponent(const UCricketReplayComponent& Replay)
{
	const FCricketReplayClip& Clip = Replay.GetClip();
	return BuildReplay(
		Replay.IsReplaying(),
		Replay.IsPaused(),
		Replay.GetPlaybackRate(),
		Replay.GetNormalizedTime(),
		Replay.GetFrameCount(),
		Clip.Duration(),
		Clip.Events.Num());
}

FCricketPhysicsVM UCricketHUDDataSource::BuildPhysics(const UCricketBallPhysicsComponent& Ball,
	bool bInHasBounce, int32 InBounceCount, const FCricketBounceReport& LastBounce)
{
	FCricketPhysicsVM VM;

	const FCricketBallState& S = Ball.GetState();
	VM.bInFlight     = Ball.IsBallInFlight();
	VM.SpeedKmh      = Ball.GetSpeedKmh();
	VM.SpinRPM       = Ball.GetSpinRPM();
	VM.SpinAxis      = S.AngularVelocity.GetSafeNormal();
	VM.SeamNormal    = S.SeamNormal.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 1, 0));
	VM.SeamStability = S.SeamStability;

	const FCricketAeroResult& Aero = Ball.GetLastAero();
	VM.DragForceN     = Aero.DragForce.Size();
	VM.MagnusForceN   = Aero.MagnusForce.Size();
	VM.SwingForceN    = Aero.SwingForce.Size();
	VM.ReverseRegime  = Aero.ReverseRegime;
	VM.ReynoldsNumber = Aero.ReynoldsNumber;

	VM.BounceCount = InBounceCount;
	VM.bHasBounce  = bInHasBounce;
	if (bInHasBounce)
	{
		VM.LastBounceAngleDeg    = LastBounce.BounceAngleDeg;
		VM.LastBounceHeightM     = LastBounce.BounceHeightM;
		VM.LastTurnMS            = LastBounce.TurnMS;
		VM.LastSeamDeviationMS   = LastBounce.SeamDeviationMS;
		VM.LastSpeedRetainedFrac = LastBounce.SpeedRetainedFrac;
	}

	return VM;
}
