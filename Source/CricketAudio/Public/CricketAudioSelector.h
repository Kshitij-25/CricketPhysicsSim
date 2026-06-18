#pragma once

#include "CoreMinimal.h"
#include "CricketAudioTypes.h"
#include "CricketMatchTypes.h"        // ECricketDismissal
#include "CricketBatTypes.h"          // FCricketBatImpactReport
#include "CricketFieldingTypes.h"     // ECricketCatchDifficulty

struct FCricketBounceReport;
struct FCricketReleaseParameters;
enum class ECricketFielderState : uint8;

/**
 * FCricketAudioSelector — the decision layer that turns SIMULATION DATA into sound
 * cues. Pure, static, deterministic and free of any engine/audio object, so the
 * whole "which sound, how loud, what pitch" logic is unit-tested headlessly.
 *
 * This is where the brief's rule lands: contact type, impact speed and contact
 * quality (never a random roll) decide the cue. A middled drive, a thin edge and a
 * defensive prod come out of the SAME bat-impact report as audibly different cues —
 * different event, different gain, different pitch — because the geometry already
 * differs. Audio reads the outcome; it never changes it.
 */
class CRICKETAUDIO_API FCricketAudioSelector
{
public:
	// --- Bat contact (sweet spot / thick edge / thin edge / block / loft) -------
	/** Pick the bat cue from the deterministic impact report (invalid cue if no contact). */
	static FCricketAudioCue FromBatImpact(const FCricketBatImpactReport& Impact);

	// --- Ball ------------------------------------------------------------------
	/** Pitch-bounce cue; loudness scales with the approach speed, pitch with hardness. */
	static FCricketAudioCue FromBounce(const FCricketBounceReport& Bounce, double ApproachSpeedKmh);
	/** Ball-release cue; loudness scales gently with pace. */
	static FCricketAudioCue FromRelease(const FCricketReleaseParameters& Release);
	/** Looping flight whoosh; gain & pitch rise with ball speed. */
	static FCricketAudioCue FlightLoop(double SpeedKmh);

	// --- Hard impacts from a dismissal -----------------------------------------
	/** Stump/pad/direct-hit/catch cue implied by how a batter was dismissed. */
	static FCricketAudioCue FromDismissal(ECricketDismissal Dismissal);

	// --- Fielding --------------------------------------------------------------
	/** Catch/dive/pickup/throw cue from a fielder state, graded by catch difficulty. */
	static FCricketAudioCue FromFielderState(ECricketFielderState State, ECricketCatchDifficulty Difficulty);

	// --- Player voice ----------------------------------------------------------
	static FCricketAudioCue Appeal(ECricketDismissal AppealedFor);
	static FCricketAudioCue WicketCelebration();
	static FCricketAudioCue RunCall(int32 Runs);
	static FCricketAudioCue Effort(float Intensity01);

	// --- Replay integration ----------------------------------------------------
	/**
	 * Global pitch scale applied to live SFX during replay: 1.0 in normal play,
	 * the playback rate during slow-motion (so a 0.25x replay drops pitch to 0.25),
	 * and near-zero when the replay is paused.
	 */
	static float ReplayPitchScale(bool bReplaying, bool bPaused, double PlaybackRate);
};
