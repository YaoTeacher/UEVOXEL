// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/Tools/VoxelTemplateEditTool.h"
#include "Voxel/Editor/VoxelToolContext.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelItemWorldSubsystem.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/Enum/GameStateEnum.h"
#include "Core/CoreTags.h"
#include "Voxel/Editor/UVoxelTemplateUtils.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Data/Item/VoxelItemBase.h"
#include "Data/Item/InteractableData.h"

// ==================== 面板需求声明 ====================

UVoxelTemplateEditTool::UVoxelTemplateEditTool()
{
	bNeedsLibraryPanel = true;
	bNeedsDetailPanel = true;
}

// ==================== UVoxelToolBase 接口实现 ====================

FText UVoxelTemplateEditTool::GetToolDisplayName() const
{
	return NSLOCTEXT("VoxelTools", "TemplateEdit", "模板放置");
}

FText UVoxelTemplateEditTool::GetToolDescription() const
{
	return NSLOCTEXT("VoxelTools", "TemplateEditDesc",
		"模板放置工具：批量生成方块和家具实例"
	);
}

EVoxelToolCategory UVoxelTemplateEditTool::GetToolCategory() const
{
	return EVoxelToolCategory::Edit;
}

FName UVoxelTemplateEditTool::GetToolID() const
{
	return FName("TemplateEdit");
}

// ==================== 输入生命周期（override） ====================

void UVoxelTemplateEditTool::OnMouseHover(const FVoxelToolContext& Context, const FHitResult& GroundHit)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	const float BlockLength = Context.GetBlockLength();

	// 模板预览状态机：
	//   HIDDEN + 有效手持 → 自动进入 BASE（跟随鼠标）
	//   BASE  → 跟随鼠标更新 TmpGeometry.Box.MinIndex
	//   EDIT  → 不更新（由 Gizmo / DetailPanel 控制）
	EPreviewInteractionState Phase = SC->GetBuildPhase();
	const FInteractableData& HandData = SC->GetInteractableDataInHand();

	// HIDDEN + 有效模板 → 切换到 BASE，开始跟随鼠标
	if (Phase == EPreviewInteractionState::IDLE && HandData.HandleId != EBlockId::Invalid)
	{
		SC->SetBuildPhase(EPreviewInteractionState::PLACING);
		Phase = EPreviewInteractionState::PLACING;
	}

	// BASE 状态：跟随鼠标更新位置（EDIT 状态不更新，由 Gizmo 控制）
	if (Phase == EPreviewInteractionState::PLACING)
	{
		const FIntVector BlockIndex = UChunkUtilsFunction::GetBlockIndexByPosition(
			GroundHit.ImpactPoint, BlockLength);

		// 位置去重：只有 BlockIndex 变化时才更新，避免每帧广播 OnBuildModeParamsUpdated
		const FVoxelGeometryDesc& CurGeom = SC->GetTmpGeometry();
		if (CurGeom.Box.MinIndex == BlockIndex)
		{
			return;
		}

		FVoxelGeometryDesc Geometry = CurGeom;
		Geometry.Box.SetBoundingBox(BlockIndex, BlockIndex);
		SC->SetTmpGeometry(Geometry);
	}
}

void UVoxelTemplateEditTool::OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	UVoxelPreviewComponent* VC = Context.PreviewComp;
	FVoxelInputState* InputState = Context.InputState;
	if (!SC || !VC || !InputState) return;

	EPreviewInteractionState BuildPhase = SC->GetBuildPhase();

	if (BuildPhase == EPreviewInteractionState::PLACING)
	{
		// BASE → 点击地面确认位置 → EDIT
		// 位置已在 Hover 时写入 TmpGeometry，无需重复设置
		if (!GroundHit.bBlockingHit) return;
		SC->SetBuildPhase(EPreviewInteractionState::EDITING);
	}
	else if (BuildPhase == EPreviewInteractionState::EDITING)
	{
		// EDIT 阶段：首次点击 Gizmo 时激活拖拽，持续拖拽由 Tick 用平面投影处理
		// HandleGizmoInteraction 负责设置 ActiveDragGizmo 并分发到 Translate/Rotate/Face
		if (Context.InteractionComp && GizmoHitActor && !InputState->ActiveDragGizmo)
		{
			const FVector WorldPos = GroundHit.bBlockingHit ? GroundHit.Location : GroundHit.TraceStart;
			Context.InteractionComp->HandleGizmoInteraction(GizmoHitActor, WorldPos);
		}
		return;
	}
}

