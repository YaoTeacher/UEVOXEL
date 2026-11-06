// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "VoxelToolContext.generated.h"

class UVoxelInteractionComponent;
class UVoxelPlayerStateCacheComponent;
class UVoxelPreviewComponent;
class UVoxelControllerHistoryComponent;
class AVoxelPlayerController;
class AVoxel3DUIActor;
class UVoxelBlockWorldSubsystem;
class UVoxelItemWorldSubsystem;
class UVoxelAreaWorldSubsystem;
class UConfigDataSubsystem;
class UVoxelGameInstance;
class UWorld;

/**
 * 输入状态（由 InteractionComp 维护，工具通过引用读写）
 * 替代原 InteractionComp 私有的 FirstPoint/SecondPoint/bIsModyfing 等
 */
USTRUCT(BlueprintType)
struct FVoxelInputState
{
	GENERATED_BODY()

	/** 第一个点（世界坐标） */
	FVector FirstPoint = FVector::ZeroVector;

	/** 第二个点（世界坐标） */
	FVector SecondPoint = FVector::ZeroVector;

	/** 是否正在修改（拖拽编辑中） */
	bool bIsModifying = false;

	/** Brush 工具拖拽中（点击进入涂抹，松开结束） */
	bool bIsBrushDragging = false;

	/** Brush 上次应用的体素索引（避免同一体素重复 stamp） */
	FIntVector LastBrushStampIndex = FIntVector::NoneValue;

	/** 当前正在拖拽的 Gizmo */
	UPROPERTY()
	TObjectPtr<AVoxel3DUIActor> ActiveDragGizmo = nullptr;
};

/**
 * 工具运行上下文
 * 每次输入事件由 InteractionComp 组装并传给工具
 */
USTRUCT(BlueprintType)
struct FVoxelToolContext
{
	GENERATED_BODY()

	/** 交互组件（输入分发者） */
	UPROPERTY()
	TObjectPtr<UVoxelInteractionComponent> InteractionComp = nullptr;

	/** 玩家状态缓存组件（我选了什么/我拿了什么/我调了什么参数的唯一数据源） */
	UPROPERTY()
	TObjectPtr<UVoxelPlayerStateCacheComponent> StateCacheComp = nullptr;

	/** 视觉 UI 组件 */
	UPROPERTY()
	TObjectPtr<UVoxelPreviewComponent> PreviewComp = nullptr;

	/** 历史组件 */
	UPROPERTY()
	TObjectPtr<UVoxelControllerHistoryComponent> HistoryComp = nullptr;

	/** 玩家控制器 */
	UPROPERTY()
	TObjectPtr<AVoxelPlayerController> PlayerController = nullptr;

	/** 输入状态（可读写） */
	FVoxelInputState* InputState = nullptr;

	/** 射线检测距离 */
	float TraceDistance = 5000.0f;

	// ===== 便捷访问器 =====

	VOXEL_API UWorld* GetWorld() const;
	VOXEL_API UVoxelBlockWorldSubsystem* GetBlockWorld() const;
	VOXEL_API UVoxelItemWorldSubsystem* GetItemWorld() const;
	VOXEL_API UVoxelAreaWorldSubsystem* GetAreaWorld() const;
	VOXEL_API UConfigDataSubsystem* GetConfigData() const;
	VOXEL_API UVoxelGameInstance* GetGameInstance() const;
	VOXEL_API float GetBlockLength() const;
};
