#pragma once

#include "CoreMinimal.h"
#include "CricketAudioTypes.generated.h"

/**
 * CricketAudioTypes — the vocabulary of the reactive audio layer.
 *
 * Audio is a CONSEQUENCE of the simulation, never a cause. The flow is one-way:
 *
 *   gameplay/physics event ──► FCricketAudioSelector ──► FCricketAudioCue ──► FCricketAudioRequest ──► Audio Manager ──► sound
 *
 *   - An EVENT (ECricketAudioEvent) names what happened (a sweet-spot bat impact, a
 *     pitch bounce, a catch, a six). It is derived from real simulation data.
 *   - A CUE (FCricketAudioCue) is the abstract sound decision: which event, in which
 *     category, at what priority, with what gain & pitch variation — all computed
 *     from the physics (impact speed, contact quality, edge type, …). No asset yet.
 *   - A REQUEST (FCricketAudioRequest) adds the WHERE/WHO: a world location for
 *     spatialisation and the source that raised it.
 *   - The Audio Manager resolves the cue against the data-driven sound bank and the
 *     routing/priority rules and actually plays it.
 *
 * Keeping the decision (cue) separate from the asset (bank) is what makes the whole
 * layer unit-testable headlessly and lets a sound designer reskin everything as data.
 */

/** Mixing/routing category. Drives the bus gain, priority defaults and spatialisation. */
UENUM(BlueprintType)
enum class ECricketAudioCategory : uint8
{
	Ball        UMETA(DisplayName = "Ball"),         // release / flight / bounce
	Bat         UMETA(DisplayName = "Bat"),          // contact: sweet spot / edges / block / loft
	Impact      UMETA(DisplayName = "Impact"),       // pad / stump / direct hit
	Fielding    UMETA(DisplayName = "Fielding"),     // catch / pickup / throw / dive
	Player      UMETA(DisplayName = "Player"),       // appeals / calls / celebrations / effort
	Crowd       UMETA(DisplayName = "Crowd"),        // ambient bed + reactions
	Environment UMETA(DisplayName = "Environment")   // stadium / day / night ambience
};

/** Mixing priority: when concurrency is tight, higher priority wins a voice. */
UENUM(BlueprintType)
enum class ECricketAudioPriority : uint8
{
	Low      UMETA(DisplayName = "Low"),
	Normal   UMETA(DisplayName = "Normal"),
	High     UMETA(DisplayName = "High"),
	Critical UMETA(DisplayName = "Critical")   // wickets, stumps — never dropped
};

/**
 * Every distinct sound moment the simulation can produce. Flat (not nested) so it
 * keys a data-asset map cleanly and reads clearly in the debug overlay.
 */
UENUM(BlueprintType)
enum class ECricketAudioEvent : uint8
{
	None             UMETA(DisplayName = "None"),

	// --- Ball ---
	BallRelease      UMETA(DisplayName = "Ball Release"),
	BallFlightLoop   UMETA(DisplayName = "Ball Flight (loop)"),
	PitchBounce      UMETA(DisplayName = "Pitch Bounce"),

	// --- Bat contact (noticeably different per contact type) ---
	BatSweetSpot     UMETA(DisplayName = "Bat: Sweet Spot"),
	BatThickEdge     UMETA(DisplayName = "Bat: Thick Edge"),
	BatThinEdge      UMETA(DisplayName = "Bat: Thin Edge"),
	BatDefensiveBlock UMETA(DisplayName = "Bat: Defensive Block"),
	BatLoftedStrike  UMETA(DisplayName = "Bat: Lofted Strike"),

	// --- Hard impacts ---
	PadImpact        UMETA(DisplayName = "Pad Impact"),
	StumpImpact      UMETA(DisplayName = "Stump Impact"),

	// --- Fielding ---
	Catch            UMETA(DisplayName = "Catch"),
	GroundPickup     UMETA(DisplayName = "Ground Pickup"),
	Throw            UMETA(DisplayName = "Throw"),
	Dive             UMETA(DisplayName = "Dive"),
	DirectHit        UMETA(DisplayName = "Direct Hit"),

