#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketAIPlayerProfile.h"
#include "CricketBatterBrain.h"
#include "CricketMatchAwareness.h"
#include "CricketBatterAIController.generated.h"

class UCricketBattingComponent;
class UCricketBallPhysicsComponent;

/**
 * UCricketBatterAIController — the Batter AI Controller component.
 *
 * Attached alongside a UCricketBattingComponent, it plays the AI's strokes through
 * the EXACT control surface a human uses. When a new ball is in flight it READS the
 * live delivery (line/length from the ball's own trajectory — no privileged data),
 * asks the FCricketBatterBrain for an intent, and pushes that intent into the
 * batting controller (shot, footwork, aim, power). It then triggers the swing /
 * defence at the right moment, judged from the live ball — the same timing the
 * player has to find. A lower difficulty MIS-times and mis-selects more; it never
 * gets extra reaction time and never scripts the contact: middling vs edging still
 * emerges from where the swing met the ball.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETAI_API UCricketBatterAIController : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketBatterAIController();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Wire the batting controller this AI drives. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Batter")
	void SetTargets(UCricketBattingComponent* InBatting);

	/** Update the match situation the brain reasons over (call when the score changes). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|AI|Batter")
	void SetSituation(const FCricketMatchSituation& InSituation) { Situation = InSituation; }

	const FCricketBatterDecision& GetLastDecision() const { return LastDecision; }
	const FCricketDecisionTrace& GetLastTrace() const { return LastDecision.Trace; }
	const FCricketDeliveryRead& GetLastRead() const { return LastRead; }

	/** This batter's AI personality. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Batter")
	FCricketAIPlayerProfile Profile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Batter")
	FCricketTeamStrategy Strategy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Batter")
	ECricketAIDifficulty Difficulty = ECricketAIDifficulty::Hard;

	/** The current match situation (settable by a higher-level system). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Batter")
	FCricketMatchSituation Situation;

	/** Nominal downswing lead the AI swings ahead of contact (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Batter")
	double SwingLeadSec = 0.16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|AI|Batter")
	int32 Seed = 1;

private:
	void OnNewDelivery(UCricketBallPhysicsComponent* BallPhysics);
	FCricketDeliveryRead ReadDelivery(UCricketBallPhysicsComponent* BallPhysics) const;

	UPROPERTY() TWeakObjectPtr<UCricketBattingComponent> Batting;

	FCricketBatterDecision LastDecision;
	FCricketDeliveryRead LastRead;
	double TimingOffsetSec = 0.0;   // this delivery's mistiming (difficulty-driven)
	bool bArmed = false;            // decided + intent pushed for the current ball
	bool bSwingFired = false;       // already committed the stroke this ball
};
