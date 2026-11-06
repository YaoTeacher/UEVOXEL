// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/Tools/VoxelSelectionEditTool.h"
#include "Voxel/Editor/VoxelToolContext.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelItemWorldSubsystem.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Core/Enum/GameStateEnum.h"
#include "Core/CoreTags.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Item/VoxelItemBaseInstance.h"
#include "Data/Item/InteractableData.h"

// ==================== 面板需求声明 ====================

UVoxelSelectionEditTool::UVoxelSelectionEditTool()
{
	bNeedsLibraryPanel = true;
	bNeedsDetailPanel = true;
}

// ==================== UVoxelToolBase 接口实现 ====================

FText UVoxelSelectionEditTool::GetToolDisplayName() const
{
	return NSLOCTEXT("VoxelTools", "SelectionEdit", "选区编辑");
}

FText UVoxelSelectionEditTool::GetToolDescription() const
{
	return NSLOCTEXT("VoxelTools", "SelectionEditDesc",
		"选区编辑工具：在已有选区内执行填充/删除/替换/复制/剪切/粘贴/移动\n"
		"参考 PS 的选区蒙版机制"
	);
}

EVoxelToolCategory UVoxelSelectionEditTool::GetToolCategory() const
{
	return EVoxelToolCategory::Edit;
}

FName UVoxelSelectionEditTool::GetToolID() const
{
	return FName("SelectionEdit");
}

// ==================== 输入生命周期（override） ====================

void UVoxelSelectionEditTool::OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	UVoxelPreviewComponent* VC = Context.PreviewComp;
	FVoxelInputState* InputState = Context.InputState;
	if (!SC || !VC || !InputState) return;

	// 仅在 EDIT 阶段处理 Gizmo 拖拽（HIDDEN/BASE 由其他流程管理）
	EPreviewInteractionState BuildPhase = SC->GetBuildPhase();
	if (BuildPhase != EPreviewInteractionState::EDITING) return;

	UE_LOG(LogTemp, Log, TEXT("[DEBUG] SelectionEditTool::OnMouseInput: BuildPhase=%d, Gizmo=%s, bIsModifying=%d"),
		(int32)BuildPhase, GizmoHitActor ? TEXT("Valid") : TEXT("Null"), InputState->bIsModifying ? 1 : 0);

	// 空点击且没有活跃拖拽 → 不执行任何操作
	if (!GizmoHitActor && !InputState->ActiveDragGizmo)
	{
		UE_LOG(LogTemp, Log, TEXT("[EditChain] SelectionEdit OnMouseInput: EDIT - click on empty space (no action)"));
		return;
	}

	const FVector WorldPosition = GroundHit.Location;

	AVoxel3DUIActor* TargetGizmo = nullptr;
	if (GizmoHitActor && !InputState->ActiveDragGizmo)
	{
		// 首次点击 Gizmo：激活拖拽，当前位置为零点（避免跳跃）
		InputState->ActiveDragGizmo = GizmoHitActor;
		InputState->FirstPoint = WorldPosition;
		InputState->SecondPoint = WorldPosition;
	}
	else
	{
		// 持续拖拽：更新鼠标位置
		InputState->SecondPoint = WorldPosition;
	}

	TargetGizmo = InputState->ActiveDragGizmo;

	// 仅处理平移 Gizmo（选区编辑核心操作：移动选区内实际数据）
	// 旋转/面 Gizmo 不适用于选区内容移动
	int32 AxisIndex = INDEX_NONE;
	if (VC->TranslateGizmos.Find(TargetGizmo, AxisIndex))
	{
		const float BlockLength = Context.GetBlockLength();
		const FVector Delta = InputState->SecondPoint - InputState->FirstPoint;

		// 【坐标转换】世界坐标增量 → 体素索引增量（仅取当前轴分量）
		const FIntVector DeltaInIndex = UChunkUtilsFunction::WorldDeltaToIndexDelta(Delta, BlockLength);
		FIntVector Offset = FIntVector::ZeroValue;
		Offset[AxisIndex] = DeltaInIndex[AxisIndex];

		UE_LOG(LogTemp, VeryVerbose, TEXT("SelectionEdit OnMouseInput: Axis %d, Delta %s, Offset %d"),
			AxisIndex, *Delta.ToString(), Offset[AxisIndex]);

		OnGizmoTranslate(Context, Offset);
	}

	InputState->FirstPoint = InputState->SecondPoint;
}

