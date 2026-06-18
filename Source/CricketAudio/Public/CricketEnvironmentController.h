#pragma once

#include "CoreMinimal.h"
#include "CricketAudioTypes.h"
#include "CricketStadiumTypes.h"   // ECricketTimeOfDay (Day / Twilight / Night) — shared with the stadium system
#include "CricketEnvironmentController.generated.h"

/**
 * FCricketEnvironmentController — the Environmental Audio Controller. Chooses the
 * ambience beds for the venue: a constant stadium bed plus a day or night layer
 * (night carries the floodlit-match hum). Reuses the stadium system's
 * ECricketTimeOfDay so audio and visuals stay in lockstep. Returns looping
 * Environment cues the manager starts and cross-fades; switching time of day just
 * restarts the layer.
 */
USTRUCT(BlueprintType)
struct CRICKETAUDIO_API FCricketEnvironmentController
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Environment") ECricketTimeOfDay TimeOfDay = ECricketTimeOfDay::Day;
	/** Whether to layer the enclosed-stadium bed under the time-of-day ambience. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Environment") bool bStadium = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Environment", meta = (ClampMin = "0.0", ClampMax = "1.0")) float AmbienceGain = 0.4f;

	/** The looping ambience cues that should currently be playing. */
	TArray<FCricketAudioCue> AmbienceCues() const
	{
		TArray<FCricketAudioCue> Cues;
		auto Add = [&](ECricketAudioEvent E)
		{
			FCricketAudioCue Cue = CricketAudio::MakeCue(E, AmbienceGain, 1.0f, ECricketAudioPriority::Low);
			Cue.bLooping = true;
			Cues.Add(Cue);
		};

		if (bStadium) { Add(ECricketAudioEvent::AmbienceStadium); }
		// Night carries the floodlit hum; day & twilight share the daytime bed.
		Add(TimeOfDay == ECricketTimeOfDay::Night ? ECricketAudioEvent::AmbienceNight : ECricketAudioEvent::AmbienceDay);
		return Cues;
	}
};
