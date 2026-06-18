#pragma once

#include "CoreMinimal.h"
#include "CricketAudioTypes.h"
#include "CricketCrowdController.generated.h"

/**
 * FCricketCrowdController — the Crowd Audio Controller (MVP). Deliberately simple:
 * a single "excitement" level in [0,1] that big moments bump up and that decays over
 * time. The excitement swells the ambient crowd bed and gates one-shot reactions
 * (four / six / wicket / close chance), so the stadium breathes with the match
 * without any complex behaviour model.
 *
 * Pure data + methods, so it is driven identically by the live subsystem and by the
 * automation tests.
 */
USTRUCT(BlueprintType)
struct CRICKETAUDIO_API FCricketCrowdController
{
	GENERATED_BODY()

	/** Current excitement [0,1]. */
	UPROPERTY(BlueprintReadOnly, Category = "Audio|Crowd") float Excitement = 0.0f;

	/** Resting volume of the ambient bed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Crowd", meta = (ClampMin = "0.0", ClampMax = "1.0")) float BaseBedGain = 0.35f;

	/** How much excitement adds to the bed at full tilt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Crowd", meta = (ClampMin = "0.0")) float ExcitementBedBoost = 0.5f;

	/** Excitement decay per second back toward calm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Crowd", meta = (ClampMin = "0.0")) float DecayPerSec = 0.25f;

	/** Bump excitement and decay it over time. */
	void Tick(float DeltaSeconds)
	{
		Excitement = FMath::Clamp(Excitement - DecayPerSec * DeltaSeconds, 0.0f, 1.0f);
	}

	/**
	 * React to a match moment. ReactEvent must be one of the Crowd* events
	 * (Four/Six/Wicket/CloseChance). Bumps excitement by the moment's weight and
	 * fills OutCue with the reaction (a 2D one-shot). Returns false for non-reactions.
	 */
	bool ReactTo(ECricketAudioEvent ReactEvent, FCricketAudioCue& OutCue)
	{
		float Bump = 0.0f;
		ECricketAudioPriority Pri = ECricketAudioPriority::Normal;
		switch (ReactEvent)
		{
		case ECricketAudioEvent::CrowdSix:        Bump = 0.60f; Pri = ECricketAudioPriority::High; break;
		case ECricketAudioEvent::CrowdFour:       Bump = 0.40f; Pri = ECricketAudioPriority::Normal; break;
		case ECricketAudioEvent::CrowdWicket:     Bump = 0.70f; Pri = ECricketAudioPriority::High; break;
		case ECricketAudioEvent::CrowdCloseChance:Bump = 0.30f; Pri = ECricketAudioPriority::Normal; break;
		default: return false;
		}
		Excitement = FMath::Clamp(Excitement + Bump, 0.0f, 1.0f);
		OutCue = CricketAudio::MakeCue(ReactEvent, 1.0f, 1.0f, Pri);
		return true;
	}

	/** Bed gain rises with excitement so the crowd lifts after a big moment. */
	float BedGain() const
	{
		return FMath::Clamp(BaseBedGain + ExcitementBedBoost * Excitement, 0.0f, 1.0f);
	}

	/** The ambient bed cue (a 2D loop) at the current bed gain. */
	FCricketAudioCue BedCue() const
	{
		FCricketAudioCue Cue = CricketAudio::MakeCue(ECricketAudioEvent::CrowdAmbientBed, BedGain(), 1.0f, ECricketAudioPriority::Low);
		Cue.bLooping = true;
		return Cue;
	}
};
