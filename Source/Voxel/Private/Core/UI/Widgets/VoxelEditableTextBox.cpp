// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Widgets/VoxelEditableTextBox.h"
#include "Core/UI/Theme/VoxelUITheme.h"
#include "Brushes/SlateColorBrush.h"

void UVoxelEditableTextBox::SynchronizeProperties()
{
	// 先改 WidgetStyle 成员，再由基类把 &WidgetStyle 推给 Slate。
	// 严禁「局部样式 + SetWidgetStyle」：UEditableTextBox::SetWidgetStyle 内部把形参地址
	// 存进 SEditableTextBox（SetStyle(&InStyle)），局部变量析构后悬垂，下一次 Prepass 解引用即崩溃。
	ApplyThemeStyle();
	Super::SynchronizeProperties();
}

void UVoxelEditableTextBox::ApplyThemeStyle()
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
	const FVoxelInputStyleDesc& Style = Theme->InputStyle;

	// 直接改 WidgetStyle 成员（UPROPERTY，生命周期随控件），样式 Setter 均为按值拷贝，安全
	WidgetStyle.SetFont(Theme->MakeFontInfo(EVoxelTextRole::Body));
	WidgetStyle.SetForegroundColor(FSlateColor(Style.ForegroundColor));
	WidgetStyle.SetPadding(Style.Padding);
	WidgetStyle.SetBackgroundImageNormal(FSlateColorBrush(Style.BackgroundColor));
	WidgetStyle.SetBackgroundImageHovered(FSlateColorBrush(Style.BackgroundColor));
	WidgetStyle.SetBackgroundImageFocused(FSlateColorBrush(Style.FocusedBackgroundColor));
	WidgetStyle.SetBackgroundImageReadOnly(FSlateColorBrush(FLinearColor(Style.BackgroundColor.R, Style.BackgroundColor.G, Style.BackgroundColor.B, Style.BackgroundColor.A * 0.4f)));
}
