// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "ItemSlotWidget.generated.h"

/**
 * 
 */


UCLASS()
class VOXEL_API UItemSlotWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category="Item Visual", meta=(BindWidget))
	TObjectPtr<UBorder> ItemBorder;

	UPROPERTY(VisibleAnywhere, Category="Item Visual", meta=(BindWidget))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(VisibleAnywhere, Category="Item Visual", meta=(BindWidget))
	TObjectPtr<UTextBlock> ItemQuantity;

	UFUNCTION(blueprintCallable, Category="Item Visual", meta=(bindWidget))
	void UpdateVisual(UTexture2D* Icon,int Quantity);
	
};
