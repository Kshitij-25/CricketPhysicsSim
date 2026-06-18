#include "CricketAudioSubsystem.h"
#include "CricketAudioBankAsset.h"
#include "CricketAudioSelector.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

#include "CricketMatchRunner.h"
#include "CricketMatchEngine.h"
#include "CricketScoringTypes.h"          // FCricketDeliveryOutcome
#include "CricketMatchTypes.h"            // ECricketDismissal
#include "CricketBallPhysicsComponent.h"
#include "CricketBowlingComponent.h"
#include "CricketReplayComponent.h"
#include "CricketFielderComponent.h"      // ECricketFielderState, GetIntercept
#include "CricketPitchInteraction.h"      // FCricketBounceReport

using namespace CricketAudio;

namespace
{
	TAutoConsoleVariable<int32> CVarAudioDebug(TEXT("cricket.Audio.Debug"), 0,
		TEXT("Audio debug overlay: active events, sources, categories, priorities, crowd/replay state. 0=off, 1=on"));

	constexpr int32 MaxDebugRows = 12;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UCricketAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Sensible default routing: everything at unity, a generous voice budget.
	Routing.MasterGain = 1.0f;
	Routing.MaxVoicesPerCategory = 8;
}

void UCricketAudioSubsystem::Deinitialize()
{
	if (FlightLoop) { FlightLoop->Stop(); FlightLoop = nullptr; }
	if (CrowdBed)   { CrowdBed->Stop();   CrowdBed = nullptr; }
	for (UAudioComponent* C : AmbienceLoops) { if (C) { C->Stop(); } }
	AmbienceLoops.Reset();
	Super::Deinitialize();
}

bool UCricketAudioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCricketAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCricketAudioSubsystem, STATGROUP_Tickables);
}

void UCricketAudioSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	NowSeconds += DeltaTime;

	ResolveAndBindSources();
	PollFielders();
	UpdateFlightLoop();
	UpdateReplayScale();
	UpdateCrowdBed(DeltaTime);
	UpdateAmbience();
	PruneVoices();

	if (CVarAudioDebug.GetValueOnGameThread() != 0) { DrawDebug(); }
}

// ---------------------------------------------------------------------------
// Discovery + event binding (the reactive listener)
// ---------------------------------------------------------------------------

void UCricketAudioSubsystem::ResolveAndBindSources()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	// Drop stale bindings so a re-created source is rebound.
	if (!MatchEngine.IsValid()) { bBoundMatch = false; }
	if (!BallPhysics.IsValid()) { bBoundBall = false; }
	if (!Bowling.IsValid())     { bBoundBowling = false; }

	if (!MatchEngine.IsValid())
	{
		for (TActorIterator<ACricketMatchRunner> It(World); It; ++It)
		{
			if (UCricketMatchEngine* E = It->GetEngine()) { MatchEngine = E; break; }
		}
	}
	if (!BallPhysics.IsValid() || !Bowling.IsValid() || !Replay.IsValid())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!BallPhysics.IsValid()) { if (auto* C = A->FindComponentByClass<UCricketBallPhysicsComponent>()) { BallPhysics = C; } }
			if (!Bowling.IsValid())     { if (auto* C = A->FindComponentByClass<UCricketBowlingComponent>())     { Bowling = C; } }
			if (!Replay.IsValid())      { if (auto* C = A->FindComponentByClass<UCricketReplayComponent>())      { Replay = C; } }
		}
	}

	if (MatchEngine.IsValid() && !bBoundMatch)
	{
		MatchEngine->OnBallApplied.AddDynamic(this, &UCricketAudioSubsystem::HandleBallApplied);
		MatchEngine->OnWicket.AddDynamic(this, &UCricketAudioSubsystem::HandleWicket);
		bBoundMatch = true;
	}
	if (BallPhysics.IsValid() && !bBoundBall)
	{
		BallPhysics->OnBounce.AddDynamic(this, &UCricketAudioSubsystem::HandleBounce);
		BallPhysics->OnBatImpact.AddDynamic(this, &UCricketAudioSubsystem::HandleBatImpact);
		bBoundBall = true;
	}
	if (Bowling.IsValid() && !bBoundBowling)
	{
		Bowling->OnDelivery.AddDynamic(this, &UCricketAudioSubsystem::HandleDelivery);
		bBoundBowling = true;
	}
}