void UVoxelSelectionEditTool::OnConfirmEdit(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC || !SC->HasSelection()) return;

	UVoxelBlockWorldSubsystem* BlockWorld = Context.GetBlockWorld();
	if (!BlockWorld) return;

	// 取当前编辑请求（包含 BlockType、PlacementMode、ToolTag 等）
	FVoxelEditRequest EditRequest = SC->GetCurrentEditRequest();

	// 委托静态方法执行选区内编辑（PS 选区蒙版模式）
	if (ExecuteWithinSelection(EditRequest, SC, Context.HistoryComp, BlockWorld))
	{
		// 重置编辑状态，准备下一次编辑
		SC->ClearTmpGeometryTransform();
		SC->SetBuildPhase(EPreviewInteractionState::IDLE);
	}
}

void UVoxelSelectionEditTool::OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 选区编辑工具：移动选区内的实际数据（方块 + 家具），同时平移选区索引
	MoveSelectionContent(
		SC,
		Context.GetBlockWorld(),
		Context.GetItemWorld(),
		Context.HistoryComp,
		Offset
	);
}

// ==================== 选区内编辑（PS 选区蒙版模式） ====================

bool UVoxelSelectionEditTool::ExecuteWithinSelection(
	const FVoxelEditRequest& EditRequest,
	UVoxelPlayerStateCacheComponent* BuildStateComp,
	UVoxelControllerHistoryComponent* HistoryComp,
	UVoxelBlockWorldSubsystem* BlockWorld)
{
	if (!BuildStateComp || !BuildStateComp->HasSelection() || !BlockWorld)
	{
		return false;
	}

	const TArray<FIntVector>& Selection = BuildStateComp->GetCurrentSelection();

	// 构建历史记录（在修改前读取 OldType）
	FVoxelEditHistoryRecord HistoryRecord;
	if (HistoryComp)
	{
		HistoryRecord.ToolTag = EditRequest.ToolTag;
		HistoryRecord.Description = FText::FromString(TEXT("选区内编辑"));
		HistoryRecord.Timestamp = FDateTime::Now();
	}

	// 根据 PlacementMode 过滤选区，构建实际要修改的位置列表
	TArray<FIntVector> ToModify;
	ToModify.Reserve(Selection.Num());

	for (const FIntVector& Index : Selection)
	{
		int32 OldType = BlockWorld->GetBlockType(Index);

		// 根据 PlacementMode 决定是否跳过
		switch (EditRequest.EditCommand.PlacementMode)
		{
		case EVoxelPlacementMode::Complement:
			// 填充模式：只在空气位置写入
			if (OldType != EBlockId::Air) continue;
			break;
		case EVoxelPlacementMode::Replace:
			// 替换模式：只替换非空方块
			if (OldType == EBlockId::Air) continue;
			break;
		case EVoxelPlacementMode::Normal:
		case EVoxelPlacementMode::ReplaceMask:
		default:
			// 覆盖模式：不跳过
			break;
		}

		ToModify.Add(Index);

		if (HistoryComp)
		{
			HistoryRecord.Add(FVoxelEdit(Index, OldType, EditRequest.HandleId));
		}
	}

	if (ToModify.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SelectionEditTool] ExecuteWithinSelection: No voxels to modify after filtering"));
		return false;
	}

	// 批量修改：复用已有的 BatchModifyVoxelData(TArray<FIntVector>)
	BlockWorld->BatchModifyVoxelData(ToModify, EditRequest.EditCommand, EditRequest.HandleId);

	// 记录历史
	if (HistoryComp && HistoryRecord.Num() > 0)
	{
		HistoryComp->SaveHistoryRecord(HistoryRecord);
	}

	UE_LOG(LogTemp, Log, TEXT("[SelectionEditTool] ExecuteWithinSelection: %d/%d voxels edited, blockType=%d, mode=%d"),
		ToModify.Num(), Selection.Num(), EditRequest.HandleId, (int32)EditRequest.EditCommand.PlacementMode);

	return true;
}

// ==================== 剪切板操作 ====================

