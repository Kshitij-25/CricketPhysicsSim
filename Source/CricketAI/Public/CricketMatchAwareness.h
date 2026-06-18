#pragma once

#include "CoreMinimal.h"
#include "CricketMatchTypes.h"   // ECricketMatchPhase
#include "CricketMatchAwareness.generated.h"

class UCricketMatchEngine;

/**
 * CricketMatchAwareness — the Match Awareness System. A single read-only snapshot,
 * FCricketMatchSituation, that distils the live match state into the handful of
 * quantities every brain reasons over (phase, score, wickets, the rates, the
 * pressure). It is built FROM the authoritative Match Engine, so the AI's view of
 * the game is exactly the public scoreboard — never privileged information.
 *
 * Pure data + a deterministic builder; no UWorld, so the simulator and the live
 * controllers share one definition of "what is the situation right now".
 */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketMatchSituation
{
	GENERATED_BODY()

	/** Coarse innings phase: Powerplay / Middle / Death (or Pre/Break/Complete). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") ECricketMatchPhase Phase = ECricketMatchPhase::PreMatch;

	/** True in the second innings — a chase with a fixed target. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") bool bChasing = false;

	// --- Batting side's live position ---
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") int32 Runs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") int32 Wickets = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") int32 WicketsInHand = 10;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") int32 LegalBalls = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") int32 BallsRemaining = 120;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") int32 CompletedOvers = 0;

	// --- Rates ---
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") double CurrentRunRate = 0.0;
	/** Required run rate in a chase (run/over); 0 in the first innings. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") double RequiredRunRate = 0.0;

	// --- Chase specifics ---
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") int32 Target = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") int32 RunsRequired = 0;

	/** Fraction of the innings' balls still to be bowled, 0..1 (a "resource" proxy). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") double BallsResourceFrac = 1.0;

	/**
	 * Scalar pressure on the BATTING side in [0,1]. Rises with a steep required rate,
	 * lost wickets and a shrinking game; the symmetric value for the bowling side is
	 * 1 - Pressure. Drives risk appetite in every brain.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") double Pressure = 0.0;

	/** Projected first-innings total at the current rate (for setting a target). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Awareness") int32 ProjectedTotal = 0;

	bool IsLive() const { return Phase == ECricketMatchPhase::Powerplay || Phase == ECricketMatchPhase::Middle || Phase == ECricketMatchPhase::Death; }

	/**
	 * Build the situation from the live Match Engine. ParTotal is the side's notional
	 * par first-innings score, used to gauge first-innings pressure/approach (it has
	 * no effect on a chase, where the real target governs).
	 */
	static FCricketMatchSituation FromEngine(const UCricketMatchEngine& Engine, int32 ParTotalT20 = 165);

	/** The same maths from raw scalars — used by the headless simulator and tests. */
	static FCricketMatchSituation Build(
		int32 InRuns, int32 InWickets, int32 InLegalBalls,
		int32 OversPerInnings, int32 BallsPerOver, int32 PlayersPerTeam, int32 PowerplayOvers,
		bool bInChasing, int32 InTarget, int32 ParTotalT20);
};
