// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "VectorUtil.h"

#if !(UE_BUILD_SHIPPING)
#include "KismetTraceUtils.h"
#endif
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelItemWorldSubsystem.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"
#include "Core/VoxelGameInstance.h"
#include "Voxel/Editor/VoxelPlacementWorldSubsystem.h"
#include "Voxel/Editor/VoxelPlacementTypes.h"
#include "Voxel/Editor/VoxelPlacementCoreTypes.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Core/UI/Interaction/Voxel3DUIActor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Voxel/VoxelPlayerController.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Voxel/Editor/VoxelHistoryData.h"
#include "Core/CoreTags.h"
#include "Voxel/Editor/VoxelPreviewActor.h"
#include "Item/VoxelItemBaseInstance.h"
#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "Core/UI/Base/VoxelDialogBase.h"
#include "Core/UI/Build/VoxelFurnitureDetailDialog.h"
#include "Voxel/Editor/Tools/VoxelBatchEditTool.h"
#include "Voxel/Editor/Tools/VoxelSelectionTool.h"
#include "Voxel/Editor/Tools/VoxelSelectionEditTool.h"
#include "Voxel/Editor/Tools/VoxelFurnitureEditTool.h"
#include "Voxel/Editor/Tools/VoxelTemplateEditTool.h"
#include "Voxel/Editor/Tools/VoxelSingleVoxelEditTool.h"
#include "Voxel/Editor/Tools/VoxelSimpleGeometryEditTool.h"
#include "Voxel/Editor/Tools/VoxelSelectionShapeTool.h"
#include "Voxel/Editor/Tools/VoxelSelectionBrushTool.h"
#include "Voxel/Editor/Tools/VoxelAreaEditTool.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Voxel/Editor/Tools/VoxelCutoutEditTool.h"
#include "Voxel/Editor/Tools/VoxelPlaceholderTools.h"
#include "Voxel/Editor/VoxelToolContext.h"
#include "Voxel/Editor/UVoxelTemplateUtils.h"
#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "GameManager/GameInstanceManager/VoxelToolSubsystem.h"

// ==================== 构造函数 ====================

UVoxelInteractionComponent::UVoxelInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVoxelInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// 缓存组件引用（在 BeginPlay 中一次性初始化；StateCache 惰性解析亦可）
	if (AActor* Owner = GetOwner())
	{
		CachedStateCache = Owner->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
		CachedPreviewComponent = Owner->FindComponentByClass<UVoxelPreviewComponent>();
		CachedHistoryComponent = Owner->FindComponentByClass<UVoxelControllerHistoryComponent>();

		if (!CachedStateCache)
		{
			UE_LOG(LogTemp, Error, TEXT("InteractionComponent: StateCacheComponent not found on %s!"), *Owner->GetName());
		}
		if (!CachedPreviewComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("InteractionComponent: PreviewComponent not found on %s!"), *Owner->GetName());
		}
	}

	// 工具注册表已全局化：UVoxelToolSubsystem（GameInstance 子系统）在 GameInstance 初始化期完成注册，
	// 先于一切 World/Controller，此处无需再注册

	// 统一鼠标射线定时器（50ms）：Hover/地面标记/Brush 共用同一次追踪结果
	// 替代蓝图 Tick → HandleMouseHover 的每帧调用
	GetWorld()->GetTimerManager().SetTimer(
		MouseRayTimerHandle, this, &UVoxelInteractionComponent::HandleMouseHover,
		MouseRayInterval, true);
}

// ==================== 组件访问器 ====================

UVoxelPlayerStateCacheComponent* UVoxelInteractionComponent::GetStateCache()
{
	if (CachedStateCache != nullptr)
	{
		return CachedStateCache.Get();
	}
	if (AActor* Owner = GetOwner())
	{
		UVoxelPlayerStateCacheComponent* Found = Owner->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
		if (Found)
		{
			CachedStateCache = Found;
		}
		return Found;
	}
	return nullptr;
}

UVoxelPlayerStateCacheComponent* UVoxelInteractionComponent::GetStateCache() const
{
	if (CachedStateCache != nullptr)
	{
		return CachedStateCache.Get();
	}
	if (AActor* Owner = GetOwner())
	{
		return Owner->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
	}
	return nullptr;
}

// ==================== 控制器模式 ====================

void UVoxelInteractionComponent::SetControllerMode(EVoxelControllerMode Mode)
{
	UE_LOG(LogTemp, Log, TEXT("[ModeToggle] Interaction::SetControllerMode ENTER: current=%d target=%d"), (int32)PlayerMode, (int32)Mode);
	if (PlayerMode != Mode)
	{
		PlayerMode = Mode;

		if (PlayerMode == EVoxelControllerMode::SceneBuildMode)
		{
			SetCurrentToolTag(FCoreTags::Tool_Edit_SingleVoxel);
		}
		else
		{
			SetCurrentToolTag(FGameplayTag::EmptyTag);
		}
		OnControllerModeChanged.Broadcast(this);
		UE_LOG(LogTemp, Log, TEXT("[ModeToggle] Interaction::SetControllerMode CHANGED to %d, broadcast done"), (int32)Mode);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ModeToggle] Interaction::SetControllerMode SKIPPED: already %d (state desync: UI thinks it needs switch)"), (int32)Mode);
	}
}

// ==================== 工具 Tag ====================

void UVoxelInteractionComponent::SetCurrentToolTag(FGameplayTag ToolTag)
{
	const bool bChanged = (CurrentToolTag != ToolTag);
	if (!bChanged)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[ToolChain] Interaction::SetCurrentToolTag skipped (unchanged): '%s'"), *ToolTag.ToString());
		return;
	}

	UVoxelPlayerStateCacheComponent* StateCache = GetStateCache();

	// 切换离开选区工具时清空选区（切到选区/编辑工具不清空，支持选区约束编辑）
	if (CurrentToolTag.MatchesTag(FCoreTags::Tool_Selection)
		&& !ToolTag.MatchesTag(FCoreTags::Tool_Selection)
		&& !ToolTag.MatchesTag(FCoreTags::Tool_Edit))
	{
		if (StateCache)
		{
			StateCache->ClearSelection();
		}
	}

	// 手持物跨工具域时清空（家具工具 ↔ 方块/其他工具切换，旧手持物对新工具无效）。
	// 广播 OnItemInHandChanged 驱动 M1 手持槽/已选信息清空（单向数据流，UI 不直改）。
	const bool bOldIsFurnitureTool = CurrentToolTag == FCoreTags::Tool_Edit_Furniture;
	const bool bNewIsFurnitureTool = ToolTag == FCoreTags::Tool_Edit_Furniture;
	if (StateCache && bOldIsFurnitureTool != bNewIsFurnitureTool)
	{
		FInteractableData EmptyHand;
		StateCache->SetInteractableDataInHand(EmptyHand);
	}

	// 工具切换时重置 BuildPhase 为 IDLE，避免旧工具的阶段状态污染新工具
	BuildPhase = EPreviewInteractionState::IDLE;

	// 几何副作用：进入 Brush/单体素工具且无形状时初始化默认 Box；只清空 Rotation
	// （不清空 Box：选区工具之间切换时 M4 需要从 Box 读取选区范围）
	if (StateCache)
	{
		FVoxelGeometryDesc Geo = StateCache->GetTmpGeometry();
		if ((ToolTag == FCoreTags::Tool_Selection_Brush || ToolTag == FCoreTags::Tool_Edit_SingleVoxel)
			&& Geo.ShapeType == EBatchShapeType::Null)
		{
			Geo.ShapeType = EBatchShapeType::Box;
		}
		Geo.Rotation = FRotator::ZeroRotator;
		if (!(Geo == StateCache->GetTmpGeometry()))
		{
			StateCache->SetTmpGeometry(Geo);
		}
	}

	const FString OldToolTagStr = CurrentToolTag.ToString();
	CurrentToolTag = ToolTag;

	// 进入区域编辑工具且未选中已有区域时，初始化临时区域数据（场景/Owner/Manager 默认值）
	if (ToolTag.MatchesTag(FCoreTags::Tool_AreaEdit) && StateCache && StateCache->GetCurrentArea() == nullptr)
	{
		StateCache->InitTmpAreaInfoForCreate();
	}

	UE_LOG(LogTemp, Log, TEXT("[ToolChain] OnToolChanged broadcast: '%s' -> '%s'"), *OldToolTagStr, *ToolTag.ToString());
	OnToolChanged.Broadcast();
}

// ==================== 建造流程阶段 ====================

void UVoxelInteractionComponent::SetBuildPhase(EPreviewInteractionState NewPhase)
{
	if (BuildPhase == NewPhase)
	{
		return;
	}
	BuildPhase = NewPhase;
	OnBuildPhaseChanged.Broadcast();
}

// ==================== 状态切换 ====================

