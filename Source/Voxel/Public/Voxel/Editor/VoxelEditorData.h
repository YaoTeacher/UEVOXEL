// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/Enum/GameStateEnum.h"
#include "core/CoreTags.h"
#include "Core/Enum/EditCommandEnums.h"
#include "Data/VoxelEditCommand.h"
#include "Data/Area/VoxelAreaBase.h"
#include "VoxelEditorData.generated.h"

/**
 * 编辑确认类型
 */
UENUM(BlueprintType)
enum class EEditConfirmType : uint8
{
	Immediate      UMETA(DisplayName = "立即执行"),
	RequireConfirm UMETA(DisplayName = "需要确认")
};

/**
 * 体素编辑描述 - 完整的编辑请求描述
 * 包含几何信息和功能参数
 * 整个系统统一使用的请求模型
 */
USTRUCT(BlueprintType)
struct FVoxelGeometryDesc
{
	GENERATED_BODY()

	// === 几何信息 ===

	/** 体素索引空间框 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Geometry")
	FAreaCoverageFragment Box = FAreaCoverageFragment();

	/** 旋转（可选，用于非轴对齐的编辑） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Geometry")
	FRotator Rotation = FRotator::ZeroRotator;
	
	/** * 是否限制为规整几何体 
	 * Box -> 正方体
	 * Sphere -> 正球体 (否则为椭球)
	 * Cylinder -> 正圆底圆柱 (否则为椭圆底)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Params")
	bool bIsRegular = false;

	// /** 缩放（可选，默认为单位缩放） */
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Geometry")
	// FVector Scale = FVector::OneVector;

	// === 形状参数 ===

	/** 形状类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Params")
	EBatchShapeType ShapeType = EBatchShapeType::Null;

	/** 厚度（用于中空形状，0 表示实心；正=向内壳（挖空内芯），负=向外壳（外扩）） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Params")
	int32 Thickness = 0;

	/**
	 * 壳体面子集：空集合 = 全部面（默认，兼容旧行为）；仅壳模式（Thickness != 0）生效
	 * 面归属按形状定义：
	 * - Box：slab 并集（体素局部坐标进入某面条带即归属该面，角柱归相邻面共有）
	 * - Sphere/Cylinder：dominant axis 扇区（局部坐标取最大分量轴+符号；圆柱 Z=顶/底盖，水平=侧壁四扇区）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Params")
	TSet<EActorDirection> ShellFaces;

	// === 变换原点 ===

	/** 变换原点模式 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Transform")
	EVoxelPivotMode PivotMode = EVoxelPivotMode::ObjectCenter;

	/** 坐标空间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Transform")
	EVoxelTransformSpace TransformSpace = EVoxelTransformSpace::Global;

	/** 游标世界坐标（仅 Cursor 模式有效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Transform")
	FVector PivotWorld = FVector::ZeroVector;

	// === 操作方法 ===

	/**
	 * 移动整个Geometry
	 * @param Delta 移动增量（体素索引）
	 * @param BlockLength 体素边长（米）
	 * 影响: Box.MinIndex, Box.MaxIndex
	 */
	void MoveGEO(FIntVector Delta);

	/**
	 * 修改包围盒形状
	 * @param Axis 轴向
	 * @param bMaxSide 是否为最大面
	 * @param Delta 偏移量
	 * 影响: Box.MinIndex 或 Box.MaxIndex 的对应分量
	 * 约束: MinIndex <= MaxIndex
	 */
	bool ModifyCoverage(EAxis::Type Axis, bool bMaxSide, int32 Delta);

	/**
	 * 旋转Geometry
	 * @param DeltaRotation 旋转增量
	 * 影响: Rotation
	 */
	void RotateGEO(FRotator DeltaRotation);

	// === 访问器方法 ===

	/**
	 * 获取几何中心（未旋转的包围盒中心）
	 * @return 几何中心（世界坐标）
	 */
	FVector GetGeometricCenter(float BlockLength) const
	{
		FVector MinPos = FVector(Box.MinIndex + Box.CoreOffsetIndex);
		FVector MaxPos = FVector(Box.MaxIndex + Box.CoreOffsetIndex) + FVector(1.0f);
		return (MinPos + MaxPos) * 0.5f * BlockLength;
	}

