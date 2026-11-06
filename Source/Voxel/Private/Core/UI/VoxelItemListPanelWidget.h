// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Voxel/VoxelGameMode.h"
#include "VoxelItemListPanelWidget.generated.h"
class UBuildSlotWidget;
class UWrapBox;
class UButton;
/**
 * 
 */
UCLASS()
class UVoxelItemListPanelWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UWrapBox> BuildListWrapBox;

	UPROPERTY(BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UButton> FrontPageButton;

	UPROPERTY(BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UButton> NextPageButton;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UBuildSlotWidget> BuildSlotWidgetClass;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,meta=(BindWidget))
	int PageSize=8;
	UFUNCTION()
	void UpdateList();
	UFUNCTION()
	UBuildSlotWidget* CreateBuildSlot(FGameplayTag InputTag, UTexture2D* InputIcon, int InputQ);
	UFUNCTION(BlueprintCallable)
	void PageButtonActionCheck(int PageChangeIndex);
	// UFUNCTION()
	// void  ChangeListPageBack();
	// UFUNCTION()
	// void  ChangeListPageNext();

	UFUNCTION()
	void  ResetPage();
	//EVoxelControllerBuildState CurBuildType=EVoxelControllerBuildState::EditConstruction;
	int CurPageNum=0;

	int MaxPage=1;
};