void UVoxelInteractionComponent::SwitchTool(FGameplayTag NewToolTag)
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC)
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolChain] SwitchTool: GetControllerState returned null"));
		return;
	}
	if (GetCurrentToolTag() == NewToolTag)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[ToolChain] SwitchTool: skipped, tool unchanged '%s'"), *NewToolTag.ToString());
		return;
	}

	// 1. 停用旧工具（工具自身的清理走 OnToolDeactivated）
	if (UVoxelToolBase* OldTool = GetCurrentTool())
	{
		OldTool->SetToolActive(false);
	}

	// 2. 清理交互组件自身持有的输入/家具拖拽状态
	InputState.ActiveDragGizmo = nullptr;
	InputState.bIsModifying = false;
	InputState.bIsBrushDragging = false;
	InputState.LastBrushStampIndex = FIntVector::NoneValue;

	// 家具拾取状态（若已拾取，先恢复原Actor显示）
	if (FurnitureDragState.bPickedUp && FurnitureDragState.Actor.IsValid())
	{
		FurnitureDragState.Actor->SetActorHiddenInGame(false);
	}
	FurnitureDragState = FFurnitureDragState();
	SC->SetSelectedFurniture(nullptr);

	// 清空旧工具预览（增量重建由新工具激活后的事件驱动）
	if (CachedPreviewComponent)
	{
		CachedPreviewComponent->ClearPreview();
	}

	// 清理 Tooltip 和 DragProgress
	if (UVoxelPrimaryGameLayout* Layout = GetPrimaryLayout())
	{
		Layout->HideInfoTooltip();
		Layout->HideDragProgress();
	}

	// 3. 切换工具 Tag（InteractionComp 内部重置 BuildPhase=IDLE、清 Rotation）
	SetCurrentToolTag(NewToolTag);

	// 4. 激活新工具
	if (UVoxelToolBase* NewTool = GetCurrentTool())
	{
		NewTool->SetToolActive(true);
	}
}

// SetBatchEditMode 已删除 (2026-01-29) - 使用 SetCurrentToolTag() 切换工具
// 替代方案：调用 SC->SetCurrentToolTag(FCoreTags::Tool_Edit_SimpleGeometry);

void UVoxelInteractionComponent::SetBatchShape(EBatchShapeType NewShape)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetCurrentBatchShape(NewShape);
	}
}

void UVoxelInteractionComponent::SetBrushSize(int32 NewSize)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetBrushSize(NewSize);
	}
}

void UVoxelInteractionComponent::SetBrushDimension(EBrushDimension NewDimension)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetBrushDimension(NewDimension);
	}
}

void UVoxelInteractionComponent::SetPlacementMode(EVoxelPlacementMode NewMode)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetPlacementMode(NewMode);
	}
}

void UVoxelInteractionComponent::SetSelectionMode(ESelectionOpMode NewMode)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetSelectionMode(NewMode);
	}
}

void UVoxelInteractionComponent::MoveSelection(const FIntVector& Offset)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->MoveSelection(Offset);
	}
}

void UVoxelInteractionComponent::ApplySelectionShapeImmediately()
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	float BlockLength = UChunkUtilsFunction::GetBlockLength(this);

	// 计算当前临时形状（只算形状，不应用算法）
	FVoxelEditRequest EditRequest = SC->GetCurrentEditRequest();
	TArray<FIntVector> EmptySelection;
	TArray<FIntVector> TempShape = UVoxelSelectionTool::CalculateSelection(
		EditRequest, BlockLength, EmptySelection, ESelectionOperationType::New);

	// 按当前选择算法（New/Add/Subtract/Intersect）应用到 CurrentSelection
	SC->ApplySelectionOp(TempShape, SC->GetSelectionMode());

	// 重置编辑状态，准备下一次选择
	SC->ResetEditSession();

	UE_LOG(LogTemp, Log, TEXT("[EditChain] ApplySelectionShapeImmediately: mode=%d, tempShape=%d, current=%d"),
		(int32)SC->GetSelectionMode(), TempShape.Num(), SC->GetCurrentSelection().Num());
}

void UVoxelInteractionComponent::ApplyEditWithinSelection(const FVoxelEditRequest& EditRequest)
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC || !SC->HasSelection()) return;

	AVoxelPlayerController* PC = GetOwner<AVoxelPlayerController>();
	UVoxelBlockWorldSubsystem* BlockWorld = PC ? PC->GetBlockWorldSubsystem() : nullptr;
	if (!BlockWorld) return;

	UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComponent();

	// 委托给 UVoxelSelectionEditTool
	if (UVoxelSelectionEditTool::ExecuteWithinSelection(EditRequest, SC, HistoryComp, BlockWorld))
	{
		// 重置编辑状态
		SC->ResetEditSession();
	}
}

// ==================== 剪切板操作（委托给 UVoxelSelectionEditTool） ====================

void UVoxelInteractionComponent::CopySelection()
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	AVoxelPlayerController* PC = GetOwner<AVoxelPlayerController>();
	UVoxelBlockWorldSubsystem* BlockWorld = PC ? PC->GetBlockWorldSubsystem() : nullptr;

	UVoxelSelectionEditTool::CopySelection(SC, BlockWorld);
}

void UVoxelInteractionComponent::CutSelection()
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	AVoxelPlayerController* PC = GetOwner<AVoxelPlayerController>();
	UVoxelBlockWorldSubsystem* BlockWorld = PC ? PC->GetBlockWorldSubsystem() : nullptr;
	UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComponent();

	UVoxelSelectionEditTool::CutSelection(SC, BlockWorld, HistoryComp);
}

void UVoxelInteractionComponent::PasteSelection(const FIntVector& TargetPosition)
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	AVoxelPlayerController* PC = GetOwner<AVoxelPlayerController>();
	UVoxelBlockWorldSubsystem* BlockWorld = PC ? PC->GetBlockWorldSubsystem() : nullptr;
	UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComponent();

	UVoxelSelectionEditTool::PasteSelection(SC, BlockWorld, HistoryComp, TargetPosition);
}

void UVoxelInteractionComponent::PasteAtMousePosition()
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC) return;

	FVector WorldLocation, WorldDirection;
	if (!PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection)) return;

	const FVector TraceEnd = WorldLocation + WorldDirection * TraceDistance;
	FHitResult GroundHit;
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;

	const bool bGroundHit = GetWorld()->LineTraceSingleByChannel(
		GroundHit, WorldLocation, TraceEnd, ECC_GameTraceChannel1, Params);

	if (!bGroundHit) return;

	float BlockLength = UChunkUtilsFunction::GetBlockLength(this);

	FIntVector TargetIndex = UChunkUtilsFunction::GetBlockIndexByPosition(GroundHit.Location, BlockLength);
	PasteSelection(TargetIndex);
}

void UVoxelInteractionComponent::MoveSelectionContent(const FIntVector& Offset)
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC || !SC->HasSelection()) return;

	AVoxelPlayerController* PC = GetOwner<AVoxelPlayerController>();
	if (!PC) return;

	UVoxelBlockWorldSubsystem* BlockWorld = PC->GetBlockWorldSubsystem();
	UVoxelItemWorldSubsystem* ItemWorld = PC->GetWorld()->GetSubsystem<UVoxelItemWorldSubsystem>();
	UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComponent();

	UVoxelSelectionEditTool::MoveSelectionContent(SC, BlockWorld, ItemWorld, HistoryComp, Offset);
}

void UVoxelInteractionComponent::SetPlayerMode(EVoxelControllerMode NewMode)
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC)
	{
		UE_LOG(LogTemp, Error, TEXT("[Interaction] SetPlayerMode(%d): CachedStateCache is NULL, ABORTED"), (int32)NewMode);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[Interaction] SetPlayerMode: %d -> %d"), (int32)GetControllerMode(), (int32)NewMode);
	SetControllerMode(NewMode);
}

void UVoxelInteractionComponent::SetGameAuthority(EVoxelGameAuthority NewAuthority)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetGameAuthority(NewAuthority);
	}
}

// ==================== 编辑操作 ====================

void UVoxelInteractionComponent::ReleaseBatchEditPreview()
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		// 选区工具：几何体在确认前一直是临时的，松手只进入 EDIT 阶段（可继续用 Gizmo 调整临时形状）
		// 真正的应用在 RequestConfirmVoxelEdit 中按算法执行
		SC->SetBuildPhase(EPreviewInteractionState::EDITING);
	}
}

void UVoxelInteractionComponent::CancelCurrentEdit()
{
	InputState.FirstPoint = FVector::ZeroVector;
	InputState.SecondPoint = FVector::ZeroVector;
	InputState.bIsModifying = false;
	InputState.ActiveDragGizmo = nullptr;
	InputState.bIsBrushDragging = false;
	InputState.LastBrushStampIndex = FIntVector::NoneValue;

	// 清空 StateComponent 的临时几何变换数据（保留形状等描述数据）
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->ResetEditSession();
	}

	// 调用 VisualUIComponent 处理渲染回退逻辑
	if (UVoxelPreviewComponent* VC = GetPreviewComponent())
	{
		VC->CancelEdit();
	}

	UE_LOG(LogTemp, Log, TEXT("Interaction: CancelCurrentEdit completed"));
}

// ==================== Gizmo 操作实现 ====================

void UVoxelInteractionComponent::HandleGizmo_Translate(AVoxel3DUIActor* Input, int AxisIndex)
{
	if (AxisIndex == INDEX_NONE) return;

	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	const float BlockLength = UChunkUtilsFunction::GetBlockLength(SC);
	const FVector Delta = InputState.SecondPoint - InputState.FirstPoint;
	const FIntVector DeltaInIndex = UChunkUtilsFunction::WorldDeltaToIndexDelta(Delta, BlockLength);

	FIntVector Offset = FIntVector::ZeroValue;
	Offset[AxisIndex] = DeltaInIndex[AxisIndex];

	if (UVoxelToolBase* Tool = GetCurrentTool())
	{
		Tool->OnGizmoTranslate(BuildToolContext(), Offset);
	}
}