	/**
	 * 获取变换原点
	 * ObjectCenter 模式：返回几何中心
	 * Cursor 模式：返回游标位置
	 */
	FVector GetPivotWorld(float BlockLength) const
	{
		return (PivotMode == EVoxelPivotMode::Cursor)
			? PivotWorld
			: GetGeometricCenter(BlockLength);
	}

	/**
	 * 获取Box实际大小
	 * @param BlockLength 体素边长（单位：米）
	 * @return 世界空间包围盒
	 */
	FBox GetBound(float BlockLength) const
	{
		const FVector min = FVector(Box.MinIndex + Box.CoreOffsetIndex) * BlockLength;
		const FVector max = FVector(Box.MaxIndex + Box.CoreOffsetIndex + FIntVector(1,1,1)) * BlockLength;
		return FBox(min, max);
	}

	/**
	 * 获取体素空间的尺寸（体素数量）
	 * @return 体素尺寸（绝对值）
	 */
	FIntVector GetSize() const
	{
		return Box.MaxIndex - Box.MinIndex + FIntVector(1,1,1);
	}

	/**
	 * 获取世界空间的尺寸
	 * @param BlockLength 体素边长（单位：米）
	 * @return 世界空间尺寸
	 */
	FVector GetWorldSize(float BlockLength) const
	{
		return FVector(GetSize()) * BlockLength;
	}

	/**
	 * 获取包围盒extents（一半尺寸）
	 * @param BlockLength 体素边长（单位：米）
	 * @return Extents（世界坐标）
	 */
	FVector GetExtents(float BlockLength) const
	{
		return FVector(
			(Box.MaxIndex.X - Box.MinIndex.X + 1) * 0.5f,
			(Box.MaxIndex.Y - Box.MinIndex.Y + 1) * 0.5f,
			(Box.MaxIndex.Z - Box.MinIndex.Z + 1) * 0.5f
		) * BlockLength;
	}

	/**
	 * 获取完整的变换
	 * @param BlockLength 体素边长（单位：米）
	 * @return 世界变换
	 */
	FTransform GetTransform(float BlockLength) const
	{
		return FTransform(Rotation, GetGeometricCenter(BlockLength), FVector::OneVector);
	}

	/**
	 * 检查几何描述是否有效
	 * @return true 如果 Box 有效
	 */
	bool IsValid() const
	{
		return Box.isValid();
	}

	bool operator==(const FVoxelGeometryDesc& Other) const
	{
		// TSet 无 operator==（UE5.8 已删除）：等 size + 包含关系即集合相等
		return Box == Other.Box
			&& Rotation == Other.Rotation
			&& bIsRegular == Other.bIsRegular
			&& ShapeType == Other.ShapeType
			&& Thickness == Other.Thickness
			&& ShellFaces.Num() == Other.ShellFaces.Num()
			&& ShellFaces.Includes(Other.ShellFaces);
	}
};


/**
 * 完整的体素编辑请求 - 统一的编辑请求模型
 * 包含建造状态和完整的几何描述（几何信息 + 功能参数）
 * 用于所有编辑场景：单体素编辑、批量编辑、预览
 */
USTRUCT(BlueprintType)
struct FVoxelEditRequest
{
	GENERATED_BODY()

	// === 工具类型 ===

	/** 工具类型（Voxel.Tool.* GameplayTag） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Params")
	FGameplayTag ToolTag = FGameplayTag::EmptyTag;

	// === 功能参数 ===

	/** 方块/模板的运行时句柄 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Params")
	int32 HandleId;

	/** 编辑命令（放置规则 + 选择模式） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Params")
	FVoxelEditCommand EditCommand = FVoxelEditCommand::BuildNormal();

	// === 几何描述信息（包含几何信息 + 形状参数） ===

	/** 几何信息子模型（包含位置、旋转、缩放、形状类型、厚度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit|Geometry")
	FVoxelGeometryDesc Geometry;

	// === 便捷方法 ===

	/**
	 * 获取中心点世界坐标
	 * @param BlockLength 体素边长（单位：米）
	 * @return 世界空间中心位置
	 */
	FVector GetCenter(float BlockLength) const
	{
		return Geometry.GetGeometricCenter(BlockLength);
	}