void UCricketAudioSubsystem::PollFielders()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	// A dynamic delegate hides which fielder fired, so we poll each fielder's state
	// machine for transitions — and get its location + catch difficulty for free.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		UCricketFielderComponent* F = It->FindComponentByClass<UCricketFielderComponent>();
		if (!F) { continue; }

		const uint8 NewState = (uint8)F->GetState();
		uint8* Prev = FielderStates.Find(F);
		if (!Prev)
		{
			FielderStates.Add(F, NewState); // first sighting: establish baseline, no sound
			continue;
		}
		if (*Prev != NewState)
		{
			*Prev = NewState;
			const FCricketAudioCue Cue = FCricketAudioSelector::FromFielderState(
				(ECricketFielderState)NewState, F->GetIntercept().Difficulty);
			if (Cue.IsValid())
			{
				const FVector Loc = F->GetHandWorldCm();
				PostCueAt(Cue, Loc, true, TEXT("Fielder"));
				if (Cue.Event == ECricketAudioEvent::Dive)
				{
					PostCueAt(FCricketAudioSelector::Effort(0.8f), Loc, true, TEXT("Fielder"));
				}
			}
		}
	}

	for (auto It = FielderStates.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) { It.RemoveCurrent(); }
	}
}

// ---------------------------------------------------------------------------
// Looping beds + replay
// ---------------------------------------------------------------------------

void UCricketAudioSubsystem::UpdateFlightLoop()
{
	UCricketBallPhysicsComponent* B = BallPhysics.Get();
	const bool bInFlight = B && B->IsBallInFlight();

	if (!bInFlight)
	{
		if (FlightLoop) { FlightLoop->Stop(); FlightLoop = nullptr; }
		return;
	}

	const FCricketAudioCue Cue = FCricketAudioSelector::FlightLoop(B->GetSpeedKmh());
	const FCricketSoundSet* Set = Bank ? Bank->Find(ECricketAudioEvent::BallFlightLoop) : nullptr;
	if (!Set || !Set->HasSound()) { return; }

	const float Vol   = Routing.ResolveGain(Cue) * Set->BaseVolume;
	const float Pitch = Cue.Pitch * Set->BasePitch * ReplayPitchScale;

	if (!FlightLoop && B->GetOwner() && B->GetOwner()->GetRootComponent())
	{
		FlightLoop = UGameplayStatics::SpawnSoundAttached(
			Set->Variations[0], B->GetOwner()->GetRootComponent(), NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
			/*bStopWhenAttachedToDestroyed*/ true, Vol, Pitch, 0.f,
			Set->Attenuation, Set->Concurrency, /*bAutoDestroy*/ false);
	}
	else if (FlightLoop)
	{
		FlightLoop->SetVolumeMultiplier(Vol);
		FlightLoop->SetPitchMultiplier(Pitch);
	}
}

void UCricketAudioSubsystem::UpdateReplayScale()
{
	float NewScale = 1.0f;
	if (UCricketReplayComponent* R = Replay.Get())
	{
		NewScale = FCricketAudioSelector::ReplayPitchScale(R->IsReplaying(), R->IsPaused(), R->GetPlaybackRate());
	}
	if (!FMath::IsNearlyEqual(NewScale, ReplayPitchScale))
	{
		ReplayPitchScale = NewScale;
		for (const FActiveVoice& V : ActiveVoices)
		{
			if (V.bSpatial && V.Comp.IsValid())
			{
				V.Comp->SetPitchMultiplier(V.BasePitch * ReplayPitchScale);
			}
		}
	}
}

void UCricketAudioSubsystem::UpdateCrowdBed(float DeltaSeconds)
{
	Crowd.Tick(DeltaSeconds);

	const FCricketSoundSet* Set = Bank ? Bank->Find(ECricketAudioEvent::CrowdAmbientBed) : nullptr;
	if (!Set || !Set->HasSound()) { return; }

	const float Vol = Routing.GainFor(ECricketAudioCategory::Crowd) * Crowd.BedGain() * Set->BaseVolume;
	if (!CrowdBed)
	{
		CrowdBed = StartLoop2D(ECricketAudioEvent::CrowdAmbientBed, Crowd.BedGain());
	}
	else
	{
		CrowdBed->SetVolumeMultiplier(Vol);
	}
}

