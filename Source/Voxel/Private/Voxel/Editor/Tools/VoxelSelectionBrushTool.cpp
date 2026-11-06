// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/Tools/VoxelSelectionBrushTool.h"
#include "Voxel/Editor/VoxelToolContext.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/Editor/VoxelPreviewActor.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Core/CoreTags.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Voxel/Editor/Tools/VoxelSelectionTool.h"

// ==================== 面板需求声明 ====================

UVoxelSelectionBrushTool::UVoxelSelectionBrushTool()
{
	bNeedsLibraryPanel = false;
	bNeedsDetailPanel = true;
}

// ==================== 元数据接口 ====================

FText UVoxelSelectionBrushTool::GetToolDisplayName() const
{
	return NSLOCTEXT("VoxelTools", "SelectionBrush", "选区笔刷工具");
}

// ==================== 输入生命周期 ====================

void UVoxelSelectionBrushTool::OnMouseHover(const FVoxelToolContext& Context, const FHitResult& GroundHit)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 拖拽中不显示 hover 预览（ApplyBrushStamp 会更新选区）
	if (Context.InputState->bIsBrushDragging) return;

	UVoxelPreviewComponent* VC = Context.PreviewComp;
	if (!VC || !VC->PreviewActor) return;

	const float BlockLength = Context.GetBlockLength();

	const FIntVector CenterIndex = UChunkUtilsFunction::GetBlockIndexByPosition(
		GroundHit.Location, BlockLength);

	// 选区笔刷预览颜色：青色（区分于编辑笔刷的绿/红）
	FLinearColor BrushColor = FLinearColor(0.0f, 0.7f, 1.0f, 1.0f);

	VC->PreviewActor->UpdateBrushPreview(
		CenterIndex,
		SC->GetBrushSize(),
		SC->GetCurrentBatchShape(),
		SC->GetBrushDimension(),
		BlockLength,
		BrushColor
	);
}

void UVoxelSelectionBrushTool::OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	UVoxelPreviewComponent* VC = Context.PreviewComp;
	if (!SC || !VC) return;

	// 获取当前建造阶段
	EPreviewInteractionState BuildPhase = SC->GetBuildPhase();

	// EDITING 状态：Gizmo 操作由 InteractionComp 保留的私有方法处理，工具不参与
	if (BuildPhase == EPreviewInteractionState::EDITING)
	{
		return;
	}

	// Brush 工具仅使用 IDLE 状态（不走 PLACING/EDITING 流程）
	if (BuildPhase != EPreviewInteractionState::IDLE)
	{
		return;
	}

	// 计算世界坐标（有地面命中用命中点，否则用射线起点作为回退）
	const FVector WorldPosition = GroundHit.bBlockingHit ? GroundHit.Location : GroundHit.TraceStart;

	// 已有活跃拖拽 Gizmo：持续处理由 InteractionComp 调用 OnGizmoTranslate，工具不重复处理
	if (Context.InputState->ActiveDragGizmo)
	{
		return;
	}

	// 选区工具：有选区 + 点击位移 Gizmo → 拖拽整个选区
	// 不进入 PLACING/EDITING，保持 IDLE 状态，由 Tick 持续调用 OnGizmoTranslate
	if (GizmoHitActor && SC->HasSelection())
	{
		int32 DummyIndex = INDEX_NONE;
		if (VC->TranslateGizmos.Find(GizmoHitActor, DummyIndex))
		{
			Context.InputState->ActiveDragGizmo = GizmoHitActor;
			Context.InputState->FirstPoint = WorldPosition;
			Context.InputState->SecondPoint = WorldPosition;
			Context.InputState->bIsBrushDragging = false; // Brush 工具关闭涂抹
			UE_LOG(LogTemp, Log, TEXT("[EditChain] IDLE - Gizmo drag start (move selection)"));
			return; // 保持 IDLE
		}
	}

	// Brush 工具：点击空地 → 涂抹（不走 PLACING/EDITING 流程）
	Context.InputState->bIsBrushDragging = true;
	Context.InputState->LastBrushStampIndex = FIntVector::NoneValue; // 重置，让第一个 stamp 一定应用
	ApplyBrushStamp(Context, WorldPosition);
}