	/**
	 * 获取完整的变换
	 * @param BlockLength 体素边长（单位：米）
	 * @return 世界变换
	 */
	FTransform GetTransform(float BlockLength) const
	{
		return Geometry.GetTransform(BlockLength);
	}

	/**
	 * 从 StateComponent 构建编辑请求
	 * 这是一个便捷方法，用于从当前状态构建请求
	 */
	static FVoxelEditRequest CreateFromState(
		FGameplayTag InToolTag,
		const FVoxelEditCommand& InEditCommand,
		int32 InHandleId,
		EBatchShapeType InShapeType,
		int32 InThickness,
		const FAreaCoverageFragment& Box)
	{
		FVoxelEditRequest Request;
		Request.ToolTag = InToolTag;
		Request.EditCommand = InEditCommand;
		Request.HandleId = InHandleId;
		Request.Geometry.ShapeType = InShapeType;
		Request.Geometry.Thickness = InThickness;
		Request.Geometry.Box =Box;
		return Request;
	}

	/** 获取调试描述 */
	FText GetDescription() const
	{
		// 根据工具类型生成描述
		FString StateStr = TEXT("未知");
		if (ToolTag == FCoreTags::Tool_Edit_SingleVoxel ||
		    ToolTag == FCoreTags::Tool_Edit_SimpleGeometry)
		{
			StateStr = TEXT("建造");
		}
		else if (ToolTag == FCoreTags::Tool_Edit_Template)
		{
			StateStr = TEXT("模板");
		}
		else if (ToolTag.MatchesTag(FCoreTags::Tool_AreaEdit))
		{
			StateStr = TEXT("区域");
		}

		const FString PlacementStr = (EditCommand.PlacementMode == EVoxelPlacementMode::Normal) ? TEXT("覆盖") :
		                             (EditCommand.PlacementMode == EVoxelPlacementMode::Complement) ? TEXT("填充") :
		                             (EditCommand.PlacementMode == EVoxelPlacementMode::Replace) ? TEXT("替换") :
		                             (EditCommand.PlacementMode == EVoxelPlacementMode::ReplaceMask) ? TEXT("指定替换") : TEXT("未知");

		const FString SelectionStr = (EditCommand.SelectionMode == ESelectionOpMode::New) ? TEXT("新选") :
		                             (EditCommand.SelectionMode == ESelectionOpMode::Add) ? TEXT("加选") :
		                             (EditCommand.SelectionMode == ESelectionOpMode::Subtract) ? TEXT("减选") :
		                             (EditCommand.SelectionMode == ESelectionOpMode::Intersect) ? TEXT("交选") : TEXT("未知");

		return FText::FromString(FString::Printf(TEXT("%s - %s - %s"), *StateStr, *PlacementStr, *SelectionStr));
	}

	/**
	 * 检查世界位置是否在编辑形状内
	 * @param WorldPosition 要检查的世界位置
	 * @param BlockSize 体素大小（单位：米）
	 * @return true 如果位置在形状内
	 */
	bool IsPointInside(const FVector& WorldPosition, float BlockSize) const;

private:
	/** 世界到局部空间的变换矩阵 */
	mutable FMatrix WorldToLocal;

	/** 是否已初始化 */
	mutable bool bIsInitialized = false;

	/**
	 * 构建世界到局部空间的变换矩阵
	 * 用于处理旋转后的形状检测
	 */
	void BuildWorldToLocalMatrix() const;
};
USTRUCT(BlueprintType)
struct FSingleEditRequest
{
	GENERATED_BODY();

