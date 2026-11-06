// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VoxelItemWorldSubsystem.generated.h"

struct FVoxelNormalItemBase;
enum class EActorDirection : uint8;
class AUVoxelItemBaseInstance;
// 前置声明
class UVoxelBlockWorldSubsystem;
class UVoxelAreaWorldSubsystem;
class UConfigDataSubsystem;

/**
 * 物品管理子系统 (Item Layer)
 * 职责：
 * 1. 管理所有放置在场景中的家具 Actor (Spawn/Destroy)
 * 2. 维护物理空间的占用索引 (Grid -> Actor)
 * 3. 协调 Area 和 Block 子系统
 */
UCLASS()
class UVoxelItemWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// =========================================================
	// [Core API] 核心操作
	// =========================================================

	/**
	 * 尝试在指定位置生成家具
	 * @param Position 世界网格坐标（从 TmpGeometry.Box.MinIndex 传入）
	 * @param Data 包含 ItemTag, Direction 等运行时数据
	 * @param bLoadFromSave 如果是读档模式，会跳过合法性检查(Collision Check)
	 * @return 生成成功的 Actor 指针，失败返回 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem")
	AUVoxelItemBaseInstance* SpawnItemAtGrid(const FIntVector& Position, const FInteractableData& Data, bool bLoadFromSave = false);

	/**
	 * 移除指定格子的家具
	 * @param GridPos 世界网格坐标
	 * @return 是否移除成功
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem")
	bool RemoveItemAtGrid(FIntVector GridPos);

	// =========================================================
	// [Modify API] 修改接口（旋转/移动/换型）
	// =========================================================

	/**
	 * 旋转家具到新朝向
	 * @param ItemActor 目标家具
	 * @param NewDirection 新朝向
	 * @return 是否成功（失败原因：新位置空间不足）
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem")
	bool RotateFurniture(AUVoxelItemBaseInstance* ItemActor, EActorDirection NewDirection);

	/**
	 * 平移家具到新位置
	 * @param ItemActor 目标家具
	 * @param DeltaPos 体素索引偏移
	 * @return 是否成功（失败原因：新位置空间不足）
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem")
	bool TranslateFurniture(AUVoxelItemBaseInstance* ItemActor, const FIntVector& DeltaPos);

	/**
	 * 替换家具类型（销毁旧 Actor + 生成新 Actor）
	 * @param ItemActor 目标家具
	 * @param NewItemId 新物品 ID
	 * @return 新生成的 Actor（失败返回 nullptr）
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem")
	AUVoxelItemBaseInstance* ReplaceFurnitureType(AUVoxelItemBaseInstance* ItemActor, int32 NewItemId);

	// =========================================================
	// [Query API] 查询接口
	// =========================================================

	/**
	 * 获取指定格子的家具
	 * @return 如果该格子被任何家具占据(包括占地多格的情况)，返回该 Actor
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemSystem")
	AUVoxelItemBaseInstance* GetItemAtGrid(FIntVector GridPos);

	/**
	 * 检查某块区域是否可放置 (空闲且无障碍)
	 * @param OriginPos 基准点
	 * @param Direction 朝向 (用于旋转 OccupiedBlocks)
	 * @param StaticData 物品静态数据
	 */
	bool CheckIsSpaceAvailable(FIntVector OriginPos, EActorDirection Direction, const FVoxelNormalItemBase* StaticData);

private:
	// =========================================================
	// [Internal] 内部逻辑
	// =========================================================

	// 辅助：计算一个物品实际占用的所有世界坐标
	// - 若 OccupiedBlocks 已配置：按配置+旋转计算（支持不规则形状）
	// - 若 OccupiedBlocks 为空：用 Volume 自动生成矩形占用（与包围盒预览一致）
	static TArray<FIntVector> CalculateOccupiedCells(FIntVector Origin, EActorDirection Dir, const FVoxelNormalItemBase* StaticData, float BlockLength = 20.0f);

private:
	// [核心] 空间索引表：记录每个被占用的格子属于哪个 Actor
	// Key: 世界网格坐标, Value: 占据该格子的 Actor
	UPROPERTY()
	TMap<FIntVector, AUVoxelItemBaseInstance*> GridToItemMap;

	// 缓存常用子系统
	UPROPERTY()
	UVoxelBlockWorldSubsystem* BlockSys;

	UPROPERTY()
	UVoxelAreaWorldSubsystem* AreaSys;

	UPROPERTY()
	UConfigDataSubsystem* ConfigSys;
};