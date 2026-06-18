#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CricketHUDTypes.h"
#include "CricketPitchInteraction.h"   // FCricketBounceReport (cached from the bounce event)
#include "CricketHUDDataSource.generated.h"

class UCricketMatchEngine;
class UCricketBattingComponent;
class UCricketBowlingComponent;
class UCricketReplayComponent;
class UCricketBallPhysicsComponent;
struct FCricketReplayClip;
struct FCricketReplayPlayer;

/**
 * UCricketHUDDataSource — the data-binding engine for the HUD.
 *
 * It discovers the authoritative gameplay systems in the world (the Match Engine,
 * the batting/bowling controllers, the replay manager and the ball physics) and,
 * once per frame, snapshots them into an immutable FCricketHUDModel. It is the ONLY
 * place the UI reaches into gameplay, and it does so through const reads exclusively
 * — the brief's rule that "the UI must never own gameplay state" is enforced here by
 * construction.
 *
 * The Build* functions are static and take const sources, so the binding is pure and
 * the automation tests drive them directly (a configured engine in, a view-model out)
 * with no world, widgets or RHI. The one piece of retained state is the rolling
 * bounce tally, which it accumulates by listening to the ball's bounce delegate.
 */
UCLASS(BlueprintType)
class CRICKETUI_API UCricketHUDDataSource : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * (Re)discover the live systems in World. Cheap and idempotent: a still-valid
	 * source is kept; only missing ones are searched for. Call once per frame.
	 */
	void ResolveSources(UWorld* World);

	/** Snapshot every resolved system into a fresh model. Pure read. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|HUD")
	FCricketHUDModel BuildModel() const;

	// --- Source access (for the canvas overlays that want the live objects) ----
	UCricketMatchEngine* GetMatchEngine() const { return MatchEngine.Get(); }
	UCricketBallPhysicsComponent* GetBallPhysics() const { return BallPhysics.Get(); }

	// --- Pure builders (the testable binding layer) ----------------------------

	/** Score, overs, run rates, batters, bowler, chase and result from the engine. */
	static FCricketScoreboardVM BuildScoreboard(const UCricketMatchEngine& Engine);

	/** Last stroke + live swing from the batting controller. */
	static FCricketBattingVM BuildBatting(const UCricketBattingComponent& Batting);

	/** Current intent + predicted delivery shape from the bowling controller. */
	static FCricketBowlingVM BuildBowling(const UCricketBowlingComponent& Bowling);

	/**
	 * Replay transport from primitive state — the single builder both the live
	 * component path and the unit tests call (tests drive a clip + player directly).
	 */
	static FCricketReplayVM BuildReplay(bool bReplaying, bool bPaused, double Rate,
		double NormalizedTime, int32 FrameCount, double DurationSec, int32 EventCount);

	/** Convenience wrapper that reads a live replay component's getters. */
	static FCricketReplayVM BuildReplayFromComponent(const UCricketReplayComponent& Replay);

	/** Live ball physics + the last bounce, mirrored for the developer overlay. */
	static FCricketPhysicsVM BuildPhysics(const UCricketBallPhysicsComponent& Ball,
		bool bHasBounce, int32 BounceCount, const FCricketBounceReport& LastBounce);

private:
	/** Bound to the ball's OnBounce; tallies bounces for the physics overlay. */
	UFUNCTION()
	void HandleBounce(FCricketBounceReport Report);

	/** Subscribe to a (possibly new) ball physics component's bounce delegate. */
	void RebindBounceDelegate(UCricketBallPhysicsComponent* NewBall);

	UPROPERTY() TWeakObjectPtr<UCricketMatchEngine> MatchEngine;
	UPROPERTY() TWeakObjectPtr<UCricketBattingComponent> Batting;
	UPROPERTY() TWeakObjectPtr<UCricketBowlingComponent> Bowling;
	UPROPERTY() TWeakObjectPtr<UCricketReplayComponent> Replay;
	UPROPERTY() TWeakObjectPtr<UCricketBallPhysicsComponent> BallPhysics;
	UPROPERTY() TWeakObjectPtr<UCricketBallPhysicsComponent> SubscribedBall;

	FCricketBounceReport LastBounce;
	int32 BounceCount = 0;
	bool bHasBounce = false;
};
