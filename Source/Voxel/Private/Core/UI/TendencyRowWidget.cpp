// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/TendencyRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"

void UTendencyRowWidget::UpdateTendencyData(FGameplayTag Tag, float Value, UConfigDataSubsystem* ConfigSubsystem)
{
	FText DisplayName = FText::FromName(Tag.GetTagName());
	FLinearColor Color = FLinearColor::White;
	// 尝试从配置系统获取友好的显示名称
	if (ConfigSubsystem)
	{
		DisplayName = ConfigSubsystem->GetTagDisplayName(Tag);
		Color=ConfigSubsystem->GetTagColor(Tag);
	}

	UpdateTendencyRaw(DisplayName, Value, Color);
}

void UTendencyRowWidget::UpdateTendencyRaw(FText InName, float Value,FLinearColor Color)
{
	// 1. 设置名称
	if (LabelText)
	{
		LabelText->SetText(InName);
	}

	// 2. 设置滑块
	if (TendencySlider)
	{
		// 【关键修改】由于你在 UMG 里设置了 Min=-1.0, Max=1.0，
		// 这里直接填入原始 Value 即可，不需要数学转换。
		// Slider 会自动将 0.0 放在正中间。
		TendencySlider->SetValue(FMath::Clamp(Value, -1.0f, 1.0f));
        
		// 锁定交互，仅作展示
		TendencySlider->SetLocked(true); 

		// 【新增】应用颜色控制
		// 设置滑条背景色
		TendencySlider->SetSliderBarColor(Color);
        
		// (可选) 如果你也想让滑块头变色，可以加上这行，
		// 或者保持白色以突出对比，视你的 UI 风格而定。
		// TendencySlider->SetSliderHandleColor(Color); 
	}

	// 3. 设置数值文本
	if (ValueText)
	{
		// 格式化数值显示
		FNumberFormattingOptions Opts;
		Opts.MaximumFractionalDigits = 1;
		Opts.MinimumFractionalDigits = 1;
		Opts.AlwaysSign = true; 

		ValueText->SetText(FText::AsNumber(Value, &Opts));
        
		// 让文字颜色也跟随传入的 Color
		ValueText->SetColorAndOpacity(FSlateColor(Color));
	}
}
