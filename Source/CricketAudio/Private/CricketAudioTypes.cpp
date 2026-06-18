#include "CricketAudioTypes.h"

namespace CricketAudio
{
	ECricketAudioCategory CategoryOf(ECricketAudioEvent Event)
	{
		switch (Event)
		{
		case ECricketAudioEvent::BallRelease:
		case ECricketAudioEvent::BallFlightLoop:
		case ECricketAudioEvent::PitchBounce:
			return ECricketAudioCategory::Ball;

		case ECricketAudioEvent::BatSweetSpot:
		case ECricketAudioEvent::BatThickEdge:
		case ECricketAudioEvent::BatThinEdge:
		case ECricketAudioEvent::BatDefensiveBlock:
		case ECricketAudioEvent::BatLoftedStrike:
			return ECricketAudioCategory::Bat;

		case ECricketAudioEvent::PadImpact:
		case ECricketAudioEvent::StumpImpact:
			return ECricketAudioCategory::Impact;

		case ECricketAudioEvent::Catch:
		case ECricketAudioEvent::GroundPickup:
		case ECricketAudioEvent::Throw:
		case ECricketAudioEvent::Dive:
		case ECricketAudioEvent::DirectHit:
			return ECricketAudioCategory::Fielding;

		case ECricketAudioEvent::Appeal:
		case ECricketAudioEvent::RunCall:
		case ECricketAudioEvent::WicketCelebration:
		case ECricketAudioEvent::Effort:
			return ECricketAudioCategory::Player;

		case ECricketAudioEvent::CrowdAmbientBed:
		case ECricketAudioEvent::CrowdFour:
		case ECricketAudioEvent::CrowdSix:
		case ECricketAudioEvent::CrowdWicket:
		case ECricketAudioEvent::CrowdCloseChance:
			return ECricketAudioCategory::Crowd;

		case ECricketAudioEvent::AmbienceStadium:
		case ECricketAudioEvent::AmbienceDay:
		case ECricketAudioEvent::AmbienceNight:
			return ECricketAudioCategory::Environment;

		default:
			return ECricketAudioCategory::Ball;
		}
	}

	bool IsSpatialCategory(ECricketAudioCategory Category)
	{
		// Crowd and environment are global beds/reactions, played 2D; everything else
		// is positioned at its world source.
		return Category != ECricketAudioCategory::Crowd && Category != ECricketAudioCategory::Environment;
	}

	FCricketAudioCue MakeCue(ECricketAudioEvent Event, float Gain, float Pitch, ECricketAudioPriority Priority)
	{
		FCricketAudioCue Cue;
		Cue.Event    = Event;
		Cue.Category = CategoryOf(Event);
		Cue.Priority = Priority;
		Cue.Gain     = Gain;
		Cue.Pitch    = Pitch;
		return Cue;
	}

	static FString EnumDisplay(const UEnum* E, int64 V)
	{
		return E ? E->GetDisplayNameTextByValue(V).ToString() : FString(TEXT("?"));
	}

	FString EventName(ECricketAudioEvent Event)       { return EnumDisplay(StaticEnum<ECricketAudioEvent>(), (int64)Event); }
	FString CategoryName(ECricketAudioCategory Category){ return EnumDisplay(StaticEnum<ECricketAudioCategory>(), (int64)Category); }
	FString PriorityName(ECricketAudioPriority Priority){ return EnumDisplay(StaticEnum<ECricketAudioPriority>(), (int64)Priority); }
}
