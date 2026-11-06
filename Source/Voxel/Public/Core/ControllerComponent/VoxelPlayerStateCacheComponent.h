// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Enum/GameStateEnum.h"
#include "Data/Area/VoxelAreaBase.h"
#include "Data/Item/InteractableData.h"
#include "Data/VoxelCutoutData.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Voxel/Editor/VoxelClipboardData.h"
#include "Voxel/Editor/VoxelPlacementCoreTypes.h"
#include "VoxelPlayerStateCacheComponent.generated.h"

class UVoxelGameInstance;
class AVoxelAreaBaseInstance;
class AUVoxelItemBaseInstance;
class AVoxelPlayerController;
class UVoxelControllerHistoryComponent;
class UVoxelInteractionComponent;
struct FVoxelEditHistoryRecord;

/**
 * 选中实体类型（实体详情面板的数据源）
 * 区域先行；生物/家居后续按同模式扩展
 */
UENUM(BlueprintType)
enum class EVoxelSelectedEntityType : uint8
{
	None		UMETA(DisplayName="无"),
	Area		UMETA(DisplayName="区域"),
	Creature	UMETA(DisplayName="生物"),
	Furniture	UMETA(DisplayName="家居")
};

/**
 * 临时区域信息（不包含 Coverage，Coverage 存储在 TmpGeometry.Box 中）
 * 用于区域编辑预览阶段
 */
USTRUCT(BlueprintType)
struct FTemporaryAreaInfo
{
	GENERATED_BODY()

	/** 区域名称 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Area")
	FName AreaName;

	/** 区域类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Area")
	FGameplayTag AreaType;

	/** 父级区域 ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Area")
	int32 ParentAreaId = 0;

	/** 所属场景 ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Area")
	int32 ParentScenceId = 0;

	/** 区域状态 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Area")
	EAreaStatus Status = EAreaStatus::Active;

	/** 所有人 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Area")
	int OwnerCreatureId = -1;

	/** 经营人 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Area")
	int ManagerCreatureId = -1;

	/** 是否有效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Area")
	bool bIsValid = false;

	FTemporaryAreaInfo()
		: AreaName(NAME_None)
		, AreaType(FGameplayTag::EmptyTag)
		, ParentAreaId(0)
		, ParentScenceId(0)
		, Status(EAreaStatus::Active)
		, OwnerCreatureId(-1)
		, ManagerCreatureId(-1)
		, bIsValid(false)
	{
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSelectedEntityChanged, EVoxelSelectedEntityType, NewType, int32, NewEntityId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectionChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPreDragChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnClipboardChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectionModeChanged, ESelectionOpMode, NewMode);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnItemInHandChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemInSightChanged, FInteractableData&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCurrentAreaChanged, AVoxelAreaBaseInstance*, OldArea, AVoxelAreaBaseInstance*, NewArea);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameAuthorityChanged, EVoxelGameAuthority, NewAuthority);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGeometryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBrushChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSliceChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPawnHeightChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLayersChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCutoutChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectedFurnitureChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTmpAreaChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlacementModeChanged);

/**
 * 玩家状态缓存组件（挂在 PlayerController 上）
 *
 * 职责：玩家侧「我选了什么 / 我拿了什么 / 我调了什么参数」的唯一数据源。
 * 纯数据 + 广播，无业务判断：
 *  - 世界实体选中（SelectedEntity）——吸收原 UVoxelEntitySelectionComponent
 *  - 体素选区/拖拽快照/统一选区/剪切板——吸收原 UVoxelSelectionComponent
 *  - 手持物/区域工作区/几何参数/切面/Pawn高度/图层/透明框/笔刷/选中家具——吸收原 UVoxelPlayerStateCacheComponent 数据段
 *
 * 交互流程状态（ControllerMode/CurrentToolTag/BuildPhase）的持有者为
 * UVoxelInteractionComponent；本组件提供同名便捷转发访问器，
 * 供工具/UI 经单一 StateCache 引用读取（实际状态不在此存储）。
 *
 * 单向数据流：状态变化经细粒度事件广播（变化才广播守卫），
 * UI/Preview 订阅刷新；写入只经业务方法，UI 禁止直改字段。
 */
UCLASS(ClassGroup=(Voxel), meta=(BlueprintSpawnableComponent))
class VOXEL_API UVoxelPlayerStateCacheComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVoxelPlayerStateCacheComponent();

	// ==================== 世界实体选中 ====================

	/** 选中指定区域（Id 无效或区域不存在时退化为清除选中） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Selection")
	void SelectAreaById(int32 AreaId);

	/** 清除实体选中（✕ 按钮 / 点击空处 / Esc）。
	 *  带 IDLE 守卫：EDITING（框选编辑中）只清展示选中，不动编辑工作区 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Selection")
	void ClearSelectedEntity();

	/** 强制清空实体选中（工具右键取消路径：绕过守卫，联动清空编辑工作区） */
	void ForceClearSelectedEntity();

	/** 选中指定区域 Actor（工具路径：可绕过 IDLE 守卫显式联动编辑工作区） */
	void SetSelectedAreaActor(AVoxelAreaBaseInstance* Area, bool bForceDriveBuildState = false);

	/** 世界点击入口：所有模式下由交互组件委托调用（与工具流程并行，互不拦截）。
	 *  射线按「生物 > 家具 > 区域」优先级命中精确目标 → 选中；点击空处 → 清除。
	 *  模式策略：区域/家具任何模式可选中；生物仅非建造模式可选中。 */
	void HandleWorldClick();

	UFUNCTION(BlueprintPure, Category = "Voxel|Selection")
	EVoxelSelectedEntityType GetSelectedEntityType() const { return SelectedType; }

	UFUNCTION(BlueprintPure, Category = "Voxel|Selection")
	int32 GetSelectedAreaId() const { return SelectedType == EVoxelSelectedEntityType::Area ? SelectedId : -1; }

	UFUNCTION(BlueprintPure, Category = "Voxel|Selection")
	bool HasSelectedEntity() const { return SelectedType != EVoxelSelectedEntityType::None; }

	/** 解析当前选中区域 Actor（经子系统按 Id 现取；区域被删除后返回 nullptr，不悬垂） */
	AVoxelAreaBaseInstance* GetSelectedArea() const;

	/** 选中实体变化广播（Old/New 由订阅方自行按 Id 解析） */
	UPROPERTY(BlueprintAssignable, Category = "Voxel|Selection")
	FOnSelectedEntityChanged OnSelectedEntityChanged;

	// ==================== 体素选区 ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|Selection")
	ESelectionOpMode GetSelectionMode() const { return SelectionMode; }

	UFUNCTION(BlueprintCallable, Category = "Voxel|Selection")
	void SetSelectionMode(ESelectionOpMode Mode);

	const TArray<FIntVector>& GetCurrentSelection() const { return CurrentSelection; }

	UFUNCTION(BlueprintPure, Category = "Voxel|Selection")
	bool HasSelection() const { return CurrentSelection.Num() > 0; }

	/** 应用选区操作（New/Add/Subtract/Intersect），广播 OnSelectionChanged 并同步包围盒到 TmpGeometry.Box */
	void ApplySelectionOp(const TArray<FIntVector>& NewShape, ESelectionOpMode Op);

	UFUNCTION(BlueprintCallable, Category = "Voxel|Selection")
	void ClearSelection();

	UFUNCTION(BlueprintCallable, Category = "Voxel|Selection")
	void MoveSelection(const FIntVector& Offset);

	UFUNCTION(BlueprintPure, Category = "Voxel|Selection")
	FIntVector GetSelectionCenter() const;

	// ==================== 拖拽前快照 ====================

	UFUNCTION(BlueprintCallable, Category = "Voxel|Selection")
	void SavePreDragSelection();

	void SetPreDragSelection(const TArray<FIntVector>& InSelection);

	void ClearPreDragSelection();

	const TArray<FIntVector>& GetPreDragSelection() const { return PreDragSelection; }

	// ==================== 统一选区（Placement） ====================

	const FPlacementSelection& GetPlacementSelection() const { return CurrentPlacementSelection; }

	void SetPlacementSelection(const FPlacementSelection& InSelection) { CurrentPlacementSelection = InSelection; }

	void ClearPlacementSelection() { CurrentPlacementSelection.Clear(); }

	UPROPERTY(BlueprintAssignable, Category = "Voxel|Selection")
	FOnSelectionChanged OnSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Voxel|Selection")
	FOnPreDragChanged OnPreDragChanged;

