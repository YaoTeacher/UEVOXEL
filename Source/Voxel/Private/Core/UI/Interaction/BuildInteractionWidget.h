// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "BuildInteractionWidget.generated.h"

/**
 * 
 */
class AVoxelGameMode;
struct FInteractableData;
class UTextBlock;
class UImage;
class UProgressBar;

UCLASS()
class UBuildInteractionWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	AVoxelGameMode* Gamemode;
	
	void UpdateWidget(FInteractableData InteractableData) const;
	
	UPROPERTY(VisibleAnywhere, Category="Hand Item Visual", meta=(BindWidget))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(VisibleAnywhere, meta = (BindWidget), Category = "Interaction Widget | Interactable Data")
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(VisibleAnywhere, meta = (BindWidget), Category = "Interaction Widget | Interactable Data")
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(VisibleAnywhere, meta = (BindWidget), Category = "Interaction Widget | Interactable Data")
	TObjectPtr<UTextBlock> QuantityText;


	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
};
