// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/Tools/VoxelFurnitureEditTool.h"
#include "Voxel/Editor/VoxelToolContext.h"
#include "Voxel/Editor/VoxelPlacementTypes.h"
#include "GameManager/WorldManager/VoxelItemWorldSubsystem.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/Enum/GameStateEnum.h"
#include "Core/CoreTags.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Item/VoxelItemBaseInstance.h"
#include "Data/Item/VoxelItemBase.h"
#include "Data/Item/InteractableData.h"

// ==================== 面板需求声明 ====================

UVoxelFurnitureEditTool::UVoxelFurnitureEditTool()
{
	bNeedsLibraryPanel = true;
	bNeedsDetailPanel = true;
}

// ==================== UVoxelToolBase 接口实现 ====================

FText UVoxelFurnitureEditTool::GetToolDisplayName() const
{
	return NSLOCTEXT("VoxelTools", "FurnitureEdit", "家具放置");
}

FText UVoxelFurnitureEditTool::GetToolDescription() const
{
	return NSLOCTEXT("VoxelTools", "FurnitureEditDesc",
		"家具放置工具：在指定网格位置生成家具 Actor"
	);
}

EVoxelToolCategory UVoxelFurnitureEditTool::GetToolCategory() const
{
	return EVoxelToolCategory::Edit;
}

FName UVoxelFurnitureEditTool::GetToolID() const
{
	return FName("FurnitureEdit");
}

// ==================== 输入生命周期（override） ====================

void UVoxelFurnitureEditTool::OnMouseHover(const FVoxelToolContext& Context, const FHitResult& GroundHit)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;
	if (!GroundHit.bBlockingHit) return;

	// 拖拽中不更新 hover 位置
	if (Context.InputState->bIsModifying) return;

	const float BlockLength = Context.GetBlockLength();

	// 家具预览状态机（与 VoxelTemplateEditTool 一致）：
	//   IDLE + 有效手持 → 自动进入 PLACING（跟随鼠标）
	//   PLACING  → 跟随鼠标更新 TmpGeometry.Box
	//   EDITING  → 不更新（由 Gizmo / DetailPanel 控制）
	EPreviewInteractionState Phase = SC->GetBuildPhase();
	const FInteractableData& HandData = SC->GetInteractableDataInHand();

	// IDLE + 有效家具 → 切换到 PLACING，开始跟随鼠标
	if (Phase == EPreviewInteractionState::IDLE && HandData.HandleId != EBlockId::Invalid)
	{
		SC->SetBuildPhase(EPreviewInteractionState::PLACING);
		Phase = EPreviewInteractionState::PLACING;
	}

	// PLACING 状态：沿法线偏移半格，放到方块上方空气格
	if (Phase == EPreviewInteractionState::PLACING)
	{
		const FIntVector PlaceIndex = UChunkUtilsFunction::GetBlockIndexByPosition(
			GroundHit.ImpactPoint + GroundHit.Normal * BlockLength / 2.0f, BlockLength);

		// 位置去重：只有 PlaceIndex 变化时才更新，避免每帧广播 OnBuildModeParamsUpdated
		const FVoxelGeometryDesc& CurGeom = SC->GetTmpGeometry();
		if (CurGeom.Box.MinIndex == PlaceIndex)
		{
			return;
		}

		FVoxelGeometryDesc Geometry = CurGeom;
		Geometry.Box.SetBoundingBox(PlaceIndex, PlaceIndex);
		SC->SetTmpGeometry(Geometry);
	}
}

