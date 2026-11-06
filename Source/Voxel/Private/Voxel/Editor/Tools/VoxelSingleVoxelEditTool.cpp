// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/Tools/VoxelSingleVoxelEditTool.h"
#include "Voxel/Editor/VoxelToolContext.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"
#include "Voxel/Editor/VoxelPreviewActor.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Voxel/Editor/VoxelHistoryData.h"
#include "Core/CoreTags.h"
#include "Utils/ChunkUtilsFunction.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "Data/Item/InteractableData.h"
#include "Voxel/Editor/Tools/VoxelSelectionTool.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"

// ==================== 面板需求声明 ====================

UVoxelSingleVoxelEditTool::UVoxelSingleVoxelEditTool()
{
	bNeedsLibraryPanel = true;
	bNeedsDetailPanel = true;
}

// ==================== 元数据接口 ====================

FText UVoxelSingleVoxelEditTool::GetToolDisplayName() const
{
	return NSLOCTEXT("VoxelTools", "SingleVoxelEdit", "单体素编辑笔刷");
}

// ==================== 输入生命周期 ====================

void UVoxelSingleVoxelEditTool::OnMouseHover(const FVoxelToolContext& Context, const FHitResult& GroundHit)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	UVoxelPreviewComponent* VC = Context.PreviewComp;
	if (!VC || !VC->PreviewActor) return;

	const float BlockLength = Context.GetBlockLength();

	// 笔刷中心 = 鼠标命中体素（删除）或命中体素的法线方向相邻体素（放置）
	FInteractableData HandData = SC->GetInteractableDataInHand();
	const bool bIsDelete = HandData.HandleId == EBlockId::Air;
	FIntVector CenterIndex;
	if (bIsDelete)
	{
		CenterIndex = UChunkUtilsFunction::GetBlockIndexByPosition(GroundHit.Location, BlockLength);
	}
	else
	{
		CenterIndex = UChunkUtilsFunction::GetBlockIndexByPosition(
			GroundHit.Location + GroundHit.Normal * BlockLength / 2.0f, BlockLength);
	}

	// 笔刷预览颜色：删除=红色，放置=绿色
	FLinearColor BrushColor = bIsDelete ? FLinearColor::Red : FLinearColor::Green;

	VC->PreviewActor->UpdateBrushPreview(
		CenterIndex,
		SC->GetBrushSize(),
		SC->GetCurrentBatchShape(),
		SC->GetBrushDimension(),
		BlockLength,
		BrushColor
	);
}

void UVoxelSingleVoxelEditTool::OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 无地面命中时不处理
	if (!GroundHit.bBlockingHit) return;

	const float BlockLength = Context.GetBlockLength();

	// 笔刷中心 = 鼠标命中体素（删除）或命中体素的法线方向相邻体素（放置）
	FInteractableData HandData = SC->GetInteractableDataInHand();
	const bool bIsDelete = HandData.HandleId == EBlockId::Air;
	FIntVector CenterIndex;
	if (bIsDelete)
	{
		CenterIndex = UChunkUtilsFunction::GetBlockIndexByPosition(GroundHit.Location, BlockLength);
	}
	else
	{
		CenterIndex = UChunkUtilsFunction::GetBlockIndexByPosition(
			GroundHit.Location + GroundHit.Normal * BlockLength / 2.0f, BlockLength);
	}

	// 单击模式：由 IA_Select Started 触发一次，按住期间不重复（OnMouseDrag 不实现，基类默认空操作）
	ApplyEditBrushStamp(Context, CenterIndex);
}

void UVoxelSingleVoxelEditTool::OnMouseCancel(const FVoxelToolContext& Context)
{
	// 统一走基类逻辑：取消 → IDLE + 清空手持 + 清空预览
	Super::OnMouseCancel(Context);
}

// ==================== 物品使用 ====================