void UVoxelInteractionComponent::HandleGizmo_Rotate(AVoxel3DUIActor* Input, int32 AxisIndex)
{
	AVoxelPlayerController* PC = GetOwner<AVoxelPlayerController>();
	if (!IsValid(PC) || !IsValid(PC->StateCacheComp) || !IsValid(PC->PreviewComp))
	{
		UE_LOG(LogTemp, Error, TEXT("HandleGizmo_Rotate: Invalid references!"));
		return;
	}

	UVoxelPlayerStateCacheComponent* SC = PC->StateCacheComp;

	const FVoxelGeometryDesc& Geo = SC->GetTmpGeometry();
	const float BlockLength = UChunkUtilsFunction::GetBlockLength(SC);
	FVector Pivot = Geo.GetPivotWorld(BlockLength);

	FVector PivotToFirst = InputState.FirstPoint - Pivot;
	FVector PivotToSecond = InputState.SecondPoint - Pivot;

	if (AxisIndex == -1)
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleGizmo_Rotate: Invalid Gizmo!"));
		return;
	}

	// 计算角度增量：将向量投影到垂直于旋转轴的平面上
	FVector V1 = FVector::ZeroVector, V2 = FVector::ZeroVector;
	EVoxelTransformSpace Space = Geo.TransformSpace;

	if (Space == EVoxelTransformSpace::Local)
	{
		FQuat InvRot = Geo.Rotation.Quaternion().Inverse();
		FVector LocalFirst  = InvRot * PivotToFirst;
		FVector LocalSecond = InvRot * PivotToSecond;

		switch (AxisIndex)
		{
		case 0:
			V1 = FVector(0, LocalFirst.Y, LocalFirst.Z);
			V2 = FVector(0, LocalSecond.Y, LocalSecond.Z);
			break;
		case 1:
			V1 = FVector(LocalFirst.X, 0, LocalFirst.Z);
			V2 = FVector(LocalSecond.X, 0, LocalSecond.Z);
			break;
		case 2:
			V1 = FVector(LocalFirst.X, LocalFirst.Y, 0);
			V2 = FVector(LocalSecond.X, LocalSecond.Y, 0);
			break;
		}
	}
	else
	{
		switch (AxisIndex)
		{
		case 0:
			V1 = FVector(0, PivotToFirst.Y, PivotToFirst.Z);
			V2 = FVector(0, PivotToSecond.Y, PivotToSecond.Z);
			break;
		case 1:
			V1 = FVector(PivotToFirst.X, 0, PivotToFirst.Z);
			V2 = FVector(PivotToSecond.X, 0, PivotToSecond.Z);
			break;
		case 2:
			V1 = FVector(PivotToFirst.X, PivotToFirst.Y, 0);
			V2 = FVector(PivotToSecond.X, PivotToSecond.Y, 0);
			break;
		}
	}

	const float Angle1 = FMath::Atan2(V1.Y, V1.X);
	const float Angle2 = FMath::Atan2(V2.Y, V2.X);
	const float DeltaAngle = FMath::RadiansToDegrees(Angle2 - Angle1);

	FRotator DeltaRotation = FRotator::ZeroRotator;
	switch (AxisIndex)
	{
	case 0: DeltaRotation.Pitch = DeltaAngle; break;
	case 1: DeltaRotation.Yaw   = DeltaAngle; break;
	case 2: DeltaRotation.Roll  = DeltaAngle; break;
	}

	if (Geo.PivotMode == EVoxelPivotMode::Cursor)
	{
		FVoxelGeometryDesc MutableGeo = Geo;
		FVector GeoCenter = MutableGeo.GetGeometricCenter(BlockLength);
		FVector Offset = GeoCenter - Pivot;
		FVector NewOffset = DeltaRotation.RotateVector(Offset);
		FVector NewGeoCenter = Pivot + NewOffset;
		FVector DeltaWorld = NewGeoCenter - GeoCenter;
		FIntVector DeltaIndex = UChunkUtilsFunction::WorldDeltaToIndexDelta(DeltaWorld, BlockLength);
		MutableGeo.Box.SetBoundingBox(MutableGeo.Box.MinIndex + DeltaIndex, MutableGeo.Box.MaxIndex + DeltaIndex);
		MutableGeo.Rotation += DeltaRotation;
		SC->SetTmpGeometry(MutableGeo);
	}
	else
	{
		FRotator NewRot = Geo.Rotation + DeltaRotation;
		SC->SetTmpGeometryRotation(NewRot);
	}
}

void UVoxelInteractionComponent::HandleGizmo_Face(AVoxel3DUIActor* Input, int FaceIndex)
{
	AVoxelPlayerController* PC = GetOwner<AVoxelPlayerController>();
	if (!IsValid(PC) || !IsValid(PC->StateCacheComp) || !IsValid(PC->PreviewComp))
	{
		UE_LOG(LogTemp, Error, TEXT("HandleGizmo_Face: Invalid references!"));
		return;
	}

	UVoxelPlayerStateCacheComponent* SC = PC->StateCacheComp;

	// 获取 BlockLength
	const float BlockLength = UChunkUtilsFunction::GetBlockLength(SC);

	// FP 和 SP 是世界坐标，计算增量（Delta = SP - FP）
	FVector Delta = InputState.SecondPoint - InputState.FirstPoint;

	// 获取当前包围盒
	FBox CurrentBox = SC->GetTmpGeometry().Box.BoundingBox();
	FVector Min = CurrentBox.Min;
	FVector Max = CurrentBox.Max;
	
	if (FaceIndex == -1)
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleGizmo_Face: Invalid Gizmo!"));
		return;
	}

	// ==================== 旋转和投影计算 ====================
	FRotator GeometryRotation = SC->GetTmpGeometry().Rotation;

	// 定义面的原始法向（统一指向正轴方向）
	// 这样向+轴方向拖拽时，投影为正，面的值增加（外扩）
	FVector FaceNormal;
	switch (FaceIndex)
	{
	case 0: FaceNormal = FVector(1, 0, 0);  break; // MinX → 拖拽时 Min.X 增加（盒子缩小）
	case 1: FaceNormal = FVector(1, 0, 0);  break; // MaxX → 拖拽时 Max.X 增加（盒子扩大）
	case 2: FaceNormal = FVector(0, 1, 0);  break; // MinY → 拖拽时 Min.Y 增加（盒子缩小）
	case 3: FaceNormal = FVector(0, 1, 0);  break; // MaxY → 拖拽时 Max.Y 增加（盒子扩大）
	case 4: FaceNormal = FVector(0, 0, 1);  break; // MinZ → 拖拽时 Min.Z 增加（盒子缩小）
	case 5: FaceNormal = FVector(0, 0, 1);  break; // MaxZ → 拖拽时 Max.Z 增加（盒子扩大）
	}

	// 应用几何旋转到法向（得到实际的世界空间法向）
	FVector RotatedNormal = GeometryRotation.RotateVector(FaceNormal);

	// 将拖动向量投影到法向上
	float ProjectionLength = FVector::DotProduct(Delta, RotatedNormal);

	// 转换为体素索引增量
	int32 VoxelOffset = UChunkUtilsFunction::WorldDeltaToIndexDelta_SingleAxis(ProjectionLength, BlockLength);

	// ==================== 修改包围盒 ====================
	switch (FaceIndex)
	{
	case 0: // MinX (-X面)
		Min.X += VoxelOffset;
		break;
	case 1: // MaxX (+X面)
		Max.X += VoxelOffset;
		break;
	case 2: // MinY (-Y面)
		Min.Y += VoxelOffset;
		break;
	case 3: // MaxY (+Y面)
		Max.Y += VoxelOffset;
		break;
	case 4: // MinZ (-Z面)
		Min.Z += VoxelOffset;
		break;
	case 5: // MaxZ (+Z面)
		Max.Z += VoxelOffset;
		break;
	}

	// ==================== 创建新的包围盒并设置 ====================
	// crossover 检测：当面 Gizmo 被拖过对面后，Min 可能 > Max。
	// 值规范化由 SetBoundingBox 内部处理（FMath::Min/Max），此处只做检测，
	// 命中时将 ActiveDragGizmo 切换到对面索引的 Actor。
	// 数组本身始终保持原始顺序（UpdateFaceGizmosPosition 每次按索引刷新位置/旋转），
	// 切换 ActiveDragGizmo 后，下次 Find 返回对面索引，自然切换到对面面的修改逻辑。
	const bool bSwapX = Min.X > Max.X;
	const bool bSwapY = Min.Y > Max.Y;
	const bool bSwapZ = Min.Z > Max.Z;

	// crossover: switch ActiveDragGizmo to opposite face index (0<->1, 2<->3, 4<->5)
	// Array stays in original order; UpdateFaceGizmosPosition always refreshes by index.
	if (PC->PreviewComp && (bSwapX || bSwapY || bSwapZ))
	{
		int32 OppositeIndex = FaceIndex ^ 1;
		if (PC->PreviewComp->FaceGizmos.IsValidIndex(OppositeIndex))
		{
			InputState.ActiveDragGizmo = PC->PreviewComp->FaceGizmos[OppositeIndex];
			UE_LOG(LogTemp, Log, TEXT("[EditChain] HandleGizmo_Face: crossover, ActiveDragGizmo -> FaceGizmos[%d]"), OppositeIndex);
		}
	}

	// Min/Max 来自 BoundingBox()（半开区间 [Min, Max+1)），直接传回 SetTmpAreaCoverage
	// 由 SetBoundingBox(FBox) 内部对 Max 做 -1 还原为闭区间 MaxIndex
	FBox NewFBox(Min, Max);

	SC->SetTmpAreaCoverage(NewFBox);
}

// ==================== 道具操作 ====================