void UVoxelFurnitureEditTool::OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;
	if (!GroundHit.bBlockingHit) return;

	// 沿法线偏移半格，放到方块上方的空气格（与 OnMouseHover / UseItemInHand 一致）
	const float BlockLength = Context.GetBlockLength();
	const FIntVector PlaceIndex = UChunkUtilsFunction::GetBlockIndexByPosition(
		GroundHit.ImpactPoint + GroundHit.Normal * BlockLength / 2.0f, BlockLength);

	// 更新 TmpGeometry（预览位置同步）
	FVoxelGeometryDesc Geometry = SC->GetTmpGeometry();
		Geometry.Box.SetBoundingBox(PlaceIndex, PlaceIndex);
	SC->SetTmpGeometry(Geometry);

	// PLACING 状态下点击 → 直接放置家具（家具不需要 EDITING 状态，朝向用旋转键调整）
	if (SC->GetBuildPhase() == EPreviewInteractionState::PLACING)
	{
		FInteractableData HandData = SC->GetInteractableDataInHand();
		if (HandData.HandleId == EBlockId::Invalid) return;

		const EActorDirection Direction = UChunkUtilsFunction::RotationToDirection(SC->GetTmpGeometry().Rotation);
		const FPlacementHandle Handle = PlaceItemWithHistory(Context, HandData.HandleId, PlaceIndex, Direction);

		UE_LOG(LogTemp, Log, TEXT("[FurnitureEdit] Click place at %s, result=%d"),
			*PlaceIndex.ToString(), Handle.IsValid() ? 1 : 0);

		// 放置成功后，若是从拾取的家具放置，通知 InteractionComp 清理原 Actor
		if (Handle.IsValid() && Context.InteractionComp)
		{
			Context.InteractionComp->OnFurniturePickedUpPlaced();
		}
	}
}

void UVoxelFurnitureEditTool::OnConfirmEdit(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	UVoxelItemWorldSubsystem* ItemWorld = Context.GetItemWorld();
	if (!ItemWorld) return;

	// 委托静态方法执行家具放置
	FVoxelEditRequest EditRequest = SC->GetCurrentEditRequest();
	PlaceFurniture(EditRequest, SC, ItemWorld);
}

bool UVoxelFurnitureEditTool::CanHandleItem(const FGameplayTag& ItemTag) const
{
	// 家具类物品统一通过 Item.Type.BuildItem 标签识别
	return ItemTag.MatchesTag(FCoreTags::Item_Type_BuildItem);
}

bool UVoxelFurnitureEditTool::UseItemInHand(const FVoxelToolContext& Context, FVector TargetWorldPosition, FVector TargetNormal)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return false;

	// 1. 取手持道具数据
	FInteractableData HandData = SC->GetInteractableDataInHand();
	if (HandData.HandleId == EBlockId::Invalid)
	{
		return false;
	}

	// 2. 计算目标体素索引（按法线偏移半格）
	const float BlockLength = Context.GetBlockLength();
	const FIntVector PlaceIndex = UChunkUtilsFunction::GetBlockIndexByPosition(
		TargetWorldPosition + TargetNormal * BlockLength / 2.0f, BlockLength);

	// 3. 通过统一放置服务放置（自动记录历史）
	const EActorDirection Direction = UChunkUtilsFunction::RotationToDirection(SC->GetTmpGeometry().Rotation);
	const FPlacementHandle Handle = PlaceItemWithHistory(Context, HandData.HandleId, PlaceIndex, Direction);

	UE_LOG(LogTemp, Log, TEXT("[UseItemInHand] Furniture place at %s, result=%d"),
		*PlaceIndex.ToString(), Handle.IsValid() ? 1 : 0);
	return Handle.IsValid();
}

// ==================== 家具相关工具方法（迁移自 InteractionComp） ====================

void UVoxelFurnitureEditTool::UpdateFurniturePosition(const FVoxelToolContext& Context, FIntVector Position)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	UVoxelPreviewComponent* VC = Context.PreviewComp;
	if (!SC) return;

	const float BlockLength = Context.GetBlockLength();

	// 更新预览 Actor
	if (VC && VC->PreviewActor)
	{
		int32 BlockType = SC->GetInteractableDataInHand().HandleId;
		VC->PreviewActor->UpdateSingleVoxelPreview(
			Position,
			BlockType,
			FLinearColor::Green,
			BlockLength
		);
	}

	// 更新 TmpGeometry 的位置（统一使用 Geometry 结构体）
	FVoxelGeometryDesc Geometry = SC->GetTmpGeometry();
	Geometry.Box.SetBoundingBox(Position, Position);
	SC->SetTmpGeometry(Geometry);
}