bool UVoxelSingleVoxelEditTool::CanHandleItem(const FGameplayTag& ItemTag) const
{
	return ItemTag.MatchesTag(FCoreTags::Item_Type_Block);
}

bool UVoxelSingleVoxelEditTool::UseItemInHand(const FVoxelToolContext& Context, FVector TargetWorldPosition, FVector TargetNormal)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return false;

	UWorld* World = Context.GetWorld();
	if (!World) return false;

	UVoxelBlockWorldSubsystem* BlockWorld = Context.GetBlockWorld();
	UConfigDataSubsystem* ConfigSys = Context.GetConfigData();
	if (!BlockWorld || !ConfigSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UseItemInHand] Subsystem missing: Block=%s Config=%s"),
			BlockWorld ? TEXT("OK") : TEXT("NULL"),
			ConfigSys ? TEXT("OK") : TEXT("NULL"));
		return false;
	}

	// 1. 获取手持物品句柄
	FInteractableData HandData = SC->GetInteractableDataInHand();
	if (HandData.HandleId == EBlockId::Invalid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UseItemInHand] No item in hand"));
		return false;
	}

	// 2. 统一查找入口：先查配置物品，再查模板
	FVoxelItemBase* Item = ConfigSys->FindByHandle(HandData.HandleId);
	if (!Item)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UseItemInHand] Item not found, Handle=%d"), HandData.HandleId);
		return false;
	}

	// 3. 计算 BlockLength
	const float BlockLength = Context.GetBlockLength();

	// 4. 计算目标体素索引（按法线偏移半格，与编辑笔刷放置逻辑一致）
	const FIntVector PlaceIndex = UChunkUtilsFunction::GetBlockIndexByPosition(
		TargetWorldPosition + TargetNormal * BlockLength / 2.0f, BlockLength);

	// CanHandleItem 已确认是方块类物品，直接执行单方块放置（含历史记录，支持撤销）
	UVoxelControllerHistoryComponent* HistoryComp = Context.HistoryComp;
	if (!HistoryComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UseItemInHand] HistoryComponent missing for block placement"));
		return false;
	}

	// 跳过场景范围外
	if (!BlockWorld->IsBlockIndexInScene(PlaceIndex))
	{
		return false;
	}

	const int32 NewType = HandData.HandleId;
	const int32 OldType = BlockWorld->GetBlockType(PlaceIndex);

	// 跳过无变化
	if (OldType == NewType)
	{
		return false;
	}

	FVoxelEditHistoryRecord HistoryRecord;
	HistoryRecord.ToolTag = SC->GetCurrentToolTag();
	HistoryRecord.EditCommand.PlacementMode = SC->GetPlacementMode();
	HistoryRecord.Timestamp = FDateTime::Now();
	HistoryRecord.Add(FVoxelEdit(PlaceIndex, OldType, NewType));
	HistoryRecord.AddChunkIndex(UChunkUtilsFunction::Get3DChunkIndexByBlockIndex(
		PlaceIndex, BlockWorld->ChunkSize));

	FVoxelEditModification Modification = HistoryRecord.CreateRedoModification();
	BlockWorld->BatchModifyVoxelData_Modification(Modification);
	HistoryComp->SaveHistoryRecord(HistoryRecord);

	UE_LOG(LogTemp, Log, TEXT("[UseItemInHand] Block placed at %s, Type=%d"),
		*PlaceIndex.ToString(), NewType);
	return true;
}

// ==================== 私有辅助方法 ====================

