#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketAIPlayerProfile.h"
#include "CricketBowlerBrain.h"
#include "CricketMatchAwareness.h"
#include "CricketBowlerAIController.generated.h"

class UCricketBowlingComponent;
class UCricketMatchEngine;

/**
 * UCricketBowlerAIController — the Bowler AI Controller component.
 *
 * Attached alongside a UCricketBowlingComponent, it is the bowler's "hands": it
 * asks the FCricketBowlerBrain what to bowl, then drives the EXISTING bowling
 * controller through the very setters a human uses (line, length, movement, pace,
 * swing, spin) and presses BowlNow(). It feeds the brain's execution scatter into
 * the component's HumanScatter, so a less skilled bowler's loose balls come from
 * the same imperfect-input channel the player is subject to — never a scripted
 * result. The ball flight then emerges from the physics core.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETAI_API UCricketBowlerAIController : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketBowlerAIController();

	/** Wire the bowling controller this AI drives, and (optionally) the match engine for awareness. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Bowler")
	void SetTargets(UCricketBowlingComponent* InBowling, UCricketMatchEngine* InEngine);

	/** Tell the AI which batter it is bowling to (drives the matchup plan). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Bowler")
	void SetCurrentBatter(const FCricketAIPlayerProfile& InBatter) { CurrentBatter = InBatter; }

	/** Run the brain for the current situation and cache the decision (no bowl yet). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Bowler")
	void PlanDelivery();

	/** Push the cached decision into the bowling controller and bowl it. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Bowler")
	void ExecutePlanned();

	/** Convenience: plan then bowl in one call. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Bowler")
	void PlanAndBowl() { PlanDelivery(); ExecutePlanned(); }

	const FCricketBowlerDecision& GetLastDecision() const { return LastDecision; }
	const FCricketDecisionTrace& GetLastTrace() const { return LastDecision.Trace; }

	/** This bowler's AI personality. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Bowler")
	FCricketAIPlayerProfile Profile;

	/** Team plan (affects nothing for a single bowler beyond difficulty defaults). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Bowler")
	FCricketTeamStrategy Strategy;

	/** Skill tier — sets decision quality and execution scatter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Bowler")
	ECricketAIDifficulty Difficulty = ECricketAIDifficulty::Hard;

	/** Par total used to gauge first-innings pressure when an engine is wired. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Bowler")
	int32 ParTotalT20 = 165;

	/** Deterministic seed; advanced each ball for variety. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Bowler")
	int32 Seed = 1;

private:
	FCricketMatchSituation BuildSituation() const;

	UPROPERTY() TWeakObjectPtr<UCricketBowlingComponent> Bowling;
	UPROPERTY() TWeakObjectPtr<UCricketMatchEngine> Engine;

	FCricketAIPlayerProfile CurrentBatter;
	FCricketBowlerMemory Memory;
	FCricketBowlerDecision LastDecision;
	bool bHasPlan = false;
};
