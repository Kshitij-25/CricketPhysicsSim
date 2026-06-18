#pragma once

#include "CoreMinimal.h"
#include "CricketAITypes.h"
#include "CricketMatchAwareness.h"

/**
 * FCricketTacticalEvaluator — the Tactical Evaluation System and the shared
 * decision MECHANISM every brain runs through.
 *
 * Two responsibilities:
 *   1. Common cricketing math the brains share — the aggression a batting side
 *      should carry given the situation, and the symmetric pressure on the bowler.
 *   2. The selection core: given a set of SCORED options and the active difficulty,
 *      turn scores into a choice. This is the single place difficulty changes
 *      DECISION QUALITY — a high temperature spreads the softmax so a weaker AI
 *      often fails to pick the best option, and MistakeRate injects the occasional
 *      gross misjudgement. It never alters reaction timing or the physics outcome.
 *
 * Pure and deterministic given the FRandomStream, so the brains it powers are
 * fully unit-testable.
 */
class CRICKETAI_API FCricketTacticalEvaluator
{
public:
	/**
	 * Choose among Trace.Options. Fills each option's softmax Probability, sets the
	 * chosen option's bChosen and Trace.ChosenIndex, and returns the chosen index.
	 *
	 * Temperature 0 (Simulation) is a pure argmax. Larger temperatures (Easy) spread
	 * the distribution so weaker options are sampled. With probability MistakeRate
	 * the brain "has a rush of blood" and samples uniformly among the non-best
	 * options instead — a believable human lapse, more likely under pressure.
	 */
	static int32 SelectOption(FCricketDecisionTrace& Trace,
		const FCricketAIDifficultyParams& Difficulty, FRandomStream& Rng,
		double PressureMistakeScale = 1.0);

	/**
	 * Target run-aggression in [0,1] for the batting side: 0 = pure survival, 1 =
	 * everything. Blends the phase intent, the required/par rate and wickets in hand,
	 * scaled by how much situational awareness the difficulty grants (a low-awareness
	 * AI drifts toward a flat, situation-blind 0.5).
	 */
	static double BattingAggressionTarget(const FCricketMatchSituation& S,
		ECricketInningsApproach PhaseApproach, double Awareness);

	/** Symmetric pressure the BOWLING side is applying / under, 0..1. */
	static double BowlingPressure(const FCricketMatchSituation& S)
	{
		// The bowling side is "on top" when the batting side is under pressure.
		return FMath::Clamp(1.0 - S.Pressure, 0.0, 1.0);
	}

	/** Pick the team's intended approach for the current phase from its strategy. */
	static ECricketInningsApproach PhaseApproach(const FCricketMatchSituation& S,
		ECricketInningsApproach Powerplay, ECricketInningsApproach Middle, ECricketInningsApproach Death);
};
