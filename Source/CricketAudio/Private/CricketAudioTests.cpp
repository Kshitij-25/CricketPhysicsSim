// Headless automation tests for the reactive audio layer. The decision core
// (selector + crowd + routing) is pure, so the whole "which sound, how loud, what
// pitch" logic is asserted with no audio device, no assets and no RHI. Playback
// itself (the subsystem) is a thin wrapper over engine audio and is not unit-tested.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.Audio; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"

#include "CricketAudioTypes.h"
#include "CricketAudioSelector.h"
#include "CricketAudioRouting.h"
#include "CricketCrowdController.h"
#include "CricketBatTypes.h"
#include "CricketMatchTypes.h"
#include "CricketFielderComponent.h"   // ECricketFielderState
#include "CricketFieldingTypes.h"      // ECricketCatchDifficulty

#if WITH_DEV_AUTOMATION_TESTS

using namespace CricketAudio;

namespace
{
	FCricketBatImpactReport Contact(ECricketContactRegion Region, double ExitMS, double LaunchDeg,
		double Quality, bool bEdge, double EdgeFactor)
	{
		FCricketBatImpactReport R;
		R.bMadeContact   = true;
		R.Region         = Region;
		R.ExitSpeedMS    = ExitMS;
		R.LaunchAngleDeg = LaunchDeg;
		R.Quality        = Quality;
		R.bIsEdge        = bEdge;
		R.EdgeFactor     = EdgeFactor;
		return R;
	}
}

// 1. BAT IMPACTS — sweet spot / lofted / block / no-contact are distinct cues, and a
//    harder strike is louder.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAudioBatImpactTest,
	"CricketSim.Audio.BatImpacts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAudioBatImpactTest::RunTest(const FString&)
{
	const FCricketAudioCue Sweet = FCricketAudioSelector::FromBatImpact(Contact(ECricketContactRegion::Middle, 30, 12, 1.0, false, 0.0));
	const FCricketAudioCue Loft  = FCricketAudioSelector::FromBatImpact(Contact(ECricketContactRegion::Middle, 28, 35, 0.95, false, 0.0));
	const FCricketAudioCue Block = FCricketAudioSelector::FromBatImpact(Contact(ECricketContactRegion::Middle, 5,  6,  0.8, false, 0.0));

	TestEqual(TEXT("Middled drive -> sweet spot"), Sweet.Event, ECricketAudioEvent::BatSweetSpot);
	TestEqual(TEXT("Sweet spot routes to Bat"), Sweet.Category, ECricketAudioCategory::Bat);
	TestTrue (TEXT("Sweet spot is loud"), Sweet.Gain > 0.8f);
	TestTrue (TEXT("Bat sounds are spatial"), IsSpatialCategory(Sweet.Category));

	TestEqual(TEXT("High launch clean -> lofted"), Loft.Event, ECricketAudioEvent::BatLoftedStrike);
	TestEqual(TEXT("Soft contact -> defensive block"), Block.Event, ECricketAudioEvent::BatDefensiveBlock);
	TestTrue (TEXT("Block is quiet"), Block.Gain < Sweet.Gain);

	const FCricketAudioCue Miss = FCricketAudioSelector::FromBatImpact(FCricketBatImpactReport{});
	TestFalse(TEXT("No contact -> no cue"), Miss.IsValid());
	return true;
}

