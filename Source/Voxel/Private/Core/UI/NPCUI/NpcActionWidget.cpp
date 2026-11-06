// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/UI/NPCUI/NpcActionWidget.h"

#include "DemandChartWidget.h"
#include "Components/TextBlock.h"

void UNpcActionWidget::InitializeNpcData(AVoxelIntelligentCreatureInstance* CurCreature)
{
	// NpcNameText->SetText(FText::FromString(CurCreature->CreatureData.CreatureName.ToString()));
}

void UNpcActionWidget::UpdateNpcData(FString AreaName)
{
	
}

void UNpcActionWidget::UpdateCurAreaData(FString AreaName)
{
	CurAreaText->SetText(FText::FromString(AreaName));
}

void UNpcActionWidget::UpdateCurTargetData(FString CurTargetName)
{
	CurTargetText->SetText(FText::FromString(CurTargetName));
}

void UNpcActionWidget::UpdateCurActionData(FString CurActionName)
{
	CurActionText->SetText(FText::FromString(CurActionName));
}

void UNpcActionWidget::UpdateCurStateSequenceData(FString CurStateSequenceName)
{
	CurStateSequenceText->SetText(FText::FromString(CurStateSequenceName));
}

void UNpcActionWidget::UpdateCurCompositeBehaviorData(FString CurCompositeBehaviorName)
{
	CurCompositeBehaviorText->SetText(FText::FromString(CurCompositeBehaviorName));
}

void UNpcActionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UNpcActionWidget::NativeConstruct()
{
	
	Super::NativeConstruct();


}
