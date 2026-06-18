#include "CricketCaptainBrain.h"
#include "CricketTacticalEvaluator.h"

FCricketBowlingChange FCricketCaptainBrain::ChooseBowler(
	const FCricketMatchSituation& S,
	const TArray<FCricketBowlerOption>& Pool,
	const FCricketTeamStrategy& Strategy,
	const FCricketAIDifficultyParams& D,
	FRandomStream& Rng)
{
	FCricketBowlingChange Out;
	FCricketDecisionTrace& T = Out.Trace;
	T.Domain = TEXT("Captain");
	T.Pressure = FCricketTacticalEvaluator::BowlingPressure(S);

	// Map eligible bowlers to scored options (index aligned with the eligible list).
	TArray<int32> Eligible;
	for (int32 i = 0; i < Pool.Num(); ++i)
	{
		const FCricketBowlerOption& O = Pool[i];
		if (!O.IsEligible()) { continue; }
		const FCricketBowlingTendencies& W = O.Profile.Bowling;
		const bool bSpin = W.IsSpin();

		double Sc = 0.25 + O.Profile.BowlingRating * 0.5;

		// --- Phase fit ---
		switch (S.Phase)
		{
		case ECricketMatchPhase::Powerplay:
			Sc += bSpin ? -0.05 : 0.25;                       // pace with the new ball
			Sc += Strategy.Captaincy.FrontloadStrikeBowlers * O.Profile.BowlingRating * 0.2;
			break;
		case ECricketMatchPhase::Middle:
			Sc += bSpin ? 0.25 : 0.05;                        // spin to squeeze
			break;
		case ECricketMatchPhase::Death:
			Sc += bSpin ? -0.05 : 0.10;
			Sc += W.DeathSkill * 0.35;                        // death specialists
			break;
		default: break;
		}

		// --- Freshness: prefer bowlers with overs in the bank, save them otherwise ---
		const double OversLeft = O.MaxOvers - O.OversBowled;
		Sc += FMath::Clamp(OversLeft / FMath::Max(1, O.MaxOvers), 0.0, 1.0) * 0.15;
		// At the death, having held a strike bowler back is rewarded.
		if (S.Phase == ECricketMatchPhase::Death && OversLeft > 0 && !bSpin) Sc += W.DeathSkill * 0.15;

		// --- Form: a bowler going for runs this spell is less attractive ---
		const double Eco = O.OversBowled > 0 ? double(O.RunsConceded) / O.OversBowled : 8.0;
		Sc -= FMath::Clamp((Eco - 8.0) / 8.0, -0.2, 0.3) * 0.4;
		Sc += O.Wickets * 0.06;

		Eligible.Add(i);
		T.Add(FName(*O.Name), Sc);
	}

	if (Eligible.Num() == 0)
	{
		// No legal eligible bowler scored: fall back to the first who can bowl at all.
		for (const FCricketBowlerOption& O : Pool)
		{
			if (O.OversBowled < O.MaxOvers && O.Profile.CanBowl() && !O.bBowledLastOver)
			{ Out.BowlerName = O.Name; Out.bValid = true; return Out; }
		}
		return Out;
	}

	const int32 ChosenTrace = FCricketTacticalEvaluator::SelectOption(T, D, Rng, 1.0);
	Out.BowlerName = Pool[Eligible[FMath::Clamp(ChosenTrace, 0, Eligible.Num() - 1)]].Name;
	Out.bValid = true;
	T.Intent = FString::Printf(TEXT("%s over -> %s"),
		S.Phase == ECricketMatchPhase::Death ? TEXT("Death") : (S.Phase == ECricketMatchPhase::Powerplay ? TEXT("PP") : TEXT("Middle")),
		*Out.BowlerName);
	return Out;
}

