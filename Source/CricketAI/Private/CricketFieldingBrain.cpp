#include "CricketFieldingBrain.h"
#include "CricketTacticalEvaluator.h"

FCricketFieldingPlan FCricketFieldingBrain::SelectChaser(
	const TArray<FCricketInterceptResult>& Intercepts,
	const FCricketAIDifficultyParams& D,
	FRandomStream& Rng)
{
	FCricketFieldingPlan Plan;
	FCricketDecisionTrace& T = Plan.Trace;
	T.Domain = TEXT("Fielding");

	if (Intercepts.Num() == 0) { return Plan; }

	// Score every fielder's intercept. A reachable catch is gold; among ground
	// fields the earliest (most slack) wins. Unreachable fielders score near zero.
	for (int32 i = 0; i < Intercepts.Num(); ++i)
	{
		const FCricketInterceptResult& R = Intercepts[i];
		double Sc = 0.0;
		if (R.bCanIntercept)
		{
			Sc = (R.Kind == ECricketInterceptKind::Catch) ? 2.0 : 1.0;   // catches before stops
			Sc += FMath::Clamp(R.SlackSec, -1.0, 2.0) * 0.5;             // more time = better placed
			Sc -= R.DistanceM * 0.01;                                    // mild tiebreak: nearer is tidier
			switch (R.Difficulty)
			{
			case ECricketCatchDifficulty::Regulation: Sc += 0.4; break;
			case ECricketCatchDifficulty::Running:    Sc += 0.1; break;
			case ECricketCatchDifficulty::Diving:     Sc -= 0.1; break;
			default: break;
			}
		}
		else
		{
			Sc = -1.0 - R.DistanceM * 0.01;                              // closest of the unreachable backs up
		}
		T.Add(FName(*FString::Printf(TEXT("F%d"), i)), Sc);
	}

	const int32 Chosen = FCricketTacticalEvaluator::SelectOption(T, D, Rng, 1.0);
	Plan.ChaserIndex = Chosen;
	if (Intercepts.IsValidIndex(Chosen)) { Plan.Kind = Intercepts[Chosen].Kind; }

	// Backup/relay: the next-best reachable fielder (or the closest one) behind the chaser.
	int32 Backup = INDEX_NONE; double BestBackup = -1e9;
	for (int32 i = 0; i < Intercepts.Num(); ++i)
	{
		if (i == Chosen) { continue; }
		const double S = T.Options[i].Score;
		if (S > BestBackup) { BestBackup = S; Backup = i; }
	}
	Plan.BackupIndex = Backup;

	T.Intent = FString::Printf(TEXT("Chaser F%d (%s)"), Chosen,
		Plan.Kind == ECricketInterceptKind::Catch ? TEXT("catch") :
		Plan.Kind == ECricketInterceptKind::GroundField ? TEXT("field") : TEXT("cover"));
	return Plan;
}

bool FCricketFieldingBrain::ShouldAttemptCatch(ECricketCatchDifficulty Difficulty,
	double FielderRating, const FCricketAIDifficultyParams& D, FRandomStream& Rng)
{
	switch (Difficulty)
	{
	case ECricketCatchDifficulty::Regulation:
		return true;                                              // always go for the sitter
	case ECricketCatchDifficulty::Running:
		// Committed unless a poor fielder on an easy setting bottles the read.
		return Rng.FRand() < FMath::Clamp(0.6 + FielderRating * 0.4 + D.SituationalAwareness * 0.1, 0.0, 1.0);
	case ECricketCatchDifficulty::Diving:
		// A worthwhile gamble for a good fielder on a hard setting; often declined otherwise.
		return Rng.FRand() < FMath::Clamp(0.15 + FielderRating * 0.5 + D.SituationalAwareness * 0.25, 0.0, 1.0);
	default:
		return false;                                            // impossible — save the boundary instead
	}
}

ECricketThrowTarget FCricketFieldingBrain::ChooseThrow(
	bool bRunOutChance,
	const FCricketThrowSolution& ToStumps,
	const FCricketThrowSolution& ToKeeper,
	const FCricketAIDifficultyParams& D,
	FRandomStream& Rng)
{
	// Go for the run-out only when it is on AND the throw at the stumps is feasible.
	if (bRunOutChance && ToStumps.bFeasible)
	{
		// A lower-awareness side sometimes throws to the keeper anyway and misses it.
		if (Rng.FRand() < FMath::Clamp(0.5 + D.SituationalAwareness * 0.5, 0.0, 1.0))
		{
			return ECricketThrowTarget::Stumps;
		}
	}
	// Default: the safe ball back. Keeper if reachable, else the nearest fielder.
	return ToKeeper.bFeasible ? ECricketThrowTarget::Keeper : ECricketThrowTarget::NearestFielder;
}
