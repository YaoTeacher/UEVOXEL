// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/Tools/VoxelAreaEditTool.h"
#include "Voxel/Editor/VoxelToolContext.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Core/CoreTags.h"
#include "Utils/ChunkUtilsFunction.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Data/Area/VoxelAreaBase.h"
#include "Data/VoxelEditCommand.h"
#include "Voxel/Editor/VoxelHistoryData.h"
#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"
#include "Core/VoxelGameInstance.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Misc/DateTime.h"

// ==================== 面板需求声明 ====================

UVoxelAreaEditTool::UVoxelAreaEditTool()
{
	// 区域工具控制几何数据与墙壁生成；区域类型通过库面板的 M12b 区域类型库选择，业务数据（名称/所有人等）由实体详情维护
	bNeedsLibraryPanel = true;
	bNeedsDetailPanel = true;
}

// ==================== 元数据接口 ====================

FText UVoxelAreaEditTool::GetToolDisplayName() const
{
	return NSLOCTEXT("VoxelTools", "AreaEdit", "区域编辑工具");
}

// ==================== 输入生命周期 ====================

void UVoxelAreaEditTool::OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// Gizmo 优先：EDITING 阶段命中 Gizmo → 交给两点状态机激活拖拽
	if (SC->GetBuildPhase() == EPreviewInteractionState::EDITING && GizmoHitActor)
	{
		HandleTwoPointPhaseInput(Context, GroundHit, GizmoHitActor);
		return;
	}

	// 已有区域优先：射线命中已有区域 → 选中它（含切换选中），不进入创建流程
	// PLACING 拖拽画框中不检测（点击语义已被两点编辑占用）
	if (SC->GetBuildPhase() != EPreviewInteractionState::PLACING)
	{
		if (AVoxelAreaBaseInstance* HitArea = TraceExistingArea(Context, GroundHit))
		{
			SelectArea(Context, HitArea);
			return;
		}
	}

	// Gizmo 流程（IDLE→PLACING→EDITING 定义新区域包围盒）
	// 两点编辑状态机收编于基类模板方法
	HandleTwoPointPhaseInput(Context, GroundHit, GizmoHitActor);
}

AVoxelAreaBaseInstance* UVoxelAreaEditTool::TraceExistingArea(const FVoxelToolContext& Context, const FHitResult& GroundHit) const
{
	if (!Context.GetWorld() || !GroundHit.bBlockingHit) return nullptr;

	// 多命中遍历：相机处于父区域盒子内部时，首个命中是父区域的贯穿命中（bStartPenetrating），
	// 该命中是容器噪声而非视觉目标，需跳过取后方真实目标（子区域/同级区域）。
	// 旧的单命中实现因贯穿命中不满足 ParentArea==CurrentParentArea 而落入创建流程，
	// 导致「点子区域却创建新区域」。
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;

	if (!Context.GetWorld()->LineTraceMultiByChannel(
		Hits, GroundHit.TraceStart, GroundHit.TraceEnd, ECC_GameTraceChannel3, Params))
	{
		return nullptr;
	}

	AVoxelAreaBaseInstance* ParentContext = Context.StateCacheComp ? Context.StateCacheComp->GetCurrentParentArea() : nullptr;
	for (const FHitResult& Hit : Hits)
	{
		AVoxelAreaBaseInstance* HitArea = Cast<AVoxelAreaBaseInstance>(Hit.GetActor());
		if (!HitArea) continue;

		// 跳过相机所在的父区域容器（贯穿命中，真实目标在其后方）
		if (Hit.bStartPenetrating && HitArea == ParentContext) continue;

		return HitArea;
	}
	return nullptr;
}

void UVoxelAreaEditTool::OnMouseRelease(const FVoxelToolContext& Context)
{
	if (Context.InteractionComp)
	{
		Context.InteractionComp->ReleaseGizmoInteraction();
	}
}

void UVoxelAreaEditTool::OnMouseCancel(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 有选中区域（任意阶段）：右键 = 清空 state 的选中区域
	// 旧逻辑 EDITING 时只重置编辑状态、CurArea 残留，需二次右键才真正取消选中
	if (SC->GetCurrentArea() != nullptr)
	{
		if (SC->GetBuildPhase() != EPreviewInteractionState::IDLE)
		{
			// 先统一取消编辑状态（重置预览/临时数据），再清空选中
			Super::OnMouseCancel(Context);
		}
		SC->ClearCurrentArea();
		UE_LOG(LogTemp, Log, TEXT("Interact: Right-click cleared current area selection"));
		return;
	}

	EPreviewInteractionState BuildPhase = SC->GetBuildPhase();
	if (BuildPhase == EPreviewInteractionState::IDLE)
	{
		// IDLE 状态：回退区域层级（工具特定行为）
		ReturnToParentArea(Context);
		UE_LOG(LogTemp, Log, TEXT("Interact: Return to parent area (BuildPhase=IDLE)"));
	}
	else
	{
		// PLACING/EDITING 状态：调用基类统一取消
		Super::OnMouseCancel(Context);
	}
}

