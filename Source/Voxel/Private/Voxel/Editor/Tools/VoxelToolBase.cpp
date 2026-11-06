// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "Voxel/Editor/VoxelPreviewActor.h"
#include "Voxel/Editor/VoxelToolContext.h"
#include "Voxel/Editor/VoxelPlacementWorldSubsystem.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Voxel/VoxelPlayerController.h"

// ==================== Placement 薄包装 ====================

UVoxelPlacementWorldSubsystem* UVoxelToolBase::GetPlacementSubsystem(const FVoxelToolContext& Context) const
{
	if (Context.PlayerController)
	{
		return Context.PlayerController->GetWorld()->GetSubsystem<UVoxelPlacementWorldSubsystem>();
	}
	return nullptr;
}

FPlacementHandle UVoxelToolBase::PlaceItemWithHistory(
	const FVoxelToolContext& Context,
	int32 ItemId,
	FIntVector Pos,
	EPlacementDirection Direction)
{
	UVoxelPlacementWorldSubsystem* Placement = GetPlacementSubsystem(Context);
	if (!Placement)
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolBase] PlaceItemWithHistory: PlacementSubsystem is null"));
		return FPlacementHandle::MakeEmpty();
	}

	// 调 PlacementSubsystem 执行放置
	FPlacementHandle Handle = Placement->Place(ItemId, Pos, Direction);

	if (Handle.IsValid())
	{
		// 自动记录历史（如果调用方有 HistoryComp）
		if (Context.HistoryComp)
		{
			Context.HistoryComp->SavePlacementOperation(Placement->GetLastOperation());
		}
	}

	return Handle;
}

FPlacementHandle UVoxelToolBase::PlaceItemWithHistory(
	const FVoxelToolContext& Context,
	int32 ItemId,
	FIntVector Pos,
	EActorDirection Direction)
{
	// EActorDirection → EPlacementDirection
	EPlacementDirection Dir = EPlacementDirection::Forward;
	switch (Direction)
	{
	case EActorDirection::Forward:  Dir = EPlacementDirection::Forward;  break;
	case EActorDirection::Right:    Dir = EPlacementDirection::Right;    break;
	case EActorDirection::Backward: Dir = EPlacementDirection::Backward; break;
	case EActorDirection::Left:     Dir = EPlacementDirection::Left;     break;
	}
	return PlaceItemWithHistory(Context, ItemId, Pos, Dir);
}

bool UVoxelToolBase::RemoveItemWithHistory(
	const FVoxelToolContext& Context,
	const FPlacementHandle& Handle)
{
	UVoxelPlacementWorldSubsystem* Placement = GetPlacementSubsystem(Context);
	if (!Placement) return false;

	if (!Placement->Remove(Handle))
	{
		return false;
	}

	// 自动记录历史
	if (Context.HistoryComp)
	{
		Context.HistoryComp->SavePlacementOperation(Placement->GetLastOperation());
	}

	return true;
}

bool UVoxelToolBase::ModifyItemWithHistory(
	const FVoxelToolContext& Context,
	const FPlacementHandle& Handle,
	const FPlacementModification& Modification)
{
	UVoxelPlacementWorldSubsystem* Placement = GetPlacementSubsystem(Context);
	if (!Placement) return false;

	if (!Placement->ModifyPlacement(Handle, Modification))
	{
		return false;
	}

	// 自动记录历史
	if (Context.HistoryComp)
	{
		Context.HistoryComp->SavePlacementOperation(Placement->GetLastOperation());
	}

	return true;
}

FPlacementHandle UVoxelToolBase::QueryItemAt(
	const FVoxelToolContext& Context,
	FIntVector Pos) const
{
	if (UVoxelPlacementWorldSubsystem* Placement = GetPlacementSubsystem(Context))
	{
		return Placement->QueryAt(Pos);
	}
	return FPlacementHandle::MakeEmpty();
}

bool UVoxelToolBase::CanPlaceItem(
	const FVoxelToolContext& Context,
	int32 ItemId,
	FIntVector Pos,
	EPlacementDirection Direction) const
{
	if (UVoxelPlacementWorldSubsystem* Placement = GetPlacementSubsystem(Context))
	{
		return Placement->CanPlaceAt(ItemId, Pos, Direction);
	}
	return false;
}

// ==================== 两点编辑状态机（模板方法） ====================

