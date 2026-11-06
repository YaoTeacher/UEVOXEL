// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Data/Creature/CreatureEnums.h"
#include "DemandRowWidget.generated.h"

struct FGameplayTag;
struct FCreatureRuntimeState;
class UTextBlock;
class UProgressBar;
/**
 * 
 */
UCLASS()
class UDemandRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

	public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UTextBlock> DemandNameTextBox;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(BindWidget))
	TObjectPtr<UTextBlock> DemandValueTextBox;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UProgressBar> DemandValueChart;
public:
	UFUNCTION(BlueprintCallable)
	void UpdateWidgetData(float Cur, float Max, FGameplayTag DemandTag);

	virtual void NativePreConstruct() override;
};
