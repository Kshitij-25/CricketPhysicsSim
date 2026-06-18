#pragma once

#include "CoreMinimal.h"
#include "CricketMatchTypes.h"      // ECricketMatchState lives in scoring types, dismissal/phase here
#include "CricketScoringTypes.h"    // ECricketMatchState
#include "CricketBatTypes.h"        // ECricketContactRegion / Side / ShotType
#include "CricketBattingTypes.h"    // ECricketFootwork / TimingQuality / SwingPhase
#include "CricketBowlingTypes.h"    // ECricketMovement / Line / Length / SwingRegime
#include "CricketHUDTypes.generated.h"

/**
 * CricketHUDTypes — the VIEW-MODEL contract between gameplay and the UI.
 *
 * This is the entire data-binding strategy in one place. Every widget and every
 * debug overlay reads ONLY from these immutable, plain-data structs; nothing in
 * the UI touches a live simulation object directly. The structs are produced once
 * per frame by UCricketHUDDataSource from the authoritative systems (Match Engine,
 * ball physics, bowling/batting controllers, replay) and handed to the widgets.
 *
 *   gameplay systems  ──(read-only)──►  UCricketHUDDataSource  ──►  FCricketHUDModel  ──►  widgets / overlays
 *
 * Consequences of routing everything through a snapshot:
 *   - The UI can NEVER own or mutate gameplay state (the brief's hard rule): a
 *     view-model is a copy, and the builders take const sources.
 *   - The whole binding layer is unit-testable headlessly — a test builds a VM from
 *     a configured engine and asserts the fields, with no widgets or RHI involved.
 *   - Pre-formatted display strings (overs "12.3", figures "3.2-0-24-1") live here,
 *     so a designer's widget is pure layout and a future broadcast skin reuses the
 *     same numbers.
 *
 * Units at this boundary are presentation units (km/h, rpm, degrees, runs); the SI
 * core is converted here, once.
 */

// ---------------------------------------------------------------------------
// Scoreboard — the always-on gameplay HUD + match information
// ---------------------------------------------------------------------------

/** Coarse innings phase, derived from overs vs the powerplay/death thresholds. */
UENUM(BlueprintType)
enum class ECricketHUDPhase : uint8
{
	PreMatch  UMETA(DisplayName = "Pre-Match"),
	Powerplay UMETA(DisplayName = "Powerplay"),
	Middle    UMETA(DisplayName = "Middle"),
	Death     UMETA(DisplayName = "Death"),
	Break     UMETA(DisplayName = "Innings Break"),
	Complete  UMETA(DisplayName = "Complete")
};

/**
 * FCricketScoreboardVM — score, wickets, overs, run rates, the two batters at the
 * crease, the bowler, the chase maths and the result. Everything the gameplay HUD
 * and the Match Information panel display. A faithful snapshot of the Match Engine.
 */
USTRUCT(BlueprintType)
struct CRICKETUI_API FCricketScoreboardVM
{
	GENERATED_BODY()

	// --- Totals -------------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") FString BattingTeam;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") FString BowlingTeam;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 Runs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 Wickets = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 Extras = 0;
	/** Whole overs completed and balls into the current over. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 CompletedOvers = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 BallsThisOver = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 TotalOvers = 20;
	/** Pre-formatted "12.3" overs string and "143/4" score string. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") FString OversText;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") FString ScoreText;
	/** Which ball of the over is next (1..6 within the over), for the ball-count readout. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 BallNumberInOver = 1;

	// --- Run rates ----------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") double CurrentRunRate = 0.0;
	/** Required run rate; 0 unless chasing. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") double RequiredRunRate = 0.0;

	// --- Batters at the crease ---------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") FString StrikerName;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 StrikerRuns = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 StrikerBalls = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") FString NonStrikerName;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 NonStrikerRuns = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 NonStrikerBalls = 0;

	// --- Current bowler -----------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") FString BowlerName;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 BowlerWickets = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") int32 BowlerRunsConceded = 0;
	/** Pre-formatted bowling figures "3.2-0-24-1" (overs-maidens-runs-wkts). */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Score") FString BowlerFiguresText;

	// --- Match information / chase ------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Match") ECricketMatchState MatchState = ECricketMatchState::PreMatch;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Match") FString MatchStateText;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Match") ECricketHUDPhase Phase = ECricketHUDPhase::PreMatch;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Match") FString PhaseText;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Match") bool bChasing = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Match") int32 Target = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Match") int32 RunsRequired = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Match") int32 BallsRemaining = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Match") bool bMatchComplete = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Match") FString ResultSummary;
};

// ---------------------------------------------------------------------------
// Batting — shot timing / footwork / direction / contact quality
// ---------------------------------------------------------------------------

/**
 * FCricketBattingVM — the batter's last stroke and live swing state. A read-out of
 * what the batting motion model + collision solver already produced; the UI adds
 * nothing to the outcome.
 */
USTRUCT(BlueprintType)
struct CRICKETUI_API FCricketBattingVM
{
	GENERATED_BODY()

