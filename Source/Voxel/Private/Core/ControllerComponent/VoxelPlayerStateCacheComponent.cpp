// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"

#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"
#include "Core/CoreTags.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Item/VoxelItemBaseInstance.h"
#include "AI/NPC/VoxelNpcBaseInstance.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "Core/VoxelGameInstance.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Voxel/Editor/VoxelHistoryData.h"
#include "Voxel/VoxelPlayerController.h"
#include "GameFramework/PlayerController.h"

UVoxelPlayerStateCacheComponent::UVoxelPlayerStateCacheComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ==================== 世界实体选中（原 EntitySelectionComponent） ====================

void UVoxelPlayerStateCacheComponent::SelectAreaById(int32 AreaId)
{
	// Id 无效或区域已被删除：退化为清除选中
	UVoxelAreaWorldSubsystem* AreaSys = GetWorld() ? GetWorld()->GetSubsystem<UVoxelAreaWorldSubsystem>() : nullptr;
	AVoxelAreaBaseInstance* Area = AreaSys ? AreaSys->GetAreaActor(AreaId) : nullptr;
	if (!Area)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StateCache] SelectAreaById(%d): 区域不存在，清除选中"), AreaId);
		ClearSelectedEntity();
		return;
	}

	SetSelectedEntity(EVoxelSelectedEntityType::Area, AreaId, false);
}

void UVoxelPlayerStateCacheComponent::ClearSelectedEntity()
{
	SetSelectedEntity(EVoxelSelectedEntityType::None, -1, false);
}

void UVoxelPlayerStateCacheComponent::ForceClearSelectedEntity()
{
	SetSelectedEntity(EVoxelSelectedEntityType::None, -1, true);
}

void UVoxelPlayerStateCacheComponent::SetSelectedAreaActor(AVoxelAreaBaseInstance* Area, bool bForceDriveBuildState)
{
	if (!Area)
	{
		SetSelectedEntity(EVoxelSelectedEntityType::None, -1, bForceDriveBuildState);
		return;
	}
	SetSelectedEntity(EVoxelSelectedEntityType::Area, Area->AreaBase.AreaId, bForceDriveBuildState);
}

void UVoxelPlayerStateCacheComponent::HandleWorldClick()
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !GetWorld()) return;

	FVector WorldLocation, WorldDirection;
	if (!PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection)) return;
	const FVector TraceEnd = WorldLocation + WorldDirection * 100000.0;

	// 模式策略：区域/家具任何模式可选中；生物仅非建造模式可选中
	//（模式持有者 InteractionComp，经本组件转发读取）
	const bool bAllowCreature = GetControllerMode() != EVoxelControllerMode::SceneBuildMode;

	// 优先级：生物 > 家具 > 区域（精确目标优先，区域包围盒体积最大兜底）

	// 生物（Visibility 通道；建造模式跳过）
	if (bAllowCreature)
	{
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.bTraceComplex = true;
		if (GetWorld()->LineTraceSingleByChannel(Hit, WorldLocation, TraceEnd, ECC_Visibility, Params))
		{
			if (AVoxelNpcBaseInstance* HitCreature = Cast<AVoxelNpcBaseInstance>(Hit.GetActor()))
			{
				UE_LOG(LogTemp, Log, TEXT("[StateCache] 点击选中生物: %s (ID: %d)"),
					*HitCreature->CreatureData.RealName, HitCreature->CreatureData.CreatureID);
				SetSelectedEntity(EVoxelSelectedEntityType::Creature, HitCreature->CreatureData.CreatureID, false);
				return;
			}
		}
	}

	// 家具（ECC_GameTraceChannel2，与交互组件的家具追踪一致）
	{
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.bTraceComplex = false;
		if (GetWorld()->LineTraceSingleByChannel(Hit, WorldLocation, TraceEnd, ECC_GameTraceChannel2, Params))
		{
			if (AUVoxelItemBaseInstance* HitFurniture = Cast<AUVoxelItemBaseInstance>(Hit.GetActor()))
			{
				UE_LOG(LogTemp, Log, TEXT("[StateCache] 点击选中家具: %s (HandleId: %d)"),
					*HitFurniture->GetName(), HitFurniture->GetFurnitureHandleId());
				SetSelectedEntity(EVoxelSelectedEntityType::Furniture, HitFurniture->GetFurnitureHandleId(), false);
				return;
			}
		}
	}

	// 区域（ECC_GameTraceChannel3，与区域编辑工具一致）
	{
		// 多命中遍历：相机处于区域盒子内部时首个命中是贯穿命中（bStartPenetrating），
		// 后方有真实目标盒（子区域/相邻区域）时优先取后者，与区域编辑工具的选中结果保持一致
		TArray<FHitResult> Hits;
		FCollisionQueryParams Params;
		Params.bTraceComplex = true;
		if (GetWorld()->LineTraceMultiByChannel(Hits, WorldLocation, TraceEnd, ECC_GameTraceChannel3, Params))
		{
			AVoxelAreaBaseInstance* PenetratingArea = nullptr;
			AVoxelAreaBaseInstance* TargetArea = nullptr;
			for (const FHitResult& Hit : Hits)
			{
				AVoxelAreaBaseInstance* HitArea = Cast<AVoxelAreaBaseInstance>(Hit.GetActor());
				if (!HitArea) continue;
				if (Hit.bStartPenetrating && !PenetratingArea)
				{
					PenetratingArea = HitArea;
					continue;
				}
				TargetArea = HitArea;
				break;
			}
			// 无后方真实目标时回退贯穿命中（站在区域内点内部空地 = 选中所在区域）
			AVoxelAreaBaseInstance* SelectedArea = TargetArea ? TargetArea : PenetratingArea;
			if (SelectedArea)
			{
				UE_LOG(LogTemp, Log, TEXT("[StateCache] 点击选中区域: %s (ID: %d)"),
					*SelectedArea->AreaBase.AreaName.ToString(), SelectedArea->AreaBase.AreaId);
				SelectAreaById(SelectedArea->AreaBase.AreaId);
				return;
			}
		}
	}

	// 点击空处：清除选中
	ClearSelectedEntity();
}

