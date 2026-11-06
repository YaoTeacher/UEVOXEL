// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/VoxelGameInstance.h"
#include "Core/CoreTags.h"
#include "Core/UI/Interaction/Voxel3DUIActor.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "Voxel/Editor/Tools/VoxelSelectionTool.h"
#include "Voxel/Editor/VoxelPreviewActor.h"
#include "Voxel/Editor/VoxelSelectionPreviewActor.h"
#include "Voxel/Editor/VoxelPivotCursorActor.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Voxel/Editor/UVoxelTemplateUtils.h"
#include "Data/VoxelEditCommand.h"
#include "Data/Item/VoxelItemBase.h"
#include "Item/VoxelItemBaseInstance.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "Cutout/VoxelCutoutBoxActor.h"
#include "Data/VoxelCutoutData.h"
#include "Voxel/VoxelPlayerController.h"
#include "Utils/ChunkBase.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/VoxelControllerPawn.h"
#include "Voxel/VoxelCharacter.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"
#include "Voxel/Utils/ChunkMeshData.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/MeshComponent.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"

// ==================== 构造函数 ====================

// 旋转 Gizmo 轴向基础旋转表（InitRotateGizmos 与 UpdateRotateGizmosPosition 共用）
static const FRotator GRotateGizmoBaseRotations[3] = {
	FRotator(0, 0, 0),    // X
	FRotator(0, 90, 90),  // Y
	FRotator(90, 0, 0)    // Z
};

// 面 Gizmo 基础旋转表（InitFaceGizmos 与 UpdateFaceGizmosPosition 共用）
// 索引对应：0:-X, 1:+X, 2:-Y, 3:+Y, 4:-Z, 5:+Z
static const FRotator GFaceGizmoBaseRotations[6] = {
	FRotator(-90, 0, 0),  // -X（Mesh 朝 -X 向外）
	FRotator(90, 0, 0),   // +X（Mesh 朝 +X 向外）
	FRotator(0, 0, 90),   // -Y
	FRotator(0, 0, -90),  // +Y
	FRotator(0, 0, 180),  // -Z
	FRotator::ZeroRotator // +Z
};

UVoxelPreviewComponent::UVoxelPreviewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// ==================== 初始化 ====================

void UVoxelPreviewComponent::BeginPlay()
{
	Super::BeginPlay();

	// 获取 StateComponent 引用
	if (AActor* Owner = GetOwner())
	{
		CachedStateComponent = Owner->FindComponentByClass<UVoxelPlayerStateCacheComponent>();

		if (!CachedStateComponent)
		{
			UE_LOG(LogTemp, Error, TEXT("VisualUIComponent: StateComponent not found on %s!"), *Owner->GetName());
		}
	}

	// 绑定事件委托
	if (CachedStateComponent)
	{
		BindDelegates(CachedStateComponent);
	}

	// 初始化所有视觉元素
	InitializeVisualElements();
}

void UVoxelPreviewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 统一销毁所持场景 Actor，防止 PC 先于 World 销毁时滞留场景
	if (PreviewActor && IsValid(PreviewActor))
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
	if (SelectionPreviewActor && IsValid(SelectionPreviewActor))
	{
		SelectionPreviewActor->Destroy();
		SelectionPreviewActor = nullptr;
	}
	for (AVoxel3DUIActor* Gizmo : TranslateGizmos)
	{
		if (Gizmo && IsValid(Gizmo)) Gizmo->Destroy();
	}
	TranslateGizmos.Empty();
	for (AVoxel3DUIActor* Gizmo : RotateGizmos)
	{
		if (Gizmo && IsValid(Gizmo)) Gizmo->Destroy();
	}
	RotateGizmos.Empty();
	for (AVoxel3DUIActor* Gizmo : FaceGizmos)
	{
		if (Gizmo && IsValid(Gizmo)) Gizmo->Destroy();
	}
	FaceGizmos.Empty();

	DestroyCursorActor();
	DestroyGroundMarkerActor();

	Super::EndPlay(EndPlayReason);
}

void UVoxelPreviewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 每帧更新地面标记位置（鼠标射线命中点）
	UpdateGroundMarker();

	// 切面区域动态更新（玩家移动时重新计算区域集合）
	CheckSliceAreaUpdate(DeltaTime);
}

void UVoxelPreviewComponent::InitializeVisualElements()
{
	InitPreviewActor();
	InitSelectionPreviewActor();
	InitTranslateGizmos();
	InitRotateGizmos();
	InitFaceGizmos();
}

void UVoxelPreviewComponent::BindDelegates(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC) return;

	// 按域细粒度绑定（只重建受影响的渲染）
	SC->OnGeometryChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleGeometryChanged);
	SC->OnBrushChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleBrushChanged);
	SC->OnSelectedFurnitureChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleSelectedFurnitureChanged);
	SC->OnItemInHandChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleItemInHandChanged);
	SC->OnTmpAreaChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleGeometryChanged);
	SC->OnPlacementModeChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleGeometryChanged);

	// 交互流程事件（持有者：UVoxelInteractionComponent）
	if (UVoxelInteractionComponent* IC = GetOwner() ? GetOwner()->FindComponentByClass<UVoxelInteractionComponent>() : nullptr)
	{
		IC->OnToolChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleToolChanged);
		IC->OnBuildPhaseChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleBuildPhaseChanged);
		IC->OnControllerModeChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleBuildModeChanged);
	}

	// 选区变更（StateCache 本体持有选区）
	SC->OnSelectionChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleSelectionChanged);
	SC->OnPreDragChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleSelectionChanged);

	// 切面高度变化（独立广播，只触发材质更新，不触发工具逻辑）
	SC->OnSliceChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleSliceHeightChanged);

	// 透明框变化（推 BlockSys + 同步边框 Actor）
	SC->OnCutoutChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleCutoutChanged);

	// 区域切换事件
	SC->OnCurrentAreaChanged.AddDynamic(this, &UVoxelPreviewComponent::HandleCurrentAreaChanged);

}

// ==================== 初始化视觉元素 ====================

