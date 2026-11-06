// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/UI/NPCUI/StatRowWidget.h"

#include "GameplayTagContainer.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"

void UStatRowWidget::UpdateStat(FGameplayTag StatTag, float Value)
{
	// 1. 设置数值文本 (保留 1 位小数)
	if (StatValueText)
	{
		// 格式化字符串: "100.0"
		FText ValueStr = FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping());
		StatValueText->SetText(ValueStr);
	}

	// 2. 获取配置子系统 (查名字和图标)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UConfigDataSubsystem* Config = GI->GetSubsystem<UConfigDataSubsystem>())
		{
			// 设置属性名 (从 TagUIDefinition 获取 DisplayName)
			if (StatNameText)
			{
				FText DisplayName = Config->GetTagDisplayName(StatTag);
				StatNameText->SetText(DisplayName);
			}

			// (可选) 设置图标
			if (StatIcon)
			{
				TSoftObjectPtr<UTexture2D> IconSoft = Config->GetTagIcon(StatTag);
				if (!IconSoft.IsNull())
				{
					StatIcon->SetBrushFromSoftTexture(IconSoft);
					StatIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				}
				else
				{
					StatIcon->SetVisibility(ESlateVisibility::Collapsed);
				}
			}
            
			// (可选) 设置颜色
			// FLinearColor Color = Config->GetTagColor(StatTag);
			// StatNameText->SetColorAndOpacity(Color);
		}
	}
}
