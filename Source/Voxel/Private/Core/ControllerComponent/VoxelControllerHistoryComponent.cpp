// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"
#include "Core/Enum/GameStateEnum.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "Voxel/Editor/VoxelPlacementWorldSubsystem.h"
#include "Voxel/Editor/VoxelPlacementTypes.h"  // FPlacementOperation
#include "GameFramework/PlayerController.h"
#include "Utils/ChunkUtilsFunction.h"

// ==================== 初始化 ====================

UVoxelControllerHistoryComponent::UVoxelControllerHistoryComponent()
{
	// 初始化统计信息
	Statistics.SessionStartTime = FDateTime::Now();

	// 默认配置
	MaxHistorySize = 50;
	bEnableOperationLogging = true;
	MaxLogSize = 1000;
}

void UVoxelControllerHistoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

// ==================== 核心功能：历史记录 ====================

void UVoxelControllerHistoryComponent::SaveHistoryRecord(const FVoxelEditHistoryRecord& Record)
{
	// 将历史记录压入撤销栈
	UndoStack.Push(Record);

	// 清空重做栈（新的操作会使之前的历史无法重做）
	RedoStack.Empty();

	// 强制执行历史记录大小限制
	EnforceHistoryLimit();

	// 广播历史变更事件
	OnHistoryChanged.Broadcast();
}

FVoxelEditHistoryRecord UVoxelControllerHistoryComponent::GetLastEditRequest() const
{
	if (CanUndo())
	{
		return UndoStack.Top();
	}

	// 返回空记录
	return FVoxelEditHistoryRecord();
}

// ==================== 撤销/重做业务 ====================

bool UVoxelControllerHistoryComponent::Undo()
{
	if (!GetOwner() || !GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("HistoryComp::Undo: Invalid owner or world"));
		return false;
	}

	if (!CanUndo())
	{
		UE_LOG(LogTemp, Warning, TEXT("HistoryComp::Undo: Cannot undo, history stack is empty"));
		return false;
	}

	// 从撤销栈弹出历史记录
	FVoxelEditHistoryRecord HistoryRecord = UndoStack.Top();
	UndoStack.Pop();

	// 验证是否可以撤销
	if (!HistoryRecord.bCanBeUndone)
	{
		OnHistoryChanged.Broadcast();
		UE_LOG(LogTemp, Warning, TEXT("HistoryComp::Undo: Record cannot be undone"));
		return false;
	}

	// 家具操作：委托 PlacementSubsystem 回滚
	if (HistoryRecord.IsFurnitureOperation())
	{
		UVoxelPlacementWorldSubsystem* Placement = GetWorld()->GetSubsystem<UVoxelPlacementWorldSubsystem>();
		if (!Placement)
		{
			UE_LOG(LogTemp, Error, TEXT("HistoryComp::Undo: PlacementSubsystem is null"));
			UndoStack.Push(HistoryRecord);  // 放回
			return false;
		}

		// 构造 FPlacementOperation 用于回滚
		FPlacementOperation Op;
		Op.OpType = HistoryRecord.PlacementOpType;
		Op.ToolTag = HistoryRecord.ToolTag;
		Op.Timestamp = HistoryRecord.Timestamp;
		Op.BlockEdits = HistoryRecord.GetEdits().Array();
		Op.BeforeSnapshot = HistoryRecord.BeforeSnapshot;
		Op.AfterSnapshot = HistoryRecord.AfterSnapshot;
		// Handle 从 Snapshot 重建
		if (!Op.BeforeSnapshot.IsEmpty())
		{
			Op.Handle = FPlacementHandle::MakeFurniture(nullptr, Op.BeforeSnapshot.HandleId);
			Op.Handle.BlockPos = Op.BeforeSnapshot.Origin;
		}

		if (Placement->ExecuteUndo(Op))
		{
			RedoStack.Push(HistoryRecord);
			Statistics.TotalUndos++;
			OnHistoryChanged.Broadcast();
			UE_LOG(LogTemp, Log, TEXT("HistoryComp::Undo: Furniture operation undone"));
			return true;
		}
		UndoStack.Push(HistoryRecord);  // 失败放回
		return false;
	}

	// 透明区域框操作：委托 BuildStateComponent 回滚
	if (HistoryRecord.IsCutoutOperation())
	{
		UVoxelPlayerStateCacheComponent* SC = GetOwner()->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
		if (!SC)
		{
			UE_LOG(LogTemp, Error, TEXT("HistoryComp::Undo: BuildStateComp is null"));
			UndoStack.Push(HistoryRecord);  // 放回
			return false;
		}
		SC->ExecuteCutoutUndo(HistoryRecord);
		RedoStack.Push(HistoryRecord);
		Statistics.TotalUndos++;
		OnHistoryChanged.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("HistoryComp::Undo: Cutout operation undone"));
		return true;
	}

	// Block 操作
	UVoxelBlockWorldSubsystem* BlockWorld = GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>();
	if (!BlockWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("HistoryComp::Undo: BlockWorldSubsystem is null"));
		UndoStack.Push(HistoryRecord);  // 失败放回
		return false;
	}

	// 转换为 Undo Modification 并执行
	FVoxelEditModification Modification = HistoryRecord.CreateUndoModification();
	BlockWorld->BatchModifyVoxelData_Modification(Modification);

	// 将历史记录压入重做栈
	RedoStack.Push(HistoryRecord);

	// 更新统计
	Statistics.TotalUndos++;

	// 广播历史变更事件
	OnHistoryChanged.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("HistoryComp::Undo: Successfully undone operation"));
	return true;
}

