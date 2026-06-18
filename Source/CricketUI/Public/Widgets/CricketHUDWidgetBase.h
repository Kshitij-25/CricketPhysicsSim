#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "CricketHUDTypes.h"
#include "CricketHUDWidgetBase.generated.h"

class UVerticalBox;
class UTextBlock;

/**
 * UCricketHUDWidgetBase — the common base for every HUD panel.
 *
 * Built on Common UI's UCommonUserWidget so the layer is ready to grow toward a
 * full presentation (input routing, activatable layers) without re-parenting. Each
 * panel is a titled, semi-transparent stack of text rows.
 *
 * The widgets are DATA-DRIVEN and asset-free: a panel constructs its own layout in
 * C++ (so it works the moment an actor drops into a level, matching the project's
 * "press Play, no assets" ethos) and refreshes purely from an FCricketHUDModel. A
 * designer can still reskin any panel by subclassing it as a Widget Blueprint — the
 * data contract (UpdateFromModel) is unchanged.
 */
UCLASS(Abstract)
class CRICKETUI_API UCricketHUDWidgetBase : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Push the latest snapshot. Hides the panel when its data isn't relevant
	 * (IsRelevant), otherwise repaints the rows (RefreshFromModel). The single entry
	 * point the layout calls each frame.
	 */
	void UpdateFromModel(const FCricketHUDModel& Model);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Subclass adds its text rows to ContentBox here (called once, after the title). */
	virtual void BuildPanel() {}

	/** Subclass writes the model into its rows here (called each relevant frame). */
	virtual void RefreshFromModel(const FCricketHUDModel& Model) {}

	/** Whether this panel should be shown for the given model. */
	virtual bool IsRelevant(const FCricketHUDModel& Model) const { return true; }

	/** Construct a text row, append it to the panel, and return it for later updates. */
	UTextBlock* AddRow(int32 FontSize = 13, FLinearColor Color = FLinearColor::White);

	/** Set a row's text (null-safe). */
	static void SetText(UTextBlock* Row, const FString& Text);

	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ContentBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleRow;

	/** Panel title shown in the accent colour at the top of the stack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD") FString PanelTitle = TEXT("PANEL");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD") FLinearColor TitleColor = FLinearColor(0.40f, 0.80f, 1.0f);
};
