// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/UI/NPCUI/CreatureBaseInformWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Data/Creature/CreatureBase.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"

void UCreatureBaseInformWidget::UpdateCreatureInfo(const FTrueCreatureData& Data)
{
	// 1. 获取配置子系统 (用于查名字)
	UConfigDataSubsystem* ConfigSubsystem = nullptr;
	if (GetGameInstance())
	{
		ConfigSubsystem = GetGameInstance()->GetSubsystem<UConfigDataSubsystem>();
	}

	// 2. 更新简单的种族/职业文本 (这里也可以用 GetTagDisplayName 查表)
	if (ConfigSubsystem)
	{
		TrueNameText->SetText(FText::FromString(Data.RealName));
		TrueAgeText->SetText(FText::AsNumber(Data.CurrentAge));
		TrueGenderText->SetText(ConfigSubsystem->GetTagDisplayName(Data.GenderTag));
		RaceNameText->SetText(ConfigSubsystem->GetTagDisplayName(Data.RaceTag));
		ProfNameText->SetText(ConfigSubsystem->GetTagDisplayName(Data.ProfessionTag));
	}

	// 3. [核心] 更新特质列表
	if (TraitsContainer && TraitChipClass)
	{
		TraitsContainer->ClearChildren(); // 先清空旧的

		// 遍历所有特质 Tag
		for (const FGameplayTag& TraitTag : Data.TraitTags)
		{
			// 创建子控件
			UTagChipWidget* ChipWidget = CreateWidget<UTagChipWidget>(this, TraitChipClass);
            
			if (ChipWidget)
			{
				
				// Cast<UTraitChipWidget>(ChipWidget)->InitTag(TraitTag);
				ChipWidget->TagNameText->SetText(ConfigSubsystem->GetTagDisplayName(TraitTag));
				ChipWidget->TagImage->SetBrushTintColor(ConfigSubsystem->GetTagColor(TraitTag));
				// 将子控件加入 WrapBox
				TraitsContainer->AddChildToWrapBox(ChipWidget);
			}
		}
	}
}
