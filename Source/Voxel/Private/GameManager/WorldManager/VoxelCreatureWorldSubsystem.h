// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/Creature/CreatureEnums.h"
#include "Subsystems/WorldSubsystem.h"
#include "VoxelCreatureWorldSubsystem.generated.h"

class AVoxelIntelligentCreatureInstance;
class AVoxelNpcBaseInstance;
/**
 * 
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class UVoxelCreatureWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Subsystem 接口重写 ---
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	// Tickable 接口

	// --- 核心功能 ---
	UFUNCTION(BlueprintCallable, Category = "Voxel|Creature")
	void StartSpawnTimer();

	UFUNCTION(BlueprintCallable, Category = "Voxel|Creature")
	void StopSpawnTimer();

	// 定时器回调函数
	void OnSpawnTimerTick();

	// 尝试生成单个生物
	UFUNCTION(BlueprintCallable, Category = "Voxel|Creature")
	void SpawnCreature();

	// 移除并回收生物
	UFUNCTION(BlueprintCallable, Category = "Voxel|Creature")
	void RemoveCreature(int CreatureID);

	UFUNCTION(BlueprintCallable, Category = "Voxel|Creature")
	void RemoveAllCreature();

	// [Helper] 注册对象池
	void RegisterCreaturePool();
	
	UFUNCTION(BlueprintCallable, Category = "Voxel|Creature")
	FTransform SetCreatureSpawnTransform(int SpawnAreaId) const;
	
	static FVector GetRandomPositionInArea(FIntVector Min, FIntVector Max);

	UFUNCTION(BlueprintCallable, Category = "Voxel|Creature")
	void AddCreature(FName CreatureId);

	/**
	 * 根据 CreatureID 获取场景中的生物实例
	 * @param CreatureId 生物 ID
	 * @return 生物实例指针（无效时返回 nullptr）
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Creature")
	AVoxelNpcBaseInstance* GetCreatureById(int32 CreatureId) const;

	UFUNCTION(BlueprintCallable, Category = "Voxel|Creature")
	TArray<int32> GetAllCreatureIds() const;

	/**
	 * 获取指定用户可管理的生物列表（用于区域编辑表单下拉选择）
	 * - 预留筛选接口，当前直接返回全部在场生物
	 * - 后续可在此加入职业/权限/区域归属等过滤逻辑
	 * @param UserId 当前操作者 CreatureId
	 * @return 生物实例数组（已按 CreatureId 排序）
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Creature")
	TArray<AVoxelNpcBaseInstance*> GetSelectableCreaturesForUser(int32 UserId) const;

	/**
	 * 统一的 CreatureId → 显示名称解析（全工程唯一实现，UI 各处只调不写）
	 * 分区语义：0 = 系统角色；负值 = 未指派；玩家主角 = 「玩家」；
	 * 其余查在场生物 RealName，查不到显示 #Id
	 *
	 * 边界约定：仅「持有 CreatureId 需要转名称」的场景必须走本入口；
	 * 生成预览类 UI（如 CreatureInfromWidget）手持 FCreatureGenerationResult.TrueData.RealName
	 * 时可直接显示，无需绕道本接口
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Creature")
	FText ResolveCreatureName(int32 CreatureId) const;

	// --- 属性配置 ---
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI/NPC")
	int CreatureNumLimit = 10; // 给个默认值防止未初始化

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI/NPC")
	TSubclassOf<AVoxelNpcBaseInstance> CreatureClass;

	// 静态需求枚举列表
	static TArray<EDemandEnum> DemandEnumlist;

protected:
	void InitializeDemandEnum();

	bool SetSpawnId(AVoxelIntelligentCreatureInstance* Input);

private:
	// 当前管理的生物列表
	UPROPERTY()
	TMap<int, TWeakObjectPtr<AVoxelNpcBaseInstance>> InScenceCreatureList;
	// 生成控制定时器
	FTimerHandle SpawnTimerHandle;
};
