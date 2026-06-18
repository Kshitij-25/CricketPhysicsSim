#pragma once

#include "CoreMinimal.h"
#include "CricketAITypes.generated.h"

/**
 * CricketAITypes — the shared vocabulary of the cricket-intelligence layer: the
 * difficulty model, the high-level action/intent enumerations the brains choose
 * between, and the decision-trace structs the debug tooling visualises.
 *
 * DESIGN PRINCIPLE — the AI never cheats. It produces the SAME intent a human
 * supplies (a shot + footwork, a line + length, a bowling change) and the result
 * EMERGES from the shared physics + rules. Difficulty therefore tunes DECISION
 * QUALITY — how well the brain reads the situation, exploits the opponent and
 * executes its plan — and never reaction-speed, omniscience, or the outcome.
 */

// ---------------------------------------------------------------------------
// Difficulty
// ---------------------------------------------------------------------------

/** The four supported skill tiers. */
UENUM(BlueprintType)
enum class ECricketAIDifficulty : uint8
{
	Easy        UMETA(DisplayName = "Easy"),
	Medium      UMETA(DisplayName = "Medium"),
	Hard        UMETA(DisplayName = "Hard"),
	Simulation  UMETA(DisplayName = "Simulation")   // near-optimal, full situational awareness
};

/**
 * FCricketAIDifficultyParams — the dials every brain consults so that one tier is
 * defined once and applied consistently. Crucially these are all DECISION-QUALITY
 * dials: a worse AI picks worse options and executes them less precisely; it does
 * NOT react slower-than-physics-allows nor see information a human couldn't.
 */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketAIDifficultyParams
{
	GENERATED_BODY()

	/**
	 * Softmax temperature used when choosing among scored options. 0 = always take
	 * the best-scoring option (Simulation); larger = more likely to pick a weaker
	 * option (the brain "doesn't see" the best play). This is the core quality dial.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Difficulty", meta = (ClampMin = "0.0"))
	double SelectionTemperature = 0.0;

	/**
	 * Execution imprecision in [0,1] applied to CONTINUOUS intent (aim degrees,
	 * power, fine line/length, pace). Routed through the same scatter channels a
	 * human is subject to (e.g. UCricketBowlingComponent::HumanScatter); it perturbs
	 * the chosen INPUT, never the physics result.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double ExecutionNoise = 0.0;

	/**
	 * How fully the brain exploits opponent tendencies and the match situation in
	 * [0,1]. At 0 it plays a generic, situation-blind game; at 1 it fully reads the
	 * batter's weaknesses, the required rate, the field and the phase.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double SituationalAwareness = 1.0;

	/**
	 * Probability in [0,1] of a gross misjudgement on a given decision (a rush of
	 * blood with the bat, a brain-fade bowling change). Models human lapses; scaled
	 * by pressure in the brains.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Difficulty", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double MistakeRate = 0.0;

	/** Resolve the canonical parameters for a tier. */
	static FCricketAIDifficultyParams Resolve(ECricketAIDifficulty Difficulty)
	{
		FCricketAIDifficultyParams P;
		switch (Difficulty)
		{
		case ECricketAIDifficulty::Easy:
			P.SelectionTemperature  = 1.30;
			P.ExecutionNoise        = 0.55;
			P.SituationalAwareness  = 0.35;
			P.MistakeRate           = 0.22;
			break;
		case ECricketAIDifficulty::Medium:
			P.SelectionTemperature  = 0.65;
			P.ExecutionNoise        = 0.30;
			P.SituationalAwareness  = 0.65;
			P.MistakeRate           = 0.10;
			break;
		case ECricketAIDifficulty::Hard:
			P.SelectionTemperature  = 0.28;
			P.ExecutionNoise        = 0.14;
			P.SituationalAwareness  = 0.88;
			P.MistakeRate           = 0.035;
			break;
		case ECricketAIDifficulty::Simulation:
		default:
			P.SelectionTemperature  = 0.08;
			P.ExecutionNoise        = 0.05;
			P.SituationalAwareness  = 1.00;
			P.MistakeRate           = 0.0;
			break;
		}
		return P;
	}
};

// ---------------------------------------------------------------------------
// High-level actions the brains choose between
// ---------------------------------------------------------------------------

/** The batter's top-level response to a delivery (the brief's five intents). */
UENUM(BlueprintType)
enum class ECricketBatterAction : uint8
{
	Leave         UMETA(DisplayName = "Leave"),          // let it go through to the keeper
	Defend        UMETA(DisplayName = "Defend"),         // soft, controlled block
	Rotate        UMETA(DisplayName = "Rotate Strike"),  // work it for a single/two
	Attack        UMETA(DisplayName = "Attack"),         // positive, along the ground for boundaries
	BoundaryHit   UMETA(DisplayName = "Boundary Attempt") // go aerial / over the top
};

/** The bowler's plan for the delivery — what the line/length is TRYING to do. */
UENUM(BlueprintType)
enum class ECricketBowlerPlan : uint8
{
	BuildPressure   UMETA(DisplayName = "Build Pressure"),   // dot-ball squeeze, tight channel
	AttackStumps    UMETA(DisplayName = "Attack the Stumps"), // straight, wicket-to-wicket
	BowlWicketBall  UMETA(DisplayName = "Wicket Ball"),       // the set-up payoff delivery
	ContainBoundary UMETA(DisplayName = "Contain"),          // deny the boundary at the death
	Variation       UMETA(DisplayName = "Variation")          // change of pace / surprise ball
};

/** A team's overarching innings posture, used by batting/captain brains. */
UENUM(BlueprintType)
enum class ECricketInningsApproach : uint8
{
	Anchor      UMETA(DisplayName = "Anchor / Build"),
	Consolidate UMETA(DisplayName = "Consolidate"),
	Accelerate  UMETA(DisplayName = "Accelerate"),
	AllOut      UMETA(DisplayName = "All-out Attack"),
	Defend      UMETA(DisplayName = "Defend a Total")
};

// ---------------------------------------------------------------------------
// Decision trace — the debug/telemetry record of how a choice was made
// ---------------------------------------------------------------------------

/** One scored candidate the brain weighed. Carried purely for visualisation. */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketScoredOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AI|Decision") FName Label;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Decision") double Score = 0.0;
	/** Softmax probability the option was chosen at the active difficulty. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Decision") double Probability = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Decision") bool bChosen = false;

	FCricketScoredOption() = default;
	FCricketScoredOption(FName InLabel, double InScore) : Label(InLabel), Score(InScore) {}
};

/**
 * FCricketDecisionTrace — the full record of a single brain decision: every option
 * it scored, which it took, and a one-line tactical intent string. The debug
 * overlay renders this so a developer can see exactly WHY the AI did what it did.
 */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketDecisionTrace
{
	GENERATED_BODY()

	/** What kind of decision this was, e.g. "Batter", "Bowler", "Captain". */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Decision") FName Domain;

	/** Human-readable tactical intent, e.g. "RRR 11.2 — must attack". */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Decision") FString Intent;

	/** Every candidate, scored and with its selection probability. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Decision") TArray<FCricketScoredOption> Options;

	/** Index into Options of the chosen action (INDEX_NONE if none). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Decision") int32 ChosenIndex = INDEX_NONE;

	/** A scalar 0..1 pressure reading at the moment of the decision (for the HUD). */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Decision") double Pressure = 0.0;

	void Add(FName Label, double Score) { Options.Emplace(Label, Score); }
	const FCricketScoredOption* Chosen() const { return Options.IsValidIndex(ChosenIndex) ? &Options[ChosenIndex] : nullptr; }
};
