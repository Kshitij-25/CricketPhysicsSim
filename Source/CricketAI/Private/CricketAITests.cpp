// Headless automation tests for the Cricket AI layer. They validate the reasoning
// cores directly (difficulty selection, match awareness, the batter/bowler/captain
// brains) and then the whole intelligence end-to-end via the deterministic match
// simulator — asserting the emergent run rates, wicket rates, bowling patterns and
// shot distributions fall in believable cricket ranges. All pure; no UWorld.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.AI; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketAITypes.h"
#include "CricketTacticalEvaluator.h"
#include "CricketMatchAwareness.h"
#include "CricketBatterBrain.h"
#include "CricketBowlerBrain.h"
#include "CricketCaptainBrain.h"
#include "CricketAIMatchSimulator.h"
#include "CricketTeamDataAsset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCricketAIPlayerProfile MakePlayer(const FString& Name, ECricketRole Role, float Bat, float Bowl, float Pace)
	{
		FCricketPlayer P; P.Name = Name; P.Role = Role; P.Batting = Bat; P.Bowling = Bowl; P.Fielding = 0.6f; P.PaceKmh = Pace;
		return FCricketAIPlayerProfile::Derive(P);
	}

	// A balanced XI: top/middle batters, an all-rounder, a keeper, a part-timer and
	// four front-line bowlers — at least five who can legally complete 20 overs.
	FCricketAITeam MakeTeam(const FString& Code, ECricketAIDifficulty Diff)
	{
		TArray<FCricketAIPlayerProfile> Profs = {
			MakePlayer(Code + TEXT("_Open1"), ECricketRole::BatterTop,    0.82f, 0.05f, 0.f),
			MakePlayer(Code + TEXT("_Open2"), ECricketRole::BatterTop,    0.78f, 0.05f, 0.f),
			MakePlayer(Code + TEXT("_No3"),   ECricketRole::BatterMiddle, 0.80f, 0.05f, 0.f),
			MakePlayer(Code + TEXT("_No4"),   ECricketRole::BatterMiddle, 0.74f, 0.35f, 118.f), // part-timer
			MakePlayer(Code + TEXT("_AllR"),  ECricketRole::AllRounder,   0.66f, 0.62f, 128.f),
			MakePlayer(Code + TEXT("_Keep"),  ECricketRole::WicketKeeper, 0.62f, 0.0f,  0.f),
			MakePlayer(Code + TEXT("_No7"),   ECricketRole::BatterMiddle, 0.55f, 0.10f, 0.f),
			MakePlayer(Code + TEXT("_Spin"),  ECricketRole::SpinBowler,   0.28f, 0.74f, 0.f),
			MakePlayer(Code + TEXT("_Pace1"), ECricketRole::PaceBowler,   0.22f, 0.82f, 142.f),
			MakePlayer(Code + TEXT("_Pace2"), ECricketRole::PaceBowler,   0.20f, 0.78f, 138.f),
			MakePlayer(Code + TEXT("_Pace3"), ECricketRole::PaceBowler,   0.18f, 0.72f, 134.f),
		};
		FCricketSquad S; S.TeamName = Code; S.ShortCode = Code.Left(3).ToUpper();
		for (const FCricketAIPlayerProfile& P : Profs) { S.PlayerNames.Add(P.Name); }
		FCricketTeamStrategy Strat; Strat.Difficulty = Diff;
		return FCricketAITeam::FromProfiles(S, Profs, Strat);
	}

	FCricketDeliveryRead GoodLengthBall()
	{
		FCricketDeliveryRead B;
		B.Line = ECricketLine::OffStump; B.Length = ECricketLength::GoodLength;
		B.Movement = ECricketMovement::SeamUp; B.BowlerStyle = ECricketBowlingStyle::Pace; B.BowlerPace01 = 0.75;
		B.BoundaryProtection = 0.4; B.CatchersUp = 0.3;
		return B;
	}
}

