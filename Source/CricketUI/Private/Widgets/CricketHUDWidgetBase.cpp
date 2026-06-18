#include "Widgets/CricketHUDWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UCricketHUDWidgetBase::RebuildWidget()
{
	// Construct the panel layout once, in C++, so the widget is fully functional
	// with no authored asset. A WBP subclass that provides its own RootWidget skips
	// this entirely (we only build when the tree is empty).
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBg"));
		Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
		Background->SetPadding(FMargin(10.f, 7.f));
		WidgetTree->RootWidget = Background;

		ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
		Background->SetContent(ContentBox);

		TitleRow = AddRow(15, TitleColor);
		SetText(TitleRow, PanelTitle);

		BuildPanel();
	}
	return Super::RebuildWidget();
}

UTextBlock* UCricketHUDWidgetBase::AddRow(int32 FontSize, FLinearColor Color)
{
	if (!WidgetTree || !ContentBox) { return nullptr; }

	UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Row->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FontSize));
	Row->SetColorAndOpacity(FSlateColor(Color));
	if (UVerticalBoxSlot* Slot = ContentBox->AddChildToVerticalBox(Row))
	{
		Slot->SetPadding(FMargin(0.f, 1.f));
	}
	return Row;
}

void UCricketHUDWidgetBase::SetText(UTextBlock* Row, const FString& Text)
{
	if (Row) { Row->SetText(FText::FromString(Text)); }
}

void UCricketHUDWidgetBase::UpdateFromModel(const FCricketHUDModel& Model)
{
	const bool bShow = IsRelevant(Model);
	SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShow)
	{
		RefreshFromModel(Model);
	}
}