	/** 方块索引列表 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSet<FIntVector> BlockIndex;

	/** 区块索引列表（已去重） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSet<FIntVector> ChunkIndex;

	/** 新方块句柄 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NewHandleId;

	/** 编辑命令（放置规则 + 选择模式） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVoxelEditCommand EditCommand = FVoxelEditCommand::BuildNormal();

	/** 修改前的方块类型映射（用于 Undo） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FIntVector, int32> PreviousBlocks;

	/** 命令描述（用于 UI 显示） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Description;

	/** 创建时间戳 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDateTime Timestamp;

	/** 执行完成时间戳 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FDateTime DoneTimestamp;

	/** 建造状态（EditConstruction/PlaceFurniture/EditArea） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	// EVoxelControllerBuildState BuildState = EVoxelControllerBuildState::None;
	FGameplayTag ToolTag;

	/** 受影响的世界空间包围盒 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBox AffectedBounds;

	/** 是否可以撤销 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCanBeUndone = true;
	FSingleEditRequest(TSet<FIntVector> Blocks,TSet<FIntVector> Chunks,int32 HandleId,const FVoxelEditCommand& Edit)
	{
		BlockIndex = Blocks;
		ChunkIndex = Chunks;
		NewHandleId = HandleId;
		EditCommand = Edit;
	}
	FSingleEditRequest()
		: Timestamp(FDateTime::Now())
		, DoneTimestamp(FDateTime::MinValue())
		, ToolTag()
	{
	}
};

// ==================== 编辑器事件委托 ====================

// ==================== 历史记录相关结构体 ====================

/**
 * 编辑操作类型
 */
UENUM(BlueprintType)
enum class EVoxelEditOperationType : uint8
{
	SingleVoxelEdit		UMETA(DisplayName = "Single Voxel Edit"),
	BatchEdit			UMETA(DisplayName = "Batch Edit"),
	Copy				UMETA(DisplayName = "Copy"),
	Cut					UMETA(DisplayName = "Cut"),
	Paste				UMETA(DisplayName = "Paste"),
	Fill				UMETA(DisplayName = "Fill"),
	Replace				UMETA(DisplayName = "Replace"),
	Hollow				UMETA(DisplayName = "Hollow"),
	Delete				UMETA(DisplayName = "Delete"),
	BlueprintPlace		UMETA(DisplayName = "Blueprint Place"),
};

/**
 * 操作日志
 * 记录所有操作的详细日志（用于调试、审计）
 */
USTRUCT(BlueprintType)
struct FVoxelOperationLog
{
	GENERATED_BODY()

	/** 时间戳 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Log")
	FDateTime Timestamp;

	/** 操作类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Log")
	EVoxelEditOperationType OperationType;

	/** 操作描述 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Log")
	FText Description;

	/** 受影响的体素数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Log")
	int32 AffectedVoxelCount = 0;

	/** 操作耗时（毫秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Log")
	float ExecutionTimeMs = 0.0f;

	/** 是否成功 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Log")
	bool bSuccess = true;

	/** 错误信息（如果失败） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Log")
	FString ErrorMessage;
};

/**
 * 编辑统计信息
 * 跟踪玩家的编辑行为统计
 */
USTRUCT(BlueprintType)
struct FVoxelEditStatistics
{
	GENERATED_BODY()