void UCricketAudioSubsystem::UpdateAmbience()
{
	if (bAmbienceStarted) { return; }
	bAmbienceStarted = true; // start once; SetTimeOfDay() restarts

	for (const FCricketAudioCue& Cue : Environment.AmbienceCues())
	{
		if (UAudioComponent* C = StartLoop2D(Cue.Event, Cue.Gain))
		{
			AmbienceLoops.Add(C);
		}
	}
}

void UCricketAudioSubsystem::SetTimeOfDay(ECricketTimeOfDay TimeOfDay)
{
	Environment.TimeOfDay = TimeOfDay;
	for (UAudioComponent* C : AmbienceLoops) { if (C) { C->Stop(); } }
	AmbienceLoops.Reset();
	bAmbienceStarted = false; // re-laid next tick
}

UAudioComponent* UCricketAudioSubsystem::StartLoop2D(ECricketAudioEvent Event, float Gain)
{
	const FCricketSoundSet* Set = Bank ? Bank->Find(Event) : nullptr;
	if (!Set || !Set->HasSound()) { return nullptr; }

	const float Vol = Routing.GainFor(CategoryOf(Event)) * Gain * Set->BaseVolume;
	UAudioComponent* C = UGameplayStatics::SpawnSound2D(GetWorld(), Set->Variations[0], Vol, Set->BasePitch,
		0.f, Set->Concurrency, /*bPersistAcrossLevelTransition*/ false, /*bAutoDestroy*/ false);
	return C;
}

void UCricketAudioSubsystem::PruneVoices()
{
	ActiveVoices.RemoveAll([](const FActiveVoice& V)
	{
		return !V.Comp.IsValid() || !V.Comp->IsPlaying();
	});
}

// ---------------------------------------------------------------------------
// Posting / playback
// ---------------------------------------------------------------------------

int32 UCricketAudioSubsystem::ActiveCountInCategory(ECricketAudioCategory Category) const
{
	int32 Count = 0;
	for (const FActiveVoice& V : ActiveVoices)
	{
		if (V.Category == Category && V.Comp.IsValid()) { ++Count; }
	}
	return Count;
}

FVector UCricketAudioSubsystem::BallLocationCm() const
{
	if (BallPhysics.IsValid() && BallPhysics->GetOwner())
	{
		return BallPhysics->GetOwner()->GetActorLocation();
	}
	return FVector::ZeroVector;
}

void UCricketAudioSubsystem::PostCueAt(const FCricketAudioCue& Cue, const FVector& WorldLocationCm, bool bSpatial, FName Source)
{
	FCricketAudioRequest Req;
	Req.Cue = Cue;
	Req.WorldLocationCm = WorldLocationCm;
	Req.bSpatial = bSpatial;
	Req.Source = Source;
	PostEvent(Req);
}

