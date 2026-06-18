#include "CricketAudioSelector.h"

#include "CricketBatTypes.h"            // FCricketBatImpactReport, ECricketContactRegion
#include "CricketPitchInteraction.h"    // FCricketBounceReport
#include "CricketBowlingTypes.h"        // FCricketReleaseParameters
#include "CricketFielderComponent.h"    // ECricketFielderState

using namespace CricketAudio;

FCricketAudioCue FCricketAudioSelector::FromBatImpact(const FCricketBatImpactReport& Impact)
{
	if (!Impact.bMadeContact) { return FCricketAudioCue(); } // no contact -> no sound

	const double Exit = Impact.ExitSpeedMS;
	// Loudness rides the impact speed: a fizzing middled six is loud, a dead bat soft.
	const float SpeedGain = (float)FMath::Clamp(Exit / 32.0, 0.25, 1.3);

	// 1) EDGE — judged before everything else, since an edge can be soft yet must still
	//    read as an edge (a feathered tick), not a block.
	const ECricketContactRegion R = Impact.Region;
	const bool bEdgeRegion =
		(R == ECricketContactRegion::ThinEdge || R == ECricketContactRegion::ThickEdge ||
		 R == ECricketContactRegion::TopEdge  || R == ECricketContactRegion::BottomEdge ||
		 R == ECricketContactRegion::Toe);

	if (Impact.bIsEdge || bEdgeRegion)
	{
		const bool bThin =
			(R == ECricketContactRegion::ThinEdge) ||
			(R != ECricketContactRegion::ThickEdge && Impact.EdgeFactor > 0.7);

		if (bThin)
		{
			// Thin edge: quiet, bright, ticky.
			return MakeCue(ECricketAudioEvent::BatThinEdge,
				FMath::Clamp(SpeedGain * 0.7f, 0.25f, 0.8f), 1.18f, ECricketAudioPriority::Normal);
		}
		// Thick edge / toe / top edge: meatier, duller than the middle.
		return MakeCue(ECricketAudioEvent::BatThickEdge,
			FMath::Clamp(SpeedGain * 0.85f, 0.3f, 0.95f), 1.05f, ECricketAudioPriority::Normal);
	}

	// 2) DEFENSIVE BLOCK — clean contact but soft (little bat speed -> low exit).
	if (Exit < 9.0)
	{
		return MakeCue(ECricketAudioEvent::BatDefensiveBlock,
			FMath::Clamp(SpeedGain * 0.6f, 0.2f, 0.5f), 0.92f, ECricketAudioPriority::Low);
	}

	// 3) LOFTED STRIKE — clean middle launched into the air.
	if (Impact.LaunchAngleDeg > 22.0 && Exit > 16.0)
	{
		return MakeCue(ECricketAudioEvent::BatLoftedStrike,
			FMath::Clamp(SpeedGain * 1.05f, 0.6f, 1.3f), 1.04f, ECricketAudioPriority::High);
	}

	// 4) SWEET SPOT — the crisp middled "thock". Pitch sharpens with quality.
	return MakeCue(ECricketAudioEvent::BatSweetSpot,
		FMath::Clamp(SpeedGain, 0.6f, 1.2f), 1.0f + 0.05f * (float)Impact.Quality, ECricketAudioPriority::High);
}

FCricketAudioCue FCricketAudioSelector::FromBounce(const FCricketBounceReport& Bounce, double ApproachSpeedKmh)
{
	const float Gain  = (float)FMath::Clamp(ApproachSpeedKmh / 140.0, 0.2, 1.0);
	// More pace retained = harder/skiddier surface = a sharper "thud".
	const float Pitch = FMath::Lerp(0.9f, 1.1f, (float)FMath::Clamp(Bounce.SpeedRetainedFrac, 0.0, 1.0));
	return MakeCue(ECricketAudioEvent::PitchBounce, Gain, Pitch, ECricketAudioPriority::Normal);
}

FCricketAudioCue FCricketAudioSelector::FromRelease(const FCricketReleaseParameters& Release)
{
	const float Gain = (float)FMath::Clamp(Release.ReleaseSpeedMS / 40.0, 0.3, 0.7);
	return MakeCue(ECricketAudioEvent::BallRelease, Gain, 1.0f, ECricketAudioPriority::Low);
}

