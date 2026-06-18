#include "CricketBowlerBrain.h"
#include "CricketTacticalEvaluator.h"

namespace
{
	/** A delivery the bowler can choose to bowl, with the plan it serves. */
	struct FCandidate
	{
		FName Label;
		ECricketBowlerPlan Plan;
		ECricketLine Line;
		ECricketLength Length;
		ECricketMovement Movement;
		double PaceMul;      // multiplies the bowler's stock pace (slower ball < 1)
		double Score = 0.0;
		double Wicket = 0.0; // intrinsic wicket-threat of the ball
	};

	ECricketMovement StockMovement(ECricketBowlingStyle Style)
	{
		switch (Style)
		{
		case ECricketBowlingStyle::Swing:   return ECricketMovement::Outswing;
		case ECricketBowlingStyle::OffSpin: return ECricketMovement::OffBreak;
		case ECricketBowlingStyle::LegSpin: return ECricketMovement::LegBreak;
		default:                            return ECricketMovement::SeamUp;
		}
	}

	void BuildRepertoire(const FCricketAIPlayerProfile& Bowler, TArray<FCandidate>& Out)
	{
		const FCricketBowlingTendencies& W = Bowler.Bowling;
		const ECricketMovement Stock = StockMovement(W.Style);

		if (W.IsSpin())
		{
			Out.Add({TEXT("Stock (good length)"), ECricketBowlerPlan::BuildPressure, ECricketLine::OffStump,   ECricketLength::GoodLength, Stock,                        1.0,  0, 0.10});
			Out.Add({TEXT("Flighted fuller"),     ECricketBowlerPlan::BowlWicketBall, ECricketLine::OffStump,   ECricketLength::Full,       Stock,                        0.92, 0, 0.22});
			Out.Add({TEXT("Quicker/flat"),        ECricketBowlerPlan::Variation,     ECricketLine::Middle,     ECricketLength::BackOfLength, Stock,                       1.18, 0, 0.16});
			Out.Add({TEXT("Wrong'un/arm ball"),   ECricketBowlerPlan::Variation,     ECricketLine::OffStump,   ECricketLength::GoodLength,  W.Style == ECricketBowlingStyle::LegSpin ? ECricketMovement::OffBreak : ECricketMovement::LegBreak, 1.0, 0, 0.20});
			Out.Add({TEXT("Wide of off"),         ECricketBowlerPlan::ContainBoundary, ECricketLine::OutsideOff, ECricketLength::GoodLength, Stock,                        1.0,  0, 0.06});
		}
		else
		{
			Out.Add({TEXT("Channel outside off"), ECricketBowlerPlan::BuildPressure,  ECricketLine::OutsideOff, ECricketLength::GoodLength,  Stock,                        1.0,  0, 0.14});
			Out.Add({TEXT("Full swinging"),       ECricketBowlerPlan::BowlWicketBall,  ECricketLine::OffStump,   ECricketLength::Full,        W.Style == ECricketBowlingStyle::Swing ? ECricketMovement::Inswing : Stock, 1.0, 0, 0.24});
			Out.Add({TEXT("Bouncer"),             ECricketBowlerPlan::Variation,       ECricketLine::Middle,     ECricketLength::Bouncer,     ECricketMovement::SeamUp,      1.05, 0, 0.18});
			Out.Add({TEXT("Yorker"),              ECricketBowlerPlan::AttackStumps,    ECricketLine::Middle,     ECricketLength::Yorker,      ECricketMovement::SeamUp,      1.0,  0, 0.20});
			Out.Add({TEXT("Slower ball"),         ECricketBowlerPlan::Variation,       ECricketLine::OffStump,   ECricketLength::BackOfLength, ECricketMovement::WobbleSeam, 0.72, 0, 0.16});
			Out.Add({TEXT("Back-of-length"),      ECricketBowlerPlan::ContainBoundary, ECricketLine::OffStump,   ECricketLength::BackOfLength, Stock,                       1.0,  0, 0.08});
		}
	}
}