void UVoxelInteractionComponent::SetItemInHand(EInteractableType Type, int32 Id, int Quantity)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		FInteractableData Data;
		Data.ResetInform();
		Data = FInteractableData(Type, Id, Quantity);
		SC->SetInteractableDataInHand(Data);
	}
}

void UVoxelInteractionComponent::SetItemInSight(EInteractableType Type, int32 Id, int Quantity)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		FInteractableData Data;
		Data.ResetInform();
		Data = FInteractableData(Type, Id, Quantity);
		SC->SetInteractableDataInSight(Data);
	}
}

// ==================== 家具操作 ====================

void UVoxelInteractionComponent::SelectFurniture(AUVoxelItemBaseInstance* Furniture)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetSelectedFurniture(Furniture);
	}
}

void UVoxelInteractionComponent::UpdateFurniturePosition(FIntVector Position)
{
	UVoxelToolSubsystem* ToolSys = GetToolSubsystem();
	if (ToolSys)
	{
		if (UVoxelFurnitureEditTool* FurnTool = Cast<UVoxelFurnitureEditTool>(ToolSys->FindTool(FCoreTags::Tool_Edit_Furniture)))
		{
			FurnTool->UpdateFurniturePosition(BuildToolContext(), Position);
		}
	}
}

void UVoxelInteractionComponent::RotateFurniture(EActorDirection Direction)
{
	UVoxelToolSubsystem* ToolSys = GetToolSubsystem();
	if (ToolSys)
	{
		if (UVoxelFurnitureEditTool* FurnTool = Cast<UVoxelFurnitureEditTool>(ToolSys->FindTool(FCoreTags::Tool_Edit_Furniture)))
		{
			FurnTool->RotateFurniture(BuildToolContext(), Direction);
		}
	}
}

FBox UVoxelInteractionComponent::GetFurnitureBounds(FIntVector Position, EActorDirection Direction)
{
	UVoxelToolSubsystem* ToolSys = GetToolSubsystem();
	if (ToolSys)
	{
		if (UVoxelFurnitureEditTool* FurnTool = Cast<UVoxelFurnitureEditTool>(ToolSys->FindTool(FCoreTags::Tool_Edit_Furniture)))
		{
			return FurnTool->GetFurnitureBounds(BuildToolContext(), Position, Direction);
		}
	}
	return FBox(FVector::ZeroVector, FVector::ZeroVector);
}

// ==================== 统一使用入口（按 ItemTag 路由） ====================

bool UVoxelInteractionComponent::UseItemInHand(FVector TargetWorldPosition, FVector TargetNormal)
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return false;

	FInteractableData HandData = SC->GetInteractableDataInHand();
	if (HandData.HandleId == EBlockId::Invalid) return false;

	UConfigDataSubsystem* ConfigSys = GetWorld()->GetGameInstance()->GetSubsystem<UConfigDataSubsystem>();
	if (!ConfigSys) return false;
	FVoxelItemBase* Item = ConfigSys->FindByHandle(HandData.HandleId);
	if (!Item) return false;

	const FGameplayTag& ItemTag = Item->ItemTag;

	// 按工具链查找能处理该 ItemTag 的工具（全局注册表遍历）
	if (UVoxelToolSubsystem* ToolSys = GetToolSubsystem())
	{
		for (auto& Pair : ToolSys->GetAllTools())
		{
			if (Pair.Value && Pair.Value->CanHandleItem(ItemTag))
			{
				return Pair.Value->UseItemInHand(BuildToolContext(), TargetWorldPosition, TargetNormal);
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[UseItemInHand] No tool handles ItemTag=%s"), *ItemTag.ToString());
	return false;
}

// ==================== 区域操作 ====================

void UVoxelInteractionComponent::SelectArea(AVoxelAreaBaseInstance* Area)
{
	UVoxelToolSubsystem* ToolSys = GetToolSubsystem();
	if (ToolSys)
	{
		if (UVoxelAreaEditTool* AreaTool = Cast<UVoxelAreaEditTool>(ToolSys->FindTool(FCoreTags::Tool_AreaEdit)))
		{
			AreaTool->SelectArea(BuildToolContext(), Area);
		}
	}
}

void UVoxelInteractionComponent::DeselectArea()
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->ClearCurrentArea();
	}
}

bool UVoxelInteractionComponent::CommitArea()
{
	if (UVoxelToolSubsystem* ToolSys = GetToolSubsystem())
	{
		if (UVoxelAreaEditTool* AreaTool = Cast<UVoxelAreaEditTool>(ToolSys->FindTool(FCoreTags::Tool_AreaEdit)))
		{
			return AreaTool->CommitArea(BuildToolContext());
		}
	}
	return false;
}

void UVoxelInteractionComponent::SetAreaType(FGameplayTag AreaTypeTag)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		// 1. 获取当前的临时区域信息 (TmpAreaInfo)
		FAreaModificationData CurrentData = SC->GetTmpAreaInfo();

		// 2. 修改区域类型 Tag
		CurrentData.AreaType = AreaTypeTag;

		// 3. 名称为空时按类型 DisplayName 生成默认名（用户已手动输入则保留）
		if (CurrentData.AreaName.IsNone() && AreaTypeTag.IsValid())
		{
			if (UWorld* World = GetWorld())
			{
				if (UVoxelGameInstance* GI = World->GetGameInstance<UVoxelGameInstance>())
				{
					if (UConfigDataSubsystem* CDS = GI->GetSubsystem<UConfigDataSubsystem>())
					{
						if (const FAreaTypeBase* TypeInfo = CDS->GetAreaTypeByTag(AreaTypeTag))
						{
							FString DisplayName = TypeInfo->DisplayName.ToString();
							if (DisplayName.IsEmpty())
							{
								DisplayName = AreaTypeTag.ToString();
							}
							const int32 Suffix = FMath::RandRange(100, 999);
							CurrentData.AreaName = FName(FString::Printf(TEXT("%s_%d"), *DisplayName, Suffix));
						}
					}
				}
			}
		}

		// 4. 将修改后的数据写回 State
		SC->SetTmpAreaInfo(CurrentData);
	}
}

void UVoxelInteractionComponent::EnterSubArea()
{
	if (UVoxelToolSubsystem* ToolSys = GetToolSubsystem())
	{
		if (UVoxelAreaEditTool* AreaTool = Cast<UVoxelAreaEditTool>(ToolSys->FindTool(FCoreTags::Tool_AreaEdit)))
		{
			AreaTool->EnterSubArea(BuildToolContext());
		}
	}
}

void UVoxelInteractionComponent::ReturnToParentArea()
{
	if (UVoxelToolSubsystem* ToolSys = GetToolSubsystem())
	{
		if (UVoxelAreaEditTool* AreaTool = Cast<UVoxelAreaEditTool>(ToolSys->FindTool(FCoreTags::Tool_AreaEdit)))
		{
			AreaTool->ReturnToParentArea(BuildToolContext());
		}
	}
}

void UVoxelInteractionComponent::BeginWallShellFromArea()
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	AVoxelAreaBaseInstance* CurrentArea = SC->GetCurrentArea();
	if (!CurrentArea)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeginWallShellFromArea: 无当前选中区域，先选中一个区域"));
		return;
	}

	// 区域包围盒是索引空间闭区间 [MinIndex, MaxIndex]，直接灌入几何体 Box
	const FBox AreaBounds = CurrentArea->GetAreaBounds();

	FVoxelGeometryDesc Geo;
	Geo.ShapeType = EBatchShapeType::Box;
	// 墙壁语义转换：墙默认建在区域边界内侧 → 几何体语义的「内壳」（正厚度）
	Geo.Thickness = 1;
	// 默认四面墙（无顶/底），用户可在几何体编辑的壳体面勾选里调整
	Geo.ShellFaces = { EActorDirection::Left, EActorDirection::Right,
		EActorDirection::Forward, EActorDirection::Backward };
	Geo.Box.SetBoundingBox(FIntVector(AreaBounds.Min), FIntVector(AreaBounds.Max));

	SC->SetTmpGeometry(Geo);

	// 切到几何体编辑工具：厚度/面/方块类型（手持）在几何体编辑侧调整，M16 确认
	SwitchTool(FCoreTags::Tool_Edit_SimpleGeometry);

	UE_LOG(LogTemp, Log, TEXT("BeginWallShellFromArea: 已以区域边界 %s 创建墙壁壳体请求"),
		*AreaBounds.ToString());
}




void UVoxelInteractionComponent::HandleMouseHover()
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;
	if (GetControllerMode() != EVoxelControllerMode::SceneBuildMode) return;

	// 家具拖拽期间不更新 Hover（避免与拖拽位置冲突，隔离家具拖拽与工具流程）
	if (FurnitureDragState.bPressed) return;

	UVoxelToolBase* Tool = GetCurrentTool();
	if (!Tool) return;

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC) return;

	FVector WorldLocation, WorldDirection;
	if (!PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection)) return;

	const FVector TraceEnd = WorldLocation + WorldDirection * TraceDistance;
	FHitResult TraceHit;
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		TraceHit, WorldLocation, TraceEnd, ECC_GameTraceChannel1, Params);

	bLatestGroundHitValid = bHit;
	if (bHit)
	{
		LatestGroundHitLocation = TraceHit.Location;
		LatestGroundHitNormal = TraceHit.Normal;
	}

#if !(UE_BUILD_SHIPPING)
	if (bShowDebugTrace)
	{
		DrawDebugLineTraceSingle(GetWorld(), WorldLocation, TraceEnd,
			EDrawDebugTrace::ForOneFrame, bHit, TraceHit,
			FLinearColor::Red, FLinearColor::Green, 0.0f);
	}
