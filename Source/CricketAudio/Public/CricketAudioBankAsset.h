#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CricketAudioTypes.h"
#include "CricketAudioBankAsset.generated.h"

class USoundBase;
class USoundAttenuation;
class USoundConcurrency;

/**
 * FCricketSoundSet — the concrete assets behind one event. Multiple Variations let
 * the manager pick a different take each time (so repeated bat impacts don't machine-
 * gun the same sample). Base volume/pitch are folded with the cue's computed
 * gain/pitch; attenuation + concurrency complete the routing for spatial sounds.
 */
USTRUCT(BlueprintType)
struct CRICKETAUDIO_API FCricketSoundSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") TArray<TObjectPtr<USoundBase>> Variations;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0")) float BaseVolume = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0")) float BasePitch = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") TObjectPtr<USoundAttenuation> Attenuation = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") TObjectPtr<USoundConcurrency> Concurrency = nullptr;

	bool HasSound() const { return Variations.Num() > 0; }
};

/** One bank row: an event keyed to its sound set. */
USTRUCT(BlueprintType)
struct CRICKETAUDIO_API FCricketAudioBankEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") ECricketAudioEvent Event = ECricketAudioEvent::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") FCricketSoundSet Sound;
};

/**
 * UCricketAudioBankAsset — the entire palette of sounds as DATA. A sound designer
 * authors one of these and assigns it (Project Settings → Cricket Audio, or the
 * subsystem's SetBank). The audio code references events, never specific assets, so
 * the bank can be swapped (stadium pack, training pack) with no code change.
 *
 * The system is fully functional with an empty/partial bank: a missing event simply
 * produces no sound (and is reported by the debug overlay), so gameplay never depends
 * on audio being present.
 */
UCLASS(BlueprintType)
class CRICKETAUDIO_API UCricketAudioBankAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TArray<FCricketAudioBankEntry> Entries;

	/** Look up the sound set for an event (nullptr if the bank doesn't define it). */
	const FCricketSoundSet* Find(ECricketAudioEvent Event) const;
};
