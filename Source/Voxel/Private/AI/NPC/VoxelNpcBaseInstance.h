// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/Creature/CreatureBase.h"
#include "GameFramework/Character.h"
#include "VoxelNpcBaseInstance.generated.h"

struct FCreatureGenerationResult;
class UDemandChartWidget;
class UWidgetComponent;

UCLASS()
class AVoxelNpcBaseInstance : public ACharacter
{
	GENERATED_BODY()

public:
	AVoxelNpcBaseInstance();

	// --- 核心数据 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Creature")
	FTrueCreatureData CreatureData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RPG Data")
	FCreatureRuntimeState RuntimeState;

	// --- 引用快捷方式 ---
	// 指向 ACharacter 自带的 Mesh 组件，作为我们的"躯干/核心"
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* MainBodyMesh;

	// --- 核心接口 ---

	// [对象池/生成] 强制唤醒并重置物理状态
	UFUNCTION(BlueprintCallable, Category = "Lifecycle")
	void ForceWakeUp(const FTransform& SpawnTransform);

	// [生成] 注入数据并触发组装
	UFUNCTION(BlueprintCallable, Category = "Lifecycle")
	void InitializeFromGenerationResult(const FCreatureGenerationResult& GenResult);

	// [视觉] 重建身体 (核心组装逻辑)
	UFUNCTION(BlueprintCallable, Category = "Visual")
	void RebuildBodyVisuals();

	// [视觉] 清理所有动态生成的部件 (用于重置或销毁前)
	UFUNCTION(BlueprintCallable, Category = "Visual")
	void ClearBodyVisuals();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
