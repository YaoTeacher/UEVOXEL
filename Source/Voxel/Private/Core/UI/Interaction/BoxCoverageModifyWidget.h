// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Voxel/VoxelPlayerController.h"
#include "BoxCoverageModifyWidget.generated.h"
class UTextBlock;
class UButton;
class UVoxelPlayerStateCacheComponent;
/**
 * 
 */
UCLASS()
class UBoxCoverageModifyWidget : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Build")
	FBox BoxData=FBox(FVector(0),FVector(0));

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Build")
	FIntVector CoreOffset=FIntVector(0);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Build")
	FIntVector MultiNum = FIntVector(1);

	UPROPERTY()
	UVoxelPlayerStateCacheComponent* StateComponent;

	
	
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> ZminAButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> ZminDButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> ZmaxAButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> ZmaxDButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> XminAButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> XminDButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> XmaxAButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> XmaxDButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> YminAButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> YminDButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> YmaxAButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> YmaxDButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UTextBlock> XMinText;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UTextBlock> XMaxText;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UTextBlock> YMinText;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UTextBlock> YMaxText;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UTextBlock> ZMinText;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UTextBlock> ZMaxText;

	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UTextBlock> CoreOffsetXText;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UTextBlock> CoreOffsetYText;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UTextBlock> CoreOffsetZText;

	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> CoreOffsetXAButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> CoreOffsetXDButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> CoreOffsetYAButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> CoreOffsetYDButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> CoreOffsetZAButton;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UButton> CoreOffsetZDButton;


	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UTextBlock> XMultiText;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UTextBlock> YMultiText;
	UPROPERTY(VisibleAnywhere,meta = (BindWidget), Category = "BatchUI")
	TObjectPtr<UTextBlock> ZMultiText;
	UFUNCTION()
	FBox GetCurBox();
	UFUNCTION(BlueprintCallable)
	void UpdateBatchBlockInform();
	void ResetCoreOffset();
	UFUNCTION(BlueprintCallable)
	virtual void ChangeCurCoverageByClick(bool IsMin,FVector Input);
	UFUNCTION(BlueprintCallable)
	virtual void ChangeCurCoreByClick(FIntVector Input);
	UFUNCTION(BlueprintCallable)
	virtual void ChangeCurCoverageByBox(FAreaCoverageFragment C);

	UFUNCTION(BlueprintCallable)
	virtual void ChangeMulNum(int ChangeIndex);
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
};

