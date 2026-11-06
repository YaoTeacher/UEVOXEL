#include "Voxel/Editor/Tools/VoxelCutoutEditTool.h"
#include "Voxel/Editor/VoxelToolContext.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Cutout/VoxelCutoutBoxActor.h"
#include "Data/VoxelCutoutData.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/Enum/GameStateEnum.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"

// ==================== 面板需求声明 ====================

UVoxelCutoutEditTool::UVoxelCutoutEditTool()
{
	bNeedsLibraryPanel = false;
	bNeedsDetailPanel = true;
}

FText UVoxelCutoutEditTool::GetToolDisplayName() const
{
	return NSLOCTEXT("VoxelTools", "Cutout", "透明框工具");
}

void UVoxelCutoutEditTool::OnToolActivated()
{
	Super::OnToolActivated();
	if (UWorld* World = GetWorld())
	{
		if (AVoxelPlayerController* PC = World->GetFirstPlayerController<AVoxelPlayerController>())
		{
			if (UVoxelPlayerStateCacheComponent* SC = PC->StateCacheComp)
			{
				SC->SetCutoutEditMode(true);
			}
		}
	}
}

void UVoxelCutoutEditTool::OnToolDeactivated()
{
	Super::OnToolDeactivated();
	SelectedBoxId = INDEX_NONE;
	if (UWorld* World = GetWorld())
	{
		if (AVoxelPlayerController* PC = World->GetFirstPlayerController<AVoxelPlayerController>())
		{
			if (UVoxelPlayerStateCacheComponent* SC = PC->StateCacheComp)
			{
				SC->SetCutoutEditMode(false);
			}
		}
	}
}

void UVoxelCutoutEditTool::OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 确保编辑模式开启
	SC->SetCutoutEditMode(true);

	// IDLE 态：优先选中已有框（Cutout 通道射线命中边框 FillBox）
	if (SC->GetBuildPhase() == EPreviewInteractionState::IDLE)
	{
		FHitResult BoxHit;
		FCollisionQueryParams Params;
		Params.bTraceComplex = true;
		const bool bHit = Context.GetWorld()->LineTraceSingleByChannel(
			BoxHit, GroundHit.TraceStart, GroundHit.TraceEnd, ECC_GameTraceChannel8, Params);
		if (bHit)
		{
			if (AVoxelCutoutBoxActor* HitActor = Cast<AVoxelCutoutBoxActor>(BoxHit.GetActor()))
			{
				BeginEditExisting(Context, HitActor->GetBoxId());
				return;
			}
		}
	}

	// 两点定义新框（IDLE->PLACING->EDITING）
	HandleTwoPointPhaseInput(Context, GroundHit, GizmoHitActor);
}

void UVoxelCutoutEditTool::OnMouseRelease(const FVoxelToolContext& Context)
{
	if (Context.InteractionComp)
	{
		Context.InteractionComp->ReleaseGizmoInteraction();
	}
}

void UVoxelCutoutEditTool::OnMouseCancel(const FVoxelToolContext& Context)
{
	ResetToIdle(Context);
}

void UVoxelCutoutEditTool::OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;
	SC->MoveTmpAreaCoverage(Offset);
}

void UVoxelCutoutEditTool::OnConfirmEdit(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cutout] OnConfirmEdit: early return - no BuildStateComp"));
		return;
	}
	const EPreviewInteractionState Phase = SC->GetBuildPhase();
	if (Phase != EPreviewInteractionState::EDITING)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cutout] OnConfirmEdit: early return - BuildPhase=%d (need EDITING)"), (int32)Phase);
		return;
	}

	// 与 Area 生成逻辑同源：直接用 TmpGeometry.Box（SetEditPoints/FaceGizmo 写入的框，Area 约定）
	const FAreaCoverageFragment& Box = SC->GetTmpGeometry().Box;
	const FIntVector Min = Box.MinIndex;
	const FIntVector Max = Box.MaxIndex;

	UVoxelControllerHistoryComponent* HistoryComp = Context.HistoryComp;
	UE_LOG(LogTemp, Warning, TEXT("[Cutout] OnConfirmEdit: proceed, SelectedBoxId=%d Min=%s Max=%s"),
		SelectedBoxId, *Min.ToString(), *Max.ToString());

	if (SelectedBoxId != INDEX_NONE)
	{
		SC->UpdateCutoutBox(SelectedBoxId, Min, Max, HistoryComp);
	}
	else
	{
		SC->AddCutoutBox(Min, Max, HistoryComp);
	}

	ResetToIdle(Context);
}

void UVoxelCutoutEditTool::BeginEditExisting(const FVoxelToolContext& Context, int32 BoxId)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	FCutoutBoxData Data;
	if (!SC->GetCutoutBox(BoxId, Data)) return;

	SelectedBoxId = BoxId;
	FVoxelGeometryDesc Geo = SC->GetTmpGeometry();
	Geo.Box.SetBoundingBox(Data.MinIndex, Data.MaxIndex);
	SC->SetTmpGeometry(Geo);
	SC->SetBuildPhase(EPreviewInteractionState::EDITING);
}

void UVoxelCutoutEditTool::ResetToIdle(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;
	SelectedBoxId = INDEX_NONE;
	SC->SetBuildPhase(EPreviewInteractionState::IDLE);
	if (Context.PreviewComp)
	{
		Context.PreviewComp->ClearPreview();
	}
}

// ==================== 预览驱动 ====================

void UVoxelCutoutEditTool::UpdatePreview(UVoxelPreviewComponent* PC)
{
	auto* SC = PC->GetStateComponent();
	if (!SC || !PC->PreviewActor) return;
	PC->UpdateBatchEditPreview(SC->GetCurrentEditRequest());
	PC->PreviewActor->SetInnerMeshVisible(true);
}

void UVoxelCutoutEditTool::UpdateSelectionHighlight(UVoxelPreviewComponent* PC)
{
	PC->ClearSelectionHighlight();
}

void UVoxelCutoutEditTool::ConfigureGizmos(UVoxelPreviewComponent* PC, EPreviewInteractionState Phase)
{
	PC->SetPreviewActorState(Phase);
	PC->ConfigureGizmoLayout(Phase, true, false, true);
}
