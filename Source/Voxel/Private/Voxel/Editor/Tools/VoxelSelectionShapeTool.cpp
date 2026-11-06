// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/Tools/VoxelSelectionShapeTool.h"
#include "Voxel/Editor/VoxelToolContext.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Core/CoreTags.h"
#include "Voxel/Editor/Tools/VoxelSelectionTool.h"

// ==================== 面板需求声明 ====================

UVoxelSelectionShapeTool::UVoxelSelectionShapeTool()
{
	bNeedsLibraryPanel = false;
	bNeedsDetailPanel = true;
}

// ==================== 元数据接口 ====================

FText UVoxelSelectionShapeTool::GetToolDisplayName() const
{
	return NSLOCTEXT("VoxelTools", "SelectionShape", "选区形状工具");
}

// ==================== 输入生命周期 ====================

void UVoxelSelectionShapeTool::OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	UVoxelPreviewComponent* VC = Context.PreviewComp;
	if (!SC || !VC) return;

	// 工具特定逻辑：IDLE + 有选区 + 点击位移 Gizmo → 拖拽整个选区（保持 IDLE）
	if (SC->GetBuildPhase() == EPreviewInteractionState::IDLE
		&& GizmoHitActor && SC->HasSelection()
		&& !Context.InputState->ActiveDragGizmo)
	{
		int32 DummyIndex = INDEX_NONE;
		if (VC->TranslateGizmos.Find(GizmoHitActor, DummyIndex))
		{
			const FVector WorldPosition = GroundHit.bBlockingHit ? GroundHit.Location : GroundHit.TraceStart;
			Context.InputState->ActiveDragGizmo = GizmoHitActor;
			Context.InputState->FirstPoint = WorldPosition;
			Context.InputState->SecondPoint = WorldPosition;
			return;
		}
	}

	// 两点编辑状态机（IDLE→PLACING→EDITING）收编于基类模板方法
	HandleTwoPointPhaseInput(Context, GroundHit, GizmoHitActor);
}

void UVoxelSelectionShapeTool::OnMouseRelease(const FVoxelToolContext& Context)
{
	// 选区形状工具：PLACING 松手后进入 EDITING（可继续用 Gizmo 调整几何体），确认由 OnConfirmEdit 处理
	if (Context.InteractionComp)
	{
		Context.InteractionComp->ReleaseGizmoInteraction();
	}
}

void UVoxelSelectionShapeTool::OnMouseCancel(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	EPreviewInteractionState BuildPhase = SC->GetBuildPhase();

	if (BuildPhase == EPreviewInteractionState::IDLE)
	{
		// IDLE 状态：右键清空已确认的选区（工具特定行为）
		SC->ClearSelection();
		UE_LOG(LogTemp, Log, TEXT("Interact: Selection tool cancel - selection cleared"));
	}
	else
	{
		// PLACING/EDITING 状态：调用基类逐级回退（EDITING→PLACING→IDLE）
		SC->ClearPreDragSelection();
		Super::OnMouseCancel(Context);
		UE_LOG(LogTemp, Log, TEXT("Interact: Selection tool cancel - edit discarded"));
	}
}

void UVoxelSelectionShapeTool::OnConfirmEdit(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC)
	{
		return;
	}

	// 选区工具确认：将临时形状按当前选择算法应用到 CurrentSelection
	ApplySelectionShapeImmediately(Context);
}

void UVoxelSelectionShapeTool::OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 选区工具：仅平移选区边框索引（不搬移实际数据）
	SC->MoveSelection(Offset);
}

// ==================== 私有辅助方法 ====================

void UVoxelSelectionShapeTool::ApplySelectionShapeImmediately(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 本地离散化 TmpGeometry 得到临时形状（不再依赖预览层回写的 PreDragSelection）
	const float BlockLength = Context.GetBlockLength();
	FVoxelEditRequest Request = SC->GetCurrentEditRequest();
	TArray<FIntVector> EmptySelection;
	const TArray<FIntVector> TempShape = UVoxelSelectionTool::CalculateSelection(
		Request, BlockLength, EmptySelection, ESelectionOperationType::New);

	// 按当前选择算法（New/Add/Subtract/Intersect）应用到 CurrentSelection
	SC->ApplySelectionOp(TempShape, SC->GetSelectionMode());

	// 重置编辑状态，准备下一次选择
	SC->ClearPreDragSelection();
	SC->ClearTmpGeometryTransform();
	SC->SetBuildPhase(EPreviewInteractionState::IDLE);

	UE_LOG(LogTemp, Log, TEXT("[EditChain] ApplySelectionShapeImmediately: mode=%d, tempShape=%d, current=%d"),
		(int32)SC->GetSelectionMode(), TempShape.Num(), SC->GetCurrentSelection().Num());
}

// ==================== 预览驱动 ====================

void UVoxelSelectionShapeTool::UpdatePreview(UVoxelPreviewComponent* PC)
{
	UVoxelPlayerStateCacheComponent* SC = PC->GetStateComponent();
	if (!SC || !PC->PreviewActor) return;
	PC->UpdateBatchEditPreview(SC->GetCurrentEditRequest());
	PC->PreviewActor->SetInnerMeshVisible(false);
}

void UVoxelSelectionShapeTool::UpdateSelectionHighlight(UVoxelPreviewComponent* PC)
{
	UVoxelPlayerStateCacheComponent* SC = PC->GetStateComponent();
	if (!SC) { PC->ShowCurrentSelection(); return; }
	if (SC->GetBuildPhase() != EPreviewInteractionState::IDLE)
		PC->ShowSelectionWithCalculated();
	else
		PC->ShowCurrentSelection();
}


void UVoxelSelectionShapeTool::ConfigureGizmos(UVoxelPreviewComponent* PC, EPreviewInteractionState Phase)
{
	PC->SetPreviewActorState(Phase);
	PC->ConfigureGizmoLayout(Phase, true, true, true);
}
