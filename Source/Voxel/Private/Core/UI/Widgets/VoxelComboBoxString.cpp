// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Widgets/VoxelComboBoxString.h"
#include "Core/UI/Theme/VoxelUITheme.h"
#include "Brushes/SlateColorBrush.h"

void UVoxelComboBoxString::NativePreConstruct()
{
	// SComboBox 在 RebuildWidget 时才消费 Font/ItemStyle/ContentPadding/MaxListHeight 成员，
	// 基类没有 SynchronizeProperties 推送；必须在构建前的 PreConstruct 应用主题才会生效。
	ApplyThemeStyle();
	Super::NativePreConstruct();
}

void UVoxelComboBoxString::ApplyThemeStyle()
{
	if (!bUseThemeStyle)
	{
		return;
	}
	const UVoxelUITheme* Theme = UVoxelUITheme::Get();
	if (!Theme)
	{
		return;
	}
	const FVoxelComboStyleDesc& Style = Theme->ComboStyle;

	Font = Theme->MakeFontInfo(EVoxelTextRole::Body);
	ForegroundColor = FSlateColor(Style.ForegroundColor);
	ContentPadding = Style.ContentPadding;
	MaxListHeight = Style.MaxListHeight;

	// 下拉行样式：常态底 + 悬停/选中底 + 文字色
	FTableRowStyle RowStyle = ItemStyle;
	RowStyle.SetEvenRowBackgroundBrush(FSlateColorBrush(Style.RowColor));
	RowStyle.SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(Style.RowHoveredColor));
	RowStyle.SetOddRowBackgroundBrush(FSlateColorBrush(Style.RowColor));
	RowStyle.SetOddRowBackgroundHoveredBrush(FSlateColorBrush(Style.RowHoveredColor));
	RowStyle.SetActiveBrush(FSlateColorBrush(Style.RowHoveredColor));
	RowStyle.SetActiveHoveredBrush(FSlateColorBrush(Style.RowHoveredColor));
	RowStyle.SetSelectorFocusedBrush(FSlateColorBrush(Style.RowHoveredColor));
	RowStyle.SetTextColor(FSlateColor(Style.ForegroundColor));
	RowStyle.SetSelectedTextColor(FSlateColor(Style.ForegroundColor));
	ItemStyle = RowStyle;
}