bool UVoxelToolBase::HandleTwoPointPhaseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return true;

	const EPreviewInteractionState BuildPhase = SC->GetBuildPhase();

	// EDITING：首次点击 Gizmo 时激活拖拽，持续拖拽由 InteractionComp Tick 用平面投影处理
	if (BuildPhase == EPreviewInteractionState::EDITING)
	{
		if (Context.InteractionComp && GizmoHitActor && !Context.InputState->ActiveDragGizmo)
		{
			const FVector WorldPos = GroundHit.bBlockingHit ? GroundHit.Location : GroundHit.TraceStart;
			Context.InteractionComp->HandleGizmoInteraction(GizmoHitActor, WorldPos);
		}
		return true;
	}

	// 计算世界坐标（有地面命中用命中点，否则用射线起点作为回退）
	const FVector WorldPosition = GroundHit.bBlockingHit ? GroundHit.Location : GroundHit.TraceStart;

	// IDLE：第一点点击后直接进 PLACING
	if (BuildPhase == EPreviewInteractionState::IDLE)
	{
		if (Context.InputState->ActiveDragGizmo)
		{
			return true;
		}

		Context.InputState->FirstPoint = WorldPosition;
		Context.InputState->SecondPoint = WorldPosition;
		Context.InputState->bIsModifying = true;
		SC->SetEditPoints(Context.InputState->FirstPoint, Context.InputState->SecondPoint);
		SC->SetBuildPhase(EPreviewInteractionState::PLACING);
		return true;
	}

	// PLACING 阶段的拖拽更新由 OnMouseDrag -> HandleTwoPointDragInput 处理（IA Triggered 驱动）
	return false;
}

// ==================== 两点编辑拖拽（OnMouseDrag） ====================

void UVoxelToolBase::OnMouseDrag(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor)
{
	// 默认：两点编辑 PLACING 阶段更新第二点。非 PLACING 阶段（单体素/笔刷/IDLE/EDITING）为空操作。
	HandleTwoPointDragInput(Context, GroundHit);
}

bool UVoxelToolBase::HandleTwoPointDragInput(const FVoxelToolContext& Context, const FHitResult& GroundHit)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return false;

	// 仅 PLACING 阶段处理拖拽
	if (SC->GetBuildPhase() != EPreviewInteractionState::PLACING)
	{
		return false;
	}

	if (!Context.InputState->bIsModifying)
	{
		return false;
	}

	const FVector WorldPosition = GroundHit.bBlockingHit ? GroundHit.Location : GroundHit.TraceStart;
	Context.InputState->SecondPoint = WorldPosition;
	SC->SetEditPoints(Context.InputState->FirstPoint, Context.InputState->SecondPoint);
	return true;
}

// ==================== 右键取消默认实现 ====================

void UVoxelToolBase::OnMouseCancel(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 统一取消：任何状态 → IDLE + 清空手持 + 清空预览
	// 对标双点/模拟人生：右键 = 取消整个操作，不逐级回退
	if (Context.InteractionComp)
	{
		Context.InteractionComp->CancelCurrentEdit();
	}
	SC->SetBuildPhase(EPreviewInteractionState::IDLE);

	// 清空手持数据（方块/家具/模板）
	FInteractableData EmptyHand;
	if (SC->GetInteractableDataInHand().HandleId != EBlockId::Invalid)
	{
		SC->SetInteractableDataInHand(EmptyHand);
	}

	// 清空 PreviewActor 的 Mesh 数据
	if (Context.PreviewComp)
	{
		Context.PreviewComp->ClearPreview();
	}

	UE_LOG(LogTemp, Log, TEXT("[EditChain] %s OnMouseCancel: -> IDLE (canceled)"), *GetToolID().ToString());
}

// ==================== 预览驱动默认实现 ====================

void UVoxelToolBase::UpdatePreview(UVoxelPreviewComponent* PreviewComp)
{
	// 默认：无 PreviewActor 预览。子类按需 override。
}

void UVoxelToolBase::UpdateSelectionHighlight(UVoxelPreviewComponent* PreviewComp)
{
	// 默认：显示当前选区高亮（大多数工具共用）
	if (PreviewComp) PreviewComp->ShowCurrentSelection();
}

void UVoxelToolBase::ConfigureGizmos(UVoxelPreviewComponent* PreviewComp, EPreviewInteractionState Phase)
{
	// 默认：不配置 Gizmo（由 SetPreviewState 旧逻辑处理，待后续迁移）
}
