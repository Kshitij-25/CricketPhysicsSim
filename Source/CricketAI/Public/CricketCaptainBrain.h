#pragma once

#include "CoreMinimal.h"
#include "CricketAITypes.h"
#include "CricketAIPlayerProfile.h"
#include "CricketMatchAwareness.h"
#include "CricketStadiumTypes.h"   // ECricketFieldPosition, FCricketFieldPlacement
#include "CricketCaptainBrain.generated.h"

/** A bowler the captain may turn to, with the constraints the laws impose. */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketBowlerOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "AI|Captain") FString Name;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Captain") FCricketAIPlayerProfile Profile;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Captain") int32 OversBowled = 0;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Captain") int32 MaxOvers = 4;
	/** True if this bowler sent down the previous over (cannot bowl two in a row). */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Captain") bool bBowledLastOver = false;
	/** Runs conceded so far this spell — a hot/cold read for the captain. */
	UPROPERTY(BlueprintReadWrite, Category = "AI|Captain") int32 RunsConceded = 0;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Captain") int32 Wickets = 0;

	bool IsEligible() const { return !bBowledLastOver && OversBowled < MaxOvers && Profile.CanBowl(); }
};

/** The captain's bowling-change verdict. */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketBowlingChange
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AI|Captain") FString BowlerName;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Captain") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "AI|Captain") FCricketDecisionTrace Trace;
};

/**
 * FCricketCaptainBrain — the Captain AI: bowling changes, field placement and
 * powerplay/match management.
 *
 * It rotates bowlers to suit the phase and the batter (pace up top and at the
 * death, spin to squeeze the middle, holding an over back for the death), and it
 * sets a field that matches the plan and obeys the powerplay fielding restriction.
 * It chooses; the laws (over caps, no consecutive overs) are still enforced by the
 * Match Engine, and the actual runs come from the physics.
 */
class CRICKETAI_API FCricketCaptainBrain
{
public:
	/** Pick the bowler for the next over from the eligible pool. */
	static FCricketBowlingChange ChooseBowler(
		const FCricketMatchSituation& Situation,
		const TArray<FCricketBowlerOption>& Pool,
		const FCricketTeamStrategy& Strategy,
		const FCricketAIDifficultyParams& Difficulty,
		FRandomStream& Rng);

	/**
	 * Set a field for the phase and intent. FieldersAllowedOutsideCircle caps the
	 * boundary riders (2 in the powerplay, 5 otherwise) so the placement is legal.
	 */
	static FCricketFieldPlacement SetField(
		const FCricketMatchSituation& Situation,
		const FCricketAIPlayerProfile& Bowler,
		const FCricketAIPlayerProfile& Batter,
		const FCricketTeamStrategy& Strategy,
		int32 FieldersAllowedOutsideCircle);

	/** Summarise a field as the (BoundaryProtection, CatchersUp) the batter brain reads. */
	static void ReadField(const FCricketFieldPlacement& Field, double& OutBoundaryProtection, double& OutCatchersUp);
};
