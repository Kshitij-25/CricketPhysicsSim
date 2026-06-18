#pragma once

#include "CoreMinimal.h"
#include "CricketAITypes.h"
#include "CricketFieldingTypes.h"           // FCricketInterceptResult, FCricketThrowSolution, difficulty/kind enums
#include "CricketFielderComponent.h"        // ECricketThrowTarget
#include "CricketFieldingBrain.generated.h"

/** The fielding side's decision on who chases and what kind of play it is. */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketFieldingPlan
{
	GENERATED_BODY()

	/** Index into the queried fielders of the designated chaser; INDEX_NONE if none. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Fielding") int32 ChaserIndex = INDEX_NONE;
	/** Index of a backing-up/relay fielder behind the chaser; INDEX_NONE if none. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Fielding") int32 BackupIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Fielding") ECricketInterceptKind Kind = ECricketInterceptKind::None;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Fielding") FCricketDecisionTrace Trace;
};

/**
 * FCricketFieldingBrain — the Fielding AI's REASONING (the coordinator side).
 *
 * The per-fielder mechanics — moving to the ball, catching, picking up, throwing —
 * already live in UCricketFielderComponent and run off the real physics prediction.
 * This brain sits above them and makes the team-level calls: which fielder should
 * chase (the best intercept), whether a hard chance is worth a dive at the chosen
 * difficulty, where to throw (run-out at the stumps vs the safe ball to the keeper),
 * and when a relay is needed. Every input is a physics-true prediction/intercept;
 * the brain only chooses among them.
 */
class CRICKETAI_API FCricketFieldingBrain
{
public:
	/**
	 * Designate the chaser from each fielder's solved intercept against the live
	 * ball. Prefers a reachable catch, then the quickest reachable ground field; a
	 * lower difficulty is more likely to send the wrong fielder.
	 */
	static FCricketFieldingPlan SelectChaser(
		const TArray<FCricketInterceptResult>& Intercepts,
		const FCricketAIDifficultyParams& Difficulty,
		FRandomStream& Rng);

	/**
	 * Decide whether to commit to a catch given its difficulty and the fielder's
	 * skill. A regulation chance is always taken; a diving chance depends on skill
	 * and difficulty (a weaker side, or a lesser fielder, may shell or shirk it).
	 * Returns the commitment; the actual outcome is still resolved by the physics.
	 */
	static bool ShouldAttemptCatch(ECricketCatchDifficulty Difficulty,
		double FielderRating, const FCricketAIDifficultyParams& AIDifficulty, FRandomStream& Rng);

	/**
	 * Choose the throw target. With a genuine run-out chance and a feasible flat
	 * throw at the stumps, go for it; otherwise return the ball safely to the keeper
	 * (or nearest fielder if the keeper is out of range). A lower difficulty more
	 * often makes the lower-percentage choice.
	 */
	static ECricketThrowTarget ChooseThrow(
		bool bRunOutChance,
		const FCricketThrowSolution& ToStumps,
		const FCricketThrowSolution& ToKeeper,
		const FCricketAIDifficultyParams& Difficulty,
		FRandomStream& Rng);
};
