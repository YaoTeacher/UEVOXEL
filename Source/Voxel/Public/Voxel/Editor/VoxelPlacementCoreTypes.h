// 放置系统核心类型（不依赖 FVoxelEdit，避免循环包含）
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "VoxelPlacementCoreTypes.generated.h"

class AActor;

/**
 * 放置物体类型
 */
UENUM(BlueprintType)
enum class EPlacementType : uint8
{
    Empty       = 0 UMETA(DisplayName = "Empty", Hidden),
    Block       = 1 UMETA(DisplayName = "Block"),
    Furniture   = 2 UMETA(DisplayName = "Furniture"),
    Network     = 3 UMETA(DisplayName = "Network", Hidden),
};

/**
 * 放置操作类型
 */
UENUM(BlueprintType)
enum class EPlacementOp : uint8
{
    Place   = 0 UMETA(DisplayName = "Place"),
    Remove  = 1 UMETA(DisplayName = "Remove"),
    Modify  = 2 UMETA(DisplayName = "Modify"),
};

/**
 * 修改操作类型（家具用）
 */
UENUM(BlueprintType)
enum class EPlacementModificationType : uint8
{
    Rotate          = 0 UMETA(DisplayName = "Rotate"),
    Translate       = 1 UMETA(DisplayName = "Translate"),
    ReplaceType     = 2 UMETA(DisplayName = "Replace Type"),
};

/**
 * 朝向枚举
 */
UENUM(BlueprintType)
enum class EPlacementDirection : uint8
{
    Forward   = 0,
    Right     = 1,
    Backward  = 2,
    Left      = 3,
};

/**
 * 放置物体句柄
 */
USTRUCT(BlueprintType)
struct FPlacementHandle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
    EPlacementType Type = EPlacementType::Empty;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
    FIntVector BlockPos = FIntVector::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
    TWeakObjectPtr<AActor> Actor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
    int32 HandleId = 0;

    FPlacementHandle() = default;

    static FPlacementHandle MakeBlock(FIntVector InPos, int32 InHandleId = 0)
    {
        FPlacementHandle H;
        H.Type = EPlacementType::Block;
        H.BlockPos = InPos;
        H.HandleId = InHandleId;
        return H;
    }

    static FPlacementHandle MakeFurniture(AActor* InActor, int32 InHandleId = 0)
    {
        FPlacementHandle H;
        H.Type = EPlacementType::Furniture;
        H.Actor = InActor;
        H.HandleId = InHandleId;
        return H;
    }

    static FPlacementHandle MakeEmpty()
    {
        FPlacementHandle H;
        H.Type = EPlacementType::Empty;
        return H;
    }

    bool IsValid() const
    {
        return Type != EPlacementType::Empty
            && (Type != EPlacementType::Furniture || Actor.IsValid());
    }

    bool IsBlock() const { return Type == EPlacementType::Block; }
    bool IsFurniture() const { return Type == EPlacementType::Furniture; }

    FIntVector GetOrigin() const
    {
        if (IsBlock()) return BlockPos;
        return BlockPos;
    }
};

/**
 * 家具快照（用于历史记录）
 */
USTRUCT(BlueprintType)
struct FActorSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
    int32 HandleId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
    FIntVector Origin = FIntVector::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
    EPlacementDirection Direction = EPlacementDirection::Forward;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
    TArray<FIntVector> OccupiedCells;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
    int32 Quantity = 1;

    bool IsEmpty() const { return HandleId == 0; }
};

/**
 * 修改操作参数
 */
USTRUCT(BlueprintType)
struct FPlacementModification
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
    EPlacementModificationType ModType = EPlacementModificationType::Rotate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
    EPlacementDirection NewDirection = EPlacementDirection::Forward;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
    FIntVector Translation = FIntVector::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
    int32 NewHandleId = 0;

    static FPlacementModification MakeRotate(EPlacementDirection InDir)
    {
        FPlacementModification M;
        M.ModType = EPlacementModificationType::Rotate;
        M.NewDirection = InDir;
        return M;
    }

    static FPlacementModification MakeTranslate(FIntVector InDelta)
    {
        FPlacementModification M;
        M.ModType = EPlacementModificationType::Translate;
        M.Translation = InDelta;
        return M;
    }

    static FPlacementModification MakeReplace(int32 InNewHandleId)
    {
        FPlacementModification M;
        M.ModType = EPlacementModificationType::ReplaceType;
        M.NewHandleId = InNewHandleId;
        return M;
    }
};

/**
 * 放置请求（批量操作用）
 */
USTRUCT(BlueprintType)
struct FPlacementRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
    int32 HandleId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
    FIntVector Position = FIntVector::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
    EPlacementDirection Direction = EPlacementDirection::Forward;
};

/**
 * 选区（统一选择系统）
 */
USTRUCT(BlueprintType)
struct FPlacementSelection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
    TArray<FPlacementHandle> Handles;

    bool Add(const FPlacementHandle& InHandle)
    {
        if (!InHandle.IsValid()) return false;
        for (const FPlacementHandle& Existing : Handles)
        {
            if (InHandle.IsBlock() && Existing.IsBlock() && Existing.BlockPos == InHandle.BlockPos)
                return false;
            if (InHandle.IsFurniture() && Existing.IsFurniture() && Existing.Actor == InHandle.Actor)
                return false;
        }
        Handles.Add(InHandle);
        return true;
    }

    bool Remove(const FPlacementHandle& InHandle)
    {
        const int32 Num = Handles.Num();
        Handles.RemoveAll([&](const FPlacementHandle& H)
        {
            if (InHandle.IsBlock() && H.IsBlock()) return H.BlockPos == InHandle.BlockPos;
            if (InHandle.IsFurniture() && H.IsFurniture()) return H.Actor == InHandle.Actor;
            return false;
        });
        return Handles.Num() < Num;
    }

    void Clear() { Handles.Empty(); }
    bool IsEmpty() const { return Handles.Num() == 0; }
    int32 Num() const { return Handles.Num(); }

    bool IsSingleActor() const
    {
        return Handles.Num() == 1 && Handles[0].IsFurniture();
    }

    TArray<FIntVector> GetAllOccupiedCells() const
    {
        TArray<FIntVector> Cells;
        for (const FPlacementHandle& H : Handles)
        {
            if (H.IsBlock())
            {
                Cells.Add(H.BlockPos);
            }
        }
        return Cells;
    }
};
