#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketAIDebugComponent.generated.h"

class UCricketBatterAIController;
class UCricketBowlerAIController;
class UCricketCaptainAIController;
class UCricketFieldingAICoordinator;

/**
 * UCricketAIDebugComponent — the AI Debug Tooling overlay.
 *
 * Point it at any of the AI controllers and, while the console variable
 * `cricket.AI.Debug 1` is set, it renders their live reasoning: the current AI
 * state and tactical intent, the scored options behind each decision (with the
 * chosen one and its selection probability), the fielding chaser decision, and the
 * batter's shot-selection logic (what it read and why it played what it played).
 *
 * It is a pure READER of the controllers' cached decision traces — it never
 * influences a decision or the simulation.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETAI_API UCricketAIDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketAIDebugComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Debug")
	void WatchBatter(UCricketBatterAIController* In);

	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Debug")
	void WatchBowler(UCricketBowlerAIController* In);

	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Debug")
	void WatchCaptain(UCricketCaptainAIController* In);

	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Debug")
	void WatchFielding(UCricketFieldingAICoordinator* In);

	/** On-screen text scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Debug")
	float TextScale = 1.0f;

private:
	UPROPERTY() TWeakObjectPtr<UCricketBatterAIController> Batter;
	UPROPERTY() TWeakObjectPtr<UCricketBowlerAIController> Bowler;
	UPROPERTY() TWeakObjectPtr<UCricketCaptainAIController> Captain;
	UPROPERTY() TWeakObjectPtr<UCricketFieldingAICoordinator> Fielding;
};