void UVoxelPreviewComponent::InitPreviewActor()
{
	if (!PreviewActorClass)
	{
		PreviewActorClass = AVoxelPreviewActor::StaticClass();
	}

	FTransform Transform = FTransform::Identity;

	PreviewActor = GetWorld()->SpawnActorDeferred<AVoxelPreviewActor>(
		PreviewActorClass,
		Transform,
		GetOwner()
	);

	if (PreviewActor)
	{
		#if WITH_EDITOR
		PreviewActor->SetActorLabel(FString::Printf(TEXT("PreviewActor_%s"), *GetOwner()->GetName()));
		#endif
		UGameplayStatics::FinishSpawningActor(PreviewActor, Transform);
		PreviewActor->SetPreviewVisible(false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UVoxelPreviewComponent::InitPreviewActor: Failed to spawn PreviewActor"));
	}
}

void UVoxelPreviewComponent::InitSelectionPreviewActor()
{
	FTransform Transform = FTransform::Identity;

	SelectionPreviewActor = GetWorld()->SpawnActorDeferred<AVoxelSelectionPreviewActor>(
		AVoxelSelectionPreviewActor::StaticClass(),
		Transform,
		GetOwner()
	);

	if (SelectionPreviewActor)
	{
		#if WITH_EDITOR
		SelectionPreviewActor->SetActorLabel(FString::Printf(TEXT("SelectionPreviewActor_%s"), *GetOwner()->GetName()));
		#endif
		UGameplayStatics::FinishSpawningActor(SelectionPreviewActor, Transform);
		SelectionPreviewActor->SetActorHiddenInGame(true);

		// 共享 PreviewActor 的材质（保持视觉一致）
		// - WireframeMaterial：选区外框（SelectionFrameMesh）
		// - VoxelGridMaterial：选区体素高亮（SelectionMesh），原 PreviewActor 选区模式使用此材质
		if (PreviewActor)
		{
			if (!SelectionPreviewActor->WireframeMaterial && PreviewActor->WireframeMaterial)
			{
				SelectionPreviewActor->WireframeMaterial = PreviewActor->WireframeMaterial;
			}
			if (!SelectionPreviewActor->SelectionMeshMaterial && PreviewActor->VoxelGridMaterial)
			{
				SelectionPreviewActor->SelectionMeshMaterial = PreviewActor->VoxelGridMaterial;
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UVoxelPreviewComponent::InitSelectionPreviewActor: Failed to spawn SelectionPreviewActor"));
	}
}

void UVoxelPreviewComponent::InitTranslateGizmos()
{
	TranslateGizmos.SetNum(3);

	const TCHAR* AxisNames[] = { TEXT("Translate_X"), TEXT("Translate_Y"), TEXT("Translate_Z") };
	const EActorDirection Dirs[] = { EActorDirection::Forward, EActorDirection::Right, EActorDirection::Up };

	for (int32 i = 0; i < 3; ++i)
	{
		TranslateGizmos[i] = CreateGizmoActor(AxisNames[i], TranslateGizmoMesh);
		if (TranslateGizmos[i])
		{
			TranslateGizmos[i]->SetDir(Dirs[i]);
		}
	}
}

void UVoxelPreviewComponent::InitRotateGizmos()
{
	RotateGizmos.SetNum(3);

	const TCHAR* AxisNames[] = { TEXT("Rotate_X"), TEXT("Rotate_Y"), TEXT("Rotate_Z") };

	for (int32 i = 0; i < 3; ++i)
	{
		RotateGizmos[i] = CreateGizmoActor(AxisNames[i], RotateGizmoMesh, GRotateGizmoBaseRotations[i]);
	}
}

void UVoxelPreviewComponent::InitFaceGizmos()
{
	FaceGizmos.SetNum(6);

	// 定义面名称（旋转表见 GFaceGizmoBaseRotations）
	const TCHAR* FaceNames[6] = {
		TEXT("Face_MinX"), TEXT("Face_MaxX"), TEXT("Face_MinY"),
		TEXT("Face_MaxY"), TEXT("Face_MinZ"), TEXT("Face_MaxZ")
	};

	for (int32 i = 0; i < 6; ++i)
	{
		FaceGizmos[i] = CreateGizmoActor(FaceNames[i], FaceGizmoMesh, GFaceGizmoBaseRotations[i]);
	}
}

// ==================== 辅助工厂与批量操作方法 ====================

AVoxel3DUIActor* UVoxelPreviewComponent::CreateGizmoActor(FString NameSuffix, UStaticMesh* Mesh, FRotator InitialRot)
{
	if (!TranslateGizmoClass)
	{
		TranslateGizmoClass = AVoxel3DUIActor::StaticClass();
	}

	AVoxelPlayerController* Controller = Cast<AVoxelPlayerController>(GetOwner());
	if (!Controller) return nullptr;

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	FTransform Transform(InitialRot, FVector::ZeroVector, FVector(GizmoHandleSize/100));

	AVoxel3DUIActor* Gizmo = World->SpawnActorDeferred<AVoxel3DUIActor>(
		TranslateGizmoClass,
		Transform,
		GetOwner()
	);

	if (Gizmo)
	{
		#if WITH_EDITOR
		Gizmo->SetActorLabel(FString::Printf(TEXT("%s_%s"), *NameSuffix, *GetOwner()->GetName()));
		#endif
		Gizmo->SetOwningController(Controller);

		UGameplayStatics::FinishSpawningActor(Gizmo, Transform);

		if (Mesh)
		{
			Gizmo->UIMesh->SetStaticMesh(Mesh);
		}

		// 默认隐藏且无碰撞
		Gizmo->SetActorHiddenInGame(true);
		Gizmo->SetActorEnableCollision(false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn Gizmo: %s"), *NameSuffix);
	}

	return Gizmo;
}

void UVoxelPreviewComponent::SetGizmoGroupVisibility(TArray<TObjectPtr<AVoxel3DUIActor>>& Group, bool bVisible)
{
	for (auto& Gizmo : Group)
	{
		if (Gizmo)
		{
			Gizmo->SetActorHiddenInGame(!bVisible);
			Gizmo->SetActorEnableCollision(bVisible);
		}
	}
}

void UVoxelPreviewComponent::UpdateGizmoGroupLocation(TArray<TObjectPtr<AVoxel3DUIActor>>& Group, FVector Location)
{
	for (auto& Gizmo : Group)
	{
		if (Gizmo)
		{
			Gizmo->SetActorLocation(Location);
		}
	}
}

// ==================== 预览更新接口 ====================

void UVoxelPreviewComponent::UpdateSingleVoxelPreview(
	FIntVector PlaceIndex,
	FIntVector DeleteIndex,
	int32 BlockType,
	const FVoxelEditCommand& EditCommand)
{
	if (!PreviewActor) return;

	FIntVector PreviewIndex;
	FLinearColor PreviewColor;

	// 根据放置规则和方块类型判断操作类型
	const bool bIsDelete = BlockType == EBlockId::Air;

	if (bIsDelete)
	{
		// 删除操作：红色
		PreviewIndex = DeleteIndex;
		PreviewColor = FLinearColor::Red;
	}
	else if (EditCommand.PlacementMode == EVoxelPlacementMode::Replace)
	{
		// 替换操作：黄色
		PreviewIndex = PlaceIndex;
		PreviewColor = FLinearColor::Yellow;
	}
	else
	{
		// 普通建造：绿色
		PreviewIndex = PlaceIndex;
		PreviewColor = FLinearColor::Green;
	}

	const float BlockLength = UChunkUtilsFunction::GetBlockLength(this);
	PreviewActor->UpdateSingleVoxelPreview(PreviewIndex, BlockType, PreviewColor, BlockLength);
}

void UVoxelPreviewComponent::UpdateBatchEditPreview(const FVoxelEditRequest& Request)
{
	if (!PreviewActor) return;

	const float BlockLength = UChunkUtilsFunction::GetBlockLength(this);

	// 更新 PreviewActor
	PreviewActor->UpdateFromEditRequest(Request);

	const FBox WorldBounds = Request.Geometry.GetBound(BlockLength);
	UpdateTranslateGizmosPosition(WorldBounds.GetCenter());
	SetTranslateGizmosVisible(true);

	UE_LOG(LogTemp, VeryVerbose, TEXT("UpdateBatchEditPreview: Bounds=%s, Shape=%d"),
		*WorldBounds.ToString(), (int32)Request.Geometry.ShapeType);
}

void UVoxelPreviewComponent::SetPreviewState(EPreviewInteractionState State)
{
	if (!PreviewActor) return;
	EPreviewInteractionState BuildPhase = CachedStateComponent ? CachedStateComponent->GetBuildPhase() : EPreviewInteractionState::IDLE;
	UE_LOG(LogTemp, Log, TEXT("[DEBUG] VisualUI SetPreviewState: BuildPhase=%d, ArgState=%d"), (int32)BuildPhase, (int32)State);
	// 工具驱动：委托给当前工具配置 Gizmo
	if (UVoxelToolBase* Tool = GetCurrentTool())
	{
		// 非选区工具：转发 PreviewActor 状态
		Tool->ConfigureGizmos(this, BuildPhase);
	}
}

void UVoxelPreviewComponent::ClearPreview()
{
	if (PreviewActor)
	{
		PreviewActor->ClearPreview();
	}
	// 清理所有 Gizmo（Translate/Rotate/Face）
	// 避免工具切换时残留旧工具的视觉元素
	SetTranslateGizmosVisible(false);
	SetGizmoGroupVisibility(RotateGizmos, false);
	SetGizmoGroupVisibility(FaceGizmos, false);
}

// ==================== 工具驱动预览 API ====================

UVoxelToolBase* UVoxelPreviewComponent::GetCurrentTool() const
{
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwner()))
	{
		return PC->InteractionComp ? PC->InteractionComp->GetCurrentTool() : nullptr;
	}
	return nullptr;
}

float UVoxelPreviewComponent::GetBlockLength() const
{
	return UChunkUtilsFunction::GetBlockLength(this);
}

void UVoxelPreviewComponent::ResetAllPreview()
{
	ClearPreview();
	ClearSelectionHighlight();
	HideAllGizmos();
	if (CursorActor) CursorActor->SetActorHiddenInGame(true);
}

void UVoxelPreviewComponent::ClearSelectionHighlight()
{
	if (SelectionPreviewActor) SelectionPreviewActor->ClearSelectionPreview();
}

void UVoxelPreviewComponent::ShowCurrentSelection()
{
	if (!CachedStateComponent || !SelectionPreviewActor) return;
	UVoxelPlayerStateCacheComponent* SelComp = CachedStateComponent;
	if (!SelComp) { ClearSelectionHighlight(); return; }
	const TArray<FIntVector>& CurrentSelection = SelComp->GetCurrentSelection();
	if (CurrentSelection.Num() > 0)
		SelectionPreviewActor->UpdateSelectionPreview(CurrentSelection, GetBlockLength());
	else
		SelectionPreviewActor->ClearSelectionPreview();
}

void UVoxelPreviewComponent::ShowSelectionWithCalculated()
{
	if (!CachedStateComponent || !SelectionPreviewActor) return;
	UVoxelPlayerStateCacheComponent* SelComp = CachedStateComponent;
	if (!SelComp) { ClearSelectionHighlight(); return; }
	const TArray<FIntVector>& CurrentSelection = SelComp->GetCurrentSelection();
	const float BL = GetBlockLength();
	FVoxelEditRequest Request = CachedStateComponent->GetCurrentEditRequest();
	TArray<FIntVector> EmptySelection;
	TArray<FIntVector> TempShape = UVoxelSelectionTool::CalculateSelection(Request, BL, EmptySelection, ESelectionOperationType::New);
	TSet<FIntVector> CombinedSet(CurrentSelection);
	CombinedSet.Append(TempShape);
	TArray<FIntVector> RenderSelection = CombinedSet.Array();
	if (RenderSelection.Num() > 0)
		SelectionPreviewActor->UpdateSelectionPreview(RenderSelection, BL);
	else
		SelectionPreviewActor->ClearSelectionPreview();
}

void UVoxelPreviewComponent::SetPreviewActorState(EPreviewInteractionState Phase)
{
	if (PreviewActor) PreviewActor->SetPreviewState(Phase);
}

void UVoxelPreviewComponent::ConfigureGizmoLayout(EPreviewInteractionState Phase, bool bTranslate, bool bRotate, bool bFace)
{
	if (!CachedStateComponent) return;
	const float BlockLength = GetBlockLength();

	switch (Phase)
	{
	case EPreviewInteractionState::IDLE:
		HideAllGizmos();
		if (CursorActor) CursorActor->SetActorHiddenInGame(true);
		// 有选区且需要 Translate 时，IDLE 下显示位移 Gizmo（供平移选区）
		if (bTranslate)
		{
			UVoxelPlayerStateCacheComponent* SelComp = CachedStateComponent;
			if (SelComp && SelComp->HasSelection())
			{
				FIntVector CenterIndex = SelComp->GetSelectionCenter();
				FVector Center = FVector(CenterIndex) * BlockLength + FVector(BlockLength * 0.5f);
				ShowTranslateGizmo(Center);
			}
		}
		break;

	case EPreviewInteractionState::PLACING:
		HideAllGizmos();
		if (CursorActor) CursorActor->SetActorHiddenInGame(true);
		break;

	case EPreviewInteractionState::EDITING:
		{
			const FVoxelGeometryDesc& Geo = CachedStateComponent->GetTmpGeometry();
			FVector Center = Geo.GetPivotWorld(BlockLength);
			FBox Bounds = Geo.GetBound(BlockLength);
			FRotator Rot = Geo.Rotation;

			if (bTranslate) ShowTranslateGizmo(Center); else HideTranslateGizmo();
			if (bRotate)    ShowRotateGizmo(Center);   else HideRotateGizmo();
			if (bFace)      { UpdateFaceGizmosPosition(Bounds, Rot, Center); SetGizmoGroupVisibility(FaceGizmos, true); }
			else            { SetGizmoGroupVisibility(FaceGizmos, false); }

			// Cursor
			if (Geo.PivotMode == EVoxelPivotMode::Cursor)
			{
				EnsureCursorActor();
				UpdateCursorActor(Geo.GetPivotWorld(BlockLength), true);
			}
			else if (CursorActor) CursorActor->SetActorHiddenInGame(true);
		}
		break;

	default:
		HideAllGizmos();
		break;
	}
}

void UVoxelPreviewComponent::UpdateFurniturePreview()
{
	if (!CachedStateComponent) return;
	const FInteractableData& HandData = CachedStateComponent->GetInteractableDataInHand();
	if (HandData.HandleId == EBlockId::Invalid) { if (PreviewActor) PreviewActor->ClearPreview(); return; }

	const FVoxelGeometryDesc& Geometry = CachedStateComponent->GetTmpGeometry();
	const float BlockLength = GetBlockLength();
	if (!PreviewActor) return;

	UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>();
	UConfigDataSubsystem* CDS = GI ? GI->GetSubsystem<UConfigDataSubsystem>() : nullptr;
	if (!CDS) return;

	const FVoxelNormalItemBase* FurnData = CDS->GetItemTypeById<FVoxelNormalItemBase>(HandData.HandleId);
	if (!FurnData) { PreviewActor->ClearPreview(); return; }

	const EActorDirection Direction = UChunkUtilsFunction::RotationToDirection(Geometry.Rotation);
	const FVector RotatedVolume = UChunkUtilsFunction::GetRotationVolume(Direction, FVector(FurnData->GetVoxelVolume(BlockLength)));
	const FVector Min = FVector(Geometry.Box.MinIndex);
	const FVector Max = Min + RotatedVolume;
	const FBox Bounds = UChunkUtilsFunction::UpdateBox(FBox(Min, Min), Max);

	PreviewActor->UpdateFurniturePreview(
		Geometry.Box.MinIndex, Direction, FurnData->AssetFragment.Mesh,
		Bounds, BlockLength, FurnData->AssetFragment.TextureSet);
}

void UVoxelPreviewComponent::UpdateTemplatePreview()
{
	if (!CachedStateComponent) return;
	const FInteractableData& HandData = CachedStateComponent->GetInteractableDataInHand();
	if (HandData.HandleId == EBlockId::Invalid) { if (PreviewActor) PreviewActor->ClearPreview(); return; }

	const FVoxelGeometryDesc& Geometry = CachedStateComponent->GetTmpGeometry();
	const float BlockLength = GetBlockLength();
	if (!PreviewActor) return;

	UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>();
	UConfigDataSubsystem* CDS = GI ? GI->GetSubsystem<UConfigDataSubsystem>() : nullptr;
	UVoxelBlockWorldSubsystem* BlockSys = GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>();
	if (!CDS || !BlockSys) return;

	const FVoxelTemplate* Tmpl = CDS->GetTemplateById(HandData.HandleId);
	if (!Tmpl) { PreviewActor->ClearPreview(); return; }

	FChunkGenContext Context;
	BlockSys->BuildTemplateContext(*Tmpl, Context, CachedStateComponent->GetTemplateRemap());
	FChunkMeshData MeshData = BlockSys->GenerateMeshData(Context).Opaque;

	PreviewActor->UpdateTemplatePreview(*Tmpl, Geometry, MeshData, HandData.HandleId, BlockLength);
	PreviewActor->UpdateTemplateFurniturePreview(*Tmpl, Geometry, CachedStateComponent->GetTemplateRemap(), CDS, BlockLength);
}

// ==================== UI 元素控制 ====================

void UVoxelPreviewComponent::SetTranslateGizmosVisible(bool bVisible)
{
	SetGizmoGroupVisibility(TranslateGizmos, bVisible);
}

void UVoxelPreviewComponent::ResetBuildVisualItem()
{
	ClearPreview();
	SetTranslateGizmosVisible(false);
}

void UVoxelPreviewComponent::CancelEdit()
{
	if (!PreviewActor) return;

	// 【重构后】不再直接设置状态，只控制视觉
	PreviewActor->SetPreviewState(EPreviewInteractionState::IDLE);

	HideAllGizmos();
}

// ==================== Gizmo 位置更新方法 ====================

void UVoxelPreviewComponent::UpdateTranslateGizmosPosition(FVector Center)
{
	UpdateGizmoGroupLocation(TranslateGizmos, Center);
}

void UVoxelPreviewComponent::UpdateRotateGizmosPosition(FVector Center, FRotator GeometryRotation)
{
	// 更新位置
	UpdateGizmoGroupLocation(RotateGizmos, Center);

	// 应用几何旋转到 RotateGizmo
	// 每个 RotateGizmo 需要叠加几何旋转，使其跟随盒体旋转
	for (int32 i = 0; i < RotateGizmos.Num(); ++i)
	{
		if (RotateGizmos[i] && i < 3)
		{
			// 基础旋转（初始化时设定的轴向，与 InitRotateGizmos 共用表）
			const FRotator BaseRotation = GRotateGizmoBaseRotations[i];

			// 叠加几何旋转（四元数合成）
			FRotator FinalRotation = (FQuat(GeometryRotation) * FQuat(BaseRotation)).Rotator();
			RotateGizmos[i]->SetActorRotation(FinalRotation);
		}
	}
}

void UVoxelPreviewComponent::UpdateFaceGizmosPosition(const FBox& Bounds, FRotator GeometryRotation, FVector GeometryCenter)
{
	if (FaceGizmos.Num() != 6) return;

	FVector Min = Bounds.Min;
	FVector Max = Bounds.Max;
	FVector Center = Bounds.GetCenter();

	// 定义面配置（基础旋转）
	struct FFaceConfig
	{
		FVector Position;
		FRotator BaseRotation;
	};

	// 计算每个面的基础位置和旋转（旋转表与 InitFaceGizmos 共用）
	FFaceConfig FaceConfigs[6];
	FaceConfigs[0] = {FVector(Min.X, Center.Y, Center.Z), GFaceGizmoBaseRotations[0]};
	FaceConfigs[1] = {FVector(Max.X, Center.Y, Center.Z), GFaceGizmoBaseRotations[1]};
	FaceConfigs[2] = {FVector(Center.X, Min.Y, Center.Z), GFaceGizmoBaseRotations[2]};
	FaceConfigs[3] = {FVector(Center.X, Max.Y, Center.Z), GFaceGizmoBaseRotations[3]};
	FaceConfigs[4] = {FVector(Center.X, Center.Y, Min.Z), GFaceGizmoBaseRotations[4]};
	FaceConfigs[5] = {FVector(Center.X, Center.Y, Max.Z), GFaceGizmoBaseRotations[5]};

	// 更新每个 FaceGizmo 的位置和旋转
	for (int32 i = 0; i < 6; ++i)
	{
		if (FaceGizmos[i])
		{
			// 设置位置（需要考虑旋转）
			// 将面位置从包围盒空间转换到世界空间（应用几何旋转）
			FVector LocalOffset = FaceConfigs[i].Position - GeometryCenter;
			FVector WorldOffset = GeometryRotation.RotateVector(LocalOffset);
			FVector WorldPosition = GeometryCenter + WorldOffset;

			FaceGizmos[i]->SetActorLocation(WorldPosition);

			// 应用旋转（基础旋转 ⊕ 几何旋转，四元数合成）
			FRotator FinalRotation = (FQuat(GeometryRotation) * FQuat(FaceConfigs[i].BaseRotation)).Rotator();
			FaceGizmos[i]->SetActorRotation(FinalRotation);
		}
	}
}

void UVoxelPreviewComponent::UpdateAllGizmosPosition(FVector Center, const FBox& Bounds, FRotator GeometryRotation)
{
	UpdateTranslateGizmosPosition(Center);
	UpdateRotateGizmosPosition(Center, GeometryRotation);
	UpdateFaceGizmosPosition(Bounds, GeometryRotation, Center);
}

// ==================== Gizmo 控制接口 ====================

void UVoxelPreviewComponent::ShowTranslateGizmo(const FVector& Center)
{
	UpdateTranslateGizmosPosition(Center);
	SetTranslateGizmosVisible(true);
	ActiveGizmoType = EInteractiveGizmoType::Translate;
}

void UVoxelPreviewComponent::HideTranslateGizmo()
{
	SetTranslateGizmosVisible(false);
	if (ActiveGizmoType == EInteractiveGizmoType::Translate)
	{
		ActiveGizmoType = EInteractiveGizmoType::None;
	}
}

void UVoxelPreviewComponent::ShowRotateGizmo(const FVector& Center)
{
	UVoxelPlayerStateCacheComponent* BuildStateComp = GetStateComponent();
	if (!BuildStateComp) return;

	FRotator GeometryRotation = BuildStateComp->GetTmpGeometry().Rotation;
	UpdateRotateGizmosPosition(Center, GeometryRotation);
	SetGizmoGroupVisibility(RotateGizmos, true);
	ActiveGizmoType = EInteractiveGizmoType::Rotate;
}

void UVoxelPreviewComponent::HideRotateGizmo()
{
	SetGizmoGroupVisibility(RotateGizmos, false);
	if (ActiveGizmoType == EInteractiveGizmoType::Rotate)
	{
		ActiveGizmoType = EInteractiveGizmoType::None;
	}
}

void UVoxelPreviewComponent::ShowScaleGizmo(const FVector& Center)
{
	UpdateTranslateGizmosPosition(Center);
	SetTranslateGizmosVisible(true);
	ActiveGizmoType = EInteractiveGizmoType::Scale;
}

void UVoxelPreviewComponent::HideScaleGizmo()
{
	SetTranslateGizmosVisible(false);
	if (ActiveGizmoType == EInteractiveGizmoType::Scale)
	{
		ActiveGizmoType = EInteractiveGizmoType::None;
	}
}

void UVoxelPreviewComponent::HideAllGizmos()
{
	SetGizmoGroupVisibility(TranslateGizmos, false);
	SetGizmoGroupVisibility(RotateGizmos, false);
	SetGizmoGroupVisibility(FaceGizmos, false);

	ActiveGizmoType = EInteractiveGizmoType::None;
	SelectedGizmoHandle = EGizmoHandleID::None;
}

// ==================== 事件处理函数 ====================

// 【统一广播】建造模式参数更新（2026-01-28）
// BatchMode 已删除 (2026-01-29)，改为通过工具TAG判断预览模式
void UVoxelPreviewComponent::HandleSliceHeightChanged()
{
	ApplySliceVisuals();
}

// ==================== 按域增量事件处理 ====================

void UVoxelPreviewComponent::HandleToolChanged()
{
	// 工具切换：重建模式相关预览 + Gizmo 显隐 + 选区高亮
	RefreshAllPreview();
}

void UVoxelPreviewComponent::HandleGeometryChanged()
{
	// 几何变更：重建几何预览 + Gizmo 位置跟随 + 选区临时形状渲染
	RefreshAllPreview();
}

void UVoxelPreviewComponent::HandleBuildPhaseChanged()
{
	// 阶段变更：只切 Gizmo 显隐（选区临时形状依赖阶段，同步刷新）
	if (CachedStateComponent)
	{
		SetPreviewState(CachedStateComponent->GetBuildPhase());
		UpdateSelectionHighlight();
	}
}

void UVoxelPreviewComponent::RefreshAllPreview()
{
	if (!CachedStateComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ToolChain] PreviewComp RefreshAllPreview: CachedStateComponent null"));
		return;
	}
	RefreshToolPreview();
	SetPreviewState(CachedStateComponent->GetBuildPhase());
	UpdateSelectionHighlight();
}

void UVoxelPreviewComponent::HandleBrushChanged()
{
	RefreshToolPreview();
}

void UVoxelPreviewComponent::HandleSelectedFurnitureChanged()
{
	UpdateFurnitureSelectionHighlight();
}

void UVoxelPreviewComponent::HandleItemInHandChanged()
{
	// 手持物变更：影响家具/模板/单体素预览
	RefreshToolPreview();
}

void UVoxelPreviewComponent::HandleSelectionChanged()
{
	UpdateSelectionHighlight();
}

void UVoxelPreviewComponent::HandleBuildModeParamsUpdated()
{
	// 全量刷新入口（初始化/模式切换时调用一次；日常变更走按域小函数）
	HandleToolChanged();
	UpdateFurnitureSelectionHighlight();
}
void UVoxelPreviewComponent::UpdateSelectionHighlight()
{
	// 工具驱动：委托给当前工具的 UpdateSelectionHighlight
	if (UVoxelToolBase* Tool = GetCurrentTool())
		Tool->UpdateSelectionHighlight(this);
}


void UVoxelPreviewComponent::RefreshToolPreview()
{
	// 工具驱动：委托给当前工具的 UpdatePreview
	if (UVoxelToolBase* Tool = GetCurrentTool())
		Tool->UpdatePreview(this);
	else
		ClearPreview();
}


void UVoxelPreviewComponent::UpdateFurnitureSelectionHighlight()
{
	if (!CachedStateComponent) return;

	AUVoxelItemBaseInstance* CurrentSelected = CachedStateComponent->GetSelectedFurniture();
	AUVoxelItemBaseInstance* LastSelected = LastHighlightedFurniture.Get();

	// 无变化则跳过
	if (CurrentSelected == LastSelected) return;

	// 取消旧高亮：BoxVisual 可见性 + 描边
	if (LastSelected && LastSelected->BoxVisual)
	{
		LastSelected->BoxVisual->SetVisibility(false, true);
	}

	// 设置新高亮：BoxVisual 可见性 + 描边
	if (CurrentSelected && CurrentSelected->BoxVisual)
	{
		CurrentSelected->BoxVisual->SetVisibility(true, true);
	}

	// 统一描边（CustomDepth + Stencil，后处理材质渲染为白色描边）
	// 同时只描边一个 Actor，内部差异更新旧/新
	SetOutlinedActor(CurrentSelected);

	LastHighlightedFurniture = CurrentSelected;

	UE_LOG(LogTemp, Log, TEXT("[FurnitureHighlight] %s"),
		CurrentSelected ? *CurrentSelected->GetName() : TEXT("cleared"));
}

// 【已废弃 - 2026-01-28 - 统一使用 HandleBuildModeParamsUpdated】
// void UVoxelPreviewComponent::HandleBatchModeChanged(bool bBatchModeEnabled)
// {
// 	if (CachedStateComponent) HandleBuildModeParamsUpdated();
// }
//
// void UVoxelPreviewComponent::HandleBatchShapeChanged(EBatchShapeType NewShape)
// {
// 	if (CachedStateComponent) HandleBuildModeParamsUpdated();
// }
//
// void UVoxelPreviewComponent::HandleBuildStateChanged(UVoxelPlayerStateCacheComponent* SC)
// {
// 	HandleBuildModeParamsUpdated();
// }
//
// void UVoxelPreviewComponent::HandleTmpEditRequestChanged(const FVoxelEditRequest& NewRequest)
// {
// 	if (!PreviewActor) return;
//
// 	float BlockLength = 20.0f;
// 	if (UVoxelGameInstance* GameInstance = GetWorld()->GetGameInstance<UVoxelGameInstance>())
// 	{
// 		BlockLength = GameInstance->GetBlockLength();
// 	}
//
// 	PreviewActor->UpdateFromEditRequest(NewRequest, BlockLength);
//
// 	const FBox WorldBounds = NewRequest.Geometry.GetBound(BlockLength);
// 	UpdateAllGizmosPosition(NewRequest.Geometry.GetCenter(BlockLength), WorldBounds, NewRequest.Geometry.Rotation);
// }
//
// void UVoxelPreviewComponent::HandleAreaFieldChanged(const FTemporaryAreaInfo& NewAreaInfo)
// {
// 	// 可选实现
// }

void UVoxelPreviewComponent::HandleBuildModeChanged(UVoxelInteractionComponent* Interaction)
{
	if (!Interaction) return;

	if (Interaction->GetControllerMode() != EVoxelControllerMode::SceneBuildMode)
	{
		ResetBuildVisualItem();
	}
	else
	{
		HandleBuildModeParamsUpdated();
	}
}

void UVoxelPreviewComponent::HandleCurrentAreaChanged(AVoxelAreaBaseInstance* OldArea, AVoxelAreaBaseInstance* NewArea)
{
	if (OldArea && OldArea->IsValidLowLevel()) OldArea->UpdateAreaVisual(true, false);
	if (NewArea && NewArea->IsValidLowLevel()) NewArea->UpdateAreaVisual(true, true);

	// 刷新当前层级的子区域显示
	if (CachedStateComponent)
	{
		UVoxelAreaWorldSubsystem* AreaSubsystem = GetWorld()->GetSubsystem<UVoxelAreaWorldSubsystem>();
		if (AreaSubsystem)
		{
			AVoxelAreaBaseInstance* DisplayParent = CachedStateComponent->GetCurrentParentArea();
			int32 CharId = -1;
			if (const UVoxelGameInstance* GameInstance = GetWorld()->GetGameInstance<UVoxelGameInstance>())
			{
				CharId = GameInstance->GetMainCharacterId();
			}
			AreaSubsystem->UpdateSubAreasVisual(DisplayParent, true, CharId);
		}
	}
}


// ==================== Geometry 操作回调处理 ====================

void UVoxelPreviewComponent::HandleMoveGEO(FIntVector MoveDelta)
{
	UVoxelPlayerStateCacheComponent* BuildStateComp = GetStateComponent();
	if (!BuildStateComp) return;

	FVoxelGeometryDesc Geometry = BuildStateComp->GetTmpGeometry();
	Geometry.MoveGEO(MoveDelta);
	BuildStateComp->SetTmpGeometry(Geometry);
}

void UVoxelPreviewComponent::HandleRotateGEO(FRotator RotateDelta)
{
	UVoxelPlayerStateCacheComponent* BuildStateComp = GetStateComponent();
	if (!BuildStateComp) return;

	FVoxelGeometryDesc Geometry = BuildStateComp->GetTmpGeometry();
	Geometry.RotateGEO(RotateDelta);
	BuildStateComp->SetTmpGeometry(Geometry);
}

void UVoxelPreviewComponent::HandleModifyCoverage(EAxis::Type Axis, bool bMaxSide, int32 Delta, float BlockLength)
{
	UVoxelPlayerStateCacheComponent* BuildStateComp = GetStateComponent();
	if (!BuildStateComp) return;

	FVoxelGeometryDesc Geometry = BuildStateComp->GetTmpGeometry();
	if (Geometry.ModifyCoverage(Axis, bMaxSide, Delta))
	{
		BuildStateComp->SetTmpGeometry(Geometry);
	}
}

void UVoxelPreviewComponent::HandleRecenter(float BlockLength)
{
	UVoxelPlayerStateCacheComponent* BuildStateComp = GetStateComponent();
	if (!BuildStateComp) return;

	// 几何中心已改为实时计算（GetGeometricCenter），不需要重定位
	// 此方法保留以兼容蓝图调用
	BuildStateComp->OnGeometryChanged.Broadcast();
}

// ==================== 辅助方法 ====================

// GetStateComponent() 已在头文件内联定义

// ==================== 游标 Actor 管理 ====================

void UVoxelPreviewComponent::EnsureCursorActor()
{
	if (CursorActor && IsValid(CursorActor)) return;

	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	CursorActor = World->SpawnActor<AVoxelPivotCursorActor>(
		AVoxelPivotCursorActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
}

void UVoxelPreviewComponent::UpdateCursorActor(const FVector& PivotWorld, bool bVisible)
{
	if (!CursorActor || !IsValid(CursorActor)) return;

	CursorActor->SetCursorWorldLocation(PivotWorld);
	CursorActor->SetActorHiddenInGame(!bVisible);
}

void UVoxelPreviewComponent::DestroyCursorActor()
{
	if (CursorActor && IsValid(CursorActor))
	{
		CursorActor->Destroy();
		CursorActor = nullptr;
	}
}

// ==================== 地面标记 Actor 管理 ====================

void UVoxelPreviewComponent::EnsureGroundMarkerActor()
{
	if (GroundMarkerActor && IsValid(GroundMarkerActor)) return;

	// PIE 结束/世界销毁途中输入事件仍可能到达，此时 GetWorld() 为空
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	GroundMarkerActor = World->SpawnActor<AVoxelPivotCursorActor>(
		AVoxelPivotCursorActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (GroundMarkerActor)
	{
		#if WITH_EDITOR
		GroundMarkerActor->SetActorLabel(TEXT("GroundMarker"));
		#endif
	}
}

void UVoxelPreviewComponent::DestroyGroundMarkerActor()
{
	if (GroundMarkerActor && IsValid(GroundMarkerActor))
	{
		GroundMarkerActor->Destroy();
		GroundMarkerActor = nullptr;
	}
}

void UVoxelPreviewComponent::UpdateGroundMarker()
{
	if (!GetWorld()) return;

	// 视野中心指示器：跟随 Pawn 实际位置（=相机注视点=屏幕中心），不再跟随鼠标。
	APawn* OwnerPawn = nullptr;
	if (AVoxelPlayerController* VoxelPC = Cast<AVoxelPlayerController>(GetOwner()))
	{
		OwnerPawn = VoxelPC->GetPawn();
	}
	else if (APawn* DirectPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn = DirectPawn;
	}

	if (!OwnerPawn) return;

	// 缓存当前地面命中点（供 OrbitCamera 等历史调用方使用，保留兼容）
	CachedGroundHitLocation = OwnerPawn->GetActorLocation();

	EnsureGroundMarkerActor();
	if (!GroundMarkerActor) return;

	// 白球放到 Pawn 实际位置
	const FVector PawnLoc = OwnerPawn->GetActorLocation();
	GroundMarkerActor->SetCursorWorldLocation(PawnLoc);
	GroundMarkerActor->SetActorHiddenInGame(false);

	// 浮空时画落地竖线：从 Pawn 向下射线找最近地面点
	FVector GroundPoint;
	if (FindGroundBelow(PawnLoc, GroundPoint))
	{
		// Pawn 高于地面 -> 显示竖线
		GroundMarkerActor->SetDropLine(PawnLoc, GroundPoint);
	}
	else
	{
		// 贴地或射线未命中（地形未加载）：隐藏竖线
		GroundMarkerActor->SetDropLine(PawnLoc, PawnLoc);
	}
}

bool UVoxelPreviewComponent::FindGroundBelow(const FVector& FromWorld, FVector& OutGroundPoint) const
{
	if (!GetWorld()) return false;

	// 从 Pawn 上方一段向下射线，找正下方最近地面（体素地形，ECC_GameTraceChannel1）
	const float TraceUp = 200.0f;
	const float TraceDown = 10000.0f;
	const FVector Start = FromWorld + FVector(0.f, 0.f, TraceUp);
	const FVector End = FromWorld - FVector(0.f, 0.f, TraceDown);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_GameTraceChannel1, Params))
	{
		// 仅当地面确实在 Pawn 下方（Pawn 浮空）才算
		if (Hit.Location.Z < FromWorld.Z - 1.0f)
		{
			OutGroundPoint = Hit.Location;
			return true;
		}
	}
	return false;
}

void UVoxelPreviewComponent::ShowRotationRings()
{
	// MMB 按下：旋转环显示在 Pawn 位置（轨道轴心=视野中心）。轨道旋转本身绕 Pawn，
	// 无需锁定鼠标命中点。
	EnsureGroundMarkerActor();
	if (GroundMarkerActor)
	{
		// 立即把标记对齐到 Pawn（与 UpdateGroundMarker 一致）
		APawn* OwnerPawn = nullptr;
		if (AVoxelPlayerController* VoxelPC = Cast<AVoxelPlayerController>(GetOwner()))
		{
			OwnerPawn = VoxelPC->GetPawn();
		}
		else if (APawn* DirectPawn = Cast<APawn>(GetOwner()))
		{
			OwnerPawn = DirectPawn;
		}
		if (OwnerPawn)
		{
			GroundMarkerActor->SetCursorWorldLocation(OwnerPawn->GetActorLocation());
		}
		GroundMarkerActor->SetRotationRingsVisible(true);
	}
}

void UVoxelPreviewComponent::HideRotationRings()
{
	// MMB 松开：隐藏旋转环
	if (GroundMarkerActor)
	{
		GroundMarkerActor->SetRotationRingsVisible(false);
	}
}

void UVoxelPreviewComponent::ApplySliceVisuals()
{
	if (!CachedStateComponent) return;

	const float SliceHeight = CachedStateComponent->GetEditSliceWorldHeight();
	const bool bEnabled = CachedStateComponent->IsSliceEnabled();

	// 计算区域配置
	const FSliceAreaConfig AreaConfig = ComputeSliceAreaConfig();

	// 1. 体素 chunk：触发视野内 dirty chunk 重建（CPU 三域双 Section）
	if (UVoxelBlockWorldSubsystem* BlockSys = GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>())
	{
		BlockSys->ApplySlice(SliceHeight, bEnabled, AreaConfig);
	}

	// 2. 家具/生物：遍历材质实例设置（跳过 chunk，避免污染 SharedChunkMID）
	if (bEnabled)
	{
		ApplySliceToFurnitureAndCreatures(AreaConfig);
	}
}

void UVoxelPreviewComponent::HandleCutoutChanged()
{
	ApplyCutoutVisuals();
}

void UVoxelPreviewComponent::ApplyCutoutVisuals()
{
	if (!CachedStateComponent) return;
	UVoxelBlockWorldSubsystem* BlockSys = GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>();
	if (!BlockSys) return;

	// 1. 推有效框集（bEnabled）到 BlockSys（地形 Ghost）
	TArray<FBox> Effective;
	for (const FCutoutBoxData& B : CachedStateComponent->GetCutoutBoxes())
	{
		if (B.bEnabled && B.IsValidBox())
		{
			Effective.Add(B.ToClosedBox());
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Cutout] ApplyCutoutVisuals: boxes=%d effective=%d"), CachedStateComponent->GetCutoutBoxes().Num(), Effective.Num());
	BlockSys->ApplyCutoutBoxes(Effective);

	// 2. 同步边框 Actor
	RefreshCutoutBorderActors();
}

void UVoxelPreviewComponent::RefreshCutoutBorderActors()
{
	if (!CachedStateComponent) return;
	UVoxelBlockWorldSubsystem* BlockSys = GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>();
	const float BL = BlockSys ? static_cast<float>(BlockSys->BlockLength) : 20.0f;
	const bool bEdit = CachedStateComponent->IsCutoutEditMode();

	const TArray<FCutoutBoxData>& Boxes = CachedStateComponent->GetCutoutBoxes();

	// 收集当前框 Id 集
	TSet<int32> LiveIds;
	for (const FCutoutBoxData& B : Boxes) LiveIds.Add(B.Id);

	// 销毁已不存在的边框 Actor
	TArray<int32> DeadIds;
	CutoutBoxActors.GetKeys(DeadIds);
	for (int32 Id : DeadIds)
	{
		if (!LiveIds.Contains(Id))
		{
			if (AVoxelCutoutBoxActor* A = CutoutBoxActors[Id])
			{
				A->Destroy();
			}
			CutoutBoxActors.Remove(Id);
		}
	}

	// 新建/更新现有框的边框
	for (const FCutoutBoxData& B : Boxes)
	{
		AVoxelCutoutBoxActor* Actor = nullptr;
		if (AVoxelCutoutBoxActor** Found = CutoutBoxActors.Find(B.Id))
		{
			Actor = *Found;
		}
		if (!Actor)
		{
			UWorld* World = GetWorld();
			if (!World) return;
			Actor = World->SpawnActor<AVoxelCutoutBoxActor>(AVoxelCutoutBoxActor::StaticClass());
			if (!Actor) continue;
			Actor->SetBoxId(B.Id);
			CutoutBoxActors.Add(B.Id, Actor);
		}
		Actor->UpdateBounds(B.MinIndex, B.MaxIndex, BL);
		Actor->SetEnabled(B.bEnabled);
		Actor->SetEditMode(bEdit);
	}
}

FSliceAreaConfig UVoxelPreviewComponent::ComputeSliceAreaConfig() const
{
	FSliceAreaConfig Config;

	if (!CachedStateComponent) return Config;

	const ESliceFilterMode FilterMode = CachedStateComponent->GetSliceFilterMode();
	if (FilterMode == ESliceFilterMode::All)
	{
		// 全局生效，无需区域参数
		Config.AreaMode = 0;
		return Config;
	}

	// PlayerManaged 模式
	UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>();
	if (!GI) return Config;

	const int32 PlayerCreatureId = GI->GetMainCharacterId();
	UVoxelAreaWorldSubsystem* AreaSys = GetWorld()->GetSubsystem<UVoxelAreaWorldSubsystem>();
	if (!AreaSys) return Config;

	const float BlockLength = UChunkUtilsFunction::GetBlockLength(this);

	// 辅助：将区域的 AreaCoverage（体素索引空间）转换为世界空间 AABB
	auto AreaToWorldBox = [BlockLength](AVoxelAreaBaseInstance* Area) -> FBox
	{
		if (!Area) return FBox(FVector::ZeroVector, FVector::ZeroVector);
		const FBox IndexBox = Area->AreaBase.AreaCoverage.BoundingBox();
		return FBox(IndexBox.Min * BlockLength, IndexBox.Max * BlockLength);
	};

	// 辅助：取距离玩家最近的 N 个区域
	auto TakeNearest = [this](TArray<AVoxelAreaBaseInstance*>& Areas, int32 MaxCount) -> TArray<AVoxelAreaBaseInstance*>
	{
		if (Areas.Num() <= MaxCount) return Areas;
		// 按到玩家的距离排序
		FVector PlayerPos = FVector::ZeroVector;
		if (AActor* Owner = GetOwner())
		{
			PlayerPos = Owner->GetActorLocation();
		}
		Areas.Sort([PlayerPos](const AVoxelAreaBaseInstance& A, const AVoxelAreaBaseInstance& B)
		{
			return FVector::Dist(A.GetActorLocation(), PlayerPos) < FVector::Dist(B.GetActorLocation(), PlayerPos);
		});
		TArray<AVoxelAreaBaseInstance*> Result;
		for (int32 i = 0; i < MaxCount && i < Areas.Num(); ++i)
		{
			Result.Add(Areas[i]);
		}
		return Result;
	};

	// 优先级 1：当前选中区域
	if (AVoxelAreaBaseInstance* CurArea = CachedStateComponent->GetCurrentArea())
	{
		Config.AreaMode = 1; // 白名单
		Config.AABBs.Add(AreaToWorldBox(CurArea));
		UE_LOG(LogTemp, Warning, TEXT("[Slice] AreaConfig: P1 SelectedArea, AreaMode=1, Boxes=%d"), Config.AABBs.Num());
		return Config;
	}

	// 优先级 2：当前选中父区域
	if (AVoxelAreaBaseInstance* CurParentArea = CachedStateComponent->GetCurrentParentArea())
	{
		Config.AreaMode = 1; // 白名单
		Config.AABBs.Add(AreaToWorldBox(CurParentArea));
		UE_LOG(LogTemp, Warning, TEXT("[Slice] AreaConfig: P2 SelectedParentArea, AreaMode=1, Boxes=%d"), Config.AABBs.Num());
		return Config;
	}

	// 优先级 3：玩家下属（经营/所有）无父级的一级区域
	TArray<AVoxelAreaBaseInstance*> PlayerAreas = AreaSys->GetPlayerTopLevelAreas(PlayerCreatureId);
	if (PlayerAreas.Num() > 0)
	{
		Config.AreaMode = 1; // 白名单
		TArray<AVoxelAreaBaseInstance*> Nearest = TakeNearest(PlayerAreas, 8);
		for (AVoxelAreaBaseInstance* Area : Nearest)
		{
			Config.AABBs.Add(AreaToWorldBox(Area));
		}
		UE_LOG(LogTemp, Warning, TEXT("[Slice] AreaConfig: P3 OwnTopLevelAreas, AreaMode=1, Boxes=%d"), Config.AABBs.Num());
		return Config;
	}

	// 优先级 4：无自己区域 -> 空白名单（没有任何区域受影响，不回退全局）
	{
		Config.AreaMode = 1; // 白名单但 AABBs 为空 -> IsInAnyAreaBox 恒 false -> 无区域受影响
		UE_LOG(LogTemp, Warning, TEXT("[Slice] AreaConfig: P4 NoOwnArea (no effect), AreaMode=1, Boxes=0"));
	}

	return Config;
}

void UVoxelPreviewComponent::ApplySliceToFurnitureAndCreatures(const FSliceAreaConfig& AreaConfig)
{
	if (!GetWorld()) return;

	const float SliceHeight = CachedStateComponent ? CachedStateComponent->GetEditSliceWorldHeight() : 0.f;
	const bool bSliceEnabled = CachedStateComponent && CachedStateComponent->IsSliceEnabled();

	// 遍历所有 Actor（家具/生物）
	int32 ProcessedCount = 0;
	int32 SkippedChunkCount = 0;
	for (TActorIterator<AActor> It(GetWorld(), AActor::StaticClass()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;
		if (Actor->IsA<AChunkBase>()) { SkippedChunkCount++; continue; }
		ApplySliceToActor(Actor, AreaConfig, SliceHeight, bSliceEnabled);
		ProcessedCount++;
	}
	UE_LOG(LogTemp, Warning, TEXT("[Slice] FurnitureAndCreatures: Processed=%d, SkippedChunks=%d"),
		ProcessedCount, SkippedChunkCount);
}

void UVoxelPreviewComponent::ApplySliceToActor(AActor* Actor, const FSliceAreaConfig& AreaConfig, float SliceHeight, bool bEnabled)
{
	if (!Actor) return;

	// 跳过 Chunk actor：体素地形由 ApplySlice -> 三域双 Section 重建处理，
	// 不能在此覆写 SharedChunkMID 参数（否则 last-writer-wins 污染全局开关）
	if (Actor->IsA<AChunkBase>()) return;

	// 计算该 Actor 是否受控（在切面作用范围内）
	bool bControlled = false;
	if (!bEnabled)
	{
		bControlled = false;
	}
	else if (AreaConfig.AreaMode == 0)
	{
		bControlled = true; // 全局生效
	}
	else
	{
		// 白名单：检查 Actor 是否在任一 AABB 内
		const FVector ActorPos = Actor->GetActorLocation();
		for (const FBox& Box : AreaConfig.AABBs)
		{
			if (Box.IsInside(ActorPos))
			{
				bControlled = true;
				break;
			}
		}
	}

	// 底部是否超过最大渲染高度（与体素规则一致：BottomZ >= SliceHeight -> 虚化）
	FVector Origin, Extent;
	Actor->GetActorBounds(false, Origin, Extent);
	const float BottomZ = Origin.Z - Extent.Z;
	const bool bGhost = bControlled && (BottomZ >= SliceHeight);

	const float IsGhostValue = bGhost ? 1.0f : 0.0f;

	// 遍历 Actor 所有 MeshComponent，设置 IsGhost 布尔参数
	TArray<UMeshComponent*> MeshComponents;
	Actor->GetComponents<UMeshComponent>(MeshComponents);

	for (UMeshComponent* MeshComp : MeshComponents)
	{
		if (!MeshComp) continue;

		const int32 NumMaterials = MeshComp->GetNumMaterials();
		for (int32 MatIdx = 0; MatIdx < NumMaterials; ++MatIdx)
		{
			UMaterialInterface* Mat = MeshComp->GetMaterial(MatIdx);
			if (!Mat) continue;

			// 确保是 MID（非 MID 则创建）
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mat);
			if (!MID)
			{
				MID = MeshComp->CreateDynamicMaterialInstance(MatIdx, Mat);
				if (!MID) continue;
			}

			MID->SetScalarParameterValue(FName(TEXT("IsGhost")), IsGhostValue);
		}
	}
}

void UVoxelPreviewComponent::CheckSliceAreaUpdate(float DeltaTime)
{
	if (!CachedStateComponent) return;
	if (!CachedStateComponent->IsSliceEnabled()) return;
	if (CachedStateComponent->GetSliceFilterMode() != ESliceFilterMode::PlayerManaged) return;

	LastSliceUpdateTime += DeltaTime;

	// 节流：间隔不足时跳过
	if (LastSliceUpdateTime < SliceUpdateInterval) return;

	// 检查玩家移动距离
	AActor* Owner = GetOwner();
	if (!Owner) return;

	const FVector CurrentPos = Owner->GetActorLocation();
	const float MoveDist = FVector::Dist(CurrentPos, LastSliceUpdatePos);

	if (MoveDist >= SliceUpdateMoveThreshold)
	{
		LastSliceUpdatePos = CurrentPos;
		LastSliceUpdateTime = 0.0f;
		// 重新计算区域集合并应用
		ApplySliceVisuals();
	}
}

void UVoxelPreviewComponent::ApplySliceToNewActor(AActor* TargetActor)
{
	if (!TargetActor || !CachedStateComponent) return;

	const float SliceHeight = CachedStateComponent->GetEditSliceWorldHeight();
	const bool bEnabled = CachedStateComponent->IsSliceEnabled();
	const FSliceAreaConfig AreaConfig = ComputeSliceAreaConfig();

	ApplySliceToActor(TargetActor, AreaConfig, SliceHeight, bEnabled);
}

// ==================== 描边管理 ====================

static constexpr int32 OutlineStencilValue = 1;

void UVoxelPreviewComponent::SetOutlinedActor(AActor* Actor)
{
	AActor* Current = OutlinedActor.Get();
	if (Current == Actor) return;

	if (Current)
	{
		RemoveOutlineFromActor(Current);
	}

	OutlinedActor = Actor;

	if (Actor)
	{
		ApplyOutlineToActor(Actor);
	}
}

void UVoxelPreviewComponent::ApplyOutlineToActor(AActor* Actor)
{
	if (!Actor) return;
	TArray<UMeshComponent*> MeshComps;
	Actor->GetComponents<UMeshComponent>(MeshComps);
	for (UMeshComponent* Mesh : MeshComps)
	{
		if (!Mesh) continue;
		Mesh->SetCustomDepthStencilValue(OutlineStencilValue);
		Mesh->SetRenderCustomDepth(true);
	}
}

void UVoxelPreviewComponent::RemoveOutlineFromActor(AActor* Actor)
{
	if (!Actor) return;
	TArray<UMeshComponent*> MeshComps;
	Actor->GetComponents<UMeshComponent>(MeshComps);
	for (UMeshComponent* Mesh : MeshComps)
	{
		if (!Mesh) continue;
		Mesh->SetRenderCustomDepth(false);
		Mesh->SetCustomDepthStencilValue(0);
	}
}