FCricketBowlerDecision FCricketBowlerBrain::Decide(
	const FCricketMatchSituation& S,
	const FCricketAIPlayerProfile& Bowler,
	const FCricketAIPlayerProfile& Batter,
	const FCricketBowlerMemory& Memory,
	const FCricketAIDifficultyParams& D,
	FRandomStream& Rng)
{
	FCricketBowlerDecision Out;
	FCricketDecisionTrace& T = Out.Trace;
	T.Domain = TEXT("Bowler");
	T.Pressure = FCricketTacticalEvaluator::BowlingPressure(S);

	const FCricketBowlingTendencies& W = Bowler.Bowling;
	const FCricketBattingTendencies& Bt = Batter.Batting;
	const double Aware = D.SituationalAwareness;

	TArray<FCandidate> Cands;
	BuildRepertoire(Bowler, Cands);

	for (FCandidate& C : Cands)
	{
		double Sc = 0.30 + C.Wicket * W.Aggression;          // base + the bowler's wicket appetite

		// --- Phase fit ---
		switch (S.Phase)
		{
		case ECricketMatchPhase::Powerplay:
			if (C.Plan == ECricketBowlerPlan::BowlWicketBall) Sc += 0.30;     // swing it, take wickets up top
			if (C.Plan == ECricketBowlerPlan::BuildPressure)  Sc += 0.18;
			if (C.Length == ECricketLength::Bouncer)          Sc += 0.05;
			break;
		case ECricketMatchPhase::Middle:
			if (C.Plan == ECricketBowlerPlan::BuildPressure)  Sc += 0.28;     // squeeze, dot pressure
			if (C.Plan == ECricketBowlerPlan::Variation)      Sc += 0.12;
			break;
		case ECricketMatchPhase::Death:
			if (C.Length == ECricketLength::Yorker)           Sc += 0.34;     // yorkers and slower balls
			if (C.Plan == ECricketBowlerPlan::Variation)      Sc += 0.20;
			if (C.Plan == ECricketBowlerPlan::ContainBoundary)Sc += 0.16;
			if (C.Length == ECricketLength::Full && C.Plan == ECricketBowlerPlan::BowlWicketBall) Sc -= 0.20; // a half-volley is death suicide
			break;
		default: break;
		}

		// --- Matchup: attack the batter's weakness, scaled by awareness ---
		if (C.Length == ECricketLength::Bouncer || C.Length == ECricketLength::Short)
			Sc += Aware * Bt.ShortBallWeakness * 0.30;
		if (C.Line == ECricketLine::OutsideOff || C.Line == ECricketLine::WideOutsideOff)
			Sc += Aware * Bt.OutsideOffTemptation * 0.25;
		if (C.Length == ECricketLength::Full || C.Length == ECricketLength::Yorker)
			Sc += Aware * Bt.FullBallWeakness * 0.20;
		// A well-set, strong batter: respect them, lean to containment.
		if (C.Plan == ECricketBowlerPlan::ContainBoundary)
			Sc += Aware * (Batter.BattingRating - 0.5) * 0.20;

		// --- Skill gating: don't attempt what you can't execute ---
		if (C.Length == ECricketLength::Yorker)   Sc *= FMath::Lerp(0.55, 1.1, W.DeathSkill);
		if (C.PaceMul < 0.9 || C.Movement == ECricketMovement::WobbleSeam) Sc *= FMath::Lerp(0.6, 1.1, W.Variation);
		if (C.Movement != ECricketMovement::SeamUp && C.Movement != StockMovement(W.Style))
			Sc *= FMath::Lerp(0.6, 1.05, W.Variation);       // a variation ball needs the skill

		// --- Unpredictability: don't bowl the same length twice running ---
		if (Memory.RecentLengths.Num() && C.Length == Memory.LastLength()) Sc -= 0.12;
		if (Memory.RecentLengths.Num() >= 2
			&& Memory.RecentLengths.Last() == C.Length
			&& Memory.RecentLengths[Memory.RecentLengths.Num() - 2] == C.Length) Sc -= 0.18;

		// --- Set-up payoff: the wicket ball is worth more once you've built dots ---
		if (C.Plan == ECricketBowlerPlan::BowlWicketBall && Memory.BallsAtBatter >= 2)
			Sc += Aware * 0.15;

		C.Score = Sc;
		T.Add(C.Label, Sc);
	}

	const int32 Chosen = FCricketTacticalEvaluator::SelectOption(T, D, Rng, 1.0);
	const FCandidate& Pick = Cands[FMath::Clamp(Chosen, 0, Cands.Num() - 1)];

	// --- Concrete intent ---
	FCricketBowlingIntent& In = Out.Intent;
	In.Arm      = (W.Style == ECricketBowlingStyle::Pace || W.Style == ECricketBowlingStyle::Swing) ? ECricketBowlingArm::RightArm : ECricketBowlingArm::RightArm;
	In.Movement = Pick.Movement;
	In.Length   = Pick.Length;
	In.Line     = Pick.Line;
	In.Pace01   = FMath::Clamp(W.Pace * Pick.PaceMul, 0.0, 1.0);
	In.SwingAmount = (W.Style == ECricketBowlingStyle::Swing || Pick.Movement == ECricketMovement::Outswing || Pick.Movement == ECricketMovement::Inswing || Pick.Movement == ECricketMovement::ReverseSwing) ? W.Movement : 0.15;
	In.SpinAmount  = W.IsSpin() ? FMath::Lerp(0.5, 1.0, W.Movement) : 0.0;

	Out.Plan = Pick.Plan;
	Out.WicketIntent = Pick.Wicket;
	// Worse control => more execution scatter (the same channel a human bowler uses).
	Out.ExecutionScatter = FMath::Clamp((1.0 - W.Control) * 0.6 + D.ExecutionNoise * 0.5, 0.0, 1.0);

	T.Intent = FString::Printf(TEXT("%s: %s @ %s"),
		*UEnum::GetDisplayValueAsText(Out.Plan).ToString(),
		*Pick.Label.ToString(),
		S.Phase == ECricketMatchPhase::Death ? TEXT("death") : (S.Phase == ECricketMatchPhase::Powerplay ? TEXT("PP") : TEXT("mid")));

	return Out;
}