void UVoxelSelectionBrushTool::OnMouseRelease(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// Brush 工具：结束拖拽涂抹
	Context.InputState->bIsBrushDragging = false;
	Context.InputState->LastBrushStampIndex = FIntVector::NoneValue;

	// 如果是 Gizmo 拖拽（移动选区），结束拖拽模式
	if (Context.InputState->ActiveDragGizmo)
	{
		if (Context.InteractionComp)
		{
			Context.InteractionComp->ReleaseGizmoInteraction();
		}
	}
}

void UVoxelSelectionBrushTool::OnMouseCancel(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// Gizmo 拖拽中取消：结束拖拽模式（已应用的选区保留）
	if (Context.InputState->ActiveDragGizmo)
	{
		if (Context.InteractionComp)
		{
			Context.InteractionComp->ReleaseGizmoInteraction();
		}
		UE_LOG(LogTemp, Log, TEXT("[Brush] Cancel: gizmo drag ended, selection kept (%d voxels)"),
			SC->GetCurrentSelection().Num());
	}
	else if (Context.InputState->bIsBrushDragging)
	{
		// 笔刷涂抹中取消：结束 stroke（已应用的选区保留）
		Context.InputState->bIsBrushDragging = false;
		Context.InputState->LastBrushStampIndex = FIntVector::NoneValue;
		UE_LOG(LogTemp, Log, TEXT("[Brush] Cancel: stroke ended, selection kept (%d voxels)"),
			SC->GetCurrentSelection().Num());
	}
	else
	{
		// 非拖拽状态：清空选区
		SC->ClearSelection();
	}
}

// ==================== 私有辅助方法 ====================

void UVoxelSelectionBrushTool::ApplyBrushStamp(const FVoxelToolContext& Context, const FVector& WorldPosition)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	const float BlockLength = Context.GetBlockLength();

	// 计算当前鼠标位置对应的体素索引
	const FIntVector CenterIndex = UChunkUtilsFunction::GetBlockIndexByPosition(WorldPosition, BlockLength);

	// 跳过同一体素（拖拽中鼠标没移到新体素就不重复应用）
	if (CenterIndex == Context.InputState->LastBrushStampIndex)
	{
		return;
	}

	const bool bIsFirstStamp = (Context.InputState->LastBrushStampIndex == FIntVector::NoneValue);
	Context.InputState->LastBrushStampIndex = CenterIndex;

	// 选区模式语义：
	// - New 模式：stroke 第一次 stamp 时清空选区，之后整条 stroke 按 Add 累加
	// - Add 模式：整条 stroke 按 Add 累加
	// - Subtract 模式：整条 stroke 按 Subtract 擦除
	// - Intersect：Brush 不适用，降级为 Add
	ESelectionOpMode UserMode = SC->GetSelectionMode();
	if (bIsFirstStamp && UserMode == ESelectionOpMode::New)
	{
		SC->ClearSelection();
	}

	ESelectionOpMode ApplyMode;
	if (UserMode == ESelectionOpMode::Subtract)
	{
		ApplyMode = ESelectionOpMode::Subtract;
	}
	else
	{
		// New（已清空）和 Add 都按 Add 累加；Intersect 降级为 Add
		ApplyMode = ESelectionOpMode::Add;
	}

	// 计算笔刷覆盖的体素
	TArray<FIntVector> StampVoxels = UVoxelSelectionTool::CalculateBrushSelection(
		CenterIndex,
		static_cast<float>(SC->GetBrushSize()),
		SC->GetCurrentBatchShape(),
		SC->GetBrushDimension()
	);

	if (StampVoxels.Num() > 0)
	{
		SC->ApplySelectionOp(StampVoxels, ApplyMode);
		UE_LOG(LogTemp, Log, TEXT("[Brush] Stamp at %s, size=%d, mode=%d, voxels=%d, total=%d"),
			*CenterIndex.ToString(), SC->GetBrushSize(), (int32)ApplyMode,
			StampVoxels.Num(), SC->GetCurrentSelection().Num());
	}
}

void UVoxelSelectionBrushTool::ConfigureGizmos(UVoxelPreviewComponent* PC, EPreviewInteractionState Phase)
{
	PC->ConfigureGizmoLayout(Phase, true, false, false);
}
