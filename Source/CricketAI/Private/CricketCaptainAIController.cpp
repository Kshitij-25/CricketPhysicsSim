#include "CricketCaptainAIController.h"
#include "CricketMatchAwareness.h"
#include "CricketMatchEngine.h"

UCricketCaptainAIController::UCricketCaptainAIController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCricketCaptainAIController::SetEngine(UCricketMatchEngine* InEngine)
{
	Engine = InEngine;
}

const FCricketAIPlayerProfile* UCricketCaptainAIController::FindProfile(const FString& Name) const
{
	for (const FCricketAIPlayerProfile& P : SquadProfiles) { if (P.Name == Name) { return &P; } }
	return nullptr;
}

FString UCricketCaptainAIController::ChooseAndSetBowler()
{
	UCricketMatchEngine* E = Engine.Get();
	if (!E || !E->IsLive()) { return FString(); }

	const FCricketInningsScorecard& Inn = E->GetActiveInnings();
	const FCricketMatchRules& Rules = E->GetRules();
	const FCricketMatchSituation Sit = FCricketMatchSituation::FromEngine(*E, ParTotalT20);

	// Build the eligible pool from this side's resources + the engine's bowler stats.
	TArray<FCricketBowlerOption> Pool;
	for (const FCricketAIPlayerProfile& Prof : SquadProfiles)
	{
		if (!Prof.CanBowl()) { continue; }
		FCricketBowlerOption Opt;
		Opt.Name = Prof.Name; Opt.Profile = Prof;
		Opt.MaxOvers = Rules.MaxOversPerBowler;
		Opt.bBowledLastOver = (Prof.Name == CurrentOverBowler);
		for (const FCricketBowlerStats& BS : Inn.Bowlers)
		{
			if (BS.Name == Prof.Name) { Opt.OversBowled = BS.CompletedOvers(); Opt.RunsConceded = BS.RunsConceded; Opt.Wickets = BS.Wickets; break; }
		}
		Pool.Add(Opt);
	}

	const FCricketAIDifficultyParams D = FCricketAIDifficultyParams::Resolve(Difficulty);
	FRandomStream Rng(Seed++);
	LastChange = FCricketCaptainBrain::ChooseBowler(Sit, Pool, Strategy, D, Rng);

	if (LastChange.bValid && E->SetBowler(LastChange.BowlerName))
	{
		CurrentOverBowler = LastChange.BowlerName;
		return CurrentOverBowler;
	}
	// Fallback: any legal bowler.
	for (const FCricketAIPlayerProfile& Prof : SquadProfiles)
	{
		if (Prof.CanBowl() && E->CanBowl(Prof.Name) && E->SetBowler(Prof.Name))
		{ CurrentOverBowler = Prof.Name; LastChange.BowlerName = Prof.Name; LastChange.bValid = true; return CurrentOverBowler; }
	}
	return FString();
}

FCricketFieldPlacement UCricketCaptainAIController::SetFieldFor(const FString& BowlerName, const FString& BatterName)
{
	UCricketMatchEngine* E = Engine.Get();
	FCricketMatchSituation Sit;
	int32 FieldersOut = 5;
	if (E && E->IsLive())
	{
		Sit = FCricketMatchSituation::FromEngine(*E, ParTotalT20);
		FieldersOut = (Sit.Phase == ECricketMatchPhase::Powerplay) ? E->GetRules().PowerplayFieldersOut : E->GetRules().NonPowerplayFieldersOut;
	}

	static const FCricketAIPlayerProfile Default;
	const FCricketAIPlayerProfile* Bowler = FindProfile(BowlerName);
	const FCricketAIPlayerProfile* Batter = FindProfile(BatterName);
	LastField = FCricketCaptainBrain::SetField(Sit,
		Bowler ? *Bowler : Default,
		Batter ? *Batter : Default,
		Strategy, FieldersOut);
	return LastField;
}