bool UVoxelSelectionEditTool::CopySelection(
	UVoxelPlayerStateCacheComponent* BuildStateComp,
	UVoxelBlockWorldSubsystem* BlockWorld)
{
	if (!BuildStateComp || !BuildStateComp->HasSelection() || !BlockWorld) return false;

	const TArray<FIntVector>& Selection = BuildStateComp->GetCurrentSelection();
	if (Selection.Num() == 0) return false;

	// 计算选区 Min/Max
	FIntVector Min, Max;
	if (!GetSelectionBounds(Selection, Min, Max)) return false;

	// 复制体素数据到剪切板（相对位置）
	FVoxelClipboardData& Clipboard = BuildStateComp->ClipboardData;
	Clipboard.Clear();
	Clipboard.SourceMin = Min;
	Clipboard.Size = Max - Min + FIntVector(1);
	Clipboard.Voxels.Reserve(Selection.Num());

	for (const FIntVector& Pos : Selection)
	{
		int32 BlockType = BlockWorld->GetBlockType(Pos);
		if (BlockType != EBlockId::Air)
		{
			FIntVector RelativePos = Pos - Min;
			// FVoxelEdit: BlockIndex 存相对位置, OldType=Air, NewType=实际类型
			Clipboard.Voxels.Add(FVoxelEdit(RelativePos, EBlockId::Air, BlockType));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SelectionEditTool] Copied %d voxels, size=%s, source=%s"),
		Clipboard.Voxels.Num(), *Clipboard.Size.ToString(), *Min.ToString());

	return true;
}