	// --- Live intent (selected before/while playing) ------------------------
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") ECricketShotType ShotType = ECricketShotType::StraightDrive;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") FString ShotText;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") ECricketFootwork Footwork = ECricketFootwork::Neutral;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") FString FootworkText;
	/** Shot direction: fine aim (deg, + opens to off for RH) + a coarse leg/straight/off label. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") double AimYawDeg = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") FString DirectionText;

	// --- Live swing ---------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") bool bSwinging = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") ECricketSwingPhase SwingPhase = ECricketSwingPhase::Idle;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") FString SwingPhaseText;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") double BatSpeedMS = 0.0;

	// --- Last contact (timing + quality feedback) ---------------------------
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") bool bHasPlayed = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") bool bMadeContact = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") ECricketTimingQuality Timing = ECricketTimingQuality::Perfect;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") FString TimingText;
	/** Signed timing error in milliseconds (>0 late). And a 0..1 closeness for a meter. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") double TimingErrorMs = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") double TimingNormalized = 0.0;
	/** Where on the blade it struck + a 0..1 contact quality (1 = middled). */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") ECricketContactRegion ContactRegion = ECricketContactRegion::Middle;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") FString ContactText;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") bool bIsEdge = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") double ContactQuality = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") double ExitSpeedKmh = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Batting") double LaunchAngleDeg = 0.0;
};

// ---------------------------------------------------------------------------
// Bowling — delivery type / line / length / swing / spin
// ---------------------------------------------------------------------------

/**
 * FCricketBowlingVM — the bowler's current intent and the physics-predicted shape
 * of the delivery (from the generator's diagnostics, the same model the live ball
 * flies through).
 */
USTRUCT(BlueprintType)
struct CRICKETUI_API FCricketBowlingVM
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") ECricketMovement Movement = ECricketMovement::SeamUp;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") FString DeliveryTypeText;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") ECricketLine Line = ECricketLine::OffStump;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") FString LineText;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") ECricketLength Length = ECricketLength::GoodLength;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") FString LengthText;

	/** The two intent dials (0..1) and the resulting pace/revs. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") double SwingAmount01 = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") double SpinAmount01 = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") double PaceKmh = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") double SpinRPM = 0.0;

	/** Physics-predicted outcome of the last delivery (for the indicators). */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") bool bHasDelivery = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") double PredictedLengthM = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") double PredictedLineM = 0.0;
	/** Honest free-flight lateral swing (m, + to off) and its conventional/reverse regime. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") double FreeFlightSwingM = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") ECricketSwingRegime Regime = ECricketSwingRegime::None;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Bowling") FString RegimeText;
};

// ---------------------------------------------------------------------------
// Replay — controls / timeline / playback speed
// ---------------------------------------------------------------------------

/** FCricketReplayVM — the replay transport state for the replay UI. */
USTRUCT(BlueprintType)
struct CRICKETUI_API FCricketReplayVM
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Replay") bool bReplaying = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Replay") bool bPaused = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Replay") double Rate = 1.0;
	/** Pre-formatted "0.25x" speed string. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Replay") FString RateText;
	/** Timeline cursor in [0,1] and a "3.4s / 6.0s" timeline string. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Replay") double NormalizedTime = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Replay") FString TimelineText;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Replay") int32 FrameCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Replay") double DurationSec = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Replay") int32 EventCount = 0;
};

// ---------------------------------------------------------------------------
// Physics — the developer overlay (ball speed / rpm / spin axis / seam / swing / bounce)
// ---------------------------------------------------------------------------

/**
 * FCricketPhysicsVM — the live ball physics, mirrored for the developer overlay.
 * Reads the same SI FCricketBallState the integrator advances, plus the last
 * resolved bounce, so the overlay shows exactly what the model computed.
 */
USTRUCT(BlueprintType)
struct CRICKETUI_API FCricketPhysicsVM
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") bool bInFlight = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double SpeedKmh = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double SpinRPM = 0.0;
	/** Unit spin axis and seam-plane normal (world axes), straight from the state. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") FVector SpinAxis = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") FVector SeamNormal = FVector(0, 1, 0);
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double SeamStability = 1.0;

	// --- Instantaneous aero breakdown (N) + regime --------------------------
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double DragForceN = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double MagnusForceN = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double SwingForceN = 0.0;
	/** 0 = conventional .. 1 = reverse, and the Reynolds number at this airspeed. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double ReverseRegime = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double ReynoldsNumber = 0.0;

	// --- Last bounce metrics ------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") int32 BounceCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") bool bHasBounce = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double LastBounceAngleDeg = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double LastBounceHeightM = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double LastTurnMS = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double LastSeamDeviationMS = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Physics") double LastSpeedRetainedFrac = 0.0;
};

// ---------------------------------------------------------------------------
// The aggregate model
// ---------------------------------------------------------------------------

/**
 * FCricketHUDModel — the complete per-frame UI snapshot. Each sub-model carries a
 * "bHas*" relevance flag so a panel hides itself when its source isn't present:
 * the same HUD then works in the match-runner scene (score only) and the batting
 * nets scene (batting + bowling + replay + physics), with no per-scene wiring.
 */
USTRUCT(BlueprintType)
struct CRICKETUI_API FCricketHUDModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HUD") bool bHasScoreboard = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD") FCricketScoreboardVM Scoreboard;

	UPROPERTY(BlueprintReadOnly, Category = "HUD") bool bHasBatting = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD") FCricketBattingVM Batting;

	UPROPERTY(BlueprintReadOnly, Category = "HUD") bool bHasBowling = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD") FCricketBowlingVM Bowling;

	UPROPERTY(BlueprintReadOnly, Category = "HUD") bool bHasReplay = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD") FCricketReplayVM Replay;

	UPROPERTY(BlueprintReadOnly, Category = "HUD") bool bHasPhysics = false;
	UPROPERTY(BlueprintReadOnly, Category = "HUD") FCricketPhysicsVM Physics;
};