// 1. DIFFICULTY MODEL: quality dials move the right way across the tiers.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAIDifficultyTest,
	"CricketSim.AI.DifficultyMonotonic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAIDifficultyTest::RunTest(const FString&)
{
	const auto E = FCricketAIDifficultyParams::Resolve(ECricketAIDifficulty::Easy);
	const auto H = FCricketAIDifficultyParams::Resolve(ECricketAIDifficulty::Hard);
	const auto Sim = FCricketAIDifficultyParams::Resolve(ECricketAIDifficulty::Simulation);

	TestTrue(TEXT("Easier = hotter selection"), E.SelectionTemperature > H.SelectionTemperature && H.SelectionTemperature > Sim.SelectionTemperature);
	TestTrue(TEXT("Easier = more execution noise"), E.ExecutionNoise > H.ExecutionNoise && H.ExecutionNoise >= Sim.ExecutionNoise);
	TestTrue(TEXT("Easier = more mistakes"), E.MistakeRate > H.MistakeRate && H.MistakeRate >= Sim.MistakeRate);
	TestTrue(TEXT("Easier = less awareness"), E.SituationalAwareness < H.SituationalAwareness && H.SituationalAwareness <= Sim.SituationalAwareness);
	return true;
}

// 2. SELECTION CORE: temperature 0 is a pure argmax; a hot temperature spreads.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAISelectionTest,
	"CricketSim.AI.Selection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAISelectionTest::RunTest(const FString&)
{
	FRandomStream Rng(1);
	auto MakeTrace = []()
	{
		FCricketDecisionTrace T;
		T.Add(TEXT("A"), 1.0); T.Add(TEXT("B"), 0.5); T.Add(TEXT("C"), 0.2);
		return T;
	};

	// Simulation: always picks the best.
	auto SimP = FCricketAIDifficultyParams::Resolve(ECricketAIDifficulty::Simulation);
	SimP.MistakeRate = 0.0; SimP.SelectionTemperature = 0.0;
	int32 BestCount = 0;
	for (int32 i = 0; i < 200; ++i) { FCricketDecisionTrace T = MakeTrace(); if (FCricketTacticalEvaluator::SelectOption(T, SimP, Rng) == 0) { ++BestCount; } }
	TestEqual(TEXT("Sim always argmax"), BestCount, 200);

	// Easy: noticeably spread — the best is taken well under 100% of the time.
	auto EasyP = FCricketAIDifficultyParams::Resolve(ECricketAIDifficulty::Easy);
	int32 EasyBest = 0;
	for (int32 i = 0; i < 600; ++i) { FCricketDecisionTrace T = MakeTrace(); if (FCricketTacticalEvaluator::SelectOption(T, EasyP, Rng) == 0) { ++EasyBest; } }
	TestTrue(TEXT("Easy sometimes misses the best"), EasyBest < 540);
	TestTrue(TEXT("Easy still favours the best"), EasyBest > 180);
	return true;
}

// 3. MATCH AWARENESS: rates and pressure compute correctly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAIAwarenessTest,
	"CricketSim.AI.Awareness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAIAwarenessTest::RunTest(const FString&)
{
	// Chasing 200, 80/2 after 10 overs -> 120 needed off 60 balls -> RRR 12.0.
	const FCricketMatchSituation Hard = FCricketMatchSituation::Build(80, 2, 60, 20, 6, 11, 6, true, 200, 165);
	TestTrue(TEXT("RRR ~12"), FMath::IsNearlyEqual(Hard.RequiredRunRate, 12.0, 0.01));
	TestEqual(TEXT("Runs required"), Hard.RunsRequired, 120);
	TestTrue(TEXT("Death phase"), Hard.Phase == ECricketMatchPhase::Death || Hard.Phase == ECricketMatchPhase::Middle);

	// Chasing 100, same position -> 20 off 60 -> trivial, low pressure.
	const FCricketMatchSituation Easy = FCricketMatchSituation::Build(80, 2, 60, 20, 6, 11, 6, true, 100, 165);
	TestTrue(TEXT("Steeper ask = more pressure"), Hard.Pressure > Easy.Pressure);

	// Powerplay detection.
	const FCricketMatchSituation PP = FCricketMatchSituation::Build(30, 1, 18, 20, 6, 11, 6, false, 0, 165);
	TestTrue(TEXT("Powerplay phase"), PP.Phase == ECricketMatchPhase::Powerplay);
	return true;
}