// 2. EDGES — thin vs thick are different events; a thin edge is quieter and brighter
//    than a middled shot.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAudioEdgeTest,
	"CricketSim.Audio.Edges", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAudioEdgeTest::RunTest(const FString&)
{
	const FCricketAudioCue Thin  = FCricketAudioSelector::FromBatImpact(Contact(ECricketContactRegion::ThinEdge, 18, 8, 0.2, true, 0.85));
	const FCricketAudioCue Thick = FCricketAudioSelector::FromBatImpact(Contact(ECricketContactRegion::ThickEdge, 20, 6, 0.4, true, 0.5));
	const FCricketAudioCue Sweet = FCricketAudioSelector::FromBatImpact(Contact(ECricketContactRegion::Middle, 30, 12, 1.0, false, 0.0));

	TestEqual(TEXT("Thin edge cue"), Thin.Event, ECricketAudioEvent::BatThinEdge);
	TestEqual(TEXT("Thick edge cue"), Thick.Event, ECricketAudioEvent::BatThickEdge);
	TestTrue (TEXT("Thin and thick are different"), Thin.Event != Thick.Event);
	TestTrue (TEXT("Thin edge is quieter than the middle"), Thin.Gain < Sweet.Gain);
	TestTrue (TEXT("Thin edge is brighter (higher pitch) than the middle"), Thin.Pitch > Sweet.Pitch);
	return true;
}

// 3. CATCHES — fielder states map to catch/dive/pickup/throw; difficulty grades a
//    catch into a dive.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAudioCatchTest,
	"CricketSim.Audio.Catches", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAudioCatchTest::RunTest(const FString&)
{
	const FCricketAudioCue Reg  = FCricketAudioSelector::FromFielderState(ECricketFielderState::Catching, ECricketCatchDifficulty::Regulation);
	const FCricketAudioCue Dive = FCricketAudioSelector::FromFielderState(ECricketFielderState::Catching, ECricketCatchDifficulty::Diving);
	const FCricketAudioCue Pick = FCricketAudioSelector::FromFielderState(ECricketFielderState::PickingUp, ECricketCatchDifficulty::Impossible);
	const FCricketAudioCue Thr  = FCricketAudioSelector::FromFielderState(ECricketFielderState::Throwing, ECricketCatchDifficulty::Regulation);
	const FCricketAudioCue Idle = FCricketAudioSelector::FromFielderState(ECricketFielderState::Idle, ECricketCatchDifficulty::Regulation);

	TestEqual(TEXT("Regulation catch"), Reg.Event, ECricketAudioEvent::Catch);
	TestEqual(TEXT("Diving catch -> dive"), Dive.Event, ECricketAudioEvent::Dive);
	TestEqual(TEXT("Pickup"), Pick.Event, ECricketAudioEvent::GroundPickup);
	TestEqual(TEXT("Throw"), Thr.Event, ECricketAudioEvent::Throw);
	TestEqual(TEXT("Fielding routes correctly"), Reg.Category, ECricketAudioCategory::Fielding);
	TestFalse(TEXT("Idle fielder makes no sound"), Idle.IsValid());
	return true;
}

// 4. WICKETS — the dismissal type implies the hard-impact sound.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAudioWicketTest,
	"CricketSim.Audio.Wickets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAudioWicketTest::RunTest(const FString&)
{
	const FCricketAudioCue Bowled = FCricketAudioSelector::FromDismissal(ECricketDismissal::Bowled);
	const FCricketAudioCue Lbw    = FCricketAudioSelector::FromDismissal(ECricketDismissal::LBW);
	const FCricketAudioCue RunOut = FCricketAudioSelector::FromDismissal(ECricketDismissal::RunOut);
	const FCricketAudioCue Caught = FCricketAudioSelector::FromDismissal(ECricketDismissal::Caught);
	const FCricketAudioCue NotOut = FCricketAudioSelector::FromDismissal(ECricketDismissal::NotOut);

	TestEqual(TEXT("Bowled -> stump"), Bowled.Event, ECricketAudioEvent::StumpImpact);
	TestEqual(TEXT("Stump is critical priority"), Bowled.Priority, ECricketAudioPriority::Critical);
	TestEqual(TEXT("Stump routes to Impact"), Bowled.Category, ECricketAudioCategory::Impact);
	TestEqual(TEXT("LBW -> pad"), Lbw.Event, ECricketAudioEvent::PadImpact);
	TestEqual(TEXT("Run out -> direct hit"), RunOut.Event, ECricketAudioEvent::DirectHit);
	TestEqual(TEXT("Caught -> catch"), Caught.Event, ECricketAudioEvent::Catch);
	TestFalse(TEXT("Not out -> no impact sound"), NotOut.IsValid());
	return true;
}

