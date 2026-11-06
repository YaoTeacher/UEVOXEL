// 放置操作历史记录（依赖 FVoxelEdit，故单独文件避免循环包含）
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Voxel/Editor/VoxelPlacementCoreTypes.h"  // FPlacementHandle / FActorSnapshot 等
#include "Voxel/Editor/VoxelHistoryData.h"          // FVoxelEdit
#include "VoxelPlacementTypes.generated.h"

/**
 * 放置操作（统一历史记录单元）
 * 一次 Place/Remove/Modify 对应一个 Operation
 * Block 操作填 BlockEdits，Furniture 操作填 BeforeSnapshot/AfterSnapshot
 */
USTRUCT(BlueprintType)
struct FPlacementOperation
{
    GENERATED_BODY()

    /** 操作类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    EPlacementOp OpType = EPlacementOp::Place;

    /** 操作的物体句柄 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    FPlacementHandle Handle;

    /** 工具标签（来源标识） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    FGameplayTag ToolTag;

    /** 时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation")
    FDateTime Timestamp = FDateTime::Now();

    // ===== Block 后端数据 =====

    /** Block 编辑列表（Place/Remove/Modify 的格子变化） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation|Block")
    TArray<FVoxelEdit> BlockEdits;

    // ===== Furniture 后端数据 =====

    /** 操作前的快照（Remove/Modify 用） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation|Furniture")
    FActorSnapshot BeforeSnapshot;

    /** 操作后的快照（Place/Modify 用） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operation|Furniture")
    FActorSnapshot AfterSnapshot;

    bool IsEmpty() const
    {
        return Handle.Type == EPlacementType::Empty;
    }
};