void UVoxelTemplateEditTool::OnConfirmEdit(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	UVoxelBlockWorldSubsystem* BlockSys = Context.GetBlockWorld();
	UVoxelItemWorldSubsystem* ItemWorld = Context.GetItemWorld();
	UConfigDataSubsystem* ConfigSys = Context.GetConfigData();
	if (!BlockSys || !ConfigSys) return;

	if (!ItemWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("[TemplatePlace] ItemWorld is null! Furniture will be skipped."));
	}

	FVoxelEditRequest EditRequest = SC->GetCurrentEditRequest();

	const bool bPlaced = PlaceTemplate(EditRequest, BlockSys, ItemWorld, ConfigSys, SC->GetTemplateRemap());

	// 放置后保持 EDIT 状态：不隐藏预览、不清几何、不清替换表。
	// 用户可以连续放置同一模板，预览继续显示在当前位置。
	UE_LOG(LogTemp, Log, TEXT("[TemplatePlace] bPlaced=%d, keeping EDIT state (preview preserved)"), bPlaced ? 1 : 0);
}

void UVoxelTemplateEditTool::OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 模板工具：平移模板实例位置（TmpGeometry.Box.MinIndex 即模板中心点）
	FVoxelGeometryDesc Geo = SC->GetTmpGeometry();
	Geo.Box.SetBoundingBox(Geo.Box.MinIndex + Offset, Geo.Box.MaxIndex + Offset);
	SC->SetTmpGeometry(Geo);
}

bool UVoxelTemplateEditTool::CanHandleItem(const FGameplayTag& ItemTag) const
{
	// 模板类物品统一通过 Item.Type.Template 标签识别
	return ItemTag.MatchesTag(FCoreTags::Item_Type_Template);
}

bool UVoxelTemplateEditTool::UseItemInHand(const FVoxelToolContext& Context, FVector TargetWorldPosition, FVector TargetNormal)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return false;

	const FInteractableData HandData = SC->GetInteractableDataInHand();

	// 计算目标体素索引（仅用于日志输出，与原逻辑保持一致）
	const float BlockLength = Context.GetBlockLength();
	const FIntVector PlaceIndex = UChunkUtilsFunction::GetBlockIndexByPosition(
		TargetWorldPosition + TargetNormal * BlockLength / 2.0f, BlockLength);

	// 模板不允许 UseItemInHand 直接落地。
	// 模板工具使用状态机：BASE 点击只进入 EDIT；最终放置统一由 M15/Confirm 调 OnConfirmEdit。
	UE_LOG(LogTemp, Warning, TEXT("[UseItemInHand] Template direct placement ignored. TemplateId=%d PlaceIndex=%s. Use M15/Confirm path instead."),
		HandData.HandleId, *PlaceIndex.ToString());
	return false;
}

// ==================== 模板放置 ====================

bool UVoxelTemplateEditTool::PlaceTemplate(
	const FVoxelEditRequest& EditRequest,
	UVoxelBlockWorldSubsystem* BlockWorld,
	UVoxelItemWorldSubsystem* ItemWorld,
	UConfigDataSubsystem* ConfigSys,
	const TMap<FString, FString>& Remap)
{
	if (!BlockWorld || !ConfigSys) return false;

	// 从模板仓库取出模板（独立于配置物品 ItemMap）
	const FVoxelTemplate* Tmpl = ConfigSys->GetTemplateById(EditRequest.HandleId);
	if (!Tmpl)
	{
		UE_LOG(LogTemp, Error, TEXT("[TemplateEditTool] Template not found, Handle=%d"), EditRequest.HandleId);
		return false;
	}

	// EditRequest.Geometry.Box.MinIndex 存储的是模板 XY 中心点；转换成实际放置原点
	const FIntVector CenterIndex = EditRequest.Geometry.Box.MinIndex;
	FIntVector Origin = UVoxelTemplateUtils::GetCenteredPlacementOrigin(*Tmpl, CenterIndex);
	EActorDirection Direction = UChunkUtilsFunction::RotationToDirection(EditRequest.Geometry.Rotation);

	UE_LOG(LogTemp, Warning, TEXT("[TemplatePlace] Source=Confirm TemplateId=%d Center=%s Origin=%s Direction=%d Remap=%d Entries=%d"),
		EditRequest.HandleId,
		*CenterIndex.ToString(),
		*Origin.ToString(),
		(int32)Direction,
		Remap.Num(),
		Tmpl->Entries.Num());

	bool bSuccess = UVoxelTemplateUtils::PlaceTemplate(
		BlockWorld, ItemWorld, ConfigSys, *Tmpl, Origin, Direction, Remap);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("[TemplateEditTool] Template placed at %s, dir=%d"),
			*Origin.ToString(), (int32)Direction);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[TemplateEditTool] PlaceTemplate failed"));
	}

	return bSuccess;
}

void UVoxelTemplateEditTool::UpdatePreview(UVoxelPreviewComponent* PC)
{
	PC->UpdateTemplatePreview();
}


void UVoxelTemplateEditTool::ConfigureGizmos(UVoxelPreviewComponent* PC, EPreviewInteractionState Phase)
{
	PC->SetPreviewActorState(Phase);
	PC->ConfigureGizmoLayout(Phase, true, true, false);
}
