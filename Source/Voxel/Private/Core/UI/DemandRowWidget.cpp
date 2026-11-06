// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/UI/DemandRowWidget.h"

#include "GameplayTagContainer.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Data/Creature/CreatureBase.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"

class UConfigDataSubsystem;

void UDemandRowWidget::UpdateWidgetData(const float Cur, const float Max, FGameplayTag DemandTag)
{
	
	// 2. 更新进度条
	if (DemandValueChart)
	{
		// 简单的百分比计算
		float Percent = FMath::Clamp(Cur / Max, 0.0f, 1.0f);
		DemandValueChart->SetPercent(Percent);
        
		// 可选：根据百分比变色 (低数值变红)
		//FLinearColor BarColor = (Percent < 0.3f) ? FLinearColor::Red : FLinearColor::Green;
		//DemandValueChart->SetFillColorAndOpacity(BarColor);
	}

	// 3. 更新名字 (查 ConfigSubsystem)
	if (DemandNameTextBox)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UConfigDataSubsystem* Config = GI->GetSubsystem<UConfigDataSubsystem>())
			{
				// 使用你在 Subsystem 里写好的接口获取显示名称
				FText DisplayName = Config->GetTagDisplayName(DemandTag);
				DemandNameTextBox->SetText(DisplayName);
			}
		}
		else
		{
			// 兜底显示 Tag 原名
			DemandNameTextBox->SetText(FText::FromName(DemandTag.GetTagName()));
		}
	}

	if (DemandValueTextBox)
	{
		// 将浮点数转为整数显示 (FMath::RoundToInt)
		FNumberFormattingOptions Options;
		Options.SetMaximumFractionalDigits(0); // 不显示小数

		FText CurText = FText::AsNumber(FMath::RoundToInt(Cur), &Options);
		FText MaxText = FText::AsNumber(FMath::RoundToInt(Max), &Options);

		// 拼接格式: "{当前} / {最大}"
		FText FinalValueStr = FText::Format(NSLOCTEXT("UI", "DemandValFormat", "{0} / {1}"), CurText, MaxText);

		DemandValueTextBox->SetText(FinalValueStr);
	}
}

void UDemandRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
}