#endif

	if (!bHit) return;

	Tool->OnMouseHover(BuildToolContext(), TraceHit);
}

void UVoxelInteractionComponent::HandleMouseInput()
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC)
	{
		return;
	}
	// 实体选中链路：所有模式都执行，与下方工具流程并行、互不拦截
	// （区域/家具任何模式可选中，生物仅非建造模式，由组件内部判断；命中→选中，空处→清除）
	if (UVoxelPlayerStateCacheComponent* SelComp = GetOwner()->FindComponentByClass<UVoxelPlayerStateCacheComponent>())
	{
		SelComp->HandleWorldClick();
	}
	if (GetControllerMode() != EVoxelControllerMode::SceneBuildMode)
	{
		// 非建造态：点击只用于实体选中，无工具流程
		return;
	}

	UVoxelToolBase* Tool = GetCurrentTool();
	if (!Tool) return;

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC) return;

	// ==================== 家具直接操作（仅限家具编辑工具 + IDLE 状态） ====================
	// 只在 FurnitureEditTool 的 IDLE 状态下检测家具长按拾取
	// - IDLE + 无手持：可以拾取/查看已有家具
	// - PLACING：手持家具准备放置，走正常放置流程
	// - 其他工具：不触发家具拾取
	if (!FurnitureDragState.bPressed && !FurnitureDragState.bPickedUp
		&& GetCurrentToolTag() == FCoreTags::Tool_Edit_Furniture
		&& GetBuildPhase() == EPreviewInteractionState::IDLE)
	{
		if (AUVoxelItemBaseInstance* Furniture = TraceFurniture())
		{
			FurnitureDragState.bPressed = true;
			FurnitureDragState.Actor = Furniture;
			FurnitureDragState.PressTime = GetWorld()->GetTimeSeconds();
			StartFurnitureDragTimer();
			UE_LOG(LogTemp, Log, TEXT("[FurnitureDrag] Pressed: %s"), *Furniture->GetName());

			// 显示拖拽进度圈（跟随鼠标位置）
			if (UVoxelPrimaryGameLayout* Layout = GetPrimaryLayout())
			{
				Layout->ShowDragProgress();
			}
			return; // 不走工具流程，等定时器检测长按
		}
	}

	FHitResult GroundHit;
	AVoxel3DUIActor* GizmoActor = nullptr;
	if (!TraceMouseScene(PC, GroundHit, GizmoActor)) return;

	Tool->OnMouseInput(BuildToolContext(), GroundHit, GizmoActor);
}

void UVoxelInteractionComponent::HandleMouseDrag()
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC)
	{
		return;
	}
	if (GetControllerMode() != EVoxelControllerMode::SceneBuildMode)
	{
		return;
	}

	UVoxelToolBase* Tool = GetCurrentTool();
	if (!Tool) return;

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC) return;

	// 拖拽阶段不重复触发家具拾取（拾取仅在按下时由 HandleMouseInput 发起）
	FHitResult GroundHit;
	AVoxel3DUIActor* GizmoActor = nullptr;
	if (!TraceMouseScene(PC, GroundHit, GizmoActor)) return;

	// 单体素/笔刷工具不覆写 OnMouseDrag，基类默认仅处理两点编辑 PLACING 阶段，故为空操作
	Tool->OnMouseDrag(BuildToolContext(), GroundHit, GizmoActor);
}

void UVoxelInteractionComponent::HandleMouseRelease()
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	// ==================== 家具直接操作分流 ====================
	// bPressed + !bPickedUp = 短按松手 → 单击查看
	// bPickedUp = 已拾取，走正常 PLACING 流程（不拦截释放）
	if (FurnitureDragState.bPressed && !FurnitureDragState.bPickedUp)
	{
		StopFurnitureDragTimer();
		// 隐藏进度圈（短按松手，未触发拾取）
		if (UVoxelPrimaryGameLayout* Layout = GetPrimaryLayout())
		{
			Layout->HideDragProgress();
		}
		HandleFurnitureInspect(FurnitureDragState.Actor.Get());
		FurnitureDragState = FFurnitureDragState();
		return; // 不走工具流程
	}

	// 松手时必须清空活跃 Gizmo 拖拽，否则 Tick 会持续驱动 Gizmo 跟随鼠标
	if (InputState.ActiveDragGizmo != nullptr)
	{
		InputState.ActiveDragGizmo = nullptr;
		InputState.bIsModifying = false;
		UE_LOG(LogTemp, Log, TEXT("[EditChain] HandleMouseRelease: ActiveDragGizmo cleared"));
	}

	if (UVoxelToolBase* Tool = GetCurrentTool())
	{
		Tool->OnMouseRelease(BuildToolContext());
	}
}

void UVoxelInteractionComponent::HandleMouseCancel()
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	// ==================== 右键统一清理：详情浮窗 + 选中描边 ====================
	// 无论处于拖拽取消分支还是工具取消分支，浮窗和描边都应被清除。
	// 清空 SelectedFurniture 会触发 OnBuildModeParamsUpdated → UpdateFurnitureSelectionHighlight
	//   → SetOutlinedActor(nullptr) → RemoveOutlineFromActor（关闭 CustomDepth）
	if (UVoxelPrimaryGameLayout* Layout = GetPrimaryLayout())
	{
		Layout->HideInfoTooltip();
	}
	if (SC->GetSelectedFurniture())
	{
		SC->SetSelectedFurniture(nullptr);
	}

	// 家具长按/拾取状态右键 → 取消
	if (FurnitureDragState.bPressed || FurnitureDragState.bPickedUp)
	{
		StopFurnitureDragTimer();
		CancelFurnitureDrag();
		FurnitureDragState = FFurnitureDragState();

		// 隐藏进度圈（右键取消时清理）
		if (UVoxelPrimaryGameLayout* Layout = GetPrimaryLayout())
		{
			Layout->HideDragProgress();
		}
		return;
	}

	if (UVoxelToolBase* Tool = GetCurrentTool())
	{
		Tool->OnMouseCancel(BuildToolContext());
	}
}

// ==================== Gizmo 操作实现 ====================

bool UVoxelInteractionComponent::DispatchGizmoAction(AVoxel3DUIActor* Gizmo, int32 Index)
{
	UVoxelPreviewComponent* VC = GetPreviewComponent();
	if (!VC || !Gizmo) return false;

	if (VC->TranslateGizmos.Find(Gizmo, Index))
	{
		HandleGizmo_Translate(Gizmo, Index);
		return true;
	}
	if (VC->RotateGizmos.Find(Gizmo, Index))
	{
		HandleGizmo_Rotate(Gizmo, Index);
		return true;
	}
	if (VC->FaceGizmos.Find(Gizmo, Index))
	{
		HandleGizmo_Face(Gizmo, Index);
		return true;
	}
	return false;
}

void UVoxelInteractionComponent::HandleGizmoInteraction(AVoxel3DUIActor* Input, FVector WorldPosition)
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	UVoxelPreviewComponent* VC = GetPreviewComponent();
	if (!SC || !VC) return;

	// 持续拖拽：不限阶段（EDITING 的 Gizmo 平移/旋转/面推拉，IDLE 的选区平移，共用此路径）
	if (InputState.ActiveDragGizmo)
	{
		InputState.SecondPoint = WorldPosition;
		DispatchGizmoAction(InputState.ActiveDragGizmo, INDEX_NONE);
		InputState.FirstPoint = InputState.SecondPoint;
		return;
	}

	// 激活拖拽：仅 EDITING 阶段有效，需要 Input 指明的 Gizmo Actor
	if (!Input || GetBuildPhase() != EPreviewInteractionState::EDITING)
	{
		return;
	}

	InputState.ActiveDragGizmo = Input;

	// 用平面投影重算 WorldPosition，确保与 Tick 持续拖拽用同一坐标系
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		FVector RayOrigin, RayDir;
		if (PC->DeprojectMousePositionToWorld(RayOrigin, RayDir))
		{
			FVector ProjectedPoint;
			if (ProjectMouseToCameraPlane(RayOrigin, RayDir, ProjectedPoint))
			{
				WorldPosition = ProjectedPoint;
			}
		}
	}

	InputState.FirstPoint = WorldPosition;
	InputState.SecondPoint = WorldPosition;

	// 激活首帧立即分发一次（delta=0），Gizmo 位置跟随由 Tick 持续调用完成
	DispatchGizmoAction(Input, INDEX_NONE);

	InputState.FirstPoint = InputState.SecondPoint;
}

void UVoxelInteractionComponent::ReleaseGizmoInteraction()
{
	InputState.ActiveDragGizmo = nullptr;

	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	const EPreviewInteractionState CurPhase = SC ? GetBuildPhase() : EPreviewInteractionState::IDLE;

	switch (CurPhase)
	{
	// HIDDEN → 无操作（正常流程中点击时已直接转BASE）
	case EPreviewInteractionState::IDLE:
		{
			UE_LOG(LogTemp, Log, TEXT("[EditChain] ReleaseGizmoInteraction: HIDDEN - no action (should not happen in normal flow)"));
			break;
		}
	// BASE → EDIT：松手后进入编辑模式
	case EPreviewInteractionState::PLACING:
		{
			ReleaseBatchEditPreview();
			UE_LOG(LogTemp, Log, TEXT("[EditChain] ReleaseGizmoInteraction: BASE -> EDIT"));
			break;
		}
	default:
		break;
	}
	// 不再清空 FirstPoint 和 SecondPoint，保留编辑数据
	InputState.bIsModifying = false;
	UE_LOG(LogTemp, VeryVerbose, TEXT("ReleaseGizmoInteraction: bIsModifying = false"));
}

