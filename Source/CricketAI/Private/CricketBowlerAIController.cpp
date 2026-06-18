#include "CricketBowlerAIController.h"
#include "CricketBowlingComponent.h"
#include "CricketMatchEngine.h"

UCricketBowlerAIController::UCricketBowlerAIController()
{
	PrimaryComponentTick.bCanEverTick = false;
	// A sensible default identity until a profile is authored / derived.
	Profile.Name = TEXT("AI Bowler");
	Profile.Role = ECricketRole::PaceBowler;
}

void UCricketBowlerAIController::SetTargets(UCricketBowlingComponent* InBowling, UCricketMatchEngine* InEngine)
{
	Bowling = InBowling;
	Engine  = InEngine;
}

FCricketMatchSituation UCricketBowlerAIController::BuildSituation() const
{
	if (Engine.IsValid() && Engine->IsLive())
	{
		return FCricketMatchSituation::FromEngine(*Engine, ParTotalT20);
	}
	// No engine wired: a neutral middle-overs context so the brain still reasons.
	return FCricketMatchSituation::Build(60, 2, 48, 20, 6, 11, 6, /*chasing*/ false, 0, ParTotalT20);
}

void UCricketBowlerAIController::PlanDelivery()
{
	const FCricketMatchSituation Sit = BuildSituation();
	const FCricketAIDifficultyParams D = FCricketAIDifficultyParams::Resolve(Difficulty);
	FRandomStream Rng(Seed++);
	LastDecision = FCricketBowlerBrain::Decide(Sit, Profile, CurrentBatter, Memory, D, Rng);
	bHasPlan = true;
}

void UCricketBowlerAIController::ExecutePlanned()
{
	if (!bHasPlan) { PlanDelivery(); }
	UCricketBowlingComponent* B = Bowling.Get();
	if (!B) { return; }

	const FCricketBowlingIntent& In = LastDecision.Intent;
	B->SetLine(In.Line);
	B->SetLength(In.Length);
	B->SetMovement(In.Movement);
	B->SetPace01(In.Pace01);
	B->SetSwingAmount(In.SwingAmount);
	B->SetSpinAmount(In.SpinAmount);
	// Execution imperfection flows through the SAME scatter channel the human uses.
	B->HumanScatter = FMath::Clamp(LastDecision.ExecutionScatter, 0.0, 1.0);
	B->BowlNow();

	Memory.Record(In.Length, In.Line);
	bHasPlan = false;
}