FCricketAudioCue FCricketAudioSelector::FlightLoop(double SpeedKmh)
{
	const float Gain  = (float)FMath::Clamp((SpeedKmh - 40.0) / 120.0, 0.1, 0.8);
	const float Pitch = (float)FMath::Clamp(SpeedKmh / 110.0, 0.6, 1.5);
	FCricketAudioCue Cue = MakeCue(ECricketAudioEvent::BallFlightLoop, Gain, Pitch, ECricketAudioPriority::Low);
	Cue.bLooping = true;
	return Cue;
}

FCricketAudioCue FCricketAudioSelector::FromDismissal(ECricketDismissal Dismissal)
{
	switch (Dismissal)
	{
	case ECricketDismissal::Bowled:
	case ECricketDismissal::Stumped:
		return MakeCue(ECricketAudioEvent::StumpImpact, 1.0f, 1.0f, ECricketAudioPriority::Critical);
	case ECricketDismissal::LBW:
		return MakeCue(ECricketAudioEvent::PadImpact, 0.9f, 1.0f, ECricketAudioPriority::High);
	case ECricketDismissal::RunOut:
		return MakeCue(ECricketAudioEvent::DirectHit, 0.95f, 1.0f, ECricketAudioPriority::High);
	case ECricketDismissal::Caught:
		return MakeCue(ECricketAudioEvent::Catch, 0.9f, 1.0f, ECricketAudioPriority::High);
	default:
		return FCricketAudioCue();
	}
}

FCricketAudioCue FCricketAudioSelector::FromFielderState(ECricketFielderState State, ECricketCatchDifficulty Difficulty)
{
	switch (State)
	{
	case ECricketFielderState::Catching:
		return (Difficulty == ECricketCatchDifficulty::Diving)
			? MakeCue(ECricketAudioEvent::Dive, 0.9f, 1.0f, ECricketAudioPriority::High)
			: MakeCue(ECricketAudioEvent::Catch, 0.8f, 1.0f, ECricketAudioPriority::High);
	case ECricketFielderState::PickingUp:
		return MakeCue(ECricketAudioEvent::GroundPickup, 0.6f, 1.0f, ECricketAudioPriority::Normal);
	case ECricketFielderState::Throwing:
		return MakeCue(ECricketAudioEvent::Throw, 0.7f, 1.0f, ECricketAudioPriority::Normal);
	default:
		return FCricketAudioCue();
	}
}

FCricketAudioCue FCricketAudioSelector::Appeal(ECricketDismissal /*AppealedFor*/)
{
	return MakeCue(ECricketAudioEvent::Appeal, 0.9f, 1.0f, ECricketAudioPriority::High);
}

FCricketAudioCue FCricketAudioSelector::WicketCelebration()
{
	return MakeCue(ECricketAudioEvent::WicketCelebration, 1.0f, 1.0f, ECricketAudioPriority::High);
}

FCricketAudioCue FCricketAudioSelector::RunCall(int32 Runs)
{
	if (Runs <= 0) { return FCricketAudioCue(); }
	return MakeCue(ECricketAudioEvent::RunCall, 0.7f, 1.0f, ECricketAudioPriority::Normal);
}

FCricketAudioCue FCricketAudioSelector::Effort(float Intensity01)
{
	const float Gain = FMath::Clamp(0.3f + 0.6f * FMath::Clamp(Intensity01, 0.0f, 1.0f), 0.3f, 1.0f);
	return MakeCue(ECricketAudioEvent::Effort, Gain, 1.0f, ECricketAudioPriority::Low);
}

float FCricketAudioSelector::ReplayPitchScale(bool bReplaying, bool bPaused, double PlaybackRate)
{
	if (!bReplaying) { return 1.0f; }
	if (bPaused)     { return 0.05f; }                 // near-frozen scrub
	return (float)FMath::Clamp(PlaybackRate, 0.05, 2.0); // slow-mo lowers pitch with the rate
}