void UCricketAudioSubsystem::PostEvent(const FCricketAudioRequest& Request)
{
	const FCricketAudioCue& Cue = Request.Cue;
	if (!Cue.IsValid())
	{
		RecordDebug(Cue, Request.Source, ECricketAudioPlayResult::Invalid, 0.f);
		return;
	}

	// Routing: voice budget + priority preemption.
	const int32 Active = ActiveCountInCategory(Cue.Category);
	if (!Routing.ShouldPlay(Cue, Active))
	{
		RecordDebug(Cue, Request.Source, ECricketAudioPlayResult::DroppedBudget, 0.f);
		return;
	}

	const float FinalGain = Routing.ResolveGain(Cue);

	// Data-driven asset lookup. A missing entry is consistent, not an error.
	const FCricketSoundSet* Set = Bank ? Bank->Find(Cue.Event) : nullptr;
	if (!Set || !Set->HasSound())
	{
		RecordDebug(Cue, Request.Source, ECricketAudioPlayResult::NoAsset, FinalGain);
		return;
	}

	const int32 Idx = (Cue.Variation >= 0 && Cue.Variation < Set->Variations.Num())
		? Cue.Variation
		: FMath::RandRange(0, Set->Variations.Num() - 1);   // RNG only picks a SOUND — never an outcome
	USoundBase* Sound = Set->Variations.IsValidIndex(Idx) ? Set->Variations[Idx].Get() : nullptr;
	if (!Sound)
	{
		RecordDebug(Cue, Request.Source, ECricketAudioPlayResult::NoAsset, FinalGain);
		return;
	}

	const bool bSpatial = Request.bSpatial && IsSpatialCategory(Cue.Category);
	const float Vol = FinalGain * Set->BaseVolume;
	const float BasePitch = Cue.Pitch * Set->BasePitch;
	const float Pitch = BasePitch * (bSpatial ? ReplayPitchScale : 1.0f);

	UAudioComponent* Comp = nullptr;
	if (bSpatial)
	{
		Comp = UGameplayStatics::SpawnSoundAtLocation(GetWorld(), Sound, Request.WorldLocationCm,
			FRotator::ZeroRotator, Vol, Pitch, 0.f, Set->Attenuation, Set->Concurrency, /*bAutoDestroy*/ true);
	}
	else
	{
		Comp = UGameplayStatics::SpawnSound2D(GetWorld(), Sound, Vol, Pitch, 0.f, Set->Concurrency,
			false, /*bAutoDestroy*/ true);
	}

	if (Comp)
	{
		ActiveVoices.Add(FActiveVoice{ Comp, Cue.Category, BasePitch, bSpatial });
	}
	RecordDebug(Cue, Request.Source, ECricketAudioPlayResult::Played, FinalGain);
}

// ---------------------------------------------------------------------------
// Bound handlers — translate gameplay events into cues
// ---------------------------------------------------------------------------

void UCricketAudioSubsystem::HandleBallApplied(FCricketDeliveryOutcome Outcome)
{
	FCricketAudioCue CrowdCue;
	if (Outcome.bBoundary && Outcome.RunsOffBat >= 6)
	{
		if (Crowd.ReactTo(ECricketAudioEvent::CrowdSix, CrowdCue)) { PostCueAt(CrowdCue, FVector::ZeroVector, false, TEXT("Crowd")); }
	}
	else if (Outcome.bBoundary && Outcome.RunsOffBat == 4)
	{
		if (Crowd.ReactTo(ECricketAudioEvent::CrowdFour, CrowdCue)) { PostCueAt(CrowdCue, FVector::ZeroVector, false, TEXT("Crowd")); }
	}
	else if (!Outcome.bBoundary && Outcome.RunsOffBat >= 1 && Outcome.RunsOffBat <= 3)
	{
		PostCueAt(FCricketAudioSelector::RunCall(Outcome.RunsOffBat), BallLocationCm(), true, TEXT("Batter"));
	}
}

void UCricketAudioSubsystem::HandleWicket(ECricketDismissal How)
{
	const FCricketAudioCue Impact = FCricketAudioSelector::FromDismissal(How);
	const FVector Loc = BallLocationCm();
	if (Impact.IsValid()) { PostCueAt(Impact, Loc, true, TEXT("Wicket")); }

	PostCueAt(FCricketAudioSelector::Appeal(How), Loc, true, TEXT("Fielder"));
	PostCueAt(FCricketAudioSelector::WicketCelebration(), Loc, true, TEXT("Fielder"));

	FCricketAudioCue CrowdCue;
	if (Crowd.ReactTo(ECricketAudioEvent::CrowdWicket, CrowdCue)) { PostCueAt(CrowdCue, FVector::ZeroVector, false, TEXT("Crowd")); }
}

void UCricketAudioSubsystem::HandleBounce(FCricketBounceReport Report)
{
	const double Kmh = BallPhysics.IsValid() ? BallPhysics->GetSpeedKmh() : 100.0;
	PostCueAt(FCricketAudioSelector::FromBounce(Report, Kmh), BallLocationCm(), true, TEXT("Ball"));
}

void UCricketAudioSubsystem::HandleBatImpact(FCricketBatImpactReport Report)
{
	if (!Report.bMadeContact) { return; }
	const FVector Loc = BallLocationCm();
	PostCueAt(FCricketAudioSelector::FromBatImpact(Report), Loc, true, TEXT("Bat"));

	// A carried edge raises a "close chance" murmur in the crowd.
	if (Report.bIsEdge)
	{
		FCricketAudioCue CrowdCue;
		if (Crowd.ReactTo(ECricketAudioEvent::CrowdCloseChance, CrowdCue)) { PostCueAt(CrowdCue, FVector::ZeroVector, false, TEXT("Crowd")); }
	}
	// A big strike grunts.
	if (Report.ExitSpeedMS > 22.0)
	{
		PostCueAt(FCricketAudioSelector::Effort(0.8f), Loc, true, TEXT("Batter"));
	}
}

