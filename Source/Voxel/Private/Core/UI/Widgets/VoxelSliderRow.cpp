// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Widgets/VoxelSliderRow.h"
#include "Core/UI/Widgets/VoxelTextBlock.h"
#include "Core/UI/Widgets/VoxelSlider.h"
#include "Core/UI/Theme/VoxelUITheme.h"

void UVoxelSliderRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Slider && !bEventsBound)
	{
		Slider->OnValueChanged.AddDynamic(this, &UVoxelSliderRow::HandleSliderChanged);
		bEventsBound = true;
	}
	if (Slider)
	{
		Slider->SetMinValue(MinValue);
		Slider->SetMaxValue(MaxValue);
	}
	RefreshValueText(GetValue());
}

void UVoxelSliderRow::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (LabelText)
	{
		LabelText->SetTextRole(EVoxelTextRole::ItemLabel);
		LabelText->SetText(Label);
	}
	if (ValueText)
	{
		ValueText->SetTextRole(EVoxelTextRole::DataValue);
		ValueText->SetVisibility(bShowValueText ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Slider)
	{
		Slider->SetMinValue(MinValue);
		Slider->SetMaxValue(MaxValue);
	}

	// 布局令牌：标签/数值列宽跨模块对齐（统一改尺寸只改主题资产）
	if (const UVoxelUITheme* Theme = UVoxelUITheme::Get())
	{
		if (LabelText)
		{
			LabelText->SetMinDesiredWidth(Theme->Layout.LabelWidth);
		}
		if (ValueText)
		{
			ValueText->SetMinDesiredWidth(Theme->Layout.ValueWidth);
		}
	}
}

void UVoxelSliderRow::SetValue(float InValue)
{
	if (Slider)
	{
		Slider->SetValue(InValue);
	}
	RefreshValueText(InValue);
}

float UVoxelSliderRow::GetValue() const
{
	return Slider ? Slider->GetValue() : 0.f;
}

void UVoxelSliderRow::SetLabelText(const FText& InText)
{
	Label = InText;
	if (LabelText)
	{
		LabelText->SetText(InText);
	}
}

void UVoxelSliderRow::HandleSliderChanged(float Value)
{
	RefreshValueText(Value);
	OnValueChanged.Broadcast(Value);
}

void UVoxelSliderRow::RefreshValueText(float Value)
{
	if (ValueText)
	{
		ValueText->SetText(FText::AsNumber(FMath::RoundToInt(Value)));
	}
}
