#include "CricketAIPlayerProfile.h"

// Derive a believable AI personality from the coarse FCricketPlayer rating. The
// mapping is deterministic and intentionally simple: role sets the archetype, the
// 0..1 ratings scale the tendencies around it, and pace seeds the bowling family.
// Authored UCricketAIProfileDataAsset entries override this where they exist.
FCricketAIPlayerProfile FCricketAIPlayerProfile::Derive(const FCricketPlayer& P)
{
	FCricketAIPlayerProfile Out;
	Out.Name          = P.Name;
	Out.Role          = P.Role;
	Out.BattingRating = P.Batting;
	Out.BowlingRating = P.Bowling;
	Out.FieldingRating = P.Fielding;

	const double Bat = FMath::Clamp((double)P.Batting, 0.0, 1.0);

	// --- Batting tendencies -------------------------------------------------
	FCricketBattingTendencies& B = Out.Batting;
	B.Technique      = FMath::Lerp(0.30, 0.95, Bat);
	B.Power          = FMath::Lerp(0.25, 0.90, Bat);
	B.StrikeRotation = FMath::Lerp(0.30, 0.90, Bat);
	B.VsPace         = FMath::Lerp(0.30, 0.92, Bat);
	B.VsSpin         = FMath::Lerp(0.30, 0.92, Bat);
	// Better players are tempted less and tangled less; tail-enders are exposed.
	B.ShortBallWeakness    = FMath::Lerp(0.75, 0.20, Bat);
	B.OutsideOffTemptation = FMath::Lerp(0.70, 0.20, Bat);
	B.FullBallWeakness     = FMath::Lerp(0.70, 0.18, Bat);

	switch (P.Role)
	{
	case ECricketRole::BatterTop:
		B.Aggression = 0.42; B.Technique = FMath::Min(1.0, B.Technique + 0.06); break;
	case ECricketRole::BatterMiddle:
		B.Aggression = 0.58; break;
	case ECricketRole::AllRounder:
		B.Aggression = 0.66; B.Power = FMath::Min(1.0, B.Power + 0.05); break;
	case ECricketRole::WicketKeeper:
		B.Aggression = 0.60; B.StrikeRotation = FMath::Min(1.0, B.StrikeRotation + 0.08); break;
	case ECricketRole::PaceBowler:
	case ECricketRole::SpinBowler:
		// Bowlers bat lower: more aggressive (have a swing), weaker technique.
		B.Aggression = 0.70; B.Technique *= 0.8; break;
	}

	// --- Bowling tendencies -------------------------------------------------
	FCricketBowlingTendencies& W = Out.Bowling;
	const double Bowl = FMath::Clamp((double)P.Bowling, 0.0, 1.0);

	// Family from role; pace bowlers with low pace become swing/seam, spinners split
	// off/leg by a stable hash of the name so a roster has both kinds.
	const bool bSpinner = (P.Role == ECricketRole::SpinBowler);
	if (bSpinner)
	{
		W.Style = (GetTypeHash(P.Name) & 1) ? ECricketBowlingStyle::LegSpin : ECricketBowlingStyle::OffSpin;
	}
	else
	{
		W.Style = (P.PaceKmh > 0.f && P.PaceKmh < 130.f) ? ECricketBowlingStyle::Swing : ECricketBowlingStyle::Pace;
	}

	W.Pace      = bSpinner ? 0.18 : FMath::Clamp(P.PaceKmh > 0.f ? (P.PaceKmh - 115.f) / 40.f : Bowl, 0.0, 1.0);
	W.Control   = FMath::Lerp(0.30, 0.95, Bowl);
	W.Movement  = FMath::Lerp(0.25, 0.90, Bowl);
	W.Variation = FMath::Lerp(0.25, 0.88, Bowl);
	W.DeathSkill = FMath::Lerp(0.25, 0.90, Bowl);
	W.Aggression = bSpinner ? 0.45 : 0.6;

	return Out;
}
