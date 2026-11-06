// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/Tools/VoxelSimpleGeometryEditTool.h"
#include "Voxel/Editor/VoxelToolContext.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Voxel/Editor/VoxelHistoryData.h"
#include "Core/CoreTags.h"
#include "Utils/ChunkUtilsFunction.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "Voxel/Editor/Tools/VoxelBatchEditTool.h"
#include "Core/Enum/GameStateEnum.h"

// ==================== 面板需求声明 ====================

UVoxelSimpleGeometryEditTool::UVoxelSimpleGeometryEditTool()
{
	bNeedsLibraryPanel = true;
	bNeedsDetailPanel = true;
}

// ==================== 元数据接口 ====================

FText UVoxelSimpleGeometryEditTool::GetToolDisplayName() const
{
	return NSLOCTEXT("VoxelTools", "SimpleGeometryEdit", "几何体编辑");
}

// ==================== 输入生命周期 ====================

void UVoxelSimpleGeometryEditTool::OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor)
{
	// 两点编辑状态机（IDLE→PLACING→EDITING）收编于基类模板方法
	HandleTwoPointPhaseInput(Context, GroundHit, GizmoHitActor);
}

void UVoxelSimpleGeometryEditTool::OnMouseRelease(const FVoxelToolContext& Context)
{
	// 默认分支：释放 Gizmo 交互（BASE → EDIT 转换在 ReleaseGizmoInteraction 内完成）
	if (Context.InteractionComp)
	{
		Context.InteractionComp->ReleaseGizmoInteraction();
	}
}

void UVoxelSimpleGeometryEditTool::OnConfirmEdit(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC)
	{
		return;
	}

	UVoxelBlockWorldSubsystem* BlockWorld = Context.GetBlockWorld();
	if (!BlockWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("[EditChain] RequestConfirmVoxelEdit: BlockWorldSubsystem not found"));
		return;
	}

	UVoxelControllerHistoryComponent* HistoryComp = Context.HistoryComp;
	if (!HistoryComp)
	{
		UE_LOG(LogTemp, Error, TEXT("[EditChain] RequestConfirmVoxelEdit: HistoryComponent not found"));
		return;
	}

	FVoxelEditRequest EditRequest = SC->GetCurrentEditRequest();

	// 默认分支：构造历史记录并应用
	FVoxelEditHistoryRecord HistoryRecord = UVoxelBatchEditTool::CreateHistoryRecord(EditRequest, BlockWorld);

	if (HistoryRecord.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EditChain] RequestConfirmVoxelEdit: No valid blocks to edit"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[EditChain] RequestConfirmVoxelEdit: Executing %d voxels"), HistoryRecord.Num());

	FVoxelEditModification Modification = HistoryRecord.CreateRedoModification();
	BlockWorld->BatchModifyVoxelData_Modification(Modification);
	HistoryComp->SaveHistoryRecord(HistoryRecord);
}

void UVoxelSimpleGeometryEditTool::OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	FVoxelGeometryDesc Geometry = SC->GetTmpGeometry();
	Geometry.MoveGEO(Offset);
	SC->SetTmpGeometry(Geometry);
}
// ==================== 预览驱动 ====================

void UVoxelSimpleGeometryEditTool::UpdatePreview(UVoxelPreviewComponent* PC)
{
	UVoxelPlayerStateCacheComponent* SC = PC->GetStateComponent();
	if (!SC || !PC->PreviewActor) return;

	const FVoxelEditRequest Request = SC->GetCurrentEditRequest();
	const float BlockLength = UChunkUtilsFunction::GetBlockLength(PC);

	// 壳模式（Thickness != 0）：渲染真实体素预览——按面子集/厚度算出精确体素集合，
	// 用 HandleId 对应的真实方块材质渲染（修复整壳近似导致的「勾面不刷新、无材质」）
	if (Request.Geometry.Thickness != 0)
	{
		PC->UpdateBatchEditPreview(Request); // 外框 + Gizmo 定位
		TArray<FIntVector> ShellVoxels = UVoxelBatchEditTool::CalculateAffectedVoxelsForPreview(Request, BlockLength);
		PC->PreviewActor->UpdateSimpleGeometryPreviewWithSelection(
			Request, BlockLength, ShellVoxels, Request.HandleId);
		PC->PreviewActor->SetInnerMeshVisible(false); // 隐藏整壳近似填充，只留外框+真实体素
		return;
	}

	PC->UpdateBatchEditPreview(Request);
	PC->PreviewActor->SetInnerMeshVisible(true);
}


void UVoxelSimpleGeometryEditTool::ConfigureGizmos(UVoxelPreviewComponent* PC, EPreviewInteractionState Phase)
{
	PC->SetPreviewActorState(Phase);
	PC->ConfigureGizmoLayout(Phase, true, true, true);
}