bool UVoxelControllerHistoryComponent::Redo()
{
	if (!GetOwner() || !GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("HistoryComp::Redo: Invalid owner or world"));
		return false;
	}

	if (!CanRedo())
	{
		UE_LOG(LogTemp, Warning, TEXT("HistoryComp::Redo: Cannot redo, no operations to redo"));
		return false;
	}

	// 从重做栈弹出历史记录
	FVoxelEditHistoryRecord HistoryRecord = RedoStack.Top();
	RedoStack.Pop();

	// 家具操作：委托 PlacementSubsystem 重做
	if (HistoryRecord.IsFurnitureOperation())
	{
		UVoxelPlacementWorldSubsystem* Placement = GetWorld()->GetSubsystem<UVoxelPlacementWorldSubsystem>();
		if (!Placement)
		{
			UE_LOG(LogTemp, Error, TEXT("HistoryComp::Redo: PlacementSubsystem is null"));
			RedoStack.Push(HistoryRecord);  // 放回
			return false;
		}

		// 构造 FPlacementOperation 用于重做
		FPlacementOperation Op;
		Op.OpType = HistoryRecord.PlacementOpType;
		Op.ToolTag = HistoryRecord.ToolTag;
		Op.Timestamp = HistoryRecord.Timestamp;
		Op.BlockEdits = HistoryRecord.GetEdits().Array();
		Op.BeforeSnapshot = HistoryRecord.BeforeSnapshot;
		Op.AfterSnapshot = HistoryRecord.AfterSnapshot;
		if (!Op.AfterSnapshot.IsEmpty())
		{
			Op.Handle = FPlacementHandle::MakeFurniture(nullptr, Op.AfterSnapshot.HandleId);
			Op.Handle.BlockPos = Op.AfterSnapshot.Origin;
		}

		if (Placement->ExecuteRedo(Op))
		{
			UndoStack.Push(HistoryRecord);
			Statistics.TotalRedos++;
			OnHistoryChanged.Broadcast();
			UE_LOG(LogTemp, Log, TEXT("HistoryComp::Redo: Furniture operation redone"));
			return true;
		}
		RedoStack.Push(HistoryRecord);  // 失败放回
		return false;
	}

	// 透明区域框操作：委托 BuildStateComponent 重做
	if (HistoryRecord.IsCutoutOperation())
	{
		UVoxelPlayerStateCacheComponent* SC = GetOwner()->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
		if (!SC)
		{
			UE_LOG(LogTemp, Error, TEXT("HistoryComp::Redo: BuildStateComp is null"));
			RedoStack.Push(HistoryRecord);  // 放回
			return false;
		}
		SC->ExecuteCutoutRedo(HistoryRecord);
		UndoStack.Push(HistoryRecord);
		Statistics.TotalRedos++;
		OnHistoryChanged.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("HistoryComp::Redo: Cutout operation redone"));
		return true;
	}

	// Block 操作
	UVoxelBlockWorldSubsystem* BlockWorld = GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>();
	if (!BlockWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("HistoryComp::Redo: BlockWorldSubsystem is null"));
		RedoStack.Push(HistoryRecord);  // 失败放回
		return false;
	}

	// 转换为 Redo Modification 并执行
	FVoxelEditModification Modification = HistoryRecord.CreateRedoModification();
	BlockWorld->BatchModifyVoxelData_Modification(Modification);

	// 将历史记录压回撤销栈
	UndoStack.Push(HistoryRecord);

	// 更新统计
	Statistics.TotalRedos++;

	// 广播历史变更事件
	OnHistoryChanged.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("HistoryComp::Redo: Successfully redone operation"));
	return true;
}

void UVoxelControllerHistoryComponent::ClearHistory()
{
	UndoStack.Empty();
	RedoStack.Empty();
	OnHistoryChanged.Broadcast();
	UE_LOG(LogTemp, Log, TEXT("HistoryComp::ClearHistory: History cleared"));
}

// ==================== 操作日志 ====================