FCricketFieldPlacement FCricketCaptainBrain::SetField(
	const FCricketMatchSituation& S,
	const FCricketAIPlayerProfile& Bowler,
	const FCricketAIPlayerProfile& Batter,
	const FCricketTeamStrategy& Strategy,
	int32 FieldersAllowedOutsideCircle)
{
	FCricketFieldPlacement Field;
	Field.Positions.Add(ECricketFieldPosition::WicketKeeper);

	// How attacking to be: phase + captaincy + how on-top the bowling side is, but
	// the powerplay hard-caps the boundary riders regardless of intent.
	double Attack = Strategy.Captaincy.FieldAggression;
	if (S.Phase == ECricketMatchPhase::Powerplay) Attack += 0.25;
	if (S.Phase == ECricketMatchPhase::Death)     Attack -= 0.30;
	Attack += (FCricketTacticalEvaluator::BowlingPressure(S) - 0.5) * 0.3;
	if (Batter.BattingRating < 0.35) Attack += Strategy.Captaincy.AttackNewBatter * 0.2; // crowd the tail
	Attack = FMath::Clamp(Attack, 0.0, 1.0);

	const int32 OutfieldSlots = 9;                       // 11 - keeper - bowler
	const int32 MaxOut = FMath::Clamp(FieldersAllowedOutsideCircle, 0, OutfieldSlots);
	// More boundary riders the less attacking we are, but never beyond the legal cap.
	int32 Deep = FMath::Clamp(FMath::RoundToInt(FMath::Lerp(1.0, double(OutfieldSlots), 1.0 - Attack)), 0, MaxOut);
	int32 Close = OutfieldSlots - Deep;

	// Attacking close catchers first (slips/gully/short catchers), then a ring, then
	// the boundary riders that mop up. Drawn from the standard position vocabulary.
	const ECricketFieldPosition Catchers[] = {
		ECricketFieldPosition::Slip, ECricketFieldPosition::Gully, ECricketFieldPosition::Point,
		ECricketFieldPosition::Cover, ECricketFieldPosition::MidOff, ECricketFieldPosition::MidOn,
		ECricketFieldPosition::Midwicket, ECricketFieldPosition::SquareLeg, ECricketFieldPosition::ThirdMan };
	const ECricketFieldPosition Riders[] = {
		ECricketFieldPosition::LongOff, ECricketFieldPosition::LongOn, ECricketFieldPosition::DeepMidwicket,
		ECricketFieldPosition::DeepCover, ECricketFieldPosition::DeepSquareLeg, ECricketFieldPosition::DeepPoint,
		ECricketFieldPosition::FineLeg, ECricketFieldPosition::ThirdMan, ECricketFieldPosition::LongOff };

	for (int32 i = 0; i < Close && i < UE_ARRAY_COUNT(Catchers); ++i) { Field.Positions.Add(Catchers[i]); }
	for (int32 i = 0; i < Deep  && i < UE_ARRAY_COUNT(Riders);   ++i) { Field.Positions.Add(Riders[i]); }

	Field.Name = FString::Printf(TEXT("%s field (atk %.0f%%, %d out)"),
		S.Phase == ECricketMatchPhase::Death ? TEXT("Death") : (S.Phase == ECricketMatchPhase::Powerplay ? TEXT("PP") : TEXT("Middle")),
		Attack * 100.0, Deep);
	return Field;
}

void FCricketCaptainBrain::ReadField(const FCricketFieldPlacement& Field, double& OutBoundaryProtection, double& OutCatchersUp)
{
	int32 Deep = 0, Catchers = 0, Total = 0;
	for (ECricketFieldPosition P : Field.Positions)
	{
		if (P == ECricketFieldPosition::WicketKeeper) { continue; }
		++Total;
		switch (P)
		{
		case ECricketFieldPosition::LongOff: case ECricketFieldPosition::LongOn:
		case ECricketFieldPosition::DeepCover: case ECricketFieldPosition::DeepMidwicket:
		case ECricketFieldPosition::DeepPoint: case ECricketFieldPosition::DeepSquareLeg:
		case ECricketFieldPosition::FineLeg: case ECricketFieldPosition::ThirdMan:
			++Deep; break;
		case ECricketFieldPosition::Slip: case ECricketFieldPosition::Gully:
			++Catchers; break;
		default: break;
		}
	}
	OutBoundaryProtection = Total > 0 ? FMath::Clamp(double(Deep) / 6.0, 0.0, 1.0) : 0.4;
	OutCatchersUp = Total > 0 ? FMath::Clamp(double(Catchers) / 3.0, 0.0, 1.0) : 0.2;
}