bool UVoxelSelectionEditTool::CutSelection(
	UVoxelPlayerStateCacheComponent* BuildStateComp,
	UVoxelBlockWorldSubsystem* BlockWorld,
	UVoxelControllerHistoryComponent* HistoryComp)
{
	if (!BuildStateComp || !BuildStateComp->HasSelection() || !BlockWorld) return false;

	// 先复制到剪切板
	if (!CopySelection(BuildStateComp, BlockWorld)) return false;

	// 再删除原内容（用 Air 填充选区）
	FVoxelEditCommand EditCommand;
	EditCommand.PlacementMode = EVoxelPlacementMode::Normal;
	BlockWorld->BatchModifyVoxelData(BuildStateComp->GetCurrentSelection(), EditCommand, EBlockId::Air);

	// 记录历史
	if (HistoryComp)
	{
		FVoxelEditHistoryRecord HistoryRecord;
		HistoryRecord.ToolTag = FCoreTags::Tool_Edit;
		HistoryRecord.Description = FText::FromString(TEXT("剪切选区"));
		HistoryRecord.Timestamp = FDateTime::Now();

		for (const FIntVector& Pos : BuildStateComp->GetCurrentSelection())
		{
			int32 OldType = BlockWorld->GetBlockType(Pos);
			HistoryRecord.Add(FVoxelEdit(Pos, OldType, EBlockId::Air));
		}

		if (HistoryRecord.Num() > 0)
		{
			HistoryComp->SaveHistoryRecord(HistoryRecord);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SelectionEditTool] Cut %d voxels"), BuildStateComp->GetCurrentSelection().Num());
	return true;
}

bool UVoxelSelectionEditTool::PasteSelection(
	UVoxelPlayerStateCacheComponent* BuildStateComp,
	UVoxelBlockWorldSubsystem* BlockWorld,
	UVoxelControllerHistoryComponent* HistoryComp,
	const FIntVector& TargetPosition)
{
	if (!BuildStateComp || !BlockWorld) return false;

	const FVoxelClipboardData& Clipboard = BuildStateComp->ClipboardData;
	if (!Clipboard.IsValid()) return false;

	// 构建粘贴位置和类型列表
	TArray<FIntVector> PastePositions;
	TArray<int32> PasteTypes;
	PastePositions.Reserve(Clipboard.Voxels.Num());
	PasteTypes.Reserve(Clipboard.Voxels.Num());

	for (const FVoxelEdit& Edit : Clipboard.Voxels)
	{
		FIntVector WorldPos = TargetPosition + Edit.BlockIndex;
		PastePositions.Add(WorldPos);
		PasteTypes.Add(Edit.NewHandleId);
	}

	// 按类型分组批量写入（性能优化）
	FVoxelEditCommand EditCommand;
	EditCommand.PlacementMode = EVoxelPlacementMode::Normal;

	TMap<int32, TArray<FIntVector>> TypeGroups;
	for (int32 i = 0; i < PastePositions.Num(); ++i)
	{
		TypeGroups.FindOrAdd(PasteTypes[i]).Add(PastePositions[i]);
	}

	// 构建历史记录
	FVoxelEditHistoryRecord HistoryRecord;
	if (HistoryComp)
	{
		HistoryRecord.ToolTag = FCoreTags::Tool_Edit;
		HistoryRecord.Description = FText::FromString(TEXT("粘贴"));
		HistoryRecord.Timestamp = FDateTime::Now();
	}

	for (const auto& Group : TypeGroups)
	{
		BlockWorld->BatchModifyVoxelData(Group.Value, EditCommand, Group.Key);

		if (HistoryComp)
		{
			for (const FIntVector& Pos : Group.Value)
			{
				int32 OldType = BlockWorld->GetBlockType(Pos);
				HistoryRecord.Add(FVoxelEdit(Pos, OldType, Group.Key));
			}
		}
	}

	if (HistoryComp && HistoryRecord.Num() > 0)
	{
		HistoryComp->SaveHistoryRecord(HistoryRecord);
	}

	UE_LOG(LogTemp, Log, TEXT("[SelectionEditTool] Pasted %d voxels at %s"),
		PastePositions.Num(), *TargetPosition.ToString());

	return true;
}

// ==================== 移动选区内实际数据 ====================

bool UVoxelSelectionEditTool::MoveSelectionContent(
	UVoxelPlayerStateCacheComponent* BuildStateComp,
	UVoxelBlockWorldSubsystem* BlockWorld,
	UVoxelItemWorldSubsystem* ItemWorld,
	UVoxelControllerHistoryComponent* HistoryComp,
	const FIntVector& Offset)
{
	if (!BuildStateComp || !BuildStateComp->HasSelection() || !BlockWorld || Offset == FIntVector::ZeroValue)
	{
		return false;
	}

	const TArray<FIntVector>& Selection = BuildStateComp->GetCurrentSelection();

	// === 步骤1：读取选区内方块的当前类型（修改前） ===
	TMap<FIntVector, int32> OriginalBlocks;
	OriginalBlocks.Reserve(Selection.Num());
	for (const FIntVector& Pos : Selection)
	{
		OriginalBlocks.Add(Pos, BlockWorld->GetBlockType(Pos));
	}

	// === 步骤2：收集选区内的家具（去重） ===
	TArray<AUVoxelItemBaseInstance*> ItemsToMove;
	TSet<AUVoxelItemBaseInstance*> Visited;
	if (ItemWorld)
	{
		for (const FIntVector& Pos : Selection)
		{
			AUVoxelItemBaseInstance* Item = ItemWorld->GetItemAtGrid(Pos);
			if (Item && !Visited.Contains(Item))
			{
				Visited.Add(Item);
				ItemsToMove.Add(Item);
			}
		}
	}

	// === 步骤3：预先读取新位置的原类型（用于历史记录）
	// 必须在步骤4清空原位置之前读，否则若新位置也在选区内会被错误地读成 Air
	TMap<FIntVector, int32> NewPosOldTypes;
	for (const auto& KV : OriginalBlocks)
	{
		if (KV.Value == EBlockId::Air) continue;
		FIntVector NewPos = KV.Key + Offset;
		if (!NewPosOldTypes.Contains(NewPos))
		{
			NewPosOldTypes.Add(NewPos, BlockWorld->GetBlockType(NewPos));
		}
	}

	// === 步骤4：清空原位置方块（设为 Air） ===
	FVoxelEditCommand ClearCmd;
	ClearCmd.PlacementMode = EVoxelPlacementMode::Normal;
	TArray<FIntVector> OldPositions;
	OldPositions.Reserve(Selection.Num());
	for (const FIntVector& Pos : Selection)
	{
		OldPositions.Add(Pos);
	}
	BlockWorld->BatchModifyVoxelData(OldPositions, ClearCmd, EBlockId::Air);

	// === 步骤5：在新位置写入方块 ===
	TArray<FIntVector> NewPositions;
	NewPositions.Reserve(OriginalBlocks.Num());
	TArray<int32> NewTypes;
	NewTypes.Reserve(OriginalBlocks.Num());

	for (const auto& KV : OriginalBlocks)
	{
		if (KV.Value != EBlockId::Air)
		{
			NewPositions.Add(KV.Key + Offset);
			NewTypes.Add(KV.Value);
		}
	}

	if (NewPositions.Num() > 0)
	{
		// 按类型分组批量写入
		TMap<int32, TArray<FIntVector>> TypeGroups;
		for (int32 i = 0; i < NewPositions.Num(); ++i)
		{
			TypeGroups.FindOrAdd(NewTypes[i]).Add(NewPositions[i]);
		}

		for (const auto& Group : TypeGroups)
		{
			BlockWorld->BatchModifyVoxelData(Group.Value, ClearCmd, Group.Key);
		}
	}

	// === 步骤6：移动家具 ===
	if (ItemWorld && ItemsToMove.Num() > 0)
	{
		for (AUVoxelItemBaseInstance* Item : ItemsToMove)
		{
			if (!IsValid(Item)) continue;

			// 记录原位置和实例数据
			FIntVector OldOrigin = Item->OccupiedBlock.IsValidIndex(0) ? Item->OccupiedBlock[0] : FIntVector::ZeroValue;
			FInteractableData Data;
			Data.InteractableType = EInteractableType::Place;
			Data.HandleId = Item->ItemInstance.HandleId;
			Data.Quantity = Item->ItemInstance.Quantity;
			Data.Direction = Item->ItemInstance.Direction;

			// 移除原位置家具
			ItemWorld->RemoveItemAtGrid(OldOrigin);

			// 在新位置生成家具
			FIntVector NewOrigin = OldOrigin + Offset;
			ItemWorld->SpawnItemAtGrid(NewOrigin, Data, false);
		}
	}

	// === 步骤7：平移选区索引本身（保持选区与内容同步） ===
	BuildStateComp->MoveSelection(Offset);

	// === 步骤8：写入历史记录 ===
	if (HistoryComp)
	{
		FVoxelEditHistoryRecord HistoryRecord;
		HistoryRecord.ToolTag = FCoreTags::Tool_Edit;
		HistoryRecord.Description = FText::FromString(TEXT("移动选区内容"));
		HistoryRecord.Timestamp = FDateTime::Now();

		// 记录每个方块的前后状态
		// - 旧位置：OldType → Air
		// - 新位置：NewPosOldType（步骤3读取的）→ OldType
		for (const auto& KV : OriginalBlocks)
		{
			if (KV.Value == EBlockId::Air) continue;

			const FIntVector& OldPos = KV.Key;
			int32 OldType = KV.Value;
			FIntVector NewPos = OldPos + Offset;

			HistoryRecord.Add(FVoxelEdit(OldPos, OldType, EBlockId::Air));

			const int32* NewPosOldTypePtr = NewPosOldTypes.Find(NewPos);
			int32 NewPosOldType = NewPosOldTypePtr ? *NewPosOldTypePtr : EBlockId::Air;
			HistoryRecord.Add(FVoxelEdit(NewPos, NewPosOldType, OldType));
		}

		if (HistoryRecord.Num() > 0)
		{
			HistoryComp->SaveHistoryRecord(HistoryRecord);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SelectionEditTool] MoveSelectionContent: %d blocks, %d items, offset=%s"),
		OriginalBlocks.Num(), ItemsToMove.Num(), *Offset.ToString());

	return true;
}

// ==================== 私有辅助方法 ====================

bool UVoxelSelectionEditTool::GetSelectionBounds(
	const TArray<FIntVector>& Selection,
	FIntVector& OutMin,
	FIntVector& OutMax)
{
	if (Selection.Num() == 0) return false;

	OutMin = Selection[0];
	OutMax = Selection[0];
	for (const FIntVector& Pos : Selection)
	{
		OutMin = FIntVector(FMath::Min(OutMin.X, Pos.X), FMath::Min(OutMin.Y, Pos.Y), FMath::Min(OutMin.Z, Pos.Z));
		OutMax = FIntVector(FMath::Max(OutMax.X, Pos.X), FMath::Max(OutMax.Y, Pos.Y), FMath::Max(OutMax.Z, Pos.Z));
	}
	return true;
}

void UVoxelSelectionEditTool::ConfigureGizmos(UVoxelPreviewComponent* PC, EPreviewInteractionState Phase)
{
	PC->ConfigureGizmoLayout(Phase, true, false, false);
}