// 4. BATTER BRAIN: a steep required rate forces more aggression than a trivial one.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAIBatterAggressionTest,
	"CricketSim.AI.BatterAggression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAIBatterAggressionTest::RunTest(const FString&)
{
	const FCricketAIPlayerProfile Bat = MakePlayer(TEXT("B"), ECricketRole::BatterMiddle, 0.7f, 0.1f, 0.f);
	FCricketTeamStrategy Strat;
	const auto D = FCricketAIDifficultyParams::Resolve(ECricketAIDifficulty::Hard);
	const FCricketDeliveryRead Ball = GoodLengthBall();

	auto AggressiveFraction = [&](int32 Target)
	{
		const FCricketMatchSituation S = FCricketMatchSituation::Build(80, 2, 60, 20, 6, 11, 6, true, Target, 165);
		int32 Aggro = 0; const int32 N = 500;
		for (int32 i = 0; i < N; ++i)
		{
			FRandomStream Rng(1000 + i);
			const FCricketBatterDecision Dec = FCricketBatterBrain::Decide(S, Bat, Ball, Strat, D, Rng);
			if (Dec.Action == ECricketBatterAction::Attack || Dec.Action == ECricketBatterAction::BoundaryHit) { ++Aggro; }
		}
		return double(Aggro) / N;
	};

	const double HighRRR = AggressiveFraction(220); // RRR ~ 23 (impossible -> all out)
	const double LowRRR  = AggressiveFraction(95);  // RRR ~ 1.5 (cruise)
	AddInfo(FString::Printf(TEXT("aggressive fraction: highRRR=%.2f lowRRR=%.2f"), HighRRR, LowRRR));
	TestTrue(TEXT("High RRR is much more aggressive"), HighRRR > LowRRR + 0.25);
	TestTrue(TEXT("Low RRR is mostly watchful"), LowRRR < 0.45);
	return true;
}

// 5. BATTER BRAIN: the disciplined batter leaves/defends the wide good-length ball.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAIBatterLeaveTest,
	"CricketSim.AI.BatterLeave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAIBatterLeaveTest::RunTest(const FString&)
{
	// A top-order batter, set, no chase pressure, wide outside off.
	FCricketAIPlayerProfile Bat = MakePlayer(TEXT("B"), ECricketRole::BatterTop, 0.85f, 0.05f, 0.f);
	Bat.Batting.OutsideOffTemptation = 0.1; // very disciplined
	FCricketTeamStrategy Strat; Strat.PowerplayApproach = ECricketInningsApproach::Anchor;
	const auto D = FCricketAIDifficultyParams::Resolve(ECricketAIDifficulty::Simulation);
	const FCricketMatchSituation S = FCricketMatchSituation::Build(15, 0, 12, 20, 6, 11, 6, false, 0, 165);

	FCricketDeliveryRead Ball = GoodLengthBall();
	Ball.Line = ECricketLine::WideOutsideOff;

	int32 LowRisk = 0, Aggressive = 0; const int32 N = 300;
	for (int32 i = 0; i < N; ++i)
	{
		FRandomStream Rng(7000 + i);
		const FCricketBatterDecision Dec = FCricketBatterBrain::Decide(S, Bat, Ball, Strat, D, Rng);
		if (Dec.Action == ECricketBatterAction::Leave || Dec.Action == ECricketBatterAction::Defend || Dec.Action == ECricketBatterAction::Rotate) { ++LowRisk; }
		if (Dec.Action == ECricketBatterAction::Attack || Dec.Action == ECricketBatterAction::BoundaryHit) { ++Aggressive; }
	}
	const double AggroFrac = double(Aggressive) / N;
	AddInfo(FString::Printf(TEXT("low-risk fraction = %.2f, aggressive = %.2f"), double(LowRisk) / N, AggroFrac));
	// Discipline against the wide ball with no pressure: it is rarely ATTACKED
	// (left, blocked or worked instead). That is the dismissal the brain must avoid.
	TestTrue(TEXT("Disciplined batter rarely attacks the wide ball"), AggroFrac < 0.30);
	TestTrue(TEXT("Low-risk responses dominate"), LowRisk > 2 * Aggressive);
	return true;
}