void UVoxelFurnitureEditTool::RotateFurniture(const FVoxelToolContext& Context, EActorDirection Direction)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	UVoxelPreviewComponent* VC = Context.PreviewComp;
	if (!SC) return;

	// 更新手持道具的方向
	FInteractableData Data = SC->GetInteractableDataInHand();
	Data.Direction = Direction;
	SC->SetInteractableDataInHand(Data);

	// 更新 TmpGeometry.Rotation（统一使用 Geometry 结构体）
	FVoxelGeometryDesc Geometry = SC->GetTmpGeometry();
	Geometry.Rotation = UChunkUtilsFunction::GetSpawnDirection(Direction);
	SC->SetTmpGeometry(Geometry);

	// 旋转预览 Actor
	if (VC && VC->PreviewActor)
	{
		VC->PreviewActor->RotatePreview(FRotator(0, UChunkUtilsFunction::GetSpawnDirection(Direction).Yaw, 0));
	}
}

FBox UVoxelFurnitureEditTool::GetFurnitureBounds(const FVoxelToolContext& Context, FIntVector Position, EActorDirection Direction)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return FBox();

	UConfigDataSubsystem* CDS = Context.GetConfigData();
	if (!CDS)
	{
		UE_LOG(LogTemp, Error, TEXT("ConfigDataSubsystem 未就绪"));
		return FBox();
	}

	FVoxelNormalItemBase* tmpInform = CDS->GetItemTypeById<FVoxelNormalItemBase>(SC->GetInteractableDataInHand().HandleId);
	if (tmpInform == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load item information!"));
		return FBox();
	}

	const float BlockLength = Context.GetBlockLength();
	const FVector RotatedVolume = UChunkUtilsFunction::GetRotationVolume(Direction, FVector(tmpInform->GetVoxelVolume(BlockLength)));
	const FVector Min = FVector(Position);
	const FVector Max = Min + RotatedVolume;
	return UChunkUtilsFunction::UpdateBox(FBox(Min, Min), Max);
}

// ==================== 家具放置 ====================

bool UVoxelFurnitureEditTool::PlaceFurniture(
	const FVoxelEditRequest& EditRequest,
	UVoxelPlayerStateCacheComponent* BuildStateComp,
	UVoxelItemWorldSubsystem* ItemWorld)
{
	if (!BuildStateComp || !ItemWorld) return false;

	FIntVector Position = EditRequest.Geometry.Box.MinIndex;
	EActorDirection Direction = UChunkUtilsFunction::RotationToDirection(EditRequest.Geometry.Rotation);

	// 更新手持道具的 Direction（保持 BuildStateComp 同步）
	FInteractableData Data = BuildStateComp->GetInteractableDataInHand();
	Data.Direction = Direction;
	BuildStateComp->SetInteractableDataInHand(Data);

	// 调用 ItemWorld 生成家具 Actor
	AUVoxelItemBaseInstance* NewItem = ItemWorld->SpawnItemAtGrid(Position, Data, false);
	if (NewItem)
	{
		UE_LOG(LogTemp, Log, TEXT("[FurnitureEditTool] Furniture placed at %s, dir=%d"),
			*Position.ToString(), (int32)Direction);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[FurnitureEditTool] Failed to place furniture at %s"), *Position.ToString());
	return false;
}

void UVoxelFurnitureEditTool::UpdatePreview(UVoxelPreviewComponent* PC)
{
	PC->UpdateFurniturePreview();
}


void UVoxelFurnitureEditTool::ConfigureGizmos(UVoxelPreviewComponent* PC, EPreviewInteractionState Phase)
{
	PC->SetPreviewActorState(Phase);
	PC->ConfigureGizmoLayout(Phase, true, true, true);
}
