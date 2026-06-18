#pragma once

#include "CoreMinimal.h"
#include "CricketAudioTypes.h"
#include "CricketAudioRouting.generated.h"

/**
 * FCricketAudioRouting — the Sound Routing System as data: a master gain, a per-
 * category bus gain, and a per-category voice budget with a priority preemption
 * rule. The Audio Manager runs every cue through ResolveGain() and ShouldPlay()
 * before it reaches a speaker, so mixing balance and "don't drown the wicket sound"
 * are tunable policy, not scattered magic numbers.
 */
USTRUCT(BlueprintType)
struct CRICKETAUDIO_API FCricketAudioRouting
{
	GENERATED_BODY()

	/** Overall output gain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Routing", meta = (ClampMin = "0.0")) float MasterGain = 1.0f;

	/** Per-category bus gain (a category absent from the map defaults to 1.0; 0 mutes it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Routing") TMap<ECricketAudioCategory, float> CategoryGain;

	/** Concurrent voices allowed per category before low-priority cues are dropped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Routing", meta = (ClampMin = "1")) int32 MaxVoicesPerCategory = 8;

	/** Bus gain for a category (master × category). */
	float GainFor(ECricketAudioCategory Category) const
	{
		const float* Found = CategoryGain.Find(Category);
		return MasterGain * (Found ? *Found : 1.0f);
	}

	/** Final linear gain for a cue = bus gain × the cue's computed gain. */
	float ResolveGain(const FCricketAudioCue& Cue) const
	{
		return FMath::Max(0.0f, GainFor(Cue.Category) * Cue.Gain);
	}

	/**
	 * Whether a cue should play given how many voices its category already has.
	 * Under budget: always. Over budget: only High/Critical preempt; Critical is
	 * never dropped. A muted bus (gain 0) is silent regardless.
	 */
	bool ShouldPlay(const FCricketAudioCue& Cue, int32 ActiveInCategory) const
	{
		if (Cue.Priority == ECricketAudioPriority::Critical) { return true; }
		if (GainFor(Cue.Category) <= 0.0f) { return false; }
		if (ActiveInCategory < MaxVoicesPerCategory) { return true; }
		return Cue.Priority >= ECricketAudioPriority::High; // preempt only for important cues
	}
};
