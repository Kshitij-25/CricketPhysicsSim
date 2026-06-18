#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CricketHUDTypes.h"
#include "CricketHUD.generated.h"

class UCricketHUDDataSource;
class UCricketHUDLayout;

/**
 * ACricketHUD — the HUD FRAMEWORK that binds the simulation to the UI.
 *
 * Each frame it asks its UCricketHUDDataSource to (re)resolve the live systems and
 * snapshot them into one FCricketHUDModel, then:
 *   - pushes that model into the UMG widget layer (the player-facing HUD), and
 *   - renders the optional developer canvas overlays — Physics Overlay, Match State
 *     Overlay, Developer HUD — each gated by its own console variable, matching the
 *     project's existing `cricket.*.Debug.*` overlay convention.
 *
 * The HUD holds no gameplay state: it only reads the model the data source produced.
 * It is wired in by ACricketHUDGameMode (HUDClass) so it appears with no level edits.
 *
 * Console variables:
 *   cricket.UI.Widgets        (default 1) — the UMG player HUD on/off
 *   cricket.UI.Debug.Physics  (default 0) — Physics Overlay (speed/rpm/spin/seam/swing/bounce)
 *   cricket.UI.Debug.Match    (default 0) — Match State Overlay (state machine + chase)
 *   cricket.UI.Debug.Dev      (default 0) — Developer HUD (which sources resolved, model summary)
 */
UCLASS()
class CRICKETUI_API ACricketHUD : public AHUD
{
	GENERATED_BODY()

public:
	ACricketHUD();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Root layout widget class (defaults to the C++ UCricketHUDLayout). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|HUD")
	TSubclassOf<UCricketHUDLayout> LayoutClass;

	const UCricketHUDDataSource* GetDataSource() const { return DataSource; }
	const FCricketHUDModel& GetModel() const { return Model; }

private:
	void DrawPhysicsOverlay();
	void DrawMatchOverlay();
	void DrawDevOverlay();

	/** A simple left-aligned text-line cursor for the canvas overlays. */
	void DrawLine(const FString& Text, const FLinearColor& Color, float& InOutY, float X = 24.f);

	UPROPERTY() TObjectPtr<UCricketHUDDataSource> DataSource;
	UPROPERTY() TObjectPtr<UCricketHUDLayout> Layout;

	FCricketHUDModel Model;
};