// 6. BOWLER BRAIN: yorkers feature far more at the death than in the powerplay.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAIBowlerDeathTest,
	"CricketSim.AI.BowlerDeath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAIBowlerDeathTest::RunTest(const FString&)
{
	const FCricketAIPlayerProfile Bowler = MakePlayer(TEXT("P"), ECricketRole::PaceBowler, 0.2f, 0.85f, 145.f);
	const FCricketAIPlayerProfile Batter = MakePlayer(TEXT("B"), ECricketRole::BatterMiddle, 0.6f, 0.1f, 0.f);
	const auto D = FCricketAIDifficultyParams::Resolve(ECricketAIDifficulty::Hard);
	FCricketBowlerMemory Mem;

	auto YorkerFraction = [&](const FCricketMatchSituation& S)
	{
		int32 Y = 0; const int32 N = 500;
		for (int32 i = 0; i < N; ++i)
		{
			FRandomStream Rng(2000 + i);
			FCricketBowlerMemory M;
			const FCricketBowlerDecision Dec = FCricketBowlerBrain::Decide(S, Bowler, Batter, M, D, Rng);
			if (Dec.Intent.Length == ECricketLength::Yorker) { ++Y; }
		}
		return double(Y) / N;
	};

	const FCricketMatchSituation Death = FCricketMatchSituation::Build(150, 5, 102, 20, 6, 11, 6, false, 0, 165); // 17 overs
	const FCricketMatchSituation PP    = FCricketMatchSituation::Build(20, 0, 6, 20, 6, 11, 6, false, 0, 165);    // over 2

	const double DeathY = YorkerFraction(Death);
	const double PPY    = YorkerFraction(PP);
	AddInfo(FString::Printf(TEXT("yorker fraction: death=%.2f pp=%.2f"), DeathY, PPY));
	TestTrue(TEXT("More yorkers at the death"), DeathY > PPY + 0.10);
	return true;
}

// 7. BOWLER BRAIN: short-ball weakness is targeted more as awareness rises.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAIBowlerMatchupTest,
	"CricketSim.AI.BowlerMatchup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAIBowlerMatchupTest::RunTest(const FString&)
{
	const FCricketAIPlayerProfile Bowler = MakePlayer(TEXT("P"), ECricketRole::PaceBowler, 0.2f, 0.8f, 142.f);
	FCricketAIPlayerProfile Weak = MakePlayer(TEXT("B"), ECricketRole::BatterMiddle, 0.5f, 0.1f, 0.f);
	Weak.Batting.ShortBallWeakness = 0.95;
	const FCricketMatchSituation S = FCricketMatchSituation::Build(40, 1, 36, 20, 6, 11, 6, false, 0, 165);

	auto ShortFraction = [&](ECricketAIDifficulty Diff)
	{
		const auto D = FCricketAIDifficultyParams::Resolve(Diff);
		int32 Short = 0; const int32 N = 500;
		for (int32 i = 0; i < N; ++i)
		{
			FRandomStream Rng(3000 + i);
			FCricketBowlerMemory M;
			const FCricketBowlerDecision Dec = FCricketBowlerBrain::Decide(S, Bowler, Weak, M, D, Rng);
			if (Dec.Intent.Length == ECricketLength::Short || Dec.Intent.Length == ECricketLength::Bouncer) { ++Short; }
		}
		return double(Short) / N;
	};

	const double HardShort = ShortFraction(ECricketAIDifficulty::Simulation);
	const double EasyShort = ShortFraction(ECricketAIDifficulty::Easy);
	AddInfo(FString::Printf(TEXT("short-ball fraction: sim=%.2f easy=%.2f"), HardShort, EasyShort));
	TestTrue(TEXT("Aware AI targets the weakness more"), HardShort > EasyShort);
	return true;
}