void UVoxelInteractionComponent::SetTempAreaPosition(FBox Bounds)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetTmpAreaCoverage(Bounds);
	}
}

void UVoxelInteractionComponent::ClearAreaInfo()
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->InitTmpAreaInfoForCreate();
		// StopModifying();
	}
}

// ==================== 几何操作 ====================

void UVoxelInteractionComponent::SetGeometryRotation(FRotator NewRotation)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetTmpGeometryRotation(NewRotation);
	}
}

void UVoxelInteractionComponent::SetGeometryThickness(int32 NewThickness)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetTmpGeometryThickness(NewThickness);
	}
}

void UVoxelInteractionComponent::SetRegularGeometry(bool bEnable)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetTmpGeometryRegular(bEnable);
	}
}

void UVoxelInteractionComponent::SetTmpGeometry(const FVoxelGeometryDesc& NewGeometry)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetTmpGeometry(NewGeometry);
	}
}

void UVoxelInteractionComponent::SetTemplateRemapEntry(const FString& OriginFullId, const FString& NewFullId)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetTemplateRemapEntry(OriginFullId, NewFullId);
	}
}

// ==================== 模板操作 ====================

bool UVoxelInteractionComponent::SaveSelectionAsTemplate(const FTemplateMeta& Meta)
{
	UVoxelPlayerStateCacheComponent* SelComp = GetStateCache();
	if (!SelComp || !SelComp->HasSelection())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Interaction] SaveSelectionAsTemplate: no selection"));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UVoxelBlockWorldSubsystem* BlockSys = World->GetSubsystem<UVoxelBlockWorldSubsystem>();
	UVoxelItemWorldSubsystem* ItemWorld = World->GetSubsystem<UVoxelItemWorldSubsystem>();
	UConfigDataSubsystem* ConfigSys = World->GetGameInstance()->GetSubsystem<UConfigDataSubsystem>();
	if (!BlockSys || !ConfigSys)
	{
		return false;
	}

	// 捕获选区为模板
	FVoxelTemplate Tmpl = UVoxelTemplateUtils::CaptureFromSelection(
		BlockSys, ItemWorld, ConfigSys,
		SelComp->GetCurrentSelection(), Meta);

	if (Tmpl.Entries.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Interaction] SaveSelectionAsTemplate: capture returned 0 entries"));
		return false;
	}

	// 自动落盘到 Saved/Templates/<Guid>.tmpl
	const FString TmplPath = UVoxelTemplateUtils::GetTmplFilePath(Tmpl.TemplateGuid);
	if (UVoxelTemplateUtils::SaveToTmplFile(Tmpl, TmplPath))
	{
		UE_LOG(LogTemp, Log, TEXT("[Interaction] SaveSelectionAsTemplate: persisted to %s"), *TmplPath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Interaction] SaveSelectionAsTemplate: failed to persist to %s"), *TmplPath);
	}

	// 注册到配置系统（模板只进 RuntimeTemplates，不进 ItemMap）
	const int32 NewId = ConfigSys->RegisterRuntimeTemplate(MoveTemp(Tmpl));
	UE_LOG(LogTemp, Log, TEXT("[Interaction] SaveSelectionAsTemplate: registered, ItemId=%d"), NewId);

	// 广播通知模板库刷新（模板库模块经 BindAllBuildEvents 订阅 OnGeometryChanged）
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->OnGeometryChanged.Broadcast();
	}
	return true;
}

// ==================== 切面 / Pawn 高度 / 渲染图层（UI 参数写入） ====================

void UVoxelInteractionComponent::SetEditSliceLayer(int32 NewLayer)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetEditSliceLayer(NewLayer);
	}
}

void UVoxelInteractionComponent::SetSliceEnabled(bool bEnabled)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetSliceEnabled(bEnabled);
	}
}

void UVoxelInteractionComponent::SetSliceFilterMode(ESliceFilterMode NewMode)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetSliceFilterMode(NewMode);
	}
}

void UVoxelInteractionComponent::SetPawnHeightLayer(int32 NewLayer)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetPawnHeightLayer(NewLayer);
	}
}

void UVoxelInteractionComponent::SetLayerActive(FGameplayTag LayerTag, bool bActive)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetLayerActive(LayerTag, bActive);
	}
}

// ==================== 透明区域框（UI 参数写入） ====================

void UVoxelInteractionComponent::SetBoxEnabled(int32 BoxId, bool bEnabled)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetBoxEnabled(BoxId, bEnabled);
	}
}

void UVoxelInteractionComponent::SetAllCutoutBoxesEnabled(bool bEnabled)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetAllCutoutBoxesEnabled(bEnabled);
	}
}

void UVoxelInteractionComponent::RemoveCutoutBox(int32 BoxId)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->RemoveCutoutBox(BoxId, GetHistoryComponent());
	}
}

bool UVoxelInteractionComponent::FocusCutoutBox(int32 BoxId)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		return SC->FocusCutoutBox(BoxId);
	}
	return false;
}

// ==================== 区域修改 ====================

void UVoxelInteractionComponent::ModifyCurrentArea(FAreaModificationData& NewData)
{
	AVoxelPlayerController* PC = GetOwner<AVoxelPlayerController>();
	if (!PC) return;

	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	AVoxelAreaBaseInstance* CurrentArea = SC->GetCurrentArea();
	if (!CurrentArea) return;

	UVoxelAreaWorldSubsystem* AreaWorldSubsystem = PC->GetAreaWorldSubsystem();
	if (!AreaWorldSubsystem) return;

	AreaWorldSubsystem->UpdateAreaByPtr(PC, CurrentArea, NewData);
}

void UVoxelInteractionComponent::ModifyArea(FAreaModificationData& NewData)
{
	AVoxelPlayerController* PC = GetOwner<AVoxelPlayerController>();
	if (!PC) return;

	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	if (AVoxelAreaBaseInstance* CurrentArea = SC->GetCurrentArea())
	{
		UVoxelAreaWorldSubsystem* AreaWorldSubsystem = PC->GetAreaWorldSubsystem();
		if (AreaWorldSubsystem)
		{
			AreaWorldSubsystem->UpdateAreaByPtr(PC, CurrentArea, NewData);
		}
	}
	else
	{
		SC->SetTmpAreaInfo(NewData);
	}
}

void UVoxelInteractionComponent::UpdateTmpAreaInfo(FAreaModificationData& NewData)
{
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		SC->SetTmpAreaInfo(NewData);
	}
}

// ==================== 编辑流程控制 ====================

void UVoxelInteractionComponent::StepBack()
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC)
	{
		return;
	}

	const EPreviewInteractionState CurPhase = GetBuildPhase();

	switch (CurPhase)
	{
	case EPreviewInteractionState::EDITING:
		// EDIT → HIDDEN: 取消编辑，完全重置
		{
			CancelCurrentEdit();
			UE_LOG(LogTemp, Log, TEXT("[EditChain] StepBack: EDIT -> HIDDEN (cancel edit)"));
			break;
		}

	case EPreviewInteractionState::PLACING:
		// BASE → HIDDEN: 取消选择，完全重置
		{
			CancelCurrentEdit();
			UE_LOG(LogTemp, Log, TEXT("[EditChain] StepBack: BASE -> HIDDEN (cancel selection)"));
			break;
		}

	case EPreviewInteractionState::IDLE:
		// HIDDEN → 完全重置
		{
			CancelCurrentEdit();
			UE_LOG(LogTemp, Log, TEXT("[EditChain] StepBack: HIDDEN -> CancelCurrentEdit"));
			break;
		}

	default:
		break;
	}
}

// ==================== 历史记录操作 ====================
// 业务已归位 HistoryComponent（Undo/Redo/ClearHistory），此处仅薄委托保持蓝图兼容

bool UVoxelInteractionComponent::Undo()
{
	UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComponent();
	if (!HistoryComp)
	{
		UE_LOG(LogTemp, Error, TEXT("Interaction::Undo: HistoryComponent not found"));
		return false;
	}
	return HistoryComp->Undo();
}

bool UVoxelInteractionComponent::Redo()
{
	UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComponent();
	if (!HistoryComp)
	{
		UE_LOG(LogTemp, Error, TEXT("Interaction::Redo: HistoryComponent not found"));
		return false;
	}
	return HistoryComp->Redo();
}

void UVoxelInteractionComponent::ClearHistory()
{
	UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComponent();
	if (!HistoryComp)
	{
		UE_LOG(LogTemp, Error, TEXT("Interaction::ClearHistory: HistoryComponent not found"));
		return;
	}
	HistoryComp->ClearHistory();
}

// ==================== 编辑器接口实现 ====================

void UVoxelInteractionComponent::RequestConfirmVoxelEdit()
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	if (UVoxelToolBase* Tool = GetCurrentTool())
	{
		Tool->OnConfirmEdit(BuildToolContext());
	}
}

void UVoxelInteractionComponent::RequestCancelVoxelEdit()
{
	if (!GetOwner() || !GetWorld())
	{
		return;
	}

	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC)
	{
		return;
	}

	// 清空临时编辑请求
	SC->ClearTmpEditRequest();

	// 清除预览显示
	AVoxelPlayerController* PC = GetOwner<AVoxelPlayerController>();
	if (PC && PC->PreviewComp)
	{
		PC->PreviewComp->ClearPreview();
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("RequestCancelVoxelEdit - Preview cancelled"));
}

void UVoxelInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;
	if (GetControllerMode() != EVoxelControllerMode::SceneBuildMode) return;

	// ==================== 家具长按检测 ====================
	// 已由 FTimerHandle（StartFurnitureDragTimer）驱动，Tick 不再参与

	// Gizmo 拖拽持续更新
	if (InputState.ActiveDragGizmo)
	{
		APlayerController* PC = Cast<APlayerController>(GetOwner());
		if (!PC) return;

		FVector WorldLocation, WorldDirection;
		if (!PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection)) return;

		// 直接用相机平面投影（替代原 HeightCollisionVisual 的碰撞面）
		// 平面经过几何体中心，法线是相机前进方向，与原 HeightCollisionVisual 行为一致
		// 不使用地面射线：避免 Z 轴拖拽时射线命中远处地面导致位置闪烁
		FVector ProjectedPoint;
		if (ProjectMouseToCameraPlane(WorldLocation, WorldDirection, ProjectedPoint))
		{
			HandleGizmoInteraction(InputState.ActiveDragGizmo, ProjectedPoint);
		}
		return;
	}

	// Brush 拖拽持续涂抹
	if (InputState.bIsBrushDragging)
	{
		APlayerController* PC = Cast<APlayerController>(GetOwner());
		if (!PC) return;

		FHitResult GroundHit;

		// 优先复用本帧 Hover 已缓存的地面命中（HandleMouseHover 由蓝图 Tick 每帧调用）
		if (bLatestGroundHitValid)
		{
			GroundHit.Location = LatestGroundHitLocation;
			GroundHit.bBlockingHit = true;

			if (UVoxelToolBase* Tool = GetCurrentTool())
			{
				Tool->OnMouseInput(BuildToolContext(), GroundHit, nullptr);
			}
			return;
		}

		FVector WorldLocation, WorldDirection;
		if (!PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection)) return;

		const FVector TraceEnd = WorldLocation + WorldDirection * TraceDistance;
		FCollisionQueryParams Params;
		Params.bTraceComplex = true;

		const bool bGroundHit = GetWorld()->LineTraceSingleByChannel(
			GroundHit, WorldLocation, TraceEnd, ECC_GameTraceChannel1, Params);

		if (bGroundHit)
		{
			// 委托给当前工具处理 Brush 涂抹
			if (UVoxelToolBase* Tool = GetCurrentTool())
			{
				Tool->OnMouseInput(BuildToolContext(), GroundHit, nullptr);
			}
		}
		return;
	}

	// 非拖拽状态：Hover 由蓝图 Tick 调用的 HandleMouseHover 处理
}

bool UVoxelInteractionComponent::ProjectMouseToCameraPlane(
	const FVector& RayOrigin, const FVector& RayDir, FVector& OutPoint) const
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC) return false;

	// 1. 获取相机方向作为平面法线
	FVector CameraLoc;
	FRotator CameraRot;
	PC->GetPlayerViewPoint(CameraLoc, CameraRot);
	const FVector PlaneNormal = CameraRot.Vector();

	// 2. 平面经过当前几何体中心
	FVector PlaneCenter = FVector::ZeroVector;
	if (UVoxelPlayerStateCacheComponent* SC = GetStateCache())
	{
		const float BlockLength = UChunkUtilsFunction::GetBlockLength(this);
		PlaneCenter = SC->GetTmpGeometry().GetPivotWorld(BlockLength);
	}

	// 3. 射线-平面求交
	const float Denom = FVector::DotProduct(PlaneNormal, RayDir);
	if (FMath::Abs(Denom) < KINDA_SMALL_NUMBER) return false; // 射线与平面平行

	const float T = FVector::DotProduct(PlaneCenter - RayOrigin, PlaneNormal) / Denom;
	if (T < 0) return false; // 交点在射线反方向

	OutPoint = RayOrigin + RayDir * T;
	return true;
}

UVoxelPrimaryGameLayout* UVoxelInteractionComponent::GetPrimaryLayout() const
{
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		return UVoxelPrimaryGameLayout::GetFromPC(PC);
	}
	return nullptr;
}

bool UVoxelInteractionComponent::TraceMouseScene(APlayerController* PC, FHitResult& OutGroundHit, AVoxel3DUIActor*& OutGizmoActor)
{
	if (!PC) return false;

	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return false;

	FVector WorldLocation, WorldDirection;
	if (!PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection)) return false;

	const FVector TraceEnd = WorldLocation + WorldDirection * TraceDistance;
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;

	// 地面射线
	const bool bGroundHit = GetWorld()->LineTraceSingleByChannel(
		OutGroundHit, WorldLocation, TraceEnd, ECC_GameTraceChannel1, Params);

	// PLACING 地面未命中 → 回退相机平面投影（拖拽第二点可能悬空）
	if (!bGroundHit && GetBuildPhase() == EPreviewInteractionState::PLACING)
	{
		FVector ProjectedPoint;
		if (ProjectMouseToCameraPlane(WorldLocation, WorldDirection, ProjectedPoint))
		{
			OutGroundHit.Location = ProjectedPoint;
			OutGroundHit.bBlockingHit = true;
			OutGroundHit.TraceStart = WorldLocation;
			OutGroundHit.TraceEnd = TraceEnd;
		}
	}

	// Gizmo 射线（EDITING 阶段）
	OutGizmoActor = nullptr;
	if (GetBuildPhase() == EPreviewInteractionState::EDITING)
	{
		FHitResult GizmoHit;
		FCollisionQueryParams GizmoParams;
		GizmoParams.bTraceComplex = false;
		if (CachedPreviewComponent && CachedPreviewComponent->PreviewActor)
		{
			GizmoParams.AddIgnoredActor(CachedPreviewComponent->PreviewActor);
		}
		const bool bGizmoHit = GetWorld()->LineTraceSingleByChannel(GizmoHit, WorldLocation, TraceEnd, ECC_GameTraceChannel6, GizmoParams);
		if (bGizmoHit)
		{
			OutGizmoActor = Cast<AVoxel3DUIActor>(GizmoHit.GetActor());
		}
	}

	return bGroundHit || OutGizmoActor || InputState.ActiveDragGizmo;
}

// ==================== 组件访问器 ====================

UVoxelPreviewComponent* UVoxelInteractionComponent::GetPreviewComponent() const
{
	// 使用缓存的引用（在 BeginPlay 中初始化）
	return CachedPreviewComponent;
}

UVoxelControllerHistoryComponent* UVoxelInteractionComponent::GetHistoryComponent() const
{
	// 使用缓存的引用（在 BeginPlay 中初始化）
	return CachedHistoryComponent;
}

// ==================== 工具系统 ====================

UVoxelToolSubsystem* UVoxelInteractionComponent::GetToolSubsystem() const
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UVoxelToolSubsystem>() : nullptr;
}

UVoxelToolBase* UVoxelInteractionComponent::GetCurrentTool() const
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return nullptr;

	const FGameplayTag ToolTag = GetCurrentToolTag();
	if (!ToolTag.IsValid()) return nullptr;

	UVoxelToolSubsystem* ToolSys = GetToolSubsystem();
	if (!ToolSys)
	{
		UE_LOG(LogTemp, Error, TEXT("[Interaction] GetCurrentTool: VoxelToolSubsystem 不可用"));
		return nullptr;
	}

	// 精确匹配：子系统在 GameInstance 初始化期完成注册（先于一切 World/Controller），
	// 查不到即 Tag 配置错误，直接报错不降级
	if (UVoxelToolBase* Found = ToolSys->FindTool(ToolTag))
	{
		return Found;
	}

	UE_LOG(LogTemp, Error, TEXT("[Interaction] GetCurrentTool: 未注册的工具 Tag '%s'"), *ToolTag.ToString());
	return nullptr;
}

FVoxelToolContext UVoxelInteractionComponent::BuildToolContext()
{
	FVoxelToolContext Ctx;
	Ctx.InteractionComp = this;
	Ctx.StateCacheComp = GetStateCache();
	Ctx.PreviewComp = GetPreviewComponent();
	Ctx.HistoryComp = GetHistoryComponent();
	Ctx.PlayerController = GetOwner<AVoxelPlayerController>();
	Ctx.InputState = &InputState;
	Ctx.TraceDistance = TraceDistance;
	return Ctx;
}

// ==================== 家具直接操作实现 ====================

AUVoxelItemBaseInstance* UVoxelInteractionComponent::TraceFurniture()
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC) return nullptr;

	FVector WorldLocation, WorldDirection;
	if (!PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection)) return nullptr;

	const FVector TraceEnd = WorldLocation + WorldDirection * TraceDistance;
	FCollisionQueryParams Params;
	Params.bTraceComplex = false;

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, WorldLocation, TraceEnd, ECC_GameTraceChannel2, Params))
	{
		return Cast<AUVoxelItemBaseInstance>(Hit.GetActor());
	}
	return nullptr;
}

