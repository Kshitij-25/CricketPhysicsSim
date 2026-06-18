#include "CricketAIMatchSimulator.h"
#include "CricketMatchAwareness.h"
#include "CricketCaptainBrain.h"
#include "CricketFieldingBrain.h"
#include "CricketContestModel.h"
#include "CricketMatchEngine.h"
#include "CricketOutcomeInterpreter.h"
#include "CricketTeamDataAsset.h"

// ---------------------------------------------------------------------------
// FCricketAITeam
// ---------------------------------------------------------------------------

const FCricketAIPlayerProfile& FCricketAITeam::ProfileByName(const FString& Name) const
{
	for (const FCricketAIPlayerProfile& P : Profiles)
	{
		if (P.Name == Name) { return P; }
	}
	static const FCricketAIPlayerProfile Default;
	return Profiles.Num() ? Profiles[0] : Default;
}

FCricketAITeam FCricketAITeam::FromProfiles(const FCricketSquad& InSquad, const TArray<FCricketAIPlayerProfile>& InProfiles, const FCricketTeamStrategy& InStrategy)
{
	FCricketAITeam T; T.Squad = InSquad; T.Profiles = InProfiles; T.Strategy = InStrategy;
	return T;
}

FCricketAITeam FCricketAITeam::FromTeamData(const UCricketTeamDataAsset& Data, ECricketAIDifficulty Difficulty)
{
	FCricketAITeam T;
	T.Squad.TeamName  = Data.TeamName;
	T.Squad.ShortCode = Data.ShortCode;
	T.Strategy.Difficulty = Difficulty;
	for (const FCricketPlayer& P : Data.Players)
	{
		T.Squad.PlayerNames.Add(P.Name);
		T.Profiles.Add(FCricketAIPlayerProfile::Derive(P));
	}
	return T;
}

// ---------------------------------------------------------------------------
// Simulator
// ---------------------------------------------------------------------------

namespace
{
	void InitTelemetry(FCricketAIInningsTelemetry& T)
	{
		T.ActionCounts.Init(0, 5);
		T.BowlLengthCounts.Init(0, 7);
		T.Dismissals.Init(0, 6);
	}
}

