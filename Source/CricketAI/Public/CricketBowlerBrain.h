#pragma once

#include "CoreMinimal.h"
#include "CricketAITypes.h"
#include "CricketAIPlayerProfile.h"
#include "CricketMatchAwareness.h"
#include "CricketBowlingTypes.h"   // FCricketBowlingIntent, ECricketLine/Length/Movement
#include "CricketBowlerBrain.generated.h"

/**
 * A short, decaying memory of what the bowler has already done — so the AI sets a
 * batter up over several balls and does not become predictable. Holds the recent
 * lengths/lines bowled this spell (most recent last).
 */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketBowlerMemory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "AI|Bowling") TArray<ECricketLength> RecentLengths;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Bowling") TArray<ECricketLine> RecentLines;
	/** Balls bowled to the CURRENT batter (drives the set-up arc). */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Bowling") int32 BallsAtBatter = 0;

	void Record(ECricketLength L, ECricketLine Ln)
	{
		RecentLengths.Add(L); RecentLines.Add(Ln); ++BallsAtBatter;
		const int32 Keep = 6;
		if (RecentLengths.Num() > Keep) { RecentLengths.RemoveAt(0); }
		if (RecentLines.Num()   > Keep) { RecentLines.RemoveAt(0); }
	}
	void NewBatter() { BallsAtBatter = 0; }
	ECricketLength LastLength() const { return RecentLengths.Num() ? RecentLengths.Last() : ECricketLength::GoodLength; }
};

/** The bowler brain's verdict: the plan, the concrete release intent, the trace. */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketBowlerDecision
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AI|Bowling") ECricketBowlerPlan Plan = ECricketBowlerPlan::BuildPressure;

	/** The concrete intent to push into UCricketBowlingComponent (or the contest model). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Bowling") FCricketBowlingIntent Intent;

	/** Human execution scatter in [0,1] to hand the controller (worse control => more). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Bowling") double ExecutionScatter = 0.0;

	/** Wicket-threat this delivery carries in [0,1] (for the contest model + telemetry). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Bowling") double WicketIntent = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "AI|Bowling") FCricketDecisionTrace Trace;
};

/**
 * FCricketBowlerBrain — the Bowler AI decision core.
 *
 * Given the situation, the bowler's tendencies, the batter being targeted, the
 * field and its own recent deliveries, it weighs a repertoire of deliveries
 * (line + length + movement + pace), scoring each on phase fit, the matchup
 * (attacking the batter's weakness, scaled by awareness), control risk and
 * unpredictability, then selects one through the difficulty-aware selector and
 * emits a concrete FCricketBowlingIntent. It changes line, length, pace, swing and
 * spin and sets dismissals up across balls — but never scripts the result.
 */
class CRICKETAI_API FCricketBowlerBrain
{
public:
	static FCricketBowlerDecision Decide(
		const FCricketMatchSituation& Situation,
		const FCricketAIPlayerProfile& Bowler,
		const FCricketAIPlayerProfile& Batter,
		const FCricketBowlerMemory& Memory,
		const FCricketAIDifficultyParams& Difficulty,
		FRandomStream& Rng);
};
