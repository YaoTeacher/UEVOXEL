// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Widgets/VoxelSlider.h"
#include "Core/UI/Theme/VoxelUITheme.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"

void UVoxelSlider::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyThemeStyle();
}

void UVoxelSlider::ApplyThemeStyle()
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
	const FVoxelSliderStyleDesc& Style = Theme->SliderStyle;

	FSliderStyle NewStyle = GetWidgetStyle();
	NewStyle.SetBarThickness(Style.BarThickness);

	// 轨道：纯色
	NewStyle.SetNormalBarImage(FSlateColorBrush(Style.BarColor));
	NewStyle.SetHoveredBarImage(FSlateColorBrush(Style.BarColor));
	NewStyle.SetDisabledBarImage(FSlateColorBrush(FLinearColor(Style.BarColor.R, Style.BarColor.G, Style.BarColor.B, Style.BarColor.A * 0.4f)));

	// 把手：优先主题贴图，缺省纯色圆点
	FSlateBrush ThumbBrush;
	if (UTexture2D* ThumbTex = Style.ThumbTexture.LoadSynchronous())
	{
		ThumbBrush.SetResourceObject(ThumbTex);
		ThumbBrush.DrawAs = ESlateBrushDrawType::Image;
	}
	else
	{
		ThumbBrush = FSlateColorBrush(Style.ThumbColor);
	}
	ThumbBrush.ImageSize = FVector2D(Style.ThumbSize, Style.ThumbSize);

	FSlateBrush ThumbHoveredBrush = ThumbBrush;
	ThumbHoveredBrush.TintColor = FSlateColor(Style.ThumbHoveredColor);
	if (!Style.ThumbTexture.IsNull())
	{
		// 贴图把手：悬停改 Tint；纯色把手：直接换色
	}
	else
	{
		ThumbHoveredBrush = FSlateColorBrush(Style.ThumbHoveredColor);
		ThumbHoveredBrush.ImageSize = ThumbBrush.ImageSize;
	}

	NewStyle.SetNormalThumbImage(ThumbBrush);
	NewStyle.SetHoveredThumbImage(ThumbHoveredBrush);
	NewStyle.SetDisabledThumbImage(ThumbBrush);

	SetWidgetStyle(NewStyle);
}