FCricketAIMatchTelemetry FCricketAIMatchSimulator::Simulate(
	const FCricketAITeam& TeamA,
	const FCricketAITeam& TeamB,
	const FCricketMatchRules& Rules,
	int32 Seed,
	bool bTeamABatsFirst)
{
	FCricketAIMatchTelemetry Tel;

	UCricketMatchEngine* Engine = NewObject<UCricketMatchEngine>();
	Engine->ConfigureMatch(Rules, TeamA.Squad, TeamB.Squad);
	Engine->StartMatch();
	Engine->PerformToss(/*TossWinner*/ bTeamABatsFirst ? 0 : 1, /*bWinnerBatsFirst*/ true);

	FRandomStream Rng(Seed);

	// Per-innings telemetry accumulators (index 0/1 aligned with engine innings).
	FCricketAIInningsTelemetry InnTel[2];
	InitTelemetry(InnTel[0]);
	InitTelemetry(InnTel[1]);

	TMap<FString, FCricketBowlerMemory> Memory;
	FString CurrentOverBowler;

	auto SideForTeam = [&TeamA, &TeamB](const FString& TeamName) -> const FCricketAITeam&
	{
		return (TeamA.Squad.TeamName == TeamName) ? TeamA : TeamB;
	};

	const int32 MaxIters = (Rules.OversPerInnings * Rules.BallsPerOver) * 4 + 64; // generous safety bound
	int32 Iters = 0;

	while (Engine->GetMatchState() != ECricketMatchState::MatchComplete && Iters++ < MaxIters)
	{
		const ECricketMatchState MS = Engine->GetMatchState();
		if (MS == ECricketMatchState::InningsBreak)
		{
			Engine->StartSecondInnings();
			CurrentOverBowler.Reset();
			continue;
		}
		if (!Engine->IsLive()) { break; }

		const FCricketInningsScorecard& Inn = Engine->GetActiveInnings();
		const FCricketAITeam& BatSide  = SideForTeam(Inn.BattingTeam);
		const FCricketAITeam& BowlSide = SideForTeam(Inn.BowlingTeam);

		const FCricketMatchSituation Sit = FCricketMatchSituation::FromEngine(*Engine, BatSide.Strategy.ParTotalT20);

		// --- Need a bowler for the new over? Captain picks one. ---
		if (Engine->NeedsBowler())
		{
			TArray<FCricketBowlerOption> Pool;
			for (const FString& Name : BowlSide.Squad.PlayerNames)
			{
				const FCricketAIPlayerProfile& Prof = BowlSide.ProfileByName(Name);
				if (!Prof.CanBowl()) { continue; }

				FCricketBowlerOption Opt;
				Opt.Name = Name; Opt.Profile = Prof;
				Opt.MaxOvers = Rules.MaxOversPerBowler;
				Opt.bBowledLastOver = (Name == CurrentOverBowler);
				for (const FCricketBowlerStats& BS : Inn.Bowlers)
				{
					if (BS.Name == Name) { Opt.OversBowled = BS.CompletedOvers(); Opt.RunsConceded = BS.RunsConceded; Opt.Wickets = BS.Wickets; break; }
				}
				Pool.Add(Opt);
			}

			FCricketAIDifficultyParams CaptainD = FCricketAIDifficultyParams::Resolve(BowlSide.Strategy.Difficulty);
			FCricketBowlingChange Change = FCricketCaptainBrain::ChooseBowler(Sit, Pool, BowlSide.Strategy, CaptainD, Rng);

			bool bSet = Change.bValid && Engine->SetBowler(Change.BowlerName);
			if (!bSet)
			{
				for (const FString& Name : BowlSide.Squad.PlayerNames)
				{
					if (BowlSide.ProfileByName(Name).CanBowl() && Engine->CanBowl(Name) && Engine->SetBowler(Name))
					{ Change.BowlerName = Name; bSet = true; break; }
				}
			}
			if (!bSet) { break; } // no legal bowler — should not happen with a full XI
			CurrentOverBowler = Change.BowlerName;
			continue;
		}

		// --- Bowl one ball ---
		const int32 InnIdx = Engine->GetActiveInningsIndex();
		FCricketAIInningsTelemetry& TT = InnTel[FMath::Clamp(InnIdx, 0, 1)];

		const FString StrikerName = Engine->GetStrikerName();
		const FString BowlerName  = Engine->GetBowlerName();
		const FCricketAIPlayerProfile& Batter = BatSide.ProfileByName(StrikerName);
		const FCricketAIPlayerProfile& Bowler = BowlSide.ProfileByName(BowlerName);

		const FCricketAIDifficultyParams BowlD = FCricketAIDifficultyParams::Resolve(BowlSide.Strategy.Difficulty);
		const FCricketAIDifficultyParams BatD  = FCricketAIDifficultyParams::Resolve(BatSide.Strategy.Difficulty);

		// Field placement (legal under the powerplay restriction) -> what the batter reads.
		const int32 FieldersOut = (Sit.Phase == ECricketMatchPhase::Powerplay) ? Rules.PowerplayFieldersOut : Rules.NonPowerplayFieldersOut;
		const FCricketFieldPlacement Field = FCricketCaptainBrain::SetField(Sit, Bowler, Batter, BowlSide.Strategy, FieldersOut);
		double BoundaryProtection = 0.4, CatchersUp = 0.3;
		FCricketCaptainBrain::ReadField(Field, BoundaryProtection, CatchersUp);

		// Bowler decides the delivery.
		FCricketBowlerMemory& Mem = Memory.FindOrAdd(BowlerName);
		const FCricketBowlerDecision BowlDec = FCricketBowlerBrain::Decide(Sit, Bowler, Batter, Mem, BowlD, Rng);

		// The batter reads that ball + field.
		FCricketDeliveryRead BallRead;
		BallRead.Line        = BowlDec.Intent.Line;
		BallRead.Length      = BowlDec.Intent.Length;
		BallRead.Movement    = BowlDec.Intent.Movement;
		BallRead.BowlerStyle = Bowler.Bowling.Style;
		BallRead.BowlerPace01 = BowlDec.Intent.Pace01;
		BallRead.BoundaryProtection = BoundaryProtection;
		BallRead.CatchersUp  = CatchersUp;

		const FCricketBatterDecision BatDec = FCricketBatterBrain::Decide(Sit, Batter, BallRead, BatSide.Strategy, BatD, Rng);

		// Resolve the contest -> raw facts -> interpret -> apply the laws.
		const FCricketBallResult Result = FCricketContestModel::Resolve(BowlDec, BatDec, Bowler, Batter, BallRead, Rng);
		const FCricketDeliveryOutcome Outcome = FCricketOutcomeInterpreter::Interpret(Result);

		// --- Telemetry (captured before applying) ---
		TT.ActionCounts[FMath::Clamp((int32)BatDec.Action, 0, 4)]++;
		TT.BowlLengthCounts[FMath::Clamp((int32)BowlDec.Intent.Length, 0, 6)]++;
		if (Outcome.bBoundary && Outcome.RunsOffBat == 4) { TT.Fours++; }
		if (Outcome.bBoundary && Outcome.RunsOffBat == 6) { TT.Sixes++; }
		if (Outcome.Legality != ECricketDeliveryLegality::Legal) { TT.Extras++; }
		else if (Outcome.RunsOffBat == 0 && Outcome.RanExtraRuns == 0 && Outcome.Dismissal == ECricketDismissal::NotOut) { TT.Dots++; }
		if (Outcome.Dismissal != ECricketDismissal::NotOut) { TT.Dismissals[FMath::Clamp((int32)Outcome.Dismissal, 0, 5)]++; }

		const bool bWicket = (Outcome.Dismissal != ECricketDismissal::NotOut);
		Engine->ApplyDelivery(Outcome);

		Mem.Record(BowlDec.Intent.Length, BowlDec.Intent.Line);
		if (bWicket) { Mem.NewBatter(); }
	}

	// --- Finalise telemetry from the authoritative scorecards ---
	for (int32 i = 0; i < 2; ++i)
	{
		const FCricketInningsScorecard& Card = Engine->GetInnings(i);
		if (Card.BattingTeam.IsEmpty() && Card.Totals.LegalBalls == 0) { continue; }
		FCricketAIInningsTelemetry T = InnTel[i];
		T.BattingTeam = Card.BattingTeam;
		T.Runs        = Card.Totals.Runs;
		T.Wickets     = Card.Totals.Wickets;
		T.LegalBalls  = Card.Totals.LegalBalls;
		Tel.Innings.Add(T);
		Tel.TotalBalls += T.LegalBalls;
	}

	Tel.bCompleted   = (Engine->GetMatchState() == ECricketMatchState::MatchComplete);
	Tel.ResultSummary = Engine->GetResult().Summary;
	return Tel;
}