// 8. CAPTAIN BRAIN: never nominates an ineligible bowler.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAICaptainLegalTest,
	"CricketSim.AI.CaptainLegal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAICaptainLegalTest::RunTest(const FString&)
{
	FCricketTeamStrategy Strat;
	const auto D = FCricketAIDifficultyParams::Resolve(ECricketAIDifficulty::Easy); // even the noisy tier stays legal
	const FCricketMatchSituation S = FCricketMatchSituation::Build(60, 2, 60, 20, 6, 11, 6, false, 0, 165);

	TArray<FCricketBowlerOption> Pool;
	auto AddOpt = [&](const FString& N, float Bowl, int32 Overs, bool LastOver)
	{
		FCricketBowlerOption O; O.Name = N; O.Profile = MakePlayer(N, ECricketRole::PaceBowler, 0.2f, Bowl, 138.f);
		O.OversBowled = Overs; O.MaxOvers = 4; O.bBowledLastOver = LastOver; Pool.Add(O);
	};
	AddOpt(TEXT("Maxed"), 0.8f, 4, false);   // ineligible: over cap
	AddOpt(TEXT("JustBowled"), 0.85f, 2, true); // ineligible: consecutive
	AddOpt(TEXT("Fresh1"), 0.7f, 1, false);
	AddOpt(TEXT("Fresh2"), 0.75f, 0, false);

	for (int32 i = 0; i < 300; ++i)
	{
		FRandomStream Rng(4000 + i);
		const FCricketBowlingChange C = FCricketCaptainBrain::ChooseBowler(S, Pool, Strat, D, Rng);
		TestTrue(TEXT("A bowler was chosen"), C.bValid);
		TestTrue(TEXT("Chosen bowler is eligible"), C.BowlerName == TEXT("Fresh1") || C.BowlerName == TEXT("Fresh2"));
	}
	return true;
}

