// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/UI/NPCUI/DemandChartWidget.h"

#include "Components/DynamicEntryBox.h"
#include "Core/UI/DemandRowWidget.h"

void UDemandChartWidget::UpdateDemandValues(EDemandEnum InputEnum, float InputPercentValue)
{
	// if(DemandChartList.Contains(InputEnum))
	// {
	// 	//UE_LOG(LogTemp, Error, TEXT("UpdateDemandValues! "));
	// 	DemandChartList[InputEnum]->UpdateWidgetData(InputEnum, InputPercentValue);
	// }
	// else
	// {
	// 	UE_LOG(LogTemp, Error, TEXT("NoneDemandEnum!:%s" ),*UEnum::GetValueAsString(InputEnum));
	// }
	
}

void UDemandChartWidget::InitiaDemandValues()
{
	UEnum* EnumPtr = StaticEnum<EDemandEnum>();
	if (!EnumPtr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to find EDemandEnum!"));
		return;
	}

	// 遍历 EDemandEnum 的所有值
	for (int32 i = 0; i < EnumPtr->NumEnums()-1; i++)
	{
		// 获取枚举值的名称
		FString EnumName = EnumPtr->GetNameStringByIndex(i);

		// 获取枚举值的实际值
		int64 EnumValue = EnumPtr->GetValueByIndex(i);

		// 将枚举值转换为 EDemandEnum 类型
		EDemandEnum DemandEnum = static_cast<EDemandEnum>(EnumValue);
	InitiaDemandValue(DemandEnum);		

		

		


	}
}
void UDemandChartWidget::InitiaDemandValue(EDemandEnum DemandEnum)
{
	if(DemandChartList.Contains(DemandEnum))
	{
		DemandChartList[DemandEnum]=DemandDynamicEntryBox->CreateEntry(BarChartWidgetClass);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("NoneDemandEnum!:%s,creating!" ),*UEnum::GetValueAsString(DemandEnum));
		DemandChartList.Add(DemandEnum,DemandDynamicEntryBox->CreateEntry(BarChartWidgetClass));
	}
	
	UpdateDemandValues(DemandEnum,0);
}


void UDemandChartWidget::NativeConstruct()
{
	Super::NativeConstruct();
}
