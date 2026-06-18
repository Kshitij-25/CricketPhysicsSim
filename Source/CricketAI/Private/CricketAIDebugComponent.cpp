#include "CricketAIDebugComponent.h"
#include "CricketBatterAIController.h"
#include "CricketBowlerAIController.h"
#include "CricketCaptainAIController.h"
#include "CricketFieldingAICoordinator.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarCricketAIDebug(
	TEXT("cricket.AI.Debug"), 0,
	TEXT("Draw the Cricket AI decision overlay (state, scores, tactical intent)."),
	ECVF_Cheat);

namespace
{
	// Render one decision trace as a coloured block of on-screen lines.
	void DrawTrace(const FCricketDecisionTrace& T, int32& Key, const FColor& HeaderColor)
	{
		if (!GEngine) { return; }
		GEngine->AddOnScreenDebugMessage(Key++, 0.0f, HeaderColor,
			FString::Printf(TEXT("[%s] %s  (pressure %.2f)"), *T.Domain.ToString(), *T.Intent, T.Pressure));
		for (const FCricketScoredOption& O : T.Options)
		{
			const FColor C = O.bChosen ? FColor::Green : FColor(150, 150, 150);
			GEngine->AddOnScreenDebugMessage(Key++, 0.0f, C,
				FString::Printf(TEXT("   %s  %-9s  score %+.2f  p=%.0f%%"),
					O.bChosen ? TEXT(">") : TEXT(" "), *O.Label.ToString(), O.Score, O.Probability * 100.0));
		}
	}
}

UCricketAIDebugComponent::UCricketAIDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCricketAIDebugComponent::WatchBatter(UCricketBatterAIController* In) { Batter = In; }
void UCricketAIDebugComponent::WatchBowler(UCricketBowlerAIController* In) { Bowler = In; }
void UCricketAIDebugComponent::WatchCaptain(UCricketCaptainAIController* In) { Captain = In; }
void UCricketAIDebugComponent::WatchFielding(UCricketFieldingAICoordinator* In) { Fielding = In; }

void UCricketAIDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CVarCricketAIDebug.GetValueOnGameThread() == 0 || !GEngine) { return; }

	int32 Key = 0x51C; // a stable base key so the lines update in place
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Yellow, TEXT("===== Cricket AI ====="));

	if (UCricketBowlerAIController* B = Bowler.Get())
	{
		DrawTrace(B->GetLastTrace(), Key, FColor::Cyan);
	}
	if (UCricketBatterAIController* Bt = Batter.Get())
	{
		const FCricketDeliveryRead& R = Bt->GetLastRead();
		GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Orange,
			FString::Printf(TEXT("[Batter] read: line=%s len=%s -> %s pwr %.2f"),
				*UEnum::GetDisplayValueAsText(R.Line).ToString(),
				*UEnum::GetDisplayValueAsText(R.Length).ToString(),
				*UEnum::GetDisplayValueAsText(Bt->GetLastDecision().Input.ShotType).ToString(),
				Bt->GetLastDecision().Input.PowerScale));
		DrawTrace(Bt->GetLastTrace(), Key, FColor::Orange);
	}
	if (UCricketCaptainAIController* C = Captain.Get())
	{
		GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Magenta,
			FString::Printf(TEXT("[Captain] bowling: %s | field: %s"),
				*C->GetLastChange().BowlerName, *C->GetLastField().Name));
		DrawTrace(C->GetLastChange().Trace, Key, FColor::Magenta);
	}
	if (UCricketFieldingAICoordinator* F = Fielding.Get())
	{
		GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::White,
			FString::Printf(TEXT("[Fielding] %s"), *F->GetPlan().Trace.Intent));
	}
}
