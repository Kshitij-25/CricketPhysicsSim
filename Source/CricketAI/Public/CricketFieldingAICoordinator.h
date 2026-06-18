#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketFieldingBrain.h"
#include "CricketFieldingAICoordinator.generated.h"

class UCricketFielderComponent;

/**
 * UCricketFieldingAICoordinator — the Fielding AI's team-level controller.
 *
 * It holds the fielding side's UCricketFielderComponents and, each tick, asks each
 * one for its physics-true intercept of the live ball (the component already solves
 * this against the real prediction), runs the FCricketFieldingBrain to pick the best
 * chaser, and designates it via the component's own SetActiveChaser. The per-fielder
 * movement, catch, pickup and throw all remain in the fielder component running off
 * the real physics — this layer only makes the coordination decision (who goes,
 * whether the hard chance is worth a dive, where to throw).
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETAI_API UCricketFieldingAICoordinator : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketFieldingAICoordinator();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Fielding")
	void SetFielders(const TArray<UCricketFielderComponent*>& InFielders);

	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Fielding")
	void AddFielder(UCricketFielderComponent* InFielder);

	const FCricketFieldingPlan& GetPlan() const { return Plan; }
	int32 GetChaserIndex() const { return Plan.ChaserIndex; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Fielding")
	ECricketAIDifficulty Difficulty = ECricketAIDifficulty::Hard;

	/** Average fielding rating of the side (gates committing to diving chances). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Fielding")
	double SideFieldingRating = 0.6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Fielding")
	int32 Seed = 1;

private:
	void Coordinate();

	UPROPERTY() TArray<TWeakObjectPtr<UCricketFielderComponent>> Fielders;
	FCricketFieldingPlan Plan;
};