void UVoxelInteractionComponent::HandleFurnitureInspect(AUVoxelItemBaseInstance* Furniture)
{
	if (!Furniture) return;

	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	// 选中家具（VisualUIComponent 可读取 BuildStateComp->GetSelectedFurniture() 显示高亮）
	SC->SetSelectedFurniture(Furniture);

	UE_LOG(LogTemp, Log, TEXT("[FurnitureDrag] Inspect: %s (HandleId=%d)"),
		*Furniture->GetName(), Furniture->GetFurnitureHandleId());

	// 接入 FurnitureDetailTooltip 浮窗（非模态，跟随鼠标位置，Context 传家具 Actor）
	if (UVoxelPrimaryGameLayout* Layout = GetPrimaryLayout())
	{
		// 从 Layout 配置获取 Tooltip 类（替代硬编码 LoadClass）
		TSubclassOf<UVoxelInfoTooltip> TooltipClass = Layout->FurnitureTooltipClass;

			if (TooltipClass)
			{
				// 若已有同一 Tooltip 打开则先关闭（避免多次单击叠加；切换不同家具也更新内容）
				if (Layout->HasInfoTooltip())
				{
					Layout->HideInfoTooltip();
				}
				Layout->ShowInfoTooltip(TooltipClass, Furniture);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[FurnitureDrag] FurnitureDetailDialog blueprint not found at /Game/Resource/UI/Menu/WBP_FurnitureDetailDialog (C++ base ready, create blueprint subclass to enable)"));
			}
	}
}

void UVoxelInteractionComponent::StartFurnitureDragTimer()
{
	StopFurnitureDragTimer();
	GetWorld()->GetTimerManager().SetTimer(
		FurnitureDragTimerHandle, this, &ThisClass::HandleFurnitureDragTimer,
		0.033f, true);
}

void UVoxelInteractionComponent::StopFurnitureDragTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FurnitureDragTimerHandle);
	}
}

void UVoxelInteractionComponent::HandleFurnitureDragTimer()
{
	// 仅在按下且未拾取时检测长按
	if (!FurnitureDragState.bPressed || FurnitureDragState.bPickedUp)
	{
		StopFurnitureDragTimer();
		return;
	}
	if (!FurnitureDragState.Actor.IsValid())
	{
		StopFurnitureDragTimer();
		return;
	}

	// 更新进度圈进度（0~1）
	if (UVoxelPrimaryGameLayout* Layout = GetPrimaryLayout())
	{
		Layout->UpdateDragProgress(GetFurnitureDragProgress());
	}

	const float Elapsed = GetWorld()->GetTimeSeconds() - FurnitureDragState.PressTime;
	if (Elapsed >= FurnitureDragDelay)
	{
		StopFurnitureDragTimer();
		PickupFurniture();
	}
}

void UVoxelInteractionComponent::PickupFurniture()
{
	if (!FurnitureDragState.Actor.IsValid()) return;

	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	AUVoxelItemBaseInstance* Furniture = FurnitureDragState.Actor.Get();

	// 1. 设置手持数据（和新建家具走相同的 PLACING 流程）
	FInteractableData HandData;
	HandData.InteractableType = EInteractableType::Place;
	HandData.HandleId = Furniture->GetFurnitureHandleId();
	HandData.Direction = Furniture->ItemInstance.Direction;
	HandData.ItemTag = Furniture->ItemInstance.ItemTag;
	HandData.Quantity = 1;
	SC->SetInteractableDataInHand(HandData);

	// 2. 同步 TmpGeometry.Rotation（让预览朝向与原家具一致）
	FVoxelGeometryDesc Geometry = SC->GetTmpGeometry();
	Geometry.Rotation = UChunkUtilsFunction::GetSpawnDirection(Furniture->ItemInstance.Direction);
	SC->SetTmpGeometry(Geometry);

	// 3. 隐藏原 Actor（放置时销毁，取消时恢复显示）
	Furniture->SetActorHiddenInGame(true);

	// 4. 标记已拾取，释放按下状态（之后走正常 PLACING 流程）
	FurnitureDragState.bPickedUp = true;
	FurnitureDragState.bPressed = false;

	// 5. 隐藏进度圈（已拾取，进入正常 PLACING 流程）
	if (UVoxelPrimaryGameLayout* Layout = GetPrimaryLayout())
	{
		Layout->HideDragProgress();
	}

	// BuildPhase 保持 IDLE，OnMouseHover 会自动切换到 PLACING
	UE_LOG(LogTemp, Log, TEXT("[FurnitureDrag] Picked up: %s (HandleId=%d)"),
		*Furniture->GetName(), Furniture->GetFurnitureHandleId());
}

void UVoxelInteractionComponent::OnFurniturePickedUpPlaced()
{
	// 非拾取状态 → 忽略（正常新建家具放置）
	if (!FurnitureDragState.bPickedUp) return;

	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return;

	// 1. 销毁原 Actor（新 Actor 已由 PlaceFurniture 生成）
	if (FurnitureDragState.Actor.IsValid())
	{
		FurnitureDragState.Actor->Destroy();
		UE_LOG(LogTemp, Log, TEXT("[FurnitureDrag] Original actor destroyed after placement"));
	}

	// 2. 清空手持数据 + 回到 IDLE
	FInteractableData EmptyHand;
	SC->SetInteractableDataInHand(EmptyHand);
	SC->SetBuildPhase(EPreviewInteractionState::IDLE);
	SC->SetSelectedFurniture(nullptr);

	// 3. 清空预览
	if (UVoxelPreviewComponent* VC = GetPreviewComponent())
	{
		VC->ClearPreview();
	}

	FurnitureDragState = FFurnitureDragState();
}

float UVoxelInteractionComponent::GetFurnitureDragProgress() const
{
	if (!FurnitureDragState.bPressed || FurnitureDragState.bPickedUp) return 0.f;
	const float Elapsed = GetWorld()->GetTimeSeconds() - FurnitureDragState.PressTime;
	return FMath::Clamp(Elapsed / FurnitureDragDelay, 0.f, 1.f);
}

bool UVoxelInteractionComponent::IsFurnitureDragPressing() const
{
	return FurnitureDragState.bPressed && !FurnitureDragState.bPickedUp;
}

void UVoxelInteractionComponent::CancelFurnitureDrag()
{
	// 拾取状态下取消 → 恢复原Actor显示 + 清空手持 + 回到IDLE
	if (FurnitureDragState.bPickedUp && FurnitureDragState.Actor.IsValid())
	{
		FurnitureDragState.Actor->SetActorHiddenInGame(false);

		UVoxelPlayerStateCacheComponent* SC = GetStateCache();
		if (SC)
		{
			FInteractableData EmptyHand;
			SC->SetInteractableDataInHand(EmptyHand);
			SC->SetBuildPhase(EPreviewInteractionState::IDLE);
			SC->SetSelectedFurniture(nullptr);
		}

		if (UVoxelPreviewComponent* VC = GetPreviewComponent())
		{
			VC->ClearPreview();
		}
		UE_LOG(LogTemp, Log, TEXT("[FurnitureDrag] Canceled: actor restored, hand cleared"));
	}
}

// ==================== 键盘快捷键处理 ====================

bool UVoxelInteractionComponent::HandleKeyDown(const FKey& Key)
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return false;
	if (GetControllerMode() != EVoxelControllerMode::SceneBuildMode) return false;

	// 交给工具处理
	if (UVoxelToolBase* Tool = GetCurrentTool())
	{
		return Tool->OnKeyDown(BuildToolContext(), Key.GetFName());
	}

	return false;
}

bool UVoxelInteractionComponent::DeleteSelectedFurniture()
{
	UVoxelPlayerStateCacheComponent* SC = GetStateCache();
	if (!SC) return false;

	AUVoxelItemBaseInstance* Furniture = SC->GetSelectedFurniture();
	if (!Furniture) return false;

	UE_LOG(LogTemp, Log, TEXT("[FurnitureDrag] Deleted: %s"), *Furniture->GetName());

	// 记录历史（可撤销）：Before 快照保存家具信息，After 留空表示删除
	if (UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComponent())
	{
		auto ToPlacementDir = [](EActorDirection D) -> EPlacementDirection {
			switch (D)
			{
			case EActorDirection::Forward:  return EPlacementDirection::Forward;
			case EActorDirection::Right:    return EPlacementDirection::Right;
			case EActorDirection::Backward: return EPlacementDirection::Backward;
			case EActorDirection::Left:     return EPlacementDirection::Left;
			default:                        return EPlacementDirection::Forward;
			}
		};

		FActorSnapshot BeforeSnap;
		BeforeSnap.HandleId = Furniture->GetFurnitureHandleId();
		const float BlockLength = UChunkUtilsFunction::GetBlockLength(this);
		BeforeSnap.Origin = FIntVector(
			FMath::RoundToInt(Furniture->GetActorLocation().X / BlockLength),
			FMath::RoundToInt(Furniture->GetActorLocation().Y / BlockLength),
			FMath::RoundToInt(Furniture->GetActorLocation().Z / BlockLength)
		);
		BeforeSnap.Direction = ToPlacementDir(Furniture->ItemInstance.Direction);
		BeforeSnap.OccupiedCells = Furniture->OccupiedBlock;
		BeforeSnap.Quantity = Furniture->ItemInstance.Quantity;

		FVoxelEditHistoryRecord Record;
		Record.ToolTag = FCoreTags::Tool_Edit_Furniture;
		Record.PlacementOpType = EPlacementOp::Remove;
		Record.BeforeSnapshot = BeforeSnap;
		// AfterSnapshot 留空表示删除
		Record.bCanBeUndone = true;
		Record.Description = FText::FromString(TEXT("Delete Furniture"));
		Record.Timestamp = FDateTime::UtcNow();
		Record.DoneTimestamp = FDateTime::UtcNow();

		HistoryComp->SaveHistoryRecord(Record);
		UE_LOG(LogTemp, Log, TEXT("[FurnitureDrag] History saved: Delete %s"),
			*Furniture->GetName());
	}

	Furniture->Destroy();
	SC->SetSelectedFurniture(nullptr);
	return true;
}