	// --- Player ---
	Appeal           UMETA(DisplayName = "Appeal"),
	RunCall          UMETA(DisplayName = "Run Call"),
	WicketCelebration UMETA(DisplayName = "Wicket Celebration"),
	Effort           UMETA(DisplayName = "Effort"),

	// --- Crowd ---
	CrowdAmbientBed  UMETA(DisplayName = "Crowd Ambient Bed"),
	CrowdFour        UMETA(DisplayName = "Crowd: Four"),
	CrowdSix         UMETA(DisplayName = "Crowd: Six"),
	CrowdWicket      UMETA(DisplayName = "Crowd: Wicket"),
	CrowdCloseChance UMETA(DisplayName = "Crowd: Close Chance"),

	// --- Environment ambience ---
	AmbienceStadium  UMETA(DisplayName = "Ambience: Stadium"),
	AmbienceDay      UMETA(DisplayName = "Ambience: Day"),
	AmbienceNight    UMETA(DisplayName = "Ambience: Night")
};

/**
 * FCricketAudioCue — the abstract sound decision. Produced by the selector from
 * simulation data; consumed by the Audio Manager + sound bank. Asset-free.
 */
USTRUCT(BlueprintType)
struct CRICKETAUDIO_API FCricketAudioCue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Audio") ECricketAudioEvent Event = ECricketAudioEvent::None;
	UPROPERTY(BlueprintReadWrite, Category = "Audio") ECricketAudioCategory Category = ECricketAudioCategory::Ball;
	UPROPERTY(BlueprintReadWrite, Category = "Audio") ECricketAudioPriority Priority = ECricketAudioPriority::Normal;

	/** Linear gain multiplier (typically 0..1.5), folded onto the bank's base volume. */
	UPROPERTY(BlueprintReadWrite, Category = "Audio") float Gain = 1.0f;

	/** Pitch multiplier (1 = unchanged), folded onto the bank's base pitch. */
	UPROPERTY(BlueprintReadWrite, Category = "Audio") float Pitch = 1.0f;

	/** Which variation in the bank's set to play; INDEX_NONE = pick at random. */
	UPROPERTY(BlueprintReadWrite, Category = "Audio") int32 Variation = INDEX_NONE;

	/** Looping one-shots (flight whoosh, ambient beds). */
	UPROPERTY(BlueprintReadWrite, Category = "Audio") bool bLooping = false;

	bool IsValid() const { return Event != ECricketAudioEvent::None; }
};

/**
 * FCricketAudioRequest — a cue plus where it happened. What the Audio Manager
 * actually receives. Crowd/environment requests are 2D (bSpatial = false).
 */
USTRUCT(BlueprintType)
struct CRICKETAUDIO_API FCricketAudioRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Audio") FCricketAudioCue Cue;
	/** World location (cm) for spatialisation — ball, player or fielder position. */
	UPROPERTY(BlueprintReadWrite, Category = "Audio") FVector WorldLocationCm = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category = "Audio") bool bSpatial = true;
	/** Who raised it (for the debug overlay): "Ball", "Bowler", "Fielder", "Match"… */
	UPROPERTY(BlueprintReadWrite, Category = "Audio") FName Source = NAME_None;
};

/** Free helpers shared by the selector, crowd controller and manager. */
namespace CricketAudio
{
	/** The mixing category an event belongs to. */
	CRICKETAUDIO_API ECricketAudioCategory CategoryOf(ECricketAudioEvent Event);

	/** Whether a category is positioned in the world (true) or played 2D (false). */
	CRICKETAUDIO_API bool IsSpatialCategory(ECricketAudioCategory Category);

	/** Build a cue, auto-filling its category from the event. */
	CRICKETAUDIO_API FCricketAudioCue MakeCue(ECricketAudioEvent Event, float Gain = 1.0f, float Pitch = 1.0f,
		ECricketAudioPriority Priority = ECricketAudioPriority::Normal);

	/** Display name for the debug overlay. */
	CRICKETAUDIO_API FString EventName(ECricketAudioEvent Event);
	CRICKETAUDIO_API FString CategoryName(ECricketAudioCategory Category);
	CRICKETAUDIO_API FString PriorityName(ECricketAudioPriority Priority);
}
