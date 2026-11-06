// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/NPCUI/EventHistoryRowWidget.h"
#include "Data/Creature/CreatureBase.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Utils/CreatureGeneratorUtils.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"


void UEventHistoryRowWidget::UpdateEventData(const FLifeHistoryEntry& Entry, UConfigDataSubsystem* ConfigSubsystem, const FString& CreatureName)
{
    if (!ConfigSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("UEventHistoryRowWidget::UpdateEventData - ConfigSubsystem is null"));
        return;
    }

    // 1. 更新年龄显示
    if (AgeText)
    {
        AgeText->SetText(FText::Format(NSLOCTEXT("EventHistory", "AgeFormat", "{0}岁"), Entry.OccuredAge));
    }

    // 2. 更新事件描述 (优先使用快照，防止资产修改导致历史文本变化)
    if (EventDescriptionText)
    {
        // 这里你原本调用的 ResolveHistoryText 其实就是返回 SnapshotOutcomeText
        FText EventText = UCreatureGeneratorUtils::ResolveHistoryText(Entry, CreatureName, ConfigSubsystem);
        EventDescriptionText->SetText(EventText);
    }

    // 3. [核心] 获取 Outcome 对象并更新结果信息
    if (EventResultText)
    {
        FText ResultInfoText = FText::GetEmpty();

        // A. 加载事件资产 (SoftObjectPtr 需要 LoadSynchronous 或 Get 才能使用)
        UCreatureEvent* EventAsset = Entry.SourceEvent.LoadSynchronous(); 
        UEventOutcomeBase* TargetOutcome = nullptr;

        if (EventAsset)
        {
            // B. 根据索引查找对应的 Outcome
            if (Entry.SelectedChoiceIndex >= 0)
            {
                // 玩家做了选择 (Choices)
                if (EventAsset->Choices.IsValidIndex(Entry.SelectedChoiceIndex))
                {
                    TargetOutcome = EventAsset->Choices[Entry.SelectedChoiceIndex].Outcome;
                }
            }
            else
            {
                // 默认结果 (DefaultOutcome)
                TargetOutcome = EventAsset->DefaultOutcome;
            }
        }

        // C. 如果找到了 Outcome，解析其内容
        if (TargetOutcome)
        {
            // 情况 1: 瞬间结算 (获得特质)
            if (UEventOutcome_Instant* InstantOutcome = Cast<UEventOutcome_Instant>(TargetOutcome))
            {
                // 遍历配置中给予的所有特质
                FString AllTraitsStr;
                for (const FGameplayTag& Tag : InstantOutcome->GrantTraits)
                {
                    FText TagName = ConfigSubsystem->GetTagDisplayName(Tag);
                    if (!AllTraitsStr.IsEmpty()) AllTraitsStr += TEXT(", ");
                    AllTraitsStr += TagName.ToString();
                }

                if (!AllTraitsStr.IsEmpty())
                {
                    ResultInfoText = FText::Format(NSLOCTEXT("EventHistory", "GainedFormat", "获得：{0}"), FText::FromString(AllTraitsStr));
                }
            }
            // 情况 2: 任务 (Task)
            else if (UEventOutcome_Task* TaskOutcome = Cast<UEventOutcome_Task>(TargetOutcome))
            {
                // 可以显示任务ID或去查任务名字
                ResultInfoText = FText::Format(NSLOCTEXT("EventHistory", "TaskFormat", "触发任务：{0}"), FText::FromString(TaskOutcome->TaskParams));
            }
            // 情况 3: 玩家任务 (Quest)
            else if (UEventOutcome_Quest* QuestOutcome = Cast<UEventOutcome_Quest>(TargetOutcome))
            {
                ResultInfoText = FText::Format(NSLOCTEXT("EventHistory", "QuestFormat", "发布委托：{0}"), QuestOutcome->ResultDescription);
            }
            // 情况 4: 仅剧情文本，无特殊系统行为
            else
            {
                // 如果没有系统行为，可以留空，或者显示 "事件结束"
            }
        }
        
        // D. 兜底：如果 Outcome 解析失败，尝试使用 Entry 里缓存的 GainedTraitTag
        if (ResultInfoText.IsEmpty() && Entry.GainedTraitTag.IsValid())
        {
            FString TraitName = ConfigSubsystem->GetTagDisplayName(Entry.GainedTraitTag).ToString();
            ResultInfoText = FText::Format(NSLOCTEXT("EventHistory", "GainedFormat", "获得：{0}"), FText::FromString(TraitName));
        }

        // 设置最终文本
        EventResultText->SetText(ResultInfoText);
    }

    // 4. 确保分割线可见
    if (Separator)
    {
        Separator->SetVisibility(ESlateVisibility::Visible);
    }
}