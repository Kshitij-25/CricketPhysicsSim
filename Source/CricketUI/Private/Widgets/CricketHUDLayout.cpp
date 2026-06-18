#include "Widgets/CricketHUDLayout.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

#include "Widgets/CricketScoreboardWidget.h"
#include "Widgets/CricketBattingWidget.h"
#include "Widgets/CricketBowlingWidget.h"
#include "Widgets/CricketReplayWidget.h"

TSharedRef<SWidget> UCricketHUDLayout::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;
	}
	return Super::RebuildWidget();
}

void UCricketHUDLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsurePanels();
}

void UCricketHUDLayout::EnsurePanels()
{
	if (!Canvas) { return; }

	// Default each slot to its C++ panel class so the HUD is fully functional with
	// no authored Widget Blueprints; a designer may override any class.
	if (!ScoreboardClass) { ScoreboardClass = UCricketScoreboardWidget::StaticClass(); }
	if (!BattingClass)    { BattingClass    = UCricketBattingWidget::StaticClass(); }
	if (!BowlingClass)    { BowlingClass    = UCricketBowlingWidget::StaticClass(); }
	if (!ReplayClass)     { ReplayClass     = UCricketReplayWidget::StaticClass(); }

	auto Place = [this](UWidget* W, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Pos)
	{
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(W))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetAlignment(Alignment);
			Slot->SetAutoSize(true);
			Slot->SetPosition(Pos);
		}
	};

	if (!Scoreboard) { Scoreboard = CreateWidget<UCricketScoreboardWidget>(this, ScoreboardClass);
		Place(Scoreboard, FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.f,  20.f)); }
	if (!Bowling)    { Bowling = CreateWidget<UCricketBowlingWidget>(this, BowlingClass);
		Place(Bowling,    FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(20.f, 20.f)); }
	if (!Batting)    { Batting = CreateWidget<UCricketBattingWidget>(this, BattingClass);
		Place(Batting,    FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(20.f, -20.f)); }
	if (!Replay)     { Replay = CreateWidget<UCricketReplayWidget>(this, ReplayClass);
		Place(Replay,     FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.f,  -20.f)); }
}

void UCricketHUDLayout::UpdateFromModel(const FCricketHUDModel& Model)
{
	EnsurePanels();   // resilient if called before NativeOnInitialized
	if (Scoreboard) { Scoreboard->UpdateFromModel(Model); }
	if (Bowling)    { Bowling->UpdateFromModel(Model); }
	if (Batting)    { Batting->UpdateFromModel(Model); }
	if (Replay)     { Replay->UpdateFromModel(Model); }
}
