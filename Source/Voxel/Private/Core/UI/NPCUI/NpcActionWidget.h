// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/NPC/VoxelIntelligentCreatureInstance.h"
#include "Core/UI/Base/VoxelScreenBase.h"
#include "NpcActionWidget.generated.h"
class UDemandChartWidget;
class UTextBlock;
class UImage;
/**
 * NPC 详情页（Screen 层），由 AVoxelHUD.OpenScreen 管理
 */
UCLASS()
class UNpcActionWidget : public UVoxelScreenBase
{
	GENERATED_BODY()
public:	
	UPROPERTY(VisibleAnywhere, Category="Hand Item Visual", meta=(BindWidget))
	TObjectPtr<UImage> NpcIcon;

	UPROPERTY(VisibleAnywhere, meta = (BindWidget), Category = "NpcWidget | Action")
	TObjectPtr<UTextBlock> NpcNameText;
	UPROPERTY(VisibleAnywhere, meta = (BindWidget), Category = "NpcWidget | Action")
	TObjectPtr<UTextBlock> CurAreaText;
	UPROPERTY(VisibleAnywhere, meta = (BindWidget), Category = "NpcWidget | Action")
	TObjectPtr<UTextBlock> CurTargetText;
	UPROPERTY(VisibleAnywhere, meta = (BindWidget), Category = "NpcWidget | Action")
	TObjectPtr<UTextBlock> CurActionText;
	UPROPERTY(VisibleAnywhere, meta = (BindWidget), Category = "NpcWidget | Action")
	TObjectPtr<UTextBlock> CurStateSequenceText;
	UPROPERTY(VisibleAnywhere, meta = (BindWidget), Category = "NpcWidget | Action")
	TObjectPtr<UTextBlock> CurCompositeBehaviorText;
	UPROPERTY(EditAnywhere, meta = (BindWidget), Category = "NpcWidget | Action")
	TObjectPtr<UDemandChartWidget> DemandChartWidget;

	
	UFUNCTION(BlueprintCallable)
	void InitializeNpcData(AVoxelIntelligentCreatureInstance* CurCreature);
	UFUNCTION(BlueprintCallable)
	void UpdateNpcData(FString AreaName);
	UFUNCTION(BlueprintCallable)
	void UpdateCurAreaData(FString AreaName);
	UFUNCTION(BlueprintCallable)
	void UpdateCurTargetData(FString AreaName);
	UFUNCTION(BlueprintCallable)
	void UpdateCurActionData(FString AreaName);
	UFUNCTION(BlueprintCallable)
	void UpdateCurStateSequenceData(FString StateSequence);
	UFUNCTION(BlueprintCallable)
	void UpdateCurCompositeBehaviorData(FString AreaName);
	
	virtual void NativeOnInitialized()override;
	virtual void NativeConstruct() override;
	
};
