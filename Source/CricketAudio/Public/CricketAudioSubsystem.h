#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CricketAudioTypes.h"
#include "CricketAudioRouting.h"
#include "CricketCrowdController.h"
#include "CricketEnvironmentController.h"
#include "CricketAudioSubsystem.generated.h"

class UCricketAudioBankAsset;
class UCricketMatchEngine;
class UCricketBallPhysicsComponent;
class UCricketBowlingComponent;
class UCricketReplayComponent;
class UCricketFielderComponent;
class UAudioComponent;
struct FCricketDeliveryOutcome;
struct FCricketBounceReport;
struct FCricketBatImpactReport;
struct FCricketReleaseParameters;
struct FCricketDeliveryDiagnostics;
enum class ECricketDismissal : uint8;
enum class ECricketFielderState : uint8;

/** How a posted request resolved — surfaced by the debug overlay. */
UENUM()
enum class ECricketAudioPlayResult : uint8
{
	Played,        // a sound was triggered
	NoAsset,       // the bank has no sound for the event (system still consistent)
	DroppedBudget, // routing dropped it (voice budget / muted bus)
	Invalid        // empty cue
};

/** One row in the debug overlay's recent-events log. */
USTRUCT()
struct FCricketAudioDebugEntry
{
	GENERATED_BODY()

	UPROPERTY() ECricketAudioEvent Event = ECricketAudioEvent::None;
	UPROPERTY() ECricketAudioCategory Category = ECricketAudioCategory::Ball;
	UPROPERTY() ECricketAudioPriority Priority = ECricketAudioPriority::Normal;
	UPROPERTY() FName Source = NAME_None;
	UPROPERTY() float Gain = 0.0f;
	UPROPERTY() float Pitch = 1.0f;
	UPROPERTY() ECricketAudioPlayResult Result = ECricketAudioPlayResult::Played;
	UPROPERTY() double TimeSeconds = 0.0;
};

/**
 * UCricketAudioSubsystem — the AUDIO MANAGER.
 *
 * A world subsystem (auto-created, no level placement) that is the single hub of the
 * reactive audio layer. It:
 *   - DISCOVERS the gameplay sources (Match Engine, ball physics, bowler, replay,
 *     fielders) and BINDS to their events — the Event-Based Audio System;
 *   - TRANSLATES each event into a cue via FCricketAudioSelector / the crowd &
 *     environment controllers;
 *   - ROUTES the cue (bus gain + priority/voice budget) and PLAYS it from the
 *     data-driven sound bank, spatialised at the world source or 2D for crowd/ambience;
 *   - maintains the looping beds (ball flight, crowd, stadium/day/night ambience) and
 *     applies the replay slow-motion pitch scale to live SFX;
 *   - records recent events for the debug overlay (cvar cricket.Audio.Debug).
 *
 * It holds NO gameplay state and writes to NO simulation system: every binding is a
 * read, and a sound never feeds back into an outcome. With no bank assigned the whole
 * layer runs silently but fully consistently (the overlay reports "NoAsset").
 */
UCLASS()
class CRICKETAUDIO_API UCricketAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Subsystem lifecycle ---------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// --- Tickable --------------------------------------------------------------
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	// --- Public API ------------------------------------------------------------

	/** Assign the sound bank (also settable in editor/data). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Audio")
	void SetBank(UCricketAudioBankAsset* InBank) { Bank = InBank; }

	/** Post a fully-formed request to the mixer. The lowest-level entry point. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Audio")
	void PostEvent(const FCricketAudioRequest& Request);

	/** Convenience: post a cue at a location. */
	void PostCueAt(const FCricketAudioCue& Cue, const FVector& WorldLocationCm, bool bSpatial, FName Source);

	/** Day/night & venue selection for ambience. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Audio")
	void SetTimeOfDay(ECricketTimeOfDay TimeOfDay);

	const FCricketCrowdController& GetCrowd() const { return Crowd; }
	const FCricketAudioRouting& GetRouting() const { return Routing; }
	FCricketAudioRouting& GetRoutingMutable() { return Routing; }
	float GetReplayPitchScale() const { return ReplayPitchScale; }

private:
	// --- Discovery + event binding (the reactive listener) ---------------------
	void ResolveAndBindSources();
	void PollFielders();
	void UpdateFlightLoop();
	void UpdateReplayScale();
	void UpdateCrowdBed(float DeltaSeconds);
	void UpdateAmbience();
	void PruneVoices();
	void DrawDebug();

	// --- Bound event handlers (single-instance sources) ------------------------
	UFUNCTION() void HandleBallApplied(FCricketDeliveryOutcome Outcome);
	UFUNCTION() void HandleWicket(ECricketDismissal How);
	UFUNCTION() void HandleBounce(FCricketBounceReport Report);
	UFUNCTION() void HandleBatImpact(FCricketBatImpactReport Report);
	UFUNCTION() void HandleDelivery(FCricketReleaseParameters Params, FCricketDeliveryDiagnostics Diagnostics);

	// --- Helpers ---------------------------------------------------------------
	int32 ActiveCountInCategory(ECricketAudioCategory Category) const;
	FVector BallLocationCm() const;
	UAudioComponent* StartLoop2D(ECricketAudioEvent Event, float Gain);
	void RecordDebug(const FCricketAudioCue& Cue, FName Source, ECricketAudioPlayResult Result, float FinalGain);

	// --- State -----------------------------------------------------------------
	UPROPERTY(Transient) TObjectPtr<UCricketAudioBankAsset> Bank;

	UPROPERTY() FCricketAudioRouting Routing;
	UPROPERTY() FCricketCrowdController Crowd;
	UPROPERTY() FCricketEnvironmentController Environment;

	// Sources (weak; rediscovered if they go away).
	UPROPERTY() TWeakObjectPtr<UCricketMatchEngine> MatchEngine;
	UPROPERTY() TWeakObjectPtr<UCricketBallPhysicsComponent> BallPhysics;
	UPROPERTY() TWeakObjectPtr<UCricketBowlingComponent> Bowling;
	UPROPERTY() TWeakObjectPtr<UCricketReplayComponent> Replay;

	// Which single-instance sources we have already bound to.
	bool bBoundMatch = false;
	bool bBoundBall = false;
	bool bBoundBowling = false;

	// Fielders are polled (the dynamic delegate hides the sender); track last state.
	TMap<TWeakObjectPtr<UCricketFielderComponent>, uint8> FielderStates;

	// Persistent loops.
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> FlightLoop;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> CrowdBed;
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> AmbienceLoops;
	bool bAmbienceStarted = false;

	// Active one-shot voices (for the per-category voice budget + replay pitch).
	struct FActiveVoice
	{
		TWeakObjectPtr<UAudioComponent> Comp;
		ECricketAudioCategory Category = ECricketAudioCategory::Ball;
		float BasePitch = 1.0f;
		bool bSpatial = true;
	};
	TArray<FActiveVoice> ActiveVoices;

	// Replay slow-mo pitch scale applied to live spatial SFX.
	float ReplayPitchScale = 1.0f;

	// Debug ring.
	TArray<FCricketAudioDebugEntry> RecentEvents;
	double NowSeconds = 0.0;
};
