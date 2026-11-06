// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Voxel/Editor/VoxelHistoryData.h"
#include "Voxel/Editor/VoxelPlacementTypes.h"  // FPlacementOperation
#include "VoxelControllerHistoryComponent.generated.h"

class UVoxelBlockWorldSubsystem;
class UVoxelPlacementWorldSubsystem;

/**
 * 体素编辑历史管理组件
 *
 * 职责：
 * - 管理撤销/重做历史记录栈
 * - 记录操作日志
 * - 统计编辑信息
 *
 * 设计原则：
 * - 每个玩家独立的历史管理
 * - 不依赖全局子系统
 * - 提供完整的历史、日志、统计功能
 *
 * @note 不处理业务逻辑，不检测输入，不渲染
 */
UCLASS(ClassGroup=(Custom), BlueprintType, meta=(BlueprintSpawnableComponent))
class VOXEL_API UVoxelControllerHistoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ==================== 初始化 ====================

	UVoxelControllerHistoryComponent();

	virtual void BeginPlay() override;

	// ==================== 核心功能：历史记录 ====================

	/**
	 * 保存历史记录到撤销栈
	 * @param Record 完整的历史记录
	 * @note 会自动清空重做栈
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|History|Edit")
	void SaveHistoryRecord(const FVoxelEditHistoryRecord& Record);

	// ==================== 撤销/重做业务 ====================

	/**
	 * 撤销上一次编辑（弹栈 → 反向回滚 → 压入重做栈）
	 * 家具操作委托 PlacementSubsystem 回滚，Block 操作走 BatchModifyVoxelData_Modification
	 * @return true=撤销成功
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|History|Edit")
	bool Undo();

	/**
	 * 重做上次撤销的操作（弹栈 → 正向重放 → 压回撤销栈）
	 * @return true=重做成功
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|History|Edit")
	bool Redo();

	/** 清空全部历史栈并广播变更 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|History|Edit")
	void ClearHistory();

	// ==================== 状态查询 ====================

	/** 是否可以撤销 */
	UFUNCTION(BlueprintPure, Category = "Voxel|History|State")
	bool CanUndo() const { return UndoStack.Num() > 0; }

	/** 是否可以重做 */
	UFUNCTION(BlueprintPure, Category = "Voxel|History|State")
	bool CanRedo() const { return RedoStack.Num() > 0; }

	/** 获取撤销栈中的命令数量 */
	UFUNCTION(BlueprintPure, Category = "Voxel|History|State")
	int32 GetUndoCount() const { return UndoStack.Num(); }

	/** 获取重做栈中的命令数量 */
	UFUNCTION(BlueprintPure, Category = "Voxel|History|State")
	int32 GetRedoCount() const { return RedoStack.Num(); }

	/** 获取最近的编辑命令（不弹出） */
	UFUNCTION(BlueprintPure, Category = "Voxel|History|State")
	FVoxelEditHistoryRecord GetLastEditRequest() const;

	/** 获取撤销栈（只读） */
	const TArray<FVoxelEditHistoryRecord>& GetUndoStack() const { return UndoStack; }

	/** 获取重做栈（只读） */
	const TArray<FVoxelEditHistoryRecord>& GetRedoStack() const { return RedoStack; }

	// ==================== Placement 历史（统一到原 UndoStack） ====================

	/**
	 * 保存放置操作（转换为 FVoxelEditHistoryRecord 入原 UndoStack）
	 * @param Operation 放置操作（含 Block 或 Furniture 数据）
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|History|Placement")
	void SavePlacementOperation(const FPlacementOperation& Operation);

	// ==================== 操作日志 ====================

	/**
	 * 记录操作日志
	 * @param Log 操作日志条目
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|History|Log")
	void LogOperation(const FVoxelOperationLog& Log);

	/**
	 * 获取操作日志
	 * @param Count 返回最近的 N 条日志（默认50）
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|History|Log")
	TArray<FVoxelOperationLog> GetOperationLogs(int32 Count = 50) const;

	/** 清除操作日志 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|History|Log")
	void ClearOperationLogs();

	// ==================== 统计信息 ====================

	/**
	 * 更新编辑统计
	 * @param HistoryRecord 历史记录
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|History|Statistics")
	void UpdateStatistics(const FVoxelEditHistoryRecord& HistoryRecord);

	/** 获取编辑统计信息 */
	UFUNCTION(BlueprintPure, Category = "Voxel|History|Statistics")
	FVoxelEditStatistics GetStatistics() const { return Statistics; }

	/** 重置统计信息 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|History|Statistics")
	void ResetStatistics();

	/** 获取会话时长 */
	UFUNCTION(BlueprintPure, Category = "Voxel|History|Statistics")
	FTimespan GetSessionDuration() const
	{
		return FDateTime::Now() - Statistics.SessionStartTime;
	}

	// ==================== 配置 ====================

	/** 最大历史记录数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Config")
	int32 MaxHistorySize = 50;

	/** 是否记录操作日志 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Config")
	bool bEnableOperationLogging = true;

	/** 最大日志条数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|History|Config")
	int32 MaxLogSize = 1000;

	// ==================== 事件委托 ====================

	/** 历史记录变更时广播（添加/清除操作） */
	UPROPERTY(BlueprintAssignable, Category = "Voxel|History|Events")
	FOnHistoryChanged OnHistoryChanged;

	/** 撤销执行时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Voxel|History|Events")
	FOnUndoExecuted OnUndoExecuted;

	/** 重做执行时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Voxel|History|Events")
	FOnRedoExecuted OnRedoExecuted;

	/** 操作日志更新时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Voxel|History|Events")
	FOnOperationLogged OnOperationLogged;

protected:
	// ==================== 辅助函数 ====================

	/** 强制执行历史记录大小限制（移除最旧的命令） */
	void EnforceHistoryLimit();

	/** 强制执行日志大小限制（移除最旧的日志） */
	void EnforceLogLimit();

	/** 生成自动描述文本 */
	FText GenerateAutoDescription() const;

	/** 获取 BlockWorld 子系统实例 */
	UFUNCTION()
	UVoxelBlockWorldSubsystem* GetBlockWorldSubsystem() const;

private:
	// ==================== 数据成员 ====================

	/** 撤销栈：存储历史记录 */
	UPROPERTY()
	TArray<FVoxelEditHistoryRecord> UndoStack;

	/** 重做栈：存储可重做的历史记录 */
	UPROPERTY()
	TArray<FVoxelEditHistoryRecord> RedoStack;

	/** 操作日志 */
	UPROPERTY()
	TArray<FVoxelOperationLog> OperationLogs;

	/** 编辑统计 */
	UPROPERTY()
	FVoxelEditStatistics Statistics;
};