void UVoxelSingleVoxelEditTool::ApplyEditBrushStamp(const FVoxelToolContext& Context, const FIntVector& CenterIndex)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	UVoxelBlockWorldSubsystem* BlockWorld = Context.GetBlockWorld();
	if (!BlockWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("[EditBrush] BlockWorld is null"));
		return;
	}

	UVoxelControllerHistoryComponent* HistoryComp = Context.HistoryComp;
	if (!HistoryComp)
	{
		UE_LOG(LogTemp, Error, TEXT("[EditBrush] HistoryComponent is null"));
		return;
	}

	const float BlockLength = Context.GetBlockLength();

	// 1. 计算笔刷覆盖的体素（复用选区笔刷的形状计算）
	TArray<FIntVector> BrushVoxels = UVoxelSelectionTool::CalculateBrushSelection(
		CenterIndex,
		static_cast<float>(SC->GetBrushSize()),
		SC->GetCurrentBatchShape(),
		SC->GetBrushDimension()
	);

	if (BrushVoxels.Num() == 0)
	{
		return;
	}

	// 2. 确定目标方块类型（手中的物品；Air 表示删除）
	FInteractableData HandData = SC->GetInteractableDataInHand();
	const int32 NewType = static_cast<int32>(HandData.HandleId);

	// 3. 构造历史记录（直接基于笔刷体素，不走 BatchEditTool 的形状计算）
	FVoxelEditHistoryRecord HistoryRecord;
	HistoryRecord.ToolTag = SC->GetCurrentToolTag();
	HistoryRecord.EditCommand.PlacementMode = SC->GetPlacementMode();
	HistoryRecord.Timestamp = FDateTime::Now();
	HistoryRecord.Reserve(BrushVoxels.Num());

	for (const FIntVector& BlockIndex : BrushVoxels)
	{
		if (!BlockWorld->IsBlockIndexInScene(BlockIndex))
		{
			continue;
		}

		const int32 OldType = BlockWorld->GetBlockType(BlockIndex);

		// 跳过没有变化的体素（避免无意义的历史记录）
		if (OldType == NewType)
		{
			continue;
		}

		HistoryRecord.Add(FVoxelEdit(BlockIndex, OldType, NewType));

		const FIntVector ChunkIndex = UChunkUtilsFunction::Get3DChunkIndexByBlockIndex(
			BlockIndex, BlockWorld->ChunkSize);
		HistoryRecord.AddChunkIndex(ChunkIndex);
	}

	if (HistoryRecord.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[EditBrush] No changes at %s"), *CenterIndex.ToString());
		return;
	}

	// 4. 应用修改到世界
	FVoxelEditModification Modification = HistoryRecord.CreateRedoModification();
	BlockWorld->BatchModifyVoxelData_Modification(Modification);

	// 5. 保存到历史栈（支持撤销/重做）
	HistoryComp->SaveHistoryRecord(HistoryRecord);

	UE_LOG(LogTemp, Log, TEXT("[EditBrush] Stamp at %s, size=%d, shape=%d, voxels=%d, applied=%d"),
		*CenterIndex.ToString(), SC->GetBrushSize(), (int32)SC->GetCurrentBatchShape(),
		BrushVoxels.Num(), HistoryRecord.Num());
}

// ==================== 预览驱动 ====================

void UVoxelSingleVoxelEditTool::UpdatePreview(UVoxelPreviewComponent* PC)
{
	UVoxelPlayerStateCacheComponent* SC = PC->GetStateComponent();
	if (!SC) return;
	FVoxelEditCommand EditCommand;
	EditCommand.PlacementMode = SC->GetPlacementMode();
	EditCommand.SelectionMode = SC->GetSelectionMode();
	const FVoxelGeometryDesc& Geo = SC->GetTmpGeometry();
	FIntVector PlaceIndex = Geo.Box.MinIndex;

int32 BlockType = SC->GetInteractableDataInHand().HandleId;
	PC->UpdateSingleVoxelPreview(PlaceIndex, PlaceIndex, BlockType, EditCommand);
}


void UVoxelSingleVoxelEditTool::ConfigureGizmos(UVoxelPreviewComponent* PC, EPreviewInteractionState Phase)
{
	PC->SetPreviewActorState(Phase);
	PC->ConfigureGizmoLayout(Phase, true, true, true);
}