void UVoxelAreaEditTool::OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 区域编辑工具：移动临时区域覆盖范围
	SC->MoveTmpAreaCoverage(Offset);
}

void UVoxelAreaEditTool::OnConfirmEdit(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 仅在 EDITING 阶段响应（box 已绘制 / 区域已选中），避免提交空包围盒
	if (SC->GetBuildPhase() != EPreviewInteractionState::EDITING) return;

	if (SC->GetCurrentArea() == nullptr)
	{
		// 创建：提交区域实例
		if (CommitArea(Context))
		{
			// 创建成功：重置到 IDLE，清预览 + 重置临时数据，准备下一次创建
			SC->SetBuildPhase(EPreviewInteractionState::IDLE);
			SC->InitTmpAreaInfoForCreate();
		}
	}
	else
	{
		// 修改：将 TmpAreaInfo 应用到当前选中区域
		FAreaModificationData Data = SC->GetTmpAreaInfo();
		if (Context.InteractionComp)
		{
			Context.InteractionComp->ModifyArea(Data);
		}
		SC->SetBuildPhase(EPreviewInteractionState::IDLE);
	}
}

// ==================== 区域操作 ====================

void UVoxelAreaEditTool::SelectArea(const FVoxelToolContext& Context, AVoxelAreaBaseInstance* Area)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	if (Area)
	{
		UE_LOG(LogTemp, Log, TEXT("Interact: Request Select Area: %s (ID: %d)"),
			*Area->AreaBase.AreaName.ToString(), Area->AreaBase.AreaId);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Interact: Request Deselect / Clicked Nothing"));
	}

	// 将目标区域传给状态组件
	SC->SetCurrentArea(Area);
}

bool UVoxelAreaEditTool::CommitArea(const FVoxelToolContext& Context)
{
	UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 2] CommitArea 进入"));

	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC)
	{
		UE_LOG(LogTemp, Error, TEXT("[CreateArea] [Step 2] 早退：BuildStateComp=NULL"));
		return false;
	}

	// 如果 CurArea 不为空，直接返回
	if (SC->GetCurrentArea() != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CreateArea] [Step 2] 早退：CurArea 非空，跳过创建"));
		return false;
	}

	// 获取 AreaWorldSubsystem
	UVoxelAreaWorldSubsystem* AreaWorldSubsystem = Context.GetAreaWorld();
	if (!AreaWorldSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[CreateArea] [Step 2] 早退：AreaWorldSubsystem 为空"));
		return false;
	}

	// 打印将要提交的数据快照
	FAreaModificationData TmpInfo = SC->GetTmpAreaInfo();
	AVoxelAreaBaseInstance* ParentArea = SC->GetCurrentParentArea();

	// 新流程：工具不再采集业务数据，名称为空时自动生成默认名（用户可稍后在实体详情改名）
	if (TmpInfo.AreaName.IsNone())
	{
		TmpInfo.AreaName = *FString::Printf(TEXT("Area_%lld"), FDateTime::Now().ToUnixTimestamp());
	}

	// 同步 CurParentArea 的 ID 到 TmpInfo.ParentAreaId
	// EnterSubArea 只设置了 CurParentArea，没有更新 TmpAreaInfo，这里补上
	if (ParentArea)
	{
		TmpInfo.ParentAreaId = ParentArea->AreaBase.AreaId;
	}
	else
	{
		TmpInfo.ParentAreaId = -1;
	}

	// 输入校验：创建必须基于「当前选择的区域类型」（由 InteractionComp->SetAreaType 写入 TmpAreaInfo）
	if (!TmpInfo.AreaType.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CreateArea] [Step 2] 早退：未选择区域类型 (TmpAreaInfo.AreaType 为空)"));
		return false;
	}

	// 默认值装配：业务数据留待实体详情面板修改
	// 所有人 = 父区域所有人（无父区域 = 当前主角角色）；经营人 = 当前主角角色；其余字段沿用 InitTmpAreaInfoForCreate 的默认值
	const UVoxelGameInstance* GameInstance = Context.GetGameInstance();
	const int32 PlayerCreatureId = GameInstance ? GameInstance->GetMainCharacterId() : 0;
	TmpInfo.OwnerCreatureId = ParentArea ? ParentArea->AreaBase.OwnerCreatureId : PlayerCreatureId;
	TmpInfo.ManagerCreatureId = PlayerCreatureId;

	UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 2] 提交数据：Name=%s, Type=%s, Min=%s, Max=%s, ParentArea=%s, ParentAreaId=%d"),
		*TmpInfo.AreaName.ToString(),
		*TmpInfo.AreaType.ToString(),
		*TmpInfo.AreaCoverage.MinIndex.ToString(),
		*TmpInfo.AreaCoverage.MaxIndex.ToString(),
		ParentArea ? *FString::Printf(TEXT("Id=%d"), ParentArea->AreaBase.AreaId) : TEXT("None"),
		TmpInfo.ParentAreaId);

	// 生成区域
	UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 2] 调用 AreaWorldSubsystem->GenerateAreaInstance ..."));
	AVoxelAreaBaseInstance* NewArea = AreaWorldSubsystem->GenerateAreaInstance(
		TmpInfo,
		ParentArea,
		Context.PlayerController
	);

	UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 2] CommitArea 结果：%s"),
		NewArea ? *FString::Printf(TEXT("成功 NewAreaId=%d"), NewArea->AreaBase.AreaId) : TEXT("失败 (返回 nullptr)"));

	// 创建成功即选中新区域：新区域成为当前工作对象（M9 建墙等后续操作依赖 CurrentArea，
	// 旧流程创建后 CurrentArea 仍为空导致建墙静默失败）
	if (NewArea)
	{
		SC->SetCurrentArea(NewArea);
	}

	return NewArea != nullptr;
}

