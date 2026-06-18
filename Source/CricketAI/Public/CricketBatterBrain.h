#pragma once

#include "CoreMinimal.h"
#include "CricketAITypes.h"
#include "CricketAIPlayerProfile.h"
#include "CricketMatchAwareness.h"
#include "CricketBattingTypes.h"   // FCricketBattingInput, ECricketShotType, ECricketFootwork
#include "CricketBowlingTypes.h"   // ECricketLine, ECricketLength, ECricketMovement, ECricketBowlingStyle
#include "CricketBatterBrain.generated.h"

/**
 * What the batter perceives about the incoming ball and the field set for it. The
 * brain reads the SAME public facts a human sees (line, length, the bowler's type,
 * how the field is set) — never the ball's scripted result.
 */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketDeliveryRead
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "AI|Batting") ECricketLine Line = ECricketLine::OffStump;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Batting") ECricketLength Length = ECricketLength::GoodLength;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Batting") ECricketMovement Movement = ECricketMovement::SeamUp;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Batting") ECricketBowlingStyle BowlerStyle = ECricketBowlingStyle::Pace;
	/** Bowler pace dial 0..1 (1 ~ express). Spin sits near 0. */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Batting") double BowlerPace01 = 0.75;

	/** How well the boundary is protected in [0,1] (sweepers out) — deters the aerial. */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Batting") double BoundaryProtection = 0.4;
	/** Catchers in close in [0,1] — raises the risk of a soft dismissal on the attack. */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Batting") double CatchersUp = 0.3;

	bool IsSpin() const { return BowlerStyle == ECricketBowlingStyle::OffSpin || BowlerStyle == ECricketBowlingStyle::LegSpin; }
};

/** The batter brain's verdict: the chosen action, the concrete swing intent, the trace. */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketBatterDecision
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AI|Batting") ECricketBatterAction Action = ECricketBatterAction::Defend;

	/** The concrete intent to push into UCricketBattingComponent (or feed the contest model). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Batting") FCricketBattingInput Input;

	/** True when the batter intends to offer no shot (Leave) — the controller won't swing. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Batting") bool bOffersNoShot = false;

	/** Intended risk this stroke carries in [0,1] (for the contest model + telemetry). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Batting") double Risk = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "AI|Batting") FCricketDecisionTrace Trace;
};

/**
 * FCricketBatterBrain — the Batter AI decision core.
 *
 * Given the match situation, the batter's tendencies, the bowler's type and the
 * ball it is reading, it weighs the five intents — Leave, Defend, Rotate, Attack,
 * Boundary — scores each from cricket logic (phase aggression, the required rate,
 * the matchup, the field), and selects one through the shared difficulty-aware
 * selector. It then translates the choice into a concrete FCricketBattingInput
 * (shot, footwork, aim, power) for the existing batting controller.
 *
 * It decides INTENT only. Whether the bat middles the ball, edges it or misses is
 * left entirely to the physics (live) or the contest model (headless).
 */
class CRICKETAI_API FCricketBatterBrain
{
public:
	static FCricketBatterDecision Decide(
		const FCricketMatchSituation& Situation,
		const FCricketAIPlayerProfile& Batter,
		const FCricketDeliveryRead& Ball,
		const FCricketTeamStrategy& Strategy,
		const FCricketAIDifficultyParams& Difficulty,
		FRandomStream& Rng);

	/** True if this length suits the back foot (short side of a good length). */
	static bool IsBackFootLength(ECricketLength L)
	{
		return L == ECricketLength::Short || L == ECricketLength::Bouncer || L == ECricketLength::BackOfLength;
	}
	/** True if this length is in the half-volley / drivable arc. */
	static bool IsDrivableLength(ECricketLength L)
	{
		return L == ECricketLength::Full || L == ECricketLength::Yorker || L == ECricketLength::FullToss;
	}
};
