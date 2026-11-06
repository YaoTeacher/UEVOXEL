// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelNpcBaseInstance.h"
#include "VoxelIntelligentCreatureInstance.generated.h"

class UConsumComponent;
UCLASS(BlueprintType, Blueprintable)
class AVoxelIntelligentCreatureInstance : public AVoxelNpcBaseInstance
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AVoxelIntelligentCreatureInstance();
	UPROPERTY(Blueprintable, EditAnywhere)
	int LiveTime =10;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "ConsumComponent")
	TObjectPtr<UConsumComponent> ConsumComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature")
	int32 MaxLiveTime = 600;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	UFUNCTION()
	void UpdateLiveTime();
	// 用于初始化组件数据 (保持你原有的命名)
	UFUNCTION(BlueprintCallable)
	void IntializeNpcSubData();

	//用于对象池复用时的重置逻辑
	UFUNCTION(BlueprintCallable)
	void ResetCreature();

	// [Modified] 不再直接 Destroy，而是请求移除
	UFUNCTION()
	void RequestDespawn();
	// 定时器句柄
	FTimerHandle LiveTimeTimerHandle;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
};