	UPROPERTY(BlueprintAssignable, Category = "Voxel|Selection")
	FOnClipboardChanged OnClipboardChanged;

	UPROPERTY(BlueprintAssignable, Category = "Voxel|Selection")
	FOnSelectionModeChanged OnSelectionModeChanged;

	// ==================== 手持物 / 视线物 ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Item")
	FInteractableData GetInteractableDataInHand() const { return InteractableDataInHand; }

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Item")
	FInteractableData GetInteractableDataInSight() const { return InteractableDataInSight; }

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Item")
	void SetInteractableDataInHand(FInteractableData& Data);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Item")
	void SetInteractableDataInSight(FInteractableData& Data);

	UPROPERTY(BlueprintAssignable, Category = "Events|Item")
	FOnItemInHandChanged OnItemInHandChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events|Item")
	FOnItemInSightChanged OnItemInSightChanged;

	// ==================== 区域工作区（原 BuildState 区域段） ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Area")
	FAreaModificationData GetTmpAreaInfo() const;

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Area")
	FBox GetTmpAreaRange() const;

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Area")
	AVoxelAreaBaseInstance* GetCurrentArea() const { return CurArea; }

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Area")
	AVoxelAreaBaseInstance* GetCurrentParentArea() const { return CurParentArea; }

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Area")
	void InitTmpAreaInfo(int32 SceneId);

	/** 创建新区域时初始化临时区域信息（按 ADR-001 规则填充默认值） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Area")
	void InitTmpAreaInfoForCreate();

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Area")
	void ResetTmpAreaInfo();

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void SetTmpAreaInfo(FAreaModificationData NewInfo);

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void SetTmpAreaName(const FName& Name);

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void SetTmpAreaType(FGameplayTag Type);

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void SetTmpAreaCoverage(const FBox& Coverage);

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void SetTmpAreaValid(bool isValid);

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void SetTmpAreaStatus(EAreaStatus Status);

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void SetTmpAreaOwner(int CreatureId);

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void SetTmpAreaManager(int CreatureId);

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void MoveTmpAreaCoverage(FIntVector Offset);

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void SetCurrentArea(AVoxelAreaBaseInstance* Area);

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void ClearCurrentArea();

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void SetCurAreaInfo(FAreaModificationData NewInfo);

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void InternalSetCurrentArea(AVoxelAreaBaseInstance* Area);

	UFUNCTION(BlueprintCallable, Category = "State|Area")
	void SetCurrentParentArea(AVoxelAreaBaseInstance* Area);

	UPROPERTY(BlueprintAssignable, Category = "Events|Area")
	FOnCurrentAreaChanged OnCurrentAreaChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events|Area")
	FOnTmpAreaChanged OnTmpAreaChanged;

	// ==================== 玩家权限 ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Player")
	EVoxelGameAuthority GetGameAuthority() const { return CurGameAuthority; }

	UFUNCTION(BlueprintCallable, Category = "State|Player")
	void SetGameAuthority(EVoxelGameAuthority Authority);

	UPROPERTY(BlueprintAssignable, Category = "Events|Player")
	FOnGameAuthorityChanged OnGameAuthorityChanged;

	// ==================== 交互流程状态便捷转发（持有者：UVoxelInteractionComponent） ====================

	/** 控制器模式（实际存储在 InteractionComp，此处便捷转发） */
	UFUNCTION(BlueprintPure, Category = "Voxel|State|Player")
	EVoxelControllerMode GetControllerMode() const;

	/** 当前工具 Tag（实际存储在 InteractionComp，此处便捷转发） */
	UFUNCTION(BlueprintPure, Category = "Voxel|State|Build")
	FGameplayTag GetCurrentToolTag() const;

	/** 建造流程阶段（实际存储在 InteractionComp，此处便捷转发） */
	UFUNCTION(BlueprintPure, Category = "Voxel|State|Build")
	EPreviewInteractionState GetBuildPhase() const;

	/** 设置建造流程阶段（转发至 InteractionComp，变化才广播 OnBuildPhaseChanged） */
	void SetBuildPhase(EPreviewInteractionState NewPhase);

