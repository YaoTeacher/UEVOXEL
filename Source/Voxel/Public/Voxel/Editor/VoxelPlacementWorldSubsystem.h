// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Voxel/Editor/VoxelPlacementTypes.h"
#include "VoxelPlacementWorldSubsystem.generated.h"

class UVoxelBlockWorldSubsystem;
class UVoxelItemWorldSubsystem;
class UConfigDataSubsystem;
class AVoxelItemBaseInstance;
enum class EActorDirection : uint8;

/**
 * 放置服务子系统
 *
 * 统一入口，处理 Block（方块）和 Furniture（家具）的放置/删除/修改/查询。
 * 工具层和 NPC AI 都通过此子系统操作世界中的物体。
 *
 * 设计原则：
 * - 子系统只负责"执行操作"，不持有历史栈
 * - 历史记录由调用方决定（工具层记录到 HistoryComp，NPC 可不记录）
 * - 调用 GetLastOperation() 获取刚执行的操作详情，供调用方记录历史
 *
 * 后端分发：
 * - Block 后端 → VoxelBlockWorldSubsystem
 * - Furniture 后端 → VoxelItemWorldSubsystem
 * - Network 后端 → 未来扩展
 */
UCLASS()
class VOXEL_API UVoxelPlacementWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// =========================================================
	// [Placement] 单次放置
	// =========================================================

	/**
	 * 放置物体（自动识别 Block / Furniture）
	 * @param ItemId 物品 ID
	 * @param Pos 目标位置（体素索引）
	 * @param Direction 朝向（Furniture 用，Block 忽略）
	 * @return 放置成功的 Handle，失败返回 Empty
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Placement")
	FPlacementHandle Place(int32 ItemId, FIntVector Pos, EPlacementDirection Direction = EPlacementDirection::Forward);

	/**
	 * 删除物体
	 * @param Handle 物体句柄
	 * @return 是否删除成功
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Placement")
	bool Remove(const FPlacementHandle& Handle);

	/**
	 * 修改物体（旋转/移动/换型）
	 * @param Handle 物体句柄
	 * @param Modification 修改参数
	 * @return 是否修改成功
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Placement")
	bool ModifyPlacement(const FPlacementHandle& Handle, const FPlacementModification& Modification);

	// =========================================================
	// [Batch] 批量操作（NPC 建造场景预留）
	// =========================================================

	/**
	 * 批量放置（合并 Block 网格重建，性能友好）
	 * @param Requests 放置请求列表
	 * @return 放置成功的 Handle 列表
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Placement|Batch")
	TArray<FPlacementHandle> BatchPlace(const TArray<FPlacementRequest>& Requests);

	/**
	 * 批量删除
	 * @param Handles 物体句柄列表
	 * @return 成功删除的数量
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Placement|Batch")
	int32 BatchRemove(const TArray<FPlacementHandle>& Handles);

	// =========================================================
	// [Query] 查询
	// =========================================================

	/**
	 * 查询某格被谁占用
	 * 优先级：Actor（家具） > Block（方块）
	 * @param Pos 世界网格坐标
	 * @return 占用者 Handle，空格返回 Empty
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Placement|Query")
	FPlacementHandle QueryAt(FIntVector Pos) const;

	/**
	 * 能否放置
	 * @param ItemId 物品 ID
	 * @param Pos 目标位置
	 * @param Direction 朝向
	 * @return 是否可放置
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Placement|Query")
	bool CanPlaceAt(int32 ItemId, FIntVector Pos, EPlacementDirection Direction = EPlacementDirection::Forward) const;

	// =========================================================
	// [History] 历史支持
	// =========================================================

	/**
	 * 获取上次操作的详情（供调用方记录历史）
	 * @return 上次操作的引用
	 */
	const FPlacementOperation& GetLastOperation() const { return LastOperation; }

	/**
	 * 执行历史回滚（撤销）
	 * @param Op 要撤销的操作
	 * @return 是否成功
	 */
	bool ExecuteUndo(const FPlacementOperation& Op);

	/**
	 * 执行历史重做
	 * @param Op 要重做的操作
	 * @return 是否成功
	 */
	bool ExecuteRedo(const FPlacementOperation& Op);

	// =========================================================
	// [Backend Accessors] 后端访问器（供工具层便捷访问）
	// =========================================================

	UVoxelBlockWorldSubsystem* GetBlockWorld() const { return BlockSys; }
	UVoxelItemWorldSubsystem* GetItemWorld() const { return ItemWorld; }
	UConfigDataSubsystem* GetConfigData() const { return ConfigSys; }

private:
	// =========================================================
	// [Backend] 后端子系统
	// =========================================================

	UPROPERTY()
	TObjectPtr<UVoxelBlockWorldSubsystem> BlockSys;

	UPROPERTY()
	TObjectPtr<UVoxelItemWorldSubsystem> ItemWorld;

	UPROPERTY()
	TObjectPtr<UConfigDataSubsystem> ConfigSys;

	/** 上次操作缓存 */
	FPlacementOperation LastOperation;

	// =========================================================
	// [Internal] 内部分发
	// =========================================================

	/** 识别物品类型 */
	EPlacementType GetPlacementType(int32 ItemId) const;

	/** EPlacementDirection → EActorDirection 转换 */
	static EActorDirection ToActorDirection(EPlacementDirection Dir);
	static EPlacementDirection FromActorDirection(EActorDirection Dir);

	// --- Block 后端 ---
	FPlacementHandle PlaceBlock(int32 ItemId, FIntVector Pos);
	bool RemoveBlock(FIntVector BlockPos);
	bool ModifyBlock(FIntVector BlockPos, const FPlacementModification& Mod);

	// --- Furniture 后端 ---
	FPlacementHandle PlaceFurniture(int32 ItemId, FIntVector Pos, EPlacementDirection Direction);
	bool RemoveFurniture(AActor* Actor);
	bool ModifyFurniture(AActor* Actor, const FPlacementModification& Mod);

	/** 创建 Actor 快照 */
	FActorSnapshot MakeSnapshot(AActor* Actor) const;
};