void UVoxelAreaEditTool::EnterSubArea(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 获取当前选中的区域（这将成为新的父级）
	if (AVoxelAreaBaseInstance* NewParent = SC->GetCurrentArea())
	{
		// 必须先设置新的父区域，再清空 CurArea
		// 因为 SetCurrentArea(nullptr) 会触发 OnCurrentAreaChanged，
		// 此时 HandleCurrentAreaChanged 读取 GetCurrentParentArea() 来刷新场景渲染
		SC->SetCurrentParentArea(NewParent);
		SC->SetCurrentArea(nullptr);

		UE_LOG(LogTemp, Log, TEXT("Interact: Enter SubArea. New Parent: %s"), *NewParent->AreaBase.AreaName.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Interact: Cannot enter SubArea (No area selected)."));
	}
}

void UVoxelAreaEditTool::ReturnToParentArea(const FVoxelToolContext& Context)
{
	UVoxelPlayerStateCacheComponent* SC = Context.StateCacheComp;
	if (!SC) return;

	// 检查当前是否选中了某个区域
	if (SC->GetCurrentArea() != nullptr)
	{
		// 有选中区域 -> 取消选中
		SC->SetCurrentArea(nullptr);
		UE_LOG(LogTemp, Log, TEXT("Interact: Deselected Current Area."));
	}
	else
	{
		// 没有选中区域 -> 尝试返回上一级
		if (AVoxelAreaBaseInstance* OldParent = SC->GetCurrentParentArea())
		{
			// 将当前的"父区域"变成"选中区域"
			SC->SetCurrentArea(OldParent);
			UE_LOG(LogTemp, Log, TEXT("Interact: Returned to Parent Level."));
		}
		else
		{
			// 已经是最顶层
			SC->SetCurrentArea(nullptr);
			UE_LOG(LogTemp, Log, TEXT("Interact: Already at root level."));
		}
	}
}

// ==================== 预览驱动 ====================

void UVoxelAreaEditTool::UpdatePreview(UVoxelPreviewComponent* PC)
{
	UVoxelPlayerStateCacheComponent* SC = PC->GetStateComponent();
	if (!SC || !PC->PreviewActor) return;

	PC->UpdateBatchEditPreview(SC->GetCurrentEditRequest());
	PC->PreviewActor->SetInnerMeshVisible(true);
}


void UVoxelAreaEditTool::ConfigureGizmos(UVoxelPreviewComponent* PC, EPreviewInteractionState Phase)
{
	PC->SetPreviewActorState(Phase);
	PC->ConfigureGizmoLayout(Phase, true, false, true);
}