void UVoxelControllerHistoryComponent::LogOperation(const FVoxelOperationLog& Log)
{
	// 检查是否启用日志
	if (!bEnableOperationLogging)
	{
		return;
	}

	// 添加日志
	OperationLogs.Add(Log);

	// 强制执行日志大小限制
	EnforceLogLimit();

	// 广播日志记录事件
	OnOperationLogged.Broadcast(Log);
}

TArray<FVoxelOperationLog> UVoxelControllerHistoryComponent::GetOperationLogs(int32 Count) const
{
	TArray<FVoxelOperationLog> Result;

	const int32 TotalLogs = OperationLogs.Num();
	const int32 StartIndex = FMath::Max(0, TotalLogs - Count);

	for (int32 i = StartIndex; i < TotalLogs; i++)
	{
		Result.Add(OperationLogs[i]);
	}

	return Result;
}

void UVoxelControllerHistoryComponent::ClearOperationLogs()
{
	OperationLogs.Empty();
}

// ==================== 统计信息 ====================

void UVoxelControllerHistoryComponent::UpdateStatistics(const FVoxelEditHistoryRecord& HistoryRecord)
{
	// 更新总编辑次数
	Statistics.TotalEdits++;

	// 更新编辑的方块总数
	const int32 BlockCount = HistoryRecord.Num();
	Statistics.TotalBlocksEdited += BlockCount;

	// 根据编辑命令更新统计（从编辑记录中获取）
	// 注意：新的HistoryRecord不再存储EditMode，需要从Edits中推断
	// 这里暂时只统计总数，不区分操作类型

	// 根据工具类型更新特定操作统计
	if (HistoryRecord.ToolTag == FCoreTags::Tool_Edit_SingleVoxel ||
	    HistoryRecord.ToolTag == FCoreTags::Tool_Edit_SimpleGeometry)
	{
		// TODO: 区分不同的操作类型（复制、粘贴等）
		// 目前暂时不特别处理
	}
}

void UVoxelControllerHistoryComponent::ResetStatistics()
{
	Statistics = FVoxelEditStatistics();
	Statistics.SessionStartTime = FDateTime::Now();
}

// ==================== 辅助函数 ====================

void UVoxelControllerHistoryComponent::EnforceHistoryLimit()
{
	// 如果撤销栈超过最大大小，移除最旧的命令（栈底元素）
	while (UndoStack.Num() > MaxHistorySize)
	{
		UndoStack.RemoveAt(0);
	}

	// 同样限制重做栈大小
	while (RedoStack.Num() > MaxHistorySize)
	{
		RedoStack.RemoveAt(0);
	}
}

// ==================== Placement 历史（统一到原 UndoStack） ====================

void UVoxelControllerHistoryComponent::SavePlacementOperation(const FPlacementOperation& Operation)
{
	if (Operation.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[History] SavePlacementOperation: empty operation ignored"));
		return;
	}

	// 转换为 FVoxelEditHistoryRecord 入原 UndoStack
	FVoxelEditHistoryRecord Record;
	Record.ToolTag = Operation.ToolTag;
	Record.Timestamp = Operation.Timestamp;
	Record.bCanBeUndone = true;
	Record.PlacementOpType = Operation.OpType;
	Record.BeforeSnapshot = Operation.BeforeSnapshot;
	Record.AfterSnapshot = Operation.AfterSnapshot;

	// Block 操作：填 Edits
	for (const FVoxelEdit& Edit : Operation.BlockEdits)
	{
		Record.Add(Edit);
	}

	// 压入原 UndoStack（SaveHistoryRecord 内部会清空 RedoStack + 限制大小 + 广播）
	SaveHistoryRecord(Record);

	UE_LOG(LogTemp, Log, TEXT("[History] Placement operation saved to unified stack: OpType=%d, Type=%d, IsFurniture=%d"),
		(int32)Operation.OpType, (int32)Operation.Handle.Type, Record.IsFurnitureOperation() ? 1 : 0);
}

void UVoxelControllerHistoryComponent::EnforceLogLimit()
{
	// 如果日志超过最大大小，移除最旧的日志（索引 0）
	while (OperationLogs.Num() > MaxLogSize)
	{
		OperationLogs.RemoveAt(0);
	}
}

FText UVoxelControllerHistoryComponent::GenerateAutoDescription() const
{
	const FDateTime Now = FDateTime::Now();
	return FText::Format(FText::FromString("编辑于 {0}"),
		FText::AsDateTime(Now));
}

UVoxelBlockWorldSubsystem* UVoxelControllerHistoryComponent::GetBlockWorldSubsystem() const
{
	// 从 Owner 获取 World
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 从 World 获取 BlockWorldSubsystem
	return World->GetSubsystem<UVoxelBlockWorldSubsystem>();
}
