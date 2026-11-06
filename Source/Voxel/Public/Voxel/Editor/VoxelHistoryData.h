// 历史记录系统数据结构
// 两段式存储设计: HistoryRecord (存储层) → Modification (执行层)

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "VoxelEditorData.h"
#include "Core/Enum/GameStateEnum.h"
#include "Data/VoxelCutoutData.h"
#include "Voxel/Editor/VoxelPlacementCoreTypes.h"  // FActorSnapshot / EPlacementOp（不依赖 FVoxelEdit）
#include "VoxelHistoryData.generated.h"

/**
 * 单个方块的编辑操作（包含前后状态）
 *
 * 用途：
 * - 作为 TSet 的元素，自动去重（只比较 BlockIndex）
 * - 记录完整的编辑信息（位置 + 操作前 + 操作后）
 */
USTRUCT(BlueprintType)
struct FVoxelEdit
{
    GENERATED_BODY()

    /** 方块位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Edit")
    FIntVector BlockIndex = FIntVector::ZeroValue;

    /** 操作前的句柄 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Edit")
    int32 OldHandleId;

    /** 操作后的句柄 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Edit")
    int32 NewHandleId;

    FVoxelEdit() = default;

    FVoxelEdit(const FIntVector& InBlockIndex,
               int32 InOldHandleId,
               int32 InNewHandleId)
        : BlockIndex(InBlockIndex)
        , OldHandleId(InOldHandleId)
        , NewHandleId(InNewHandleId)
    {}

    // === TSet 需要的函数 ===

    /** 相等性判断（只比较 BlockIndex，用于 TSet 去重） */
    bool operator==(const FVoxelEdit& Other) const
    {
        return BlockIndex == Other.BlockIndex;
    }

    /** 辅助方法 */
    bool IsChanged() const { return OldHandleId != NewHandleId; }

    FString ToString() const
    {
        return FString::Printf(TEXT("(%s) %d → %d"),
            *BlockIndex.ToString(),
            OldHandleId,
            NewHandleId);
    }
};

/** TSet 哈希函数 */
inline uint32 GetTypeHash(const FVoxelEdit& Edit)
{
    return GetTypeHash(Edit.BlockIndex);
}

/**
 * 完整的编辑历史记录（存储层）
 *
 * 职责：
 * - 存储在 UndoStack/RedoStack 中
 * - 包含操作前后的完整状态
 * - 提供转换方法：CreateUndoModification() / CreateRedoModification()
 *
 * 数据结构：
 * - TSet<FVoxelEdit> Edits - 自动去重的编辑集合
 * - TArray<FIntVector> ChunkIndices - 受影响的区块（去重）
 */
USTRUCT(BlueprintType)
struct FVoxelEditHistoryRecord
{
    GENERATED_BODY()

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true), Category = "Voxel|History|Record")
    TSet<FVoxelEdit> Edits;

    

public:
    // === 编辑模式（已废弃，使用 FVoxelEditCommand） ===

    // === 工具类型 ===

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record")
    FGameplayTag ToolTag = FGameplayTag::EmptyTag;

    /** 编辑命令（放置模式等）—— Redo 时透传，Undo 时强制 Normal */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record")
    FVoxelEditCommand EditCommand = FVoxelEditCommand::BuildNormal();

    // === 元数据 ===

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true), Category = "Voxel|History|Record")
    TArray<FIntVector> ChunkIndices;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record")
    FDateTime Timestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record")
    FDateTime DoneTimestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record")
    bool bCanBeUndone = true;

    // === 家具操作数据（统一历史：Block 和 Furniture 共用同一栈） ===

    /** 操作类型（Block 编辑默认 Modify，家具为 Place/Remove/Modify） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record|Furniture")
    EPlacementOp PlacementOpType = EPlacementOp::Modify;

    /** 家具操作前的快照（Block 操作留空） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record|Furniture")
    FActorSnapshot BeforeSnapshot;

    /** 家具操作后的快照（Block 操作留空） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record|Furniture")
    FActorSnapshot AfterSnapshot;

    /** 是否是家具操作（用于 Undo/Redo 分发） */
    bool IsFurnitureOperation() const { return !BeforeSnapshot.IsEmpty() || !AfterSnapshot.IsEmpty(); }

    // === 透明区域框操作数据（统一历史第三类：与 Block/Furniture 共用同一栈） ===

    /** 是否是透明框操作（Add/Remove/Update 几何；生效开关不入历史） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record|Cutout")
    bool bIsCutoutOp = false;

    /** 框 Id（始终为有效框的 Id；用于 Undo/Redo 定位） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record|Cutout")
    int32 CutoutBoxId = INDEX_NONE;

    /** 操作前框状态（Id==INDEX_NONE 表示操作前不存在，如新建） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record|Cutout")
    FCutoutBoxData CutoutBefore;

    /** 操作后框状态（Id==INDEX_NONE 表示操作后不存在，如删除） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Record|Cutout")
    FCutoutBoxData CutoutAfter;

    /** 是否是透明框操作（用于 Undo/Redo 分发） */
    bool IsCutoutOperation() const { return bIsCutoutOp; }

    // === 转换方法（存储层 → 执行层）===

    /**
     * 创建 Undo 修改模型（使用 OldType）
     *
     * 性能：O(n)，n = Edits.Num()
     * 优化：Reserve() 预分配，避免数组扩容
     */
    FVoxelEditModification CreateUndoModification() const;

    /**
     * 创建 Redo 修改模型（使用 NewType）
     *
     * 性能：O(n)，n = Edits.Num()
     * 优化：Reserve() 预分配，避免数组扩容
     */
    FVoxelEditModification CreateRedoModification() const;

    // === 访问器 ===

    int32 Num() const { return Edits.Num(); }
    bool IsEmpty() const { return Edits.Num() == 0; }
    const TSet<FVoxelEdit>& GetEdits() const { return Edits; }
    const TArray<FIntVector>& GetChunkIndices() const { return ChunkIndices; }

    // === 修改方法 ===

    /** 预分配空间（性能优化） */
    void Reserve(int32 Number)
    {
        Edits.Reserve(Number);
        ChunkIndices.Reserve(Number / 64); // 假设每 64 个方块一个 Chunk
    }

    /** 添加单个编辑项（自动去重） */
    void Add(const FVoxelEdit& Edit)
    {
        Edits.Add(Edit);
    }

    /** 添加 Chunk 索引（自动去重） */
    void AddChunkIndex(const FIntVector& ChunkIndex)
    {
        ChunkIndices.AddUnique(ChunkIndex);
    }

    // === 辅助方法 ===

    int32 GetAffectedVoxelCount() const { return Edits.Num(); }

    /**
     * 检查历史记录是否有效
     */
    bool IsValid() const
    {
        return Edits.Num() > 0;
    }

    /**
     * 获取调试字符串
     */
    FString GetDebugString() const
    {
        return FString::Printf(TEXT("HistoryRecord: %d edits, ToolTag=%s"),
            Edits.Num(),
            *ToolTag.ToString());
    }
};
