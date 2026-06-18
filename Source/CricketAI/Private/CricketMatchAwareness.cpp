#include "CricketMatchAwareness.h"
#include "CricketMatchEngine.h"
#include "CricketScoringTypes.h"

FCricketMatchSituation FCricketMatchSituation::Build(
	int32 InRuns, int32 InWickets, int32 InLegalBalls,
	int32 OversPerInnings, int32 BallsPerOver, int32 PlayersPerTeam, int32 PowerplayOvers,
	bool bInChasing, int32 InTarget, int32 ParTotalT20)
{
	FCricketMatchSituation S;
	BallsPerOver     = FMath::Max(1, BallsPerOver);
	OversPerInnings  = FMath::Max(1, OversPerInnings);
	PlayersPerTeam   = FMath::Max(2, PlayersPerTeam);

	const int32 TotalBalls = OversPerInnings * BallsPerOver;
	S.Runs            = InRuns;
	S.Wickets         = InWickets;
	S.WicketsInHand   = FMath::Max(0, (PlayersPerTeam - 1) - InWickets);
	S.LegalBalls      = InLegalBalls;
	S.BallsRemaining  = FMath::Max(0, TotalBalls - InLegalBalls);
	S.CompletedOvers  = InLegalBalls / BallsPerOver;
	S.bChasing        = bInChasing;
	S.Target          = bInChasing ? InTarget : 0;

	S.CurrentRunRate  = InLegalBalls > 0 ? (double(BallsPerOver) * InRuns / InLegalBalls) : 0.0;
	S.BallsResourceFrac = double(S.BallsRemaining) / double(TotalBalls);

	if (bInChasing && S.BallsRemaining > 0)
	{
		S.RunsRequired   = FMath::Max(0, InTarget - InRuns);
		S.RequiredRunRate = double(BallsPerOver) * S.RunsRequired / S.BallsRemaining;
	}
	else if (bInChasing)
	{
		S.RunsRequired = FMath::Max(0, InTarget - InRuns);
		S.RequiredRunRate = S.RunsRequired > 0 ? 99.0 : 0.0;
	}

	S.ProjectedTotal = FMath::RoundToInt(InRuns + S.CurrentRunRate / BallsPerOver * S.BallsRemaining);

	// --- Phase ---
	const int32 DeathStartOver = FMath::Max(OversPerInnings - 5, PowerplayOvers);
	if (S.CompletedOvers < PowerplayOvers)        { S.Phase = ECricketMatchPhase::Powerplay; }
	else if (S.CompletedOvers < DeathStartOver)   { S.Phase = ECricketMatchPhase::Middle; }
	else                                          { S.Phase = ECricketMatchPhase::Death; }

	// --- Pressure on the batting side, 0..1 ---
	// Three contributors blended: rate gap (chase), wickets lost, and time squeeze.
	const double WicketStress = 1.0 - double(S.WicketsInHand) / FMath::Max(1, PlayersPerTeam - 1);
	double RateStress = 0.0;
	if (bInChasing)
	{
		// How far the required rate sits above a comfortable ~8 rpo, saturating by ~15.
		RateStress = FMath::Clamp((S.RequiredRunRate - 7.5) / 7.5, 0.0, 1.0);
		// Almost-impossible asks pin it high.
		if (S.RequiredRunRate > 13.0) { RateStress = FMath::Max(RateStress, 0.9); }
	}
	else
	{
		// First innings: pressure to keep up with par for the phase.
		const double ParRate = double(ParTotalT20) / OversPerInnings;
		RateStress = FMath::Clamp((ParRate - S.CurrentRunRate) / 6.0, 0.0, 1.0) * 0.6;
	}
	const double TimeSqueeze = bInChasing ? (1.0 - S.BallsResourceFrac) * 0.4 : 0.0;
	S.Pressure = FMath::Clamp(0.55 * RateStress + 0.35 * WicketStress + TimeSqueeze, 0.0, 1.0);

	return S;
}

FCricketMatchSituation FCricketMatchSituation::FromEngine(const UCricketMatchEngine& Engine, int32 ParTotalT20)
{
	const FCricketMatchRules& R = Engine.GetRules();
	const ECricketMatchState MS = Engine.GetMatchState();

	if (MS != ECricketMatchState::FirstInnings && MS != ECricketMatchState::SecondInnings)
	{
		FCricketMatchSituation S;
		S.Phase = (MS == ECricketMatchState::MatchComplete) ? ECricketMatchPhase::Complete
		        : (MS == ECricketMatchState::InningsBreak)  ? ECricketMatchPhase::InningsBreak
		        :                                             ECricketMatchPhase::PreMatch;
		return S;
	}

	const FCricketInningsScorecard& Inn = Engine.GetActiveInnings();
	const bool bChasing = (MS == ECricketMatchState::SecondInnings);

	return Build(
		Inn.Totals.Runs, Inn.Totals.Wickets, Inn.Totals.LegalBalls,
		R.OversPerInnings, R.BallsPerOver, R.PlayersPerTeam, R.PowerplayOvers,
		bChasing, Engine.GetTarget(), ParTotalT20);
}