// 5. CROWD REACTIONS — excitement bumps by moment weight, swells the bed, then decays.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAudioCrowdTest,
	"CricketSim.Audio.CrowdReactions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAudioCrowdTest::RunTest(const FString&)
{
	FCricketAudioCue Cue;

	FCricketCrowdController Four;
	TestTrue (TEXT("Four is a reaction"), Four.ReactTo(ECricketAudioEvent::CrowdFour, Cue));
	TestEqual(TEXT("Four cue"), Cue.Event, ECricketAudioEvent::CrowdFour);
	const float FourExcite = Four.Excitement;

	FCricketCrowdController Six;
	Six.ReactTo(ECricketAudioEvent::CrowdSix, Cue);
	TestTrue (TEXT("A six excites the crowd more than a four"), Six.Excitement > FourExcite);
	TestTrue (TEXT("Excited bed is louder than the resting bed"), Six.BedGain() > Six.BaseBedGain);
	TestFalse(TEXT("Crowd reactions are 2D"), IsSpatialCategory(ECricketAudioCategory::Crowd));

	FCricketCrowdController NotAReaction;
	TestFalse(TEXT("The ambient bed is not a reaction"), NotAReaction.ReactTo(ECricketAudioEvent::CrowdAmbientBed, Cue));

	Six.Tick(10.0f); // long enough to fully decay
	TestTrue(TEXT("Excitement decays back to calm"), Six.Excitement <= KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Bed returns toward its resting level"), FMath::IsNearlyEqual(Six.BedGain(), Six.BaseBedGain, 1e-3f));
	return true;
}

// 6. REPLAY PLAYBACK — slow-motion scales pitch by the playback rate; routing gates
//    voices by priority and mutes a zeroed bus.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketAudioReplayTest,
	"CricketSim.Audio.ReplayPlayback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketAudioReplayTest::RunTest(const FString&)
{
	TestTrue(TEXT("Normal play = unity pitch"), FMath::IsNearlyEqual(FCricketAudioSelector::ReplayPitchScale(false, false, 1.0), 1.0f, 1e-4f));
	TestTrue(TEXT("0.25x slow-mo scales pitch to 0.25"), FMath::IsNearlyEqual(FCricketAudioSelector::ReplayPitchScale(true, false, 0.25), 0.25f, 1e-4f));
	TestTrue(TEXT("Paused replay is near-frozen"), FCricketAudioSelector::ReplayPitchScale(true, true, 0.25) < 0.1f);

	// Routing: resolve + priority gate + mute.
	FCricketAudioRouting R;
	R.MaxVoicesPerCategory = 1;
	const FCricketAudioCue Loud = MakeCue(ECricketAudioEvent::BatSweetSpot, 0.8f, 1.0f, ECricketAudioPriority::Normal);
	TestTrue(TEXT("Resolved gain = master*category*cue"), FMath::IsNearlyEqual(R.ResolveGain(Loud), 0.8f, 1e-4f));
	TestTrue (TEXT("Plays when the bus is free"), R.ShouldPlay(Loud, 0));
	TestFalse(TEXT("A normal cue is dropped when the bus is full"), R.ShouldPlay(Loud, 1));

	const FCricketAudioCue Wicket = MakeCue(ECricketAudioEvent::StumpImpact, 1.0f, 1.0f, ECricketAudioPriority::Critical);
	TestTrue(TEXT("A critical cue is never dropped"), R.ShouldPlay(Wicket, 5));

	R.CategoryGain.Add(ECricketAudioCategory::Bat, 0.0f);
	TestFalse(TEXT("A muted bus plays nothing"), R.ShouldPlay(Loud, 0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
