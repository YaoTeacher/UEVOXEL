// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Data/Creature/CreatureEnums.h"
#include "DemandChartWidget.generated.h"

class UDemandRowWidget;
class UDynamicEntryBox;
/**
 * 
 */
UCLASS()
class UDemandChartWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UDynamicEntryBox> DemandDynamicEntryBox;
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TSubclassOf<UDemandRowWidget> BarChartWidgetClass;
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TMap<EDemandEnum,UDemandRowWidget*> DemandChartList;

	UFUNCTION(BlueprintCallable)
	void UpdateDemandValues(EDemandEnum InputEnum,float InputPercentValue);
	UFUNCTION(BlueprintCallable)
	void InitiaDemandValues();
	void InitiaDemandValue(EDemandEnum DemandEnum);

	virtual void NativeConstruct() override;
};