void UCricketAudioSubsystem::HandleDelivery(FCricketReleaseParameters Params, FCricketDeliveryDiagnostics /*Diagnostics*/)
{
	const FVector Loc = Bowling.IsValid() ? Bowling->GetReleaseWorldCm() : BallLocationCm();
	PostCueAt(FCricketAudioSelector::FromRelease(Params), Loc, true, TEXT("Bowler"));
}

// ---------------------------------------------------------------------------
// Debug overlay
// ---------------------------------------------------------------------------

void UCricketAudioSubsystem::RecordDebug(const FCricketAudioCue& Cue, FName Source, ECricketAudioPlayResult Result, float FinalGain)
{
	FCricketAudioDebugEntry E;
	E.Event = Cue.Event;
	E.Category = Cue.Category;
	E.Priority = Cue.Priority;
	E.Source = Source;
	E.Gain = FinalGain;
	E.Pitch = Cue.Pitch;
	E.Result = Result;
	E.TimeSeconds = NowSeconds;
	RecentEvents.Add(E);
	while (RecentEvents.Num() > MaxDebugRows) { RecentEvents.RemoveAt(0, 1, EAllowShrinking::No); }
}

void UCricketAudioSubsystem::DrawDebug()
{
	if (!GEngine) { return; }
	auto Line = [&](int32 Key, const FColor& C, const FString& T) { GEngine->AddOnScreenDebugMessage(Key, 0.f, C, T); };

	Line(7000, FColor(120, 220, 255), TEXT("=== CRICKET AUDIO ==="));
	Line(7001, FColor::Silver, FString::Printf(TEXT("Master %.2f  ReplayPitch %.2f  Voices %d  Crowd excite %.2f (bed %.2f)"),
		Routing.MasterGain, ReplayPitchScale, ActiveVoices.Num(), Crowd.Excitement, Crowd.BedGain()));
	Line(7002, FColor::Silver, FString::Printf(TEXT("Active by category — Ball %d  Bat %d  Impact %d  Fielding %d  Player %d  Crowd %d  Env %d"),
		ActiveCountInCategory(ECricketAudioCategory::Ball), ActiveCountInCategory(ECricketAudioCategory::Bat),
		ActiveCountInCategory(ECricketAudioCategory::Impact), ActiveCountInCategory(ECricketAudioCategory::Fielding),
		ActiveCountInCategory(ECricketAudioCategory::Player), ActiveCountInCategory(ECricketAudioCategory::Crowd),
		ActiveCountInCategory(ECricketAudioCategory::Environment)));
	Line(7003, FColor(180, 180, 180), TEXT("recent events (event | category | priority | source | gain | result):"));

	int32 Key = 7010;
	for (int32 i = RecentEvents.Num() - 1; i >= 0; --i)
	{
		const FCricketAudioDebugEntry& E = RecentEvents[i];
		FColor C = FColor::Green;
		switch (E.Result)
		{
		case ECricketAudioPlayResult::NoAsset:       C = FColor(160, 160, 160); break;
		case ECricketAudioPlayResult::DroppedBudget: C = FColor::Orange; break;
		case ECricketAudioPlayResult::Invalid:       C = FColor::Red; break;
		default: break;
		}
		const TCHAR* ResultStr =
			E.Result == ECricketAudioPlayResult::Played ? TEXT("PLAYED") :
			E.Result == ECricketAudioPlayResult::NoAsset ? TEXT("no-asset") :
			E.Result == ECricketAudioPlayResult::DroppedBudget ? TEXT("dropped") : TEXT("invalid");
		Line(Key++, C, FString::Printf(TEXT("  %-20s | %-11s | %-8s | %-8s | %.2f | %s"),
			*EventName(E.Event), *CategoryName(E.Category), *PriorityName(E.Priority),
			*E.Source.ToString(), E.Gain, ResultStr));
	}
}