AVoxelAreaBaseInstance* UVoxelPlayerStateCacheComponent::GetSelectedArea() const
{
	if (SelectedType != EVoxelSelectedEntityType::Area || SelectedId < 0) return nullptr;
	UVoxelAreaWorldSubsystem* AreaSys = GetWorld() ? GetWorld()->GetSubsystem<UVoxelAreaWorldSubsystem>() : nullptr;
	return AreaSys ? AreaSys->GetAreaActor(SelectedId) : nullptr;
}

void UVoxelPlayerStateCacheComponent::SetSelectedEntity(EVoxelSelectedEntityType NewType, int32 NewId, bool bForceDriveBuildState)
{
	// 变化才广播守卫
	if (SelectedType == NewType && SelectedId == NewId) return;

	// 选中高亮：旧选中恢复、新选中高亮（与 PreviewComponent 对 CurArea 的处理同款）
	UVoxelAreaWorldSubsystem* AreaSys = GetWorld() ? GetWorld()->GetSubsystem<UVoxelAreaWorldSubsystem>() : nullptr;
	AVoxelAreaBaseInstance* NewAreaActor = (NewType == EVoxelSelectedEntityType::Area && AreaSys) ? AreaSys->GetAreaActor(NewId) : nullptr;
	if (AreaSys)
	{
		if (SelectedType == EVoxelSelectedEntityType::Area)
		{
			if (AVoxelAreaBaseInstance* OldArea = AreaSys->GetAreaActor(SelectedId))
			{
				OldArea->UpdateAreaVisual(true, false);
			}
		}
		if (NewAreaActor)
		{
			NewAreaActor->UpdateAreaVisual(true, true);
		}
	}

	// CurArea 派生联动（区域工作区，本组件持有）：
	//  - 选中区域 → CurArea 跟随；清空/选中其他类型 → CurArea 真清空
	//  - IDLE 守卫：EDITING（框选编辑中）不抢工具的编辑工作区；bForceDriveBuildState 绕过守卫
	{
		const bool bIdleOrForce = bForceDriveBuildState || GetBuildPhase() == EPreviewInteractionState::IDLE;
		if (bIdleOrForce)
		{
			if (NewAreaActor)
			{
				InternalSetCurrentArea(NewAreaActor);
			}
			else
			{
				ClearCurrentArea();
			}
		}
	}

	SelectedType = NewType;
	SelectedId = NewId;

	UE_LOG(LogTemp, Log, TEXT("[StateCache] SelectedEntity changed: Type=%d Id=%d (Force=%d)"), (int32)NewType, NewId, bForceDriveBuildState ? 1 : 0);
	OnSelectedEntityChanged.Broadcast(NewType, NewId);
}

// ==================== 体素选区（原 SelectionComponent） ====================

void UVoxelPlayerStateCacheComponent::SetSelectionMode(ESelectionOpMode Mode)
{
	if (SelectionMode != Mode)
	{
		SelectionMode = Mode;
		OnSelectionModeChanged.Broadcast(SelectionMode);
	}
}