	/** 总编辑次数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Statistics")
	int32 TotalEdits = 0;

	/** 总撤销次数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Statistics")
	int32 TotalUndos = 0;

	/** 总重做次数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Statistics")
	int32 TotalRedos = 0;

	/** 编辑的方块总数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Statistics")
	int32 TotalBlocksEdited = 0;

	/** 添加的方块数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Statistics")
	int32 BlocksAdded = 0;

	/** 删除的方块数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Statistics")
	int32 BlocksDeleted = 0;

	/** 替换的方块数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Statistics")
	int32 BlocksReplaced = 0;

	/** 复制次数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Statistics")
	int32 CopyOperations = 0;

	/** 粘贴次数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Statistics")
	int32 PasteOperations = 0;

	/** 蓝图放置次数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Statistics")
	int32 BlueprintPlacements = 0;

	/** 会话开始时间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Statistics")
	FDateTime SessionStartTime;
};

/**
 * 历史变更委托
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHistoryChanged);

/**
 * 撤销执行委托（参数：操作描述）
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUndoExecuted, FText, Description);

/**
 * 重做执行委托（参数：操作描述）
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRedoExecuted, FText, Description);

/**
 * 操作日志记录委托
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOperationLogged, FVoxelOperationLog, Log);

// ==================== 执行层修改模型 ====================

/**
 * 通用的体素修改模型（执行层）
 *
 * 职责：
 * - 传递给 BlockWorld 执行
 * - 只包含目标状态（BlockIndex → TargetType）
 * - 不关心是 Undo 还是 Redo
 *
 * 数据结构：
 * - TMap<FIntVector, FGameplayTag> Blocks - 方块映射（自动去重）
 * - TArray<FIntVector> ChunkIndices - 需要更新的区块
 * - FVoxelEditCommand EditCommand - 编辑命令（放置规则 + 选择模式）
 */
USTRUCT(BlueprintType)
struct FVoxelEditModification
{
    GENERATED_BODY()

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true), Category = "Voxel|Edit|Modification")
    TMap<FIntVector, int32> Blocks;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true), Category = "Voxel|Edit|Modification")
    TArray<FIntVector> ChunkIndices;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true), Category = "Voxel|Edit|Modification")
    FVoxelEditCommand EditCommand = FVoxelEditCommand::BuildNormal();

public:
    // === 操作方法 ===

    /**
     * 添加方块修改（自动去重）
     *
     * 时间复杂度：O(1) 平均
     */
    void Add(const FIntVector& BlockIndex, int32 HandleId)
    {
        Blocks.Add(BlockIndex, HandleId);
    }

    /**
     * 批量添加
     */
    void AddFromMap(const TMap<FIntVector, int32>& BlockMap)
    {
        Blocks.Append(BlockMap);
    }

    /**
     * 预分配空间（优化性能）
     */
    void Reserve(int32 Number)
    {
        Blocks.Reserve(Number);
    }

    // === 区块管理 ===

    void AddChunkIndex(const FIntVector& ChunkIndex)
    {
        ChunkIndices.AddUnique(ChunkIndex);
    }

    // === 访问器 ===

    int32 Num() const { return Blocks.Num(); }
    bool IsEmpty() const { return Blocks.Num() == 0; }
    const TMap<FIntVector, int32>& GetBlocks() const { return Blocks; }
    const TArray<FIntVector>& GetChunkIndices() const { return ChunkIndices; }

    const FVoxelEditCommand& GetEditCommand() const { return EditCommand; }
    void SetEditCommand(const FVoxelEditCommand& InEditCommand) { EditCommand = InEditCommand; }

    /**
     * 检查修改是否有效
     */
    bool IsValid() const
    {
        return Blocks.Num() > 0;
    }

    /**
     * 获取调试字符串
     */
    FString GetDebugString() const
    {
        return FString::Printf(TEXT("Modification: %d blocks, Placement=%d, Selection=%d"),
            Blocks.Num(),
            static_cast<int32>(EditCommand.PlacementMode),
            static_cast<int32>(EditCommand.SelectionMode));
    }
};

// ==================== Geometry 操作回调委托 ====================

/**
 * 移动 Geometry 操作回调
 * @param MoveDelta 移动增量（体素索引空间）
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoveGizmoDraged, FIntVector, MoveDelta);

/**
 * 旋转 Geometry 操作回调
 * @param RotateDelta 旋转增量
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRotateGizmoDragged, FRotator, RotateDelta);

/**
 * 修改包围盒操作回调
 * @param Axis 拖动轴
 * @param bMaxSide 是否为最大面
 * @param Delta 偏移量（体素索引空间）
 * @param BlockLength 体素边长（米）
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnModifyCoverage, EAxis::Type, Axis, bool, bMaxSide, int32, Delta, float, BlockLength);

/**
 * 重置中心操作回调
 * @param BlockLength 体素边长（米）
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRecenter, float, BlockLength);