	// ==================== 放置规则 ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Build")
	EVoxelPlacementMode GetPlacementMode() const { return PlacementMode; }

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Build")
	EBatchShapeType GetCurrentBatchShape() const { return TmpGeometry.ShapeType; }

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Build")
	void SetPlacementMode(EVoxelPlacementMode Mode);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Build")
	void SetCurrentBatchShape(EBatchShapeType Shape);

	UPROPERTY(BlueprintAssignable, Category = "Events|Build")
	FOnPlacementModeChanged OnPlacementModeChanged;

	// ==================== 临时几何 ====================

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit|Preview")
	void SetTmpGeometry(const FVoxelGeometryDesc& Geometry);

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Edit|Preview")
	FVoxelGeometryDesc GetTmpGeometry() const { return TmpGeometry; }

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit|Preview")
	void SetEditPoints(const FVector& First, const FVector& Second);

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Edit|Preview")
	FVector GetFirstEditPoint() const { return FirstEditPoint; }

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Edit|Preview")
	FVector GetSecondEditPoint() const { return SecondEditPoint; }

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit|Preview")
	void SetTmpGeometryRotation(FRotator NewRotation);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit|Preview")
	void SetTmpGeometryThickness(int32 NewThickness);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit|Preview")
	void SetTmpGeometryRegular(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit|Preview")
	void ClearTmpGeometryTransform();

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit|Preview")
	void ResetEditSession();

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit|Preview")
	void ClearTmpEditBounds();

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit|Preview")
	void ClearTmpEditRequest();

	UPROPERTY(BlueprintAssignable, Category = "Events|Build")
	FOnGeometryChanged OnGeometryChanged;

	// ==================== 模板素材替换 ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Edit|Template")
	const TMap<FString, FString>& GetTemplateRemap() const { return TmpTemplateRemap; }

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit|Template")
	void SetTemplateRemapEntry(const FString& OriginFullId, const FString& NewFullId);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit|Template")
	void ClearTemplateRemap();

	// ==================== 编辑请求 ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Edit")
	FVoxelEditRequest GetCurrentEditRequest() const;

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit")
	void SetCurrentEditRequest(const FVoxelEditRequest& NewRequest);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit")
	void UpdateEditRequestPosition(const FIntVector& NewOffset);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Edit")
	FVoxelEditRequest BuildEditRequestFromState(const FIntVector& MinIndex, const FIntVector& MaxIndex, const FRotator& Rotation) const;

	// ==================== 切面高度控制 ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Slice")
	int32 GetEditSliceLayer() const { return EditSliceLayer; }

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Slice")
	bool IsSliceEnabled() const { return bSliceEnabled; }

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Slice")
	ESliceFilterMode GetSliceFilterMode() const { return SliceFilterMode; }

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Slice")
	int32 GetMaxSliceLayer() const;

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Slice")
	int32 GetMinSliceLayer() const;

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Slice")
	float GetEditSliceWorldHeight() const;

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Slice")
	bool IsSliceAtMaxLayer() const { return EditSliceLayer >= GetMaxSliceLayer(); }

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Slice")
	bool IsSliceAtMinLayer() const { return EditSliceLayer <= GetMinSliceLayer(); }

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Slice")
	void SetEditSliceLayer(int32 NewLayer);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Slice")
	void SetSliceEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Slice")
	void SetSliceFilterMode(ESliceFilterMode NewMode);

	UPROPERTY(BlueprintAssignable, Category = "Events|Slice")
	FOnSliceChanged OnSliceChanged;

	// ==================== Pawn 高度控制（体素层） ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Pawn")
	int32 GetPawnHeightLayer() const { return PawnHeightLayer; }

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Pawn")
	float GetPawnWorldHeight() const;

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Pawn")
	void SetPawnHeightLayer(int32 NewLayer);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Pawn")
	void SyncPawnHeightLayerFromPawn();

	UPROPERTY(BlueprintAssignable, Category = "Events|Pawn")
	FOnPawnHeightChanged OnPawnHeightChanged;

	// ==================== 渲染图层（多选开关） ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Layer")
	const TSet<FGameplayTag>& GetActiveLayers() const { return ActiveLayers; }

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Layer")
	bool IsLayerActive(FGameplayTag LayerTag) const { return ActiveLayers.Contains(LayerTag); }

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Layer")
	void SetLayerActive(FGameplayTag LayerTag, bool bActive);

	UPROPERTY(BlueprintAssignable, Category = "Events|Layer")
	FOnLayersChanged OnLayersChanged;

	// ==================== 透明区域框 ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Cutout")
	TArray<FCutoutBoxData> GetCutoutBoxes() const { return CutoutBoxes; }

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Cutout")
	bool GetCutoutBox(int32 BoxId, FCutoutBoxData& OutBox) const;

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Cutout")
	bool IsCutoutEditMode() const { return bCutoutEditMode; }

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Cutout")
	int32 AddCutoutBox(FIntVector MinIndex, FIntVector MaxIndex, UVoxelControllerHistoryComponent* HistoryComp = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Cutout")
	void RemoveCutoutBox(int32 BoxId, UVoxelControllerHistoryComponent* HistoryComp = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Cutout")
	void UpdateCutoutBox(int32 BoxId, FIntVector MinIndex, FIntVector MaxIndex, UVoxelControllerHistoryComponent* HistoryComp = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Cutout")
	void SetBoxEnabled(int32 BoxId, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Cutout")
	void SetAllCutoutBoxesEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Cutout")
	bool FocusCutoutBox(int32 BoxId);

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Cutout")
	void ClearAllCutouts();

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Cutout")
	void SetCutoutEditMode(bool bActive);

	TArray<FCutoutBoxData> GetCutoutBoxesForSave() const;

	void LoadCutoutBoxes(const TArray<FCutoutBoxData>& InBoxes);

	void ExecuteCutoutUndo(const FVoxelEditHistoryRecord& Record);
	void ExecuteCutoutRedo(const FVoxelEditHistoryRecord& Record);

	UPROPERTY(BlueprintAssignable, Category = "Events|Cutout")
	FOnCutoutChanged OnCutoutChanged;

	// ==================== 笔刷参数 ====================

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Voxel|Brush")
	int32 BrushSize = 1;

	UFUNCTION(BlueprintCallable, Category = "Voxel|Brush")
	void SetBrushSize(int32 NewSize);

	int32 GetBrushSize() const { return BrushSize; }

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Voxel|Brush")
	EBrushDimension BrushDimension = EBrushDimension::Volumetric;

	UFUNCTION(BlueprintCallable, Category = "Voxel|Brush")
	void SetBrushDimension(EBrushDimension NewDimension);

	EBrushDimension GetBrushDimension() const { return BrushDimension; }

	UPROPERTY(BlueprintAssignable, Category = "Events|Build")
	FOnBrushChanged OnBrushChanged;

	// ==================== 选中家具 ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|State|Furniture")
	AUVoxelItemBaseInstance* GetSelectedFurniture() const { return SelectedFurnitureActor.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Voxel|State|Furniture")
	void SetSelectedFurniture(AUVoxelItemBaseInstance* Furniture);

	UPROPERTY(BlueprintAssignable, Category = "Events|Furniture")
	FOnSelectedFurnitureChanged OnSelectedFurnitureChanged;

	// ==================== 便捷绑定 ====================

	/**
	 * 一次性绑定全部建造参数事件（等价旧 OnBuildModeParamsUpdated 语义）
	 * 数据事件绑定本组件；工具/流程事件（OnToolChanged/OnBuildPhaseChanged，持有者
	 * InteractionComp）经同 Owner 查找后一并绑定，订阅方无感知
	 */
	void BindAllBuildEvents(UObject* Obj, FName FuncName);

	/** 解绑 BindAllBuildEvents 绑定的全部事件 */
	void UnbindAllBuildEvents(UObject* Obj, FName FuncName);

	// ==================== 状态数据成员 ====================

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Voxel|Selection")
	TArray<FIntVector> PreDragSelection;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Voxel|Selection")
	FPlacementSelection CurrentPlacementSelection;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Voxel|Clipboard")
	FVoxelClipboardData ClipboardData;

	UPROPERTY(VisibleInstanceOnly, Category = "State")
	FInteractableData InteractableDataInHand;

	UPROPERTY(VisibleInstanceOnly, Category = "State")
	FInteractableData InteractableDataInSight;

	UPROPERTY(VisibleInstanceOnly, Category = "State")
	FTemporaryAreaInfo TmpAreaInfo = FTemporaryAreaInfo();

	UPROPERTY(VisibleInstanceOnly, Category = "State")
	TObjectPtr<AVoxelAreaBaseInstance> CurArea;

	UPROPERTY(VisibleInstanceOnly, Category = "State")
	TObjectPtr<AVoxelAreaBaseInstance> CurParentArea;

	UPROPERTY(VisibleInstanceOnly, Category = "State")
	TWeakObjectPtr<AUVoxelItemBaseInstance> SelectedFurnitureActor;

	UPROPERTY(VisibleInstanceOnly, Category = "State")
	EVoxelGameAuthority CurGameAuthority = EVoxelGameAuthority::Developer;

	UPROPERTY(VisibleInstanceOnly, Category = "State")
	EVoxelPlacementMode PlacementMode = EVoxelPlacementMode::Normal;

	UPROPERTY(VisibleInstanceOnly, Category = "State|Edit|Preview")
	FVoxelGeometryDesc TmpGeometry;

	UPROPERTY(VisibleInstanceOnly, Category = "State|Edit|Template")
	TMap<FString, FString> TmpTemplateRemap;

	UPROPERTY(VisibleInstanceOnly, Category = "State|Slice")
	int32 EditSliceLayer = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "State|Slice")
	bool bSliceEnabled = false;

	UPROPERTY(VisibleInstanceOnly, Category = "State|Slice")
	ESliceFilterMode SliceFilterMode = ESliceFilterMode::PlayerManaged;

	UPROPERTY(VisibleInstanceOnly, Category = "State|Pawn")
	int32 PawnHeightLayer = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "State|Layer")
	TSet<FGameplayTag> ActiveLayers;

	UPROPERTY(VisibleInstanceOnly, Category = "State|Cutout")
	TArray<FCutoutBoxData> CutoutBoxes;

	UPROPERTY(VisibleInstanceOnly, Category = "State|Cutout")
	int32 NextCutoutId = 1;

	UPROPERTY(VisibleInstanceOnly, Category = "State|Cutout")
	bool bCutoutEditMode = false;

	UPROPERTY(VisibleInstanceOnly, Category = "State|Edit|Preview")
	FVector FirstEditPoint = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, Category = "State|Edit|Preview")
	FVector SecondEditPoint = FVector::ZeroVector;

protected:
	virtual void BeginPlay() override;

private:
	// === 实体选中状态 ===
	EVoxelSelectedEntityType SelectedType = EVoxelSelectedEntityType::None;
	int32 SelectedId = -1;

	// === 体素选区状态 ===
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Voxel|Selection")
	TArray<FIntVector> CurrentSelection;

	UPROPERTY(VisibleInstanceOnly, Category = "State")
	ESelectionOpMode SelectionMode = ESelectionOpMode::New;

	/** 缓存的交互组件引用（BeginPlay 解析，转发模式/工具/Phase 访问用） */
	UPROPERTY()
	TObjectPtr<UVoxelInteractionComponent> CachedInteractionComp;

	/** 取交互组件（优先缓存，miss 时 FindComponentByClass 兜底并缓存） */
	UVoxelInteractionComponent* GetInteractionComp() const;

	/** 写入选中状态：变化才广播；同时驱动区域 Actor 的选中高亮与编辑工作区派生。
	 *  bForceDriveBuildState=false 时带 IDLE 守卫（EDITING 框选保护中不动编辑工作区） */
	void SetSelectedEntity(EVoxelSelectedEntityType NewType, int32 NewId, bool bForceDriveBuildState);

	/** 选区变更：同步选区包围盒到 TmpGeometry.Box（供 M5 等 UI 模块读取展示） */
	void HandleSelectionChanged();

	// === 私有辅助方法 ===

	void ResetTmpAreaInfoInternal(int32 SceneId);

	bool IsValidArea(AVoxelAreaBaseInstance* Area) const;

	UVoxelGameInstance* GetVoxelGameInstance() const;

	// === 透明框私有辅助 ===

	int32 GetOwnerPlayerId() const;

	void ClampCutoutToWorldBorder(FIntVector& MinIndex, FIntVector& MaxIndex) const;

	void SetCutoutBoxStateInternal(const FCutoutBoxData& Data);

	void RemoveCutoutBoxInternal(int32 BoxId);

	void RecordCutoutHistory(const FCutoutBoxData& Before, const FCutoutBoxData& After, UVoxelControllerHistoryComponent* HistoryComp);

	void BroadcastCutoutChanged();
};
