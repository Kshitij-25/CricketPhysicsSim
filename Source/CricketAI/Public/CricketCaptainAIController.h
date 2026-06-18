#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketCaptainBrain.h"
#include "CricketAIPlayerProfile.h"
#include "CricketCaptainAIController.generated.h"

class UCricketMatchEngine;

/**
 * UCricketCaptainAIController — the Captain AI Controller component.
 *
 * The on-field captain for one side: it manages the bowling rotation and the field
 * against the live Match Engine. Each over it builds the eligible-bowler pool from
 * the engine's own bowler stats (overs bowled, runs, wickets) and the no-consecutive
 * rule, asks the FCricketCaptainBrain who to bowl, and nominates them through the
 * engine's legal SetBowler API. It also produces a phase- and matchup-appropriate
 * field (legal under the powerplay restriction) for the batter being faced.
 *
 * It only CHOOSES; the laws (over caps, no consecutive overs) are still enforced by
 * the engine, and the runs come from the physics/contest, never from the captain.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETAI_API UCricketCaptainAIController : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketCaptainAIController();

	/** Wire the match engine the captain commands a side within. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Captain")
	void SetEngine(UCricketMatchEngine* InEngine);

	/** Provide the bowling resources (this side's player personalities). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Captain")
	void SetSquadProfiles(const TArray<FCricketAIPlayerProfile>& InProfiles) { SquadProfiles = InProfiles; }

	/**
	 * Pick the next over's bowler and nominate it on the engine. Returns the chosen
	 * name (empty if none could be legally set). Call when the engine NeedsBowler().
	 */
	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Captain")
	FString ChooseAndSetBowler();

	/** Build a field for the given bowler/batter matchup in the current situation. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Captain")
	FCricketFieldPlacement SetFieldFor(const FString& BowlerName, const FString& BatterName);

	const FCricketBowlingChange& GetLastChange() const { return LastChange; }
	const FCricketFieldPlacement& GetLastField() const { return LastField; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Captain")
	FCricketTeamStrategy Strategy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Captain")
	ECricketAIDifficulty Difficulty = ECricketAIDifficulty::Hard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Captain")
	int32 ParTotalT20 = 165;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Captain")
	int32 Seed = 7;

private:
	const FCricketAIPlayerProfile* FindProfile(const FString& Name) const;

	UPROPERTY() TWeakObjectPtr<UCricketMatchEngine> Engine;
	TArray<FCricketAIPlayerProfile> SquadProfiles;
	FString CurrentOverBowler;
	FCricketBowlingChange LastChange;
	FCricketFieldPlacement LastField;
};
