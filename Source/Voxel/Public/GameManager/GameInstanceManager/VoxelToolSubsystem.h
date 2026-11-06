// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VoxelToolSubsystem.generated.h"

class UVoxelToolBase;

/**
 * 全局工具注册表（GameInstance 子系统）
 *
 * 职责：管理"本游戏实例有哪些编辑工具"的静态能力，全局唯一实例。
 * 工具实例无状态（状态经 FVoxelToolContext 从 StateCache/Interaction/世界子系统传入），
 * 因此注册表是 GameInstance 级共享能力，不依赖任何 World。
 *
 * 生命周期：GameInstance 初始化时创建并注册全部工具，GameInstance 销毁时随子系统回收
 * （先于一切 World / Controller 初始化，天然消除"启动期注册表为空"时序问题）。
 */
UCLASS()
class VOXEL_API UVoxelToolSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// --- UGameInstanceSubsystem 接口 ---
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 按工具 Tag 查找工具实例
	 * @return 未注册的 Tag 返回 nullptr（调用方负责判空并报错）
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Tool")
	UVoxelToolBase* FindTool(const FGameplayTag& ToolTag) const;

	/** 全部已注册工具（供遍历查找能处理某 ItemTag 的工具） */
	const TMap<FGameplayTag, TObjectPtr<UVoxelToolBase>>& GetAllTools() const { return ToolRegistry; }

private:
	/** 注册全部编辑工具（Initialize 调用，先于一切 World 初始化） */
	void RegisterTools();

	/** 工具注册表：ToolTag → 工具实例（Outer=this，生命周期=GameInstance） */
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UVoxelToolBase>> ToolRegistry;
};