void UVoxelPlayerStateCacheComponent::ApplySelectionOp(const TArray<FIntVector>& NewShape, ESelectionOpMode Op)
{
	switch (Op)
	{
	case ESelectionOpMode::New:
		CurrentSelection = NewShape;
		break;
	case ESelectionOpMode::Add:
		for (const FIntVector& Pos : NewShape)
		{
			CurrentSelection.AddUnique(Pos);
		}
		break;
	case ESelectionOpMode::Subtract:
		for (const FIntVector& Pos : NewShape)
		{
			CurrentSelection.Remove(Pos);
		}
		break;
	case ESelectionOpMode::Intersect:
	{
		TSet<FIntVector> NewSet(NewShape);
		TArray<FIntVector> Result;
		for (const FIntVector& Pos : CurrentSelection)
		{
			if (NewSet.Contains(Pos))
			{
				Result.Add(Pos);
			}
		}
		CurrentSelection = MoveTemp(Result);
		break;
	}
	default:
		break;
	}

	OnSelectionChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::ClearSelection()
{
	if (CurrentSelection.Num() == 0 && PreDragSelection.Num() == 0)
	{
		return;
	}
	CurrentSelection.Empty();
	PreDragSelection.Empty();
	OnSelectionChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::MoveSelection(const FIntVector& Offset)
{
	if (Offset == FIntVector::ZeroValue || CurrentSelection.Num() == 0)
	{
		return;
	}

	for (FIntVector& Voxel : CurrentSelection)
	{
		Voxel += Offset;
	}

	OnSelectionChanged.Broadcast();
}

FIntVector UVoxelPlayerStateCacheComponent::GetSelectionCenter() const
{
	if (CurrentSelection.Num() == 0)
	{
		return FIntVector::ZeroValue;
	}

	FIntVector Min = CurrentSelection[0];
	FIntVector Max = CurrentSelection[0];
	for (const FIntVector& Pos : CurrentSelection)
	{
		Min = FIntVector(FMath::Min(Min.X, Pos.X), FMath::Min(Min.Y, Pos.Y), FMath::Min(Min.Z, Pos.Z));
		Max = FIntVector(FMath::Max(Max.X, Pos.X), FMath::Max(Max.Y, Pos.Y), FMath::Max(Max.Z, Pos.Z));
	}
	return (Min + Max) / 2;
}

void UVoxelPlayerStateCacheComponent::SavePreDragSelection()
{
	PreDragSelection = CurrentSelection;
	OnPreDragChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetPreDragSelection(const TArray<FIntVector>& InSelection)
{
	PreDragSelection = InSelection;
	OnPreDragChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::ClearPreDragSelection()
{
	if (PreDragSelection.Num() == 0)
	{
		return;
	}
	PreDragSelection.Empty();
	OnPreDragChanged.Broadcast();
}

// ==================== 初始化 ====================

void UVoxelPlayerStateCacheComponent::BeginPlay()
{
	Super::BeginPlay();

	CurArea = nullptr;
	CurParentArea = nullptr;
	CachedInteractionComp = nullptr; // 惰性解析（GetInteractionComp 首次调用时缓存）
}

// ==================== 交互流程状态转发（持有者：InteractionComponent） ====================

UVoxelInteractionComponent* UVoxelPlayerStateCacheComponent::GetInteractionComp() const
{
	if (CachedInteractionComp != nullptr)
	{
		return CachedInteractionComp.Get();
	}
	if (AActor* Owner = GetOwner())
	{
		UVoxelInteractionComponent* Found = Owner->FindComponentByClass<UVoxelInteractionComponent>();
		if (Found)
		{
			const_cast<UVoxelPlayerStateCacheComponent*>(this)->CachedInteractionComp = Found;
		}
		return Found;
	}
	return nullptr;
}

EVoxelControllerMode UVoxelPlayerStateCacheComponent::GetControllerMode() const
{
	if (const UVoxelInteractionComponent* Interaction = GetInteractionComp())
	{
		return Interaction->GetControllerMode();
	}
	return EVoxelControllerMode::SceneBuildMode;
}

FGameplayTag UVoxelPlayerStateCacheComponent::GetCurrentToolTag() const
{
	if (const UVoxelInteractionComponent* Interaction = GetInteractionComp())
	{
		return Interaction->GetCurrentToolTag();
	}
	return FGameplayTag::EmptyTag;
}

EPreviewInteractionState UVoxelPlayerStateCacheComponent::GetBuildPhase() const
{
	if (const UVoxelInteractionComponent* Interaction = GetInteractionComp())
	{
		return Interaction->GetBuildPhase();
	}
	return EPreviewInteractionState::IDLE;
}

void UVoxelPlayerStateCacheComponent::SetBuildPhase(EPreviewInteractionState NewPhase)
{
	if (UVoxelInteractionComponent* Interaction = GetInteractionComp())
	{
		Interaction->SetBuildPhase(NewPhase);
	}
}

// ==================== 手持物 / 视线物 ====================

void UVoxelPlayerStateCacheComponent::SetInteractableDataInHand(FInteractableData& Data)
{
	// 手持物变化时清空模板素材替换表（切换模板/切换手持都应重置替换）
	if (InteractableDataInHand.HandleId != Data.HandleId)
	{
		TmpTemplateRemap.Empty();
	}
	InteractableDataInHand = Data;
	OnItemInHandChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetInteractableDataInSight(FInteractableData& Data)
{
	InteractableDataInSight = Data;
	OnItemInSightChanged.Broadcast(Data);
}

// ==================== 临时区域信息（原 BuildState 区域段） ====================

void UVoxelPlayerStateCacheComponent::InitTmpAreaInfo(int32 SceneId)
{
	ResetTmpAreaInfoInternal(SceneId);
}

void UVoxelPlayerStateCacheComponent::ResetTmpAreaInfo()
{
	int32 SceneId = -1;
	if (UVoxelGameInstance* GameInstance = GetVoxelGameInstance())
	{
		SceneId = GameInstance->SceneData.BaseData.SceneId;
	}

	ResetTmpAreaInfoInternal(SceneId);
}

void UVoxelPlayerStateCacheComponent::InitTmpAreaInfoForCreate()
{
	UVoxelGameInstance* GI = GetVoxelGameInstance();
	const int32 SceneId = GI ? GI->SceneData.BaseData.SceneId : -1;
	// 主角角色 ID：统一走 GetMainCharacterId 查询
	const int32 PlayerCreatureId = GI ? GI->GetMainCharacterId() : 0;

	// 基础字段重置（名称/类型/场景/状态/有效性/Coverage）
	TmpAreaInfo.AreaName = NAME_None;
	TmpAreaInfo.AreaType = FGameplayTag::EmptyTag;
	TmpAreaInfo.ParentScenceId = SceneId;
	TmpAreaInfo.Status = EAreaStatus::Active;
	TmpAreaInfo.bIsValid = false;
	TmpGeometry.Box = FAreaCoverageFragment();

	// 父区域 / Owner 继承（ADR-001：Owner = 父区域 Owner；无父区域 = 当前主角角色 ID）
	if (CurParentArea)
	{
		TmpAreaInfo.ParentAreaId = CurParentArea->AreaBase.AreaId;
		TmpAreaInfo.OwnerCreatureId = CurParentArea->AreaBase.OwnerCreatureId;
	}
	else
	{
		TmpAreaInfo.ParentAreaId = -1;
		TmpAreaInfo.OwnerCreatureId = PlayerCreatureId;
	}

	// Manager 默认 = 当前主角角色 ID
	TmpAreaInfo.ManagerCreatureId = PlayerCreatureId;

	OnTmpAreaChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetTmpAreaInfo(FAreaModificationData NewInfo)
{
	// 分离存储：区域信息存储到 TmpAreaInfo，Coverage 存储到 TmpGeometry.Box
	TmpAreaInfo.AreaName = NewInfo.AreaName;
	TmpAreaInfo.AreaType = NewInfo.AreaType;
	TmpAreaInfo.ParentScenceId = NewInfo.ParentScenceId;
	TmpAreaInfo.ParentAreaId = NewInfo.ParentAreaId;
	TmpAreaInfo.Status = NewInfo.Status;
	TmpAreaInfo.OwnerCreatureId = NewInfo.OwnerCreatureId;
	TmpAreaInfo.ManagerCreatureId = NewInfo.ManagerCreatureId;
	TmpAreaInfo.bIsValid = NewInfo.bIsValid;

	TmpGeometry.Box = NewInfo.AreaCoverage;

	OnTmpAreaChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetTmpAreaName(const FName& Name)
{
	if (TmpAreaInfo.AreaName != Name)
	{
		TmpAreaInfo.AreaName = Name;
		OnTmpAreaChanged.Broadcast();
	}
}

void UVoxelPlayerStateCacheComponent::SetTmpAreaType(FGameplayTag Type)
{
	if (TmpAreaInfo.AreaType != Type)
	{
		TmpAreaInfo.AreaType = Type;
		OnTmpAreaChanged.Broadcast();
	}
}

void UVoxelPlayerStateCacheComponent::SetTmpAreaCoverage(const FBox& Coverage)
{
	TmpGeometry.Box.SetBoundingBox(Coverage);
	OnTmpAreaChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetTmpAreaValid(const bool isValid)
{
	if (TmpAreaInfo.bIsValid == isValid) return;
	TmpAreaInfo.bIsValid = isValid;
	OnTmpAreaChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetTmpAreaStatus(EAreaStatus Status)
{
	if (TmpAreaInfo.Status != Status)
	{
		TmpAreaInfo.Status = Status;
		OnTmpAreaChanged.Broadcast();
	}
}

void UVoxelPlayerStateCacheComponent::SetTmpAreaOwner(int CreatureId)
{
	if (TmpAreaInfo.OwnerCreatureId != CreatureId)
	{
		TmpAreaInfo.OwnerCreatureId = CreatureId;
		OnTmpAreaChanged.Broadcast();
	}
}

void UVoxelPlayerStateCacheComponent::SetTmpAreaManager(int CreatureId)
{
	if (TmpAreaInfo.ManagerCreatureId != CreatureId)
	{
		TmpAreaInfo.ManagerCreatureId = CreatureId;
		OnTmpAreaChanged.Broadcast();
	}
}

void UVoxelPlayerStateCacheComponent::MoveTmpAreaCoverage(const FIntVector Offset)
{
	TmpGeometry.MoveGEO(Offset);
	OnTmpAreaChanged.Broadcast();
}

FAreaModificationData UVoxelPlayerStateCacheComponent::GetTmpAreaInfo() const
{
	FAreaModificationData Result;
	Result.AreaName = TmpAreaInfo.AreaName;
	Result.AreaType = TmpAreaInfo.AreaType;
	Result.ParentScenceId = TmpAreaInfo.ParentScenceId;
	Result.ParentAreaId = TmpAreaInfo.ParentAreaId;
	Result.Status = TmpAreaInfo.Status;
	Result.OwnerCreatureId = TmpAreaInfo.OwnerCreatureId;
	Result.ManagerCreatureId = TmpAreaInfo.ManagerCreatureId;
	Result.bIsValid = TmpAreaInfo.bIsValid;

	Result.AreaCoverage = TmpGeometry.Box;

	return Result;
}

FBox UVoxelPlayerStateCacheComponent::GetTmpAreaRange() const
{
	return TmpGeometry.Box.BoundingBox();
}

// ==================== 区域数据设置器 ====================

void UVoxelPlayerStateCacheComponent::SetCurrentArea(AVoxelAreaBaseInstance* Area)
{
	// 特殊处理：当传入 nullptr 时，执行右键回退逻辑
	if (Area == nullptr)
	{
		if (CurArea != nullptr)
		{
			InternalSetCurrentArea(nullptr);
		}
		else if (CurParentArea != nullptr)
		{
			InternalSetCurrentArea(CurParentArea);
		}
		return;
	}

	InternalSetCurrentArea(Area);
}

void UVoxelPlayerStateCacheComponent::ClearCurrentArea()
{
	// 真清空：与 SetCurrentArea(nullptr) 的回退逻辑不同，
	// 无条件清空 CurArea（CurArea 为空时无副作用），CurParentArea 保持当前层级
	if (CurArea != nullptr)
	{
		InternalSetCurrentArea(nullptr);
	}
}

void UVoxelPlayerStateCacheComponent::InternalSetCurrentArea(AVoxelAreaBaseInstance* Area)
{
	if (CurArea != Area)
	{
		AVoxelAreaBaseInstance* Old = CurArea;
		CurArea = Area;

		if (Area)
		{
			TmpAreaInfo.AreaName = Area->AreaBase.AreaName;
			TmpAreaInfo.AreaType = Area->AreaBase.Function.AreaType;
			TmpAreaInfo.ParentScenceId = Area->AreaBase.ParentScenceId;
			TmpAreaInfo.ParentAreaId = Area->AreaBase.ParentAreaId;
			TmpAreaInfo.Status = Area->AreaBase.Function.Status;
			TmpAreaInfo.OwnerCreatureId = Area->AreaBase.OwnerCreatureId;
			TmpAreaInfo.ManagerCreatureId = Area->AreaBase.ManagerCreatureId;
			TmpAreaInfo.bIsValid = true;

			TmpGeometry.Box = Area->AreaBase.AreaCoverage;

			SetCurrentParentArea(Area->ParentArea);

			SetBuildPhase(EPreviewInteractionState::EDITING);
		}
		else
		{
			// 清空 CurArea：进入"创建新区域"模式，按 ADR-001 规则初始化默认值
			// （EnterSubArea 已先设置 CurParentArea，此处可正确继承父区域 Owner）
			InitTmpAreaInfoForCreate();
			SetBuildPhase(EPreviewInteractionState::IDLE);
		}
		OnCurrentAreaChanged.Broadcast(Old, CurArea);
	}
}

void UVoxelPlayerStateCacheComponent::SetCurAreaInfo(FAreaModificationData NewInfo)
{
	if (CurArea)
	{
		CurArea->UpdateBaseAreaInfo(NewInfo, -1);
	}
}

void UVoxelPlayerStateCacheComponent::SetCurrentParentArea(AVoxelAreaBaseInstance* Area)
{
	if (CurParentArea != Area)
	{
		CurParentArea = Area;
	}
}

// ==================== 玩家权限 ====================

void UVoxelPlayerStateCacheComponent::SetGameAuthority(EVoxelGameAuthority Authority)
{
	if (CurGameAuthority != Authority)
	{
		CurGameAuthority = Authority;
		OnGameAuthorityChanged.Broadcast(CurGameAuthority);
	}
}

// ==================== 放置规则 ====================

void UVoxelPlayerStateCacheComponent::SetPlacementMode(EVoxelPlacementMode Mode)
{
	if (PlacementMode != Mode)
	{
		PlacementMode = Mode;
		OnPlacementModeChanged.Broadcast();
	}
}

void UVoxelPlayerStateCacheComponent::SetCurrentBatchShape(EBatchShapeType Shape)
{
	EBatchShapeType TargetShape = Shape;

	// 只在几何体编辑工具/笔刷工具下才允许设置形状
	const FGameplayTag ToolTag = GetCurrentToolTag();
	if (ToolTag != FCoreTags::Tool_Edit_SimpleGeometry &&
	    ToolTag != FCoreTags::Tool_Selection_SimpleGeometry &&
	    ToolTag != FCoreTags::Tool_Selection_Brush &&
	    ToolTag != FCoreTags::Tool_AreaEdit &&
	    ToolTag != FCoreTags::Tool_Edit_SingleVoxel)
	{
		TargetShape = EBatchShapeType::Null;
	}

	if (TmpGeometry.ShapeType != TargetShape)
	{
		TmpGeometry.ShapeType = TargetShape;
		OnBrushChanged.Broadcast();
	}
}

// ==================== 临时几何信息 ====================

void UVoxelPlayerStateCacheComponent::SetTmpGeometry(const FVoxelGeometryDesc& Geometry)
{
	if (TmpGeometry == Geometry) return;
	TmpGeometry = Geometry;
	OnGeometryChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetEditPoints(const FVector& First, const FVector& Second)
{
	FirstEditPoint = First;
	SecondEditPoint = Second;

	const float BlockLength = UChunkUtilsFunction::GetBlockLength(this);

	FIntVector FirstIndex = UChunkUtilsFunction::GetBlockIndexByPosition(First, BlockLength);
	FIntVector SecondIndex = UChunkUtilsFunction::GetBlockIndexByPosition(Second, BlockLength);

	FIntVector MinIndex = FIntVector(
		FMath::Min(FirstIndex.X, SecondIndex.X),
		FMath::Min(FirstIndex.Y, SecondIndex.Y),
		FMath::Min(FirstIndex.Z, SecondIndex.Z)
	);

	FIntVector MaxIndex = FIntVector(
		FMath::Max(FirstIndex.X, SecondIndex.X),
		FMath::Max(FirstIndex.Y, SecondIndex.Y),
		FMath::Max(FirstIndex.Z, SecondIndex.Z)
	);

	TmpGeometry.Box.SetBoundingBox(FBox(FVector(MinIndex), FVector(MaxIndex)));

	OnGeometryChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetTmpGeometryRotation(FRotator NewRotation)
{
	TmpGeometry.Rotation = NewRotation;
	OnGeometryChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetTmpGeometryThickness(int32 NewThickness)
{
	if (TmpGeometry.Thickness == NewThickness)
	{
		return;
	}

	TmpGeometry.Thickness = NewThickness;
	OnGeometryChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetTmpGeometryRegular(bool bEnable)
{
	TmpGeometry.bIsRegular = bEnable;
	OnGeometryChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::ClearTmpGeometryTransform()
{
	TmpGeometry.Box = FAreaCoverageFragment();
	TmpGeometry.Rotation = FRotator::ZeroRotator;

	OnGeometryChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::ResetEditSession()
{
	ClearTmpGeometryTransform();
	SetBuildPhase(EPreviewInteractionState::IDLE);
}

void UVoxelPlayerStateCacheComponent::ClearTmpEditBounds()
{
	TmpGeometry.Box.SetBoundingBox(FBox());
}

void UVoxelPlayerStateCacheComponent::ClearTmpEditRequest()
{
	TmpGeometry = FVoxelGeometryDesc();
}

// ==================== 模板素材替换 ====================

void UVoxelPlayerStateCacheComponent::SetTemplateRemapEntry(const FString& OriginFullId, const FString& NewFullId)
{
	if (OriginFullId == NewFullId)
	{
		if (TmpTemplateRemap.Remove(OriginFullId) == 0)
		{
			return;
		}
	}
	else
	{
		const FString* Existing = TmpTemplateRemap.Find(OriginFullId);
		if (Existing && *Existing == NewFullId)
		{
			return;
		}
		TmpTemplateRemap.Add(OriginFullId, NewFullId);
	}
	OnGeometryChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::ClearTemplateRemap()
{
	if (TmpTemplateRemap.Num() == 0) return;
	TmpTemplateRemap.Empty();
	OnGeometryChanged.Broadcast();
}

// ==================== 编辑请求管理 ====================

FVoxelEditRequest UVoxelPlayerStateCacheComponent::GetCurrentEditRequest() const
{
	FVoxelEditRequest Request;
	Request.ToolTag = GetCurrentToolTag();
	Request.EditCommand.PlacementMode = PlacementMode;
	Request.EditCommand.SelectionMode = SelectionMode;
	Request.HandleId = InteractableDataInHand.HandleId;
	Request.Geometry = TmpGeometry;

	return Request;
}

void UVoxelPlayerStateCacheComponent::SetCurrentEditRequest(const FVoxelEditRequest& NewRequest)
{
	if (UVoxelInteractionComponent* Interaction = GetInteractionComp())
	{
		Interaction->SetCurrentToolTag(NewRequest.ToolTag);
	}
	PlacementMode = NewRequest.EditCommand.PlacementMode;
	SetSelectionMode(NewRequest.EditCommand.SelectionMode);

	TmpGeometry = NewRequest.Geometry;

	if (NewRequest.HandleId != EBlockId::Invalid)
	{
		InteractableDataInHand.HandleId = NewRequest.HandleId;
	}
}

void UVoxelPlayerStateCacheComponent::UpdateEditRequestPosition(const FIntVector& NewOffset)
{
	const FIntVector OldMin = TmpGeometry.Box.MinIndex;
	const FIntVector OldMax = TmpGeometry.Box.MaxIndex;
	const FIntVector Delta = NewOffset - OldMin;

	TmpGeometry.Box.SetBoundingBox(OldMin + Delta, OldMax + Delta);

	OnGeometryChanged.Broadcast();
}

FVoxelEditRequest UVoxelPlayerStateCacheComponent::BuildEditRequestFromState(const FIntVector& MinIndex, const FIntVector& MaxIndex, const FRotator& Rotation) const
{
	FVoxelEditRequest Request;
	Request.ToolTag = GetCurrentToolTag();
	Request.EditCommand.PlacementMode = PlacementMode;
	Request.EditCommand.SelectionMode = SelectionMode;
	Request.HandleId = InteractableDataInHand.HandleId;
	Request.Geometry.Rotation = Rotation;
	Request.Geometry.Box.SetBoundingBox(MinIndex, MaxIndex);

	return Request;
}

// ==================== 切面高度控制 ====================

int32 UVoxelPlayerStateCacheComponent::GetMaxSliceLayer() const
{
	// 直接取 BlockSystem 的世界真实 Z 上限（块索引，开区间上界），不自己猜公式。
	if (const UWorld* World = GetWorld())
	{
		if (UVoxelBlockWorldSubsystem* BlockSys = World->GetSubsystem<UVoxelBlockWorldSubsystem>())
		{
			return FMath::TruncToInt32(BlockSys->GetWorldBorder().Max.Z);
		}
	}
	return 128; // fallback
}

int32 UVoxelPlayerStateCacheComponent::GetMinSliceLayer() const
{
	// 世界真实 Z 下限（块索引，含负层地下）。
	if (const UWorld* World = GetWorld())
	{
		if (UVoxelBlockWorldSubsystem* BlockSys = World->GetSubsystem<UVoxelBlockWorldSubsystem>())
		{
			return FMath::TruncToInt32(BlockSys->GetWorldBorder().Min.Z);
		}
	}
	return -128; // fallback
}

float UVoxelPlayerStateCacheComponent::GetEditSliceWorldHeight() const
{
	if (UVoxelGameInstance* GI = GetVoxelGameInstance())
	{
		return static_cast<float>(EditSliceLayer) * GI->GetBlockLength();
	}
	return static_cast<float>(EditSliceLayer) * 20.0f; // fallback
}

void UVoxelPlayerStateCacheComponent::SetEditSliceLayer(int32 NewLayer)
{
	const int32 MaxLayer = GetMaxSliceLayer();
	const int32 MinLayer = GetMinSliceLayer();
	NewLayer = FMath::Clamp(NewLayer, MinLayer, MaxLayer);
	if (EditSliceLayer == NewLayer)
	{
		return;
	}
	EditSliceLayer = NewLayer;
	OnSliceChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetSliceEnabled(bool bEnabled)
{
	if (bSliceEnabled == bEnabled)
	{
		return;
	}
	bSliceEnabled = bEnabled;
	OnSliceChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetSliceFilterMode(ESliceFilterMode NewMode)
{
	if (SliceFilterMode == NewMode)
	{
		return;
	}
	SliceFilterMode = NewMode;
	OnSliceChanged.Broadcast();
}

// ==================== Pawn 高度控制（体素层） ====================

float UVoxelPlayerStateCacheComponent::GetPawnWorldHeight() const
{
	if (UVoxelGameInstance* GI = GetVoxelGameInstance())
	{
		return static_cast<float>(PawnHeightLayer) * GI->GetBlockLength();
	}
	return static_cast<float>(PawnHeightLayer) * 20.0f; // fallback
}

void UVoxelPlayerStateCacheComponent::SetPawnHeightLayer(int32 NewLayer)
{
	const int32 MaxLayer = GetMaxSliceLayer();
	const int32 MinLayer = GetMinSliceLayer();
	NewLayer = FMath::Clamp(NewLayer, MinLayer, MaxLayer);
	if (PawnHeightLayer == NewLayer)
	{
		return;
	}
	PawnHeightLayer = NewLayer;

	if (const AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwner()))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			FVector Loc = Pawn->GetActorLocation();
			Loc.Z = GetPawnWorldHeight();
			Pawn->SetActorLocation(Loc);
		}
	}
	OnPawnHeightChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SyncPawnHeightLayerFromPawn()
{
	if (const AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwner()))
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			UVoxelGameInstance* GI = GetVoxelGameInstance();
			const float BlockLength = GI ? GI->GetBlockLength() : 20.0f;
			const int32 NewLayer = UChunkUtilsFunction::GetBlockIndexByPosition(Pawn->GetActorLocation(), BlockLength).Z;
			if (PawnHeightLayer != NewLayer)
			{
				PawnHeightLayer = NewLayer;
				OnPawnHeightChanged.Broadcast();
			}
		}
	}
}

// ==================== 渲染图层（多选开关） ====================

void UVoxelPlayerStateCacheComponent::SetLayerActive(FGameplayTag LayerTag, bool bActive)
{
	const bool bContains = ActiveLayers.Contains(LayerTag);
	if (bActive == bContains)
	{
		return;
	}
	if (bActive)
	{
		ActiveLayers.Add(LayerTag);
	}
	else
	{
		ActiveLayers.Remove(LayerTag);
	}
	OnLayersChanged.Broadcast();
}

// ==================== 透明区域框 ====================

int32 UVoxelPlayerStateCacheComponent::GetOwnerPlayerId() const
{
	if (const AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwner()))
	{
		return PC->PlayerId;
	}
	return INDEX_NONE;
}

void UVoxelPlayerStateCacheComponent::ClampCutoutToWorldBorder(FIntVector& MinIndex, FIntVector& MaxIndex) const
{
	UWorld* World = GetWorld();
	if (!World) return;
	UVoxelBlockWorldSubsystem* BlockSys = World->GetSubsystem<UVoxelBlockWorldSubsystem>();
	if (!BlockSys) return;
	const FBox Border = BlockSys->GetWorldBorder();
	const FIntVector Lo(FMath::TruncToInt32(Border.Min.X), FMath::TruncToInt32(Border.Min.Y), FMath::TruncToInt32(Border.Min.Z));
	const FIntVector Hi(FMath::TruncToInt32(Border.Max.X) - 1, FMath::TruncToInt32(Border.Max.Y) - 1, FMath::TruncToInt32(Border.Max.Z) - 1);
	for (int32 i = 0; i < 3; ++i)
	{
		MinIndex[i] = FMath::Clamp(MinIndex[i], Lo[i], Hi[i]);
		MaxIndex[i] = FMath::Clamp(MaxIndex[i], Lo[i], Hi[i]);
		if (MinIndex[i] > MaxIndex[i]) MinIndex[i] = MaxIndex[i];
	}
}

void UVoxelPlayerStateCacheComponent::BroadcastCutoutChanged()
{
	OnCutoutChanged.Broadcast();
}

void UVoxelPlayerStateCacheComponent::SetCutoutBoxStateInternal(const FCutoutBoxData& Data)
{
	if (!Data.IsValidBox()) return;
	// 覆盖或新增
	bool bFound = false;
	for (FCutoutBoxData& B : CutoutBoxes)
	{
		if (B.Id == Data.Id) { B = Data; bFound = true; break; }
	}
	if (!bFound) CutoutBoxes.Add(Data);
	BroadcastCutoutChanged();
}

void UVoxelPlayerStateCacheComponent::RemoveCutoutBoxInternal(int32 BoxId)
{
	CutoutBoxes.RemoveAll([BoxId](const FCutoutBoxData& B) { return B.Id == BoxId; });
	BroadcastCutoutChanged();
}

void UVoxelPlayerStateCacheComponent::RecordCutoutHistory(const FCutoutBoxData& Before, const FCutoutBoxData& After, UVoxelControllerHistoryComponent* HistoryComp)
{
	if (!HistoryComp) return;
	FVoxelEditHistoryRecord Record;
	Record.bIsCutoutOp = true;
	Record.CutoutBoxId = After.IsValidBox() ? After.Id : (Before.IsValidBox() ? Before.Id : INDEX_NONE);
	Record.CutoutBefore = Before;
	Record.CutoutAfter = After;
	Record.ToolTag = FGameplayTag::EmptyTag;
	Record.Description = FText::FromString(TEXT("透明框操作"));
	HistoryComp->SaveHistoryRecord(Record);
}

bool UVoxelPlayerStateCacheComponent::GetCutoutBox(int32 BoxId, FCutoutBoxData& OutBox) const
{
	for (const FCutoutBoxData& B : CutoutBoxes)
	{
		if (B.Id == BoxId) { OutBox = B; return true; }
	}
	return false;
}

int32 UVoxelPlayerStateCacheComponent::AddCutoutBox(FIntVector MinIndex, FIntVector MaxIndex, UVoxelControllerHistoryComponent* HistoryComp)
{
	ClampCutoutToWorldBorder(MinIndex, MaxIndex);
	const int32 NewId = NextCutoutId++;
	FCutoutBoxData NewBox(NewId, MinIndex, MaxIndex, true, FName(*FString::Printf(TEXT("框%d"), NewId)), GetOwnerPlayerId());
	UE_LOG(LogTemp, Warning, TEXT("[Cutout] AddCutoutBox: Id=%d Owner=%d Min=%s Max=%s"), NewId, NewBox.OwnerPlayerId, *MinIndex.ToString(), *MaxIndex.ToString());
	SetCutoutBoxStateInternal(NewBox);
	RecordCutoutHistory(FCutoutBoxData(), NewBox, HistoryComp);
	return NewId;
}

void UVoxelPlayerStateCacheComponent::RemoveCutoutBox(int32 BoxId, UVoxelControllerHistoryComponent* HistoryComp)
{
	FCutoutBoxData Before;
	if (!GetCutoutBox(BoxId, Before)) return;
	RemoveCutoutBoxInternal(BoxId);
	RecordCutoutHistory(Before, FCutoutBoxData(), HistoryComp);
}

void UVoxelPlayerStateCacheComponent::UpdateCutoutBox(int32 BoxId, FIntVector MinIndex, FIntVector MaxIndex, UVoxelControllerHistoryComponent* HistoryComp)
{
	FCutoutBoxData Before;
	if (!GetCutoutBox(BoxId, Before)) return;
	ClampCutoutToWorldBorder(MinIndex, MaxIndex);
	FCutoutBoxData After = Before;
	After.MinIndex = MinIndex;
	After.MaxIndex = MaxIndex;
	SetCutoutBoxStateInternal(After);
	RecordCutoutHistory(Before, After, HistoryComp);
}

void UVoxelPlayerStateCacheComponent::SetBoxEnabled(int32 BoxId, bool bEnabled)
{
	for (FCutoutBoxData& B : CutoutBoxes)
	{
		if (B.Id == BoxId && B.bEnabled != bEnabled)
		{
			B.bEnabled = bEnabled;
			BroadcastCutoutChanged();
			return;
		}
	}
}

void UVoxelPlayerStateCacheComponent::SetAllCutoutBoxesEnabled(bool bEnabled)
{
	bool bChanged = false;
	for (FCutoutBoxData& B : CutoutBoxes)
	{
		if (B.bEnabled != bEnabled)
		{
			B.bEnabled = bEnabled;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		BroadcastCutoutChanged();
	}
}

bool UVoxelPlayerStateCacheComponent::FocusCutoutBox(int32 BoxId)
{
	FCutoutBoxData Box;
	if (!GetCutoutBox(BoxId, Box))
	{
		return false;
	}
	if (const AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwner()))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			// 框中心（INDEX 空间，闭区间 Min~Max 取中点）
			const FVector CenterIndex((Box.MinIndex.X + Box.MaxIndex.X + 1) * 0.5f,
				(Box.MinIndex.Y + Box.MaxIndex.Y + 1) * 0.5f,
				(Box.MinIndex.Z + Box.MaxIndex.Z + 1) * 0.5f);
			UVoxelGameInstance* GI = GetVoxelGameInstance();
			const float BlockLength = GI ? GI->GetBlockLength() : 20.0f;

			FVector Loc = Pawn->GetActorLocation();
			Loc.X = CenterIndex.X * BlockLength;
			Loc.Y = CenterIndex.Y * BlockLength;
			Loc.Z = static_cast<float>(Box.MaxIndex.Z + 1) * BlockLength; // 框顶上一层
			Pawn->SetActorLocation(Loc);

			const int32 NewLayer = UChunkUtilsFunction::GetBlockIndexByPosition(Loc, BlockLength).Z;
			if (PawnHeightLayer != NewLayer)
			{
				PawnHeightLayer = NewLayer;
				OnPawnHeightChanged.Broadcast();
			}
		}
	}
	return true;
}

void UVoxelPlayerStateCacheComponent::ClearAllCutouts()
{
	CutoutBoxes.Empty();
	BroadcastCutoutChanged();
}

void UVoxelPlayerStateCacheComponent::SetCutoutEditMode(bool bActive)
{
	if (bCutoutEditMode == bActive) return;
	bCutoutEditMode = bActive;
	BroadcastCutoutChanged();
}

TArray<FCutoutBoxData> UVoxelPlayerStateCacheComponent::GetCutoutBoxesForSave() const
{
	TArray<FCutoutBoxData> Result = CutoutBoxes;
	const int32 Pid = GetOwnerPlayerId();
	for (FCutoutBoxData& B : Result)
	{
		B.OwnerPlayerId = Pid;  // 确保归属戳印
	}
	return Result;
}

void UVoxelPlayerStateCacheComponent::LoadCutoutBoxes(const TArray<FCutoutBoxData>& InBoxes)
{
	const int32 Pid = GetOwnerPlayerId();
	CutoutBoxes.Empty();
	int32 MaxId = 0;
	for (const FCutoutBoxData& B : InBoxes)
	{
		if (B.IsValidBox() && B.OwnerPlayerId == Pid)  // 仅本玩家的框
		{
			CutoutBoxes.Add(B);
			MaxId = FMath::Max(MaxId, B.Id);
		}
	}
	NextCutoutId = MaxId + 1;
	BroadcastCutoutChanged();
	UE_LOG(LogTemp, Warning, TEXT("[Cutout] LoadCutoutBoxes: PlayerId=%d loaded=%d"), Pid, CutoutBoxes.Num());
}

void UVoxelPlayerStateCacheComponent::ExecuteCutoutUndo(const FVoxelEditHistoryRecord& Record)
{
	if (Record.CutoutBefore.IsValidBox())
	{
		SetCutoutBoxStateInternal(Record.CutoutBefore);
	}
	else
	{
		RemoveCutoutBoxInternal(Record.CutoutBoxId);
	}
}

void UVoxelPlayerStateCacheComponent::ExecuteCutoutRedo(const FVoxelEditHistoryRecord& Record)
{
	if (Record.CutoutAfter.IsValidBox())
	{
		SetCutoutBoxStateInternal(Record.CutoutAfter);
	}
	else
	{
		RemoveCutoutBoxInternal(Record.CutoutBoxId);
	}
}

// ==================== 笔刷参数 ====================

void UVoxelPlayerStateCacheComponent::SetBrushSize(int32 NewSize)
{
	NewSize = FMath::Max(1, NewSize);
	if (BrushSize != NewSize)
	{
		BrushSize = NewSize;
		OnBrushChanged.Broadcast();
	}
}

void UVoxelPlayerStateCacheComponent::SetBrushDimension(EBrushDimension NewDimension)
{
	if (BrushDimension != NewDimension)
	{
		BrushDimension = NewDimension;
		OnBrushChanged.Broadcast();
	}
}

// ==================== 选中家具 ====================

void UVoxelPlayerStateCacheComponent::SetSelectedFurniture(AUVoxelItemBaseInstance* Furniture)
{
	if (SelectedFurnitureActor.Get() == Furniture) return;
	SelectedFurnitureActor = Furniture;
	OnSelectedFurnitureChanged.Broadcast();
}

// ==================== 便捷绑定 ====================

void UVoxelPlayerStateCacheComponent::BindAllBuildEvents(UObject* Obj, FName FuncName)
{
	if (!Obj || FuncName.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolChain] BindAllBuildEvents: invalid params obj=%s func=%s"), *GetNameSafe(Obj), *FuncName.ToString());
		return;
	}
	FScriptDelegate Delegate;
	Delegate.BindUFunction(Obj, FuncName);
	// 数据事件（本组件持有）
	OnGeometryChanged.Add(Delegate);
	OnBrushChanged.Add(Delegate);
	OnPlacementModeChanged.Add(Delegate);
	OnTmpAreaChanged.Add(Delegate);
	OnSelectedFurnitureChanged.Add(Delegate);
	OnItemInHandChanged.Add(Delegate);
	// 工具/流程事件（InteractionComp 持有，经同 Owner 查找转发绑定）
	if (UVoxelInteractionComponent* Interaction = GetInteractionComp())
	{
		Interaction->OnToolChanged.Add(Delegate);
		Interaction->OnBuildPhaseChanged.Add(Delegate);
	}
	UE_LOG(LogTemp, Log, TEXT("[ToolChain] BindAllBuildEvents: bound 8 events for obj=%s"), *Obj->GetName());
}

void UVoxelPlayerStateCacheComponent::UnbindAllBuildEvents(UObject* Obj, FName FuncName)
{
	if (!Obj || FuncName.IsNone()) return;
	OnGeometryChanged.RemoveAll(Obj);
	OnBrushChanged.RemoveAll(Obj);
	OnPlacementModeChanged.RemoveAll(Obj);
	OnTmpAreaChanged.RemoveAll(Obj);
	OnSelectedFurnitureChanged.RemoveAll(Obj);
	OnItemInHandChanged.RemoveAll(Obj);
	if (UVoxelInteractionComponent* Interaction = GetInteractionComp())
	{
		Interaction->OnToolChanged.RemoveAll(Obj);
		Interaction->OnBuildPhaseChanged.RemoveAll(Obj);
	}
}

// ==================== 私有辅助方法 ====================

void UVoxelPlayerStateCacheComponent::HandleSelectionChanged()
{
	// 同步选区包围盒到 TmpGeometry.Box（供 M5 等 UI 模块读取展示）
	if (HasSelection())
	{
		const TArray<FIntVector>& Selection = GetCurrentSelection();
		FIntVector MinV = Selection[0];
		FIntVector MaxV = Selection[0];
		for (const FIntVector& Pos : Selection)
		{
			MinV = FIntVector(FMath::Min(MinV.X, Pos.X), FMath::Min(MinV.Y, Pos.Y), FMath::Min(MinV.Z, Pos.Z));
			MaxV = FIntVector(FMath::Max(MaxV.X, Pos.X), FMath::Max(MaxV.Y, Pos.Y), FMath::Max(MaxV.Z, Pos.Z));
		}
		TmpGeometry.Box.SetBoundingBox(MinV, MaxV);
	}
	else
	{
		TmpGeometry.Box = FAreaCoverageFragment();
	}
}

void UVoxelPlayerStateCacheComponent::ResetTmpAreaInfoInternal(int32 SceneId)
{
	TmpAreaInfo.AreaName = NAME_None;
	TmpAreaInfo.AreaType = FGameplayTag::EmptyTag;
	TmpAreaInfo.ParentScenceId = SceneId;
	TmpAreaInfo.ParentAreaId = -1;
	TmpAreaInfo.Status = EAreaStatus::Active;
	TmpAreaInfo.bIsValid = false;
	TmpAreaInfo.OwnerCreatureId = -1;
	TmpAreaInfo.ManagerCreatureId = -1;

	TmpGeometry.Box = FAreaCoverageFragment();

	OnTmpAreaChanged.Broadcast();
}

bool UVoxelPlayerStateCacheComponent::IsValidArea(AVoxelAreaBaseInstance* Area) const
{
	return Area != nullptr && Area->IsValidLowLevel();
}

UVoxelGameInstance* UVoxelPlayerStateCacheComponent::GetVoxelGameInstance() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetGameInstance<UVoxelGameInstance>();
	}
	return nullptr;
}
