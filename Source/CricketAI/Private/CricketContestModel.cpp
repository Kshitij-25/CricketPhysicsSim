#include "CricketContestModel.h"

namespace
{
	// Unique names: anonymous-namespace helpers must not collide with other CricketAI
	// .cpp files when the unity build buckets them into one translation unit.
	bool ContestStumpsLine(ECricketLine L) { return L == ECricketLine::OffStump || L == ECricketLine::Middle || L == ECricketLine::LegStump; }
	bool ContestFullLength(ECricketLength L) { return L == ECricketLength::Full || L == ECricketLength::Yorker || L == ECricketLength::FullToss; }
	bool ContestShortLength(ECricketLength L) { return L == ECricketLength::Short || L == ECricketLength::Bouncer; }

	/** Sample a small running-runs count for a non-boundary stroke. */
	int32 SampleRunningRuns(double Single, double Two, double Three, FRandomStream& Rng)
	{
		const double R = Rng.FRand();
		if (R < Single) { return 1; }
		if (R < Single + Two) { return 2; }
		if (R < Single + Two + Three) { return 3; }
		return 0;
	}
}

FCricketBallResult FCricketContestModel::Resolve(
	const FCricketBowlerDecision& Bowl,
	const FCricketBatterDecision& Bat,
	const FCricketAIPlayerProfile& Bowler,
	const FCricketAIPlayerProfile& Batter,
	const FCricketDeliveryRead& Ball,
	FRandomStream& Rng)
{
	FCricketBallResult R;

	const FCricketBattingTendencies& Bt = Batter.Batting;
	const FCricketBowlingTendencies& W = Bowler.Bowling;
	const double Competence = Ball.IsSpin() ? Bt.VsSpin : Bt.VsPace;
	const double Scatter    = Bowl.ExecutionScatter;

	// ---------------------------------------------------------------------
	// 1. Legality — wides and no-balls from execution scatter / wide intent.
	// ---------------------------------------------------------------------
	const double WideProb = 0.012 + Scatter * 0.09 + (Bowl.Intent.Line == ECricketLine::WideOutsideOff ? 0.05 : 0.0)
	                      + (Bowl.Intent.Line == ECricketLine::DownLeg ? 0.04 : 0.0);
	if (Rng.FRand() < WideProb)
	{
		R.bWide = true;
		// Occasionally byes are run on the wide.
		R.RunsRun = (Rng.FRand() < 0.10) ? 1 : 0;
		return R;
	}
	const double NoBallProb = 0.006 + Scatter * 0.03;
	const bool bNoBall = Rng.FRand() < NoBallProb;
	if (bNoBall) { R.bNoBall = true; } // play continues; runs off bat still count

	// ---------------------------------------------------------------------
	// 2. The leave — offering no shot. A misjudged leave at the stumps is fatal.
	// ---------------------------------------------------------------------
	if (Bat.bOffersNoShot)
	{
		R.bStruck = false;
		if (ContestStumpsLine(Bowl.Intent.Line) && (ContestFullLength(Bowl.Intent.Length) || Bowl.Intent.Length == ECricketLength::GoodLength) && !bNoBall)
		{
			const double pPunish = Bat.Risk * FMath::Lerp(1.1, 0.5, Competence);
			if (Rng.FRand() < pPunish)
			{
				// Full -> usually bowled/lbw; good length -> lbw or bowled.
				if (Rng.FRand() < 0.5) { R.bHitStumps = true; } else { R.bLbw = true; }
			}
		}
		return R; // a clean leave is a dot
	}

	R.bStruck = true;

	// ---------------------------------------------------------------------
	// 3. Wicket probability — built from the intent, the matchup and the field.
	// ---------------------------------------------------------------------
	double pWicket = 0.0;
	switch (Bat.Action)
	{
	case ECricketBatterAction::Defend:      pWicket = 0.005; break;
	case ECricketBatterAction::Rotate:      pWicket = 0.011; break;
	case ECricketBatterAction::Attack:      pWicket = 0.027; break;
	case ECricketBatterAction::BoundaryHit: pWicket = 0.052; break;
	default:                                pWicket = 0.011; break;
	}

	// Skill: a competent batter survives; the risk the batter accepted raises it.
	pWicket *= FMath::Lerp(1.25, 0.5, Competence);
	pWicket *= FMath::Lerp(0.8, 1.4, Bat.Risk);

	// Matchup traps: attacking your weakness is how you get out.
	if (ContestShortLength(Bowl.Intent.Length) && Bat.Input.ShotType == ECricketShotType::PullShot)
		pWicket += Bt.ShortBallWeakness * 0.06;                       // top-edged pull
	if ((Bowl.Intent.Line == ECricketLine::OutsideOff || Bowl.Intent.Line == ECricketLine::WideOutsideOff)
		&& Bat.Action >= ECricketBatterAction::Attack)
		pWicket += Bt.OutsideOffTemptation * 0.05;                    // nick off
	if (ContestFullLength(Bowl.Intent.Length) && ContestStumpsLine(Bowl.Intent.Line))
		pWicket += Bt.FullBallWeakness * 0.035;                       // bowled/lbw driving

	// Bowling quality: movement and the wicket ball threaten more; a loose (scattered)
	// ball threatens less even as it leaks runs.
	pWicket += (Bowl.Intent.Movement != ECricketMovement::SeamUp ? W.Movement * 0.03 : 0.0);
	pWicket += Bowl.WicketIntent * W.Aggression * 0.04;
	pWicket *= FMath::Lerp(1.15, 0.8, Scatter);

	// Close catchers convert the aerial/edged chance.
	if (Bat.Action >= ECricketBatterAction::Attack)
		pWicket += Ball.CatchersUp * 0.03;

	pWicket = FMath::Clamp(pWicket, 0.0, 0.6);

	const bool bOut = (!bNoBall) && (Rng.FRand() < pWicket); // can't be bowled off a no-ball

	if (bOut)
	{
		// Pick a dismissal mode consistent with the shot and the ball.
		const bool bAerial = (Bat.Action == ECricketBatterAction::BoundaryHit) || (Bat.Action == ECricketBatterAction::Attack && Rng.FRand() < 0.5);
		if (ContestShortLength(Bowl.Intent.Length) && Bat.Input.ShotType == ECricketShotType::PullShot)
		{
			R.bCaught = true;                                         // top edge
		}
		else if (ContestFullLength(Bowl.Intent.Length) && ContestStumpsLine(Bowl.Intent.Line) && !bAerial)
		{
			if (Rng.FRand() < 0.55) { R.bHitStumps = true; } else { R.bLbw = true; }
		}
		else if (Ball.IsSpin() && Bat.Input.Footwork == ECricketFootwork::FrontFoot && Rng.FRand() < 0.18)
		{
			R.bStumped = true;                                        // beaten down the track
		}
		else
		{
			R.bCaught = true;                                         // edged or holed out
		}
		return R; // dismissed — no runs off the bat (run-outs handled below survive path)
	}

	// ---------------------------------------------------------------------
	// 4. Survived — distribute runs by intent, power, contact quality and field.
	// ---------------------------------------------------------------------
	// Mistiming: even surviving, an attacking shot can be a dot if poorly struck.
	const double CleanContact = FMath::Clamp(Competence * 0.6 + (1.0 - Bat.Risk) * 0.4 + Rng.FRandRange(-0.15, 0.15), 0.0, 1.0);
	const double FieldOpen = 1.0 - Ball.BoundaryProtection;

	switch (Bat.Action)
	{
	case ECricketBatterAction::Defend:
		R.RunsRun = (Rng.FRand() < 0.24) ? 1 : 0;                     // a nudged single off the block
		break;

	case ECricketBatterAction::Rotate:
		R.RunsRun = SampleRunningRuns(0.64 * Bt.StrikeRotation + 0.30, 0.21, 0.03, Rng);
		break;

	case ECricketBatterAction::Attack:
	{
		const double pFour = FMath::Clamp(0.38 * (0.50 + Bt.Power) * FieldOpen * CleanContact + 0.08, 0.0, 0.64);
		if (Rng.FRand() < pFour) { R.bBoundaryFour = true; }
		else { R.RunsRun = SampleRunningRuns(0.50, 0.28, 0.04, Rng); }  // ones and twos, sometimes a dot
		break;
	}

	case ECricketBatterAction::BoundaryHit:
	{
		const double pSix = FMath::Clamp(0.33 * (0.50 + Bt.Power) * FieldOpen * CleanContact, 0.0, 0.60);
		const double pFour = FMath::Clamp(0.26 * FieldOpen * CleanContact + 0.03, 0.0, 0.5);
		const double Roll = Rng.FRand();
		if (Roll < pSix)            { R.bBoundarySix = true; }
		else if (Roll < pSix + pFour){ R.bBoundaryFour = true; }
		else { R.RunsRun = SampleRunningRuns(0.34, 0.22, 0.03, Rng); } // mishit, scrambled
		break;
	}
	default: break;
	}

	// A small chance the running between the wickets brings a run-out.
	if (R.RunsRun >= 1 && !R.bBoundaryFour && !R.bBoundarySix)
	{
		const double pRunOut = 0.012 + (R.RunsRun >= 2 ? 0.02 : 0.0) + (1.0 - Batter.FieldingRating) * 0.0;
		if (Rng.FRand() < pRunOut)
		{
			R.bRunOut = true;
			R.bRunOutStriker = (Rng.FRand() < 0.5);
			R.RunsRun = FMath::Max(0, R.RunsRun - 1);                  // the completed runs before the out
		}
	}

	return R;
}