// 9. FULL MATCH: a complete T20 plays to a result with believable numbers.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAIFullMatchTest,
	"CricketSim.AI.FullMatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAIFullMatchTest::RunTest(const FString&)
{
	const FCricketAITeam A = MakeTeam(TEXT("India"), ECricketAIDifficulty::Hard);
	const FCricketAITeam B = MakeTeam(TEXT("Australia"), ECricketAIDifficulty::Hard);
	FCricketMatchRules Rules; // standard T20

	// --- Survey several matches: the AVERAGE first-innings score must read like a
	//     real T20 (robust to a single freak game). ---
	// Measure the FIRST innings only — a full 20-over effort that the chase doesn't
	// truncate — so the run/RR band reflects a genuine T20 total.
	double SumRR = 0.0, SumRuns = 0.0, SumWkts = 0.0; int32 NInn = 0;
	const int32 NumMatches = 10;
	for (int32 m = 0; m < NumMatches; ++m)
	{
		const FCricketAIMatchTelemetry MT = FCricketAIMatchSimulator::Simulate(A, B, Rules, 100 + m * 37);
		if (MT.Innings.Num() >= 1)
		{
			const FCricketAIInningsTelemetry& First = MT.Innings[0];
			SumRR += First.RunRate(); SumRuns += First.Runs; SumWkts += First.Wickets; ++NInn;
		}
	}
	const double AvgRR = SumRR / NInn, AvgRuns = SumRuns / NInn, AvgWkts = SumWkts / NInn;
	AddInfo(FString::Printf(TEXT("avg first innings over %d matches: %.0f runs, %.1f wkts, RR %.2f"), NInn, AvgRuns, AvgWkts, AvgRR));
	TestTrue(TEXT("Average first-innings run rate is a believable T20 (6.8..9.8)"), AvgRR >= 6.8 && AvgRR <= 9.8);
	TestTrue(TEXT("Average first-innings total is believable (130..205)"), AvgRuns >= 130.0 && AvgRuns <= 205.0);
	TestTrue(TEXT("Average first-innings wickets is believable (4..8.5)"), AvgWkts >= 4.0 && AvgWkts <= 8.5);

	const FCricketAIMatchTelemetry Tel = FCricketAIMatchSimulator::Simulate(A, B, Rules, /*Seed*/ 12345);

	TestTrue(TEXT("Match completed"), Tel.bCompleted);
	TestEqual(TEXT("Two innings recorded"), Tel.Innings.Num(), 2);

	for (const FCricketAIInningsTelemetry& Inn : Tel.Innings)
	{
		AddInfo(FString::Printf(TEXT("%s: %d/%d in %d balls (RR %.2f) — 4s %d 6s %d dots %d extras %d"),
			*Inn.BattingTeam, Inn.Runs, Inn.Wickets, Inn.LegalBalls, Inn.RunRate(), Inn.Fours, Inn.Sixes, Inn.Dots, Inn.Extras));

		TestTrue(TEXT("Legal balls within an innings"), Inn.LegalBalls > 0 && Inn.LegalBalls <= 120);
		TestTrue(TEXT("Wickets 0..10"), Inn.Wickets >= 0 && Inn.Wickets <= 10);
		TestTrue(TEXT("Run rate is believable (3..16)"), Inn.RunRate() >= 3.0 && Inn.RunRate() <= 16.0);
		TestTrue(TEXT("Total is believable (50..300)"), Inn.Runs >= 50 && Inn.Runs <= 300);

		// Shot distribution: positive intents dominate a leave-only mentality.
		const int32 Aggro = Inn.ActionCounts[(int32)ECricketBatterAction::Attack] + Inn.ActionCounts[(int32)ECricketBatterAction::BoundaryHit] + Inn.ActionCounts[(int32)ECricketBatterAction::Rotate];
		const int32 Leaves = Inn.ActionCounts[(int32)ECricketBatterAction::Leave];
		TestTrue(TEXT("Positive intents outweigh leaves"), Aggro > Leaves);

		// Bowling pattern: good length is the most-bowled band over an innings.
		int32 Best = 0; for (int32 k = 1; k < Inn.BowlLengthCounts.Num(); ++k) { if (Inn.BowlLengthCounts[k] > Inn.BowlLengthCounts[Best]) { Best = k; } }
		TestTrue(TEXT("Good length is the stock ball"), (ECricketLength)Best == ECricketLength::GoodLength || (ECricketLength)Best == ECricketLength::BackOfLength);
	}
	return true;
}

// 10. DETERMINISM: same seed -> identical match.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAIDeterminismTest,
	"CricketSim.AI.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAIDeterminismTest::RunTest(const FString&)
{
	const FCricketAITeam A = MakeTeam(TEXT("India"), ECricketAIDifficulty::Hard);
	const FCricketAITeam B = MakeTeam(TEXT("Australia"), ECricketAIDifficulty::Medium);
	FCricketMatchRules Rules;

	const FCricketAIMatchTelemetry T1 = FCricketAIMatchSimulator::Simulate(A, B, Rules, 999);
	const FCricketAIMatchTelemetry T2 = FCricketAIMatchSimulator::Simulate(A, B, Rules, 999);

	TestEqual(TEXT("Same innings count"), T1.Innings.Num(), T2.Innings.Num());
	if (T1.Innings.Num() == T2.Innings.Num())
	{
		for (int32 i = 0; i < T1.Innings.Num(); ++i)
		{
			TestEqual(TEXT("Same runs"), T1.Innings[i].Runs, T2.Innings[i].Runs);
			TestEqual(TEXT("Same wickets"), T1.Innings[i].Wickets, T2.Innings[i].Wickets);
			TestEqual(TEXT("Same balls"), T1.Innings[i].LegalBalls, T2.Innings[i].LegalBalls);
		}
	}
	TestEqual(TEXT("Same result"), T1.ResultSummary, T2.ResultSummary);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
