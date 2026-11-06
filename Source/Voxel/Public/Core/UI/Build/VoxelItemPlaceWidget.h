// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "VoxelItemPlaceWidget.generated.h"

class UTextBlock;
class UDynamicEntryBox;
class UButton;
class AVoxelPlayerController;
class UItemSlotWidget;
/**
 * 
 */
UCLASS()
class VOXEL_API UVoxelItemPlaceWidget : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UItemSlotWidget> ItemInHand;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UTextBlock> DirTextBox;
	UPROPERTY(BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UButton> TurnLeftButton;
	UPROPERTY(BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UButton> TurnRightButton;
	UPROPERTY(BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UButton> PlaceButton;
	UPROPERTY(BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UDynamicEntryBox> SubTypeBox;

	
	UFUNCTION(BlueprintCallable)
	void UpdateItemIcon() const;
	UFUNCTION(BlueprintCallable)
	void UpdateCurItemDir(bool IsTurnLeft) const;
	


	
};
