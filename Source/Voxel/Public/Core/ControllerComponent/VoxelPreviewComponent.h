// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Components/ActorComponent.h"
#include "Core/Enum/GameStateEnum.h"
#include "Core/Enum/EditCommandEnums.h"
#include "Data/VoxelEditCommand.h"
#include "Voxel/Editor/VoxelPreviewActor.h"
#include "VoxelPreviewComponent.generated.h"

struct FTemporaryAreaInfo;
class AVoxelPlayerController;
struct FInteractableData;
struct FVoxelEditRequest;
class AVoxel3DUIActor;
class AVoxelPivotCursorActor;
class UVoxelPlayerStateCacheComponent;
class UVoxelInteractionComponent;
class AVoxelPreviewActor;
class AVoxelSelectionPreviewActor;
class AUVoxelItemBaseInstance;
class AVoxelCutoutBoxActor;
class UStaticMesh;
class UMaterialInterface;

/**
 * 切面区域配置（传递给材质端做 WorldPosition AABB 判定）
 * - AreaMode=0: 全局生效（All 模式）
 * - AreaMode=1: 白名单（PlayerManaged 模式，有自己区域时）
 * - AreaMode=2: 黑名单（PlayerManaged 模式，无自己区域时排除别人的）
 */
USTRUCT(BlueprintType)
struct FSliceAreaConfig
{
	GENERATED_BODY()

	/** 0=全局, 1=白名单, 2=黑名单 */
	UPROPERTY(VisibleAnywhere, Category = "Slice")
	int32 AreaMode = 0;

	/** AABB 列表（最多 8 个，世界空间） */
	UPROPERTY(VisibleAnywhere, Category = "Slice")
	TArray<FBox> AABBs;
};

/**
 * Gizmo 交互类型
 */
UENUM(BlueprintType)
enum class EInteractiveGizmoType : uint8
{
	None UMETA(DisplayName = "None"),
	Translate UMETA(DisplayName = "Translate"),
	Rotate UMETA(DisplayName = "Rotate"),
	Scale UMETA(DisplayName = "Scale"),
};

/**
 * Gizmo 手柄 ID（用于轴向平移手柄）
 */
UENUM(BlueprintType)
enum class EGizmoHandleID : uint8
{
	None UMETA(DisplayName = "None"),
	/** X 轴向手柄 */
	XAxis UMETA(DisplayName = "X Axis"),
	/** Y 轴向手柄 */
	YAxis UMETA(DisplayName = "Y Axis"),
	/** Z 轴向手柄 */
	ZAxis UMETA(DisplayName = "Z Axis"),
};

/**
 * 体素控制器视觉 UI 组件
 *
 * 职责：
 * - 持有和管理所有UI元素（3D UI Actor + 2D UI Widget）
 * - 控制所有Actor本身的渲染变动
 * - 读取业务数据，转换为渲染指令
 * - 调用各UI元素更新渲染
 * - 管理Gizmo显示和交互
 *
 * 不做：
 * - 不处理业务逻辑（由InteractComponent处理）
 * - 不进行射线检测（由蓝图处理）
 * - 不管理业务数据（由StateComponent管理）
 */
UCLASS(ClassGroup=(Custom), BlueprintType, meta=(BlueprintSpawnableComponent))
class VOXEL_API UVoxelPreviewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVoxelPreviewComponent();

	/** 游戏开始时初始化 */
	virtual void BeginPlay() override;

	/** 组件销毁时统一清理所持场景 Actor（防止滞留） */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ==================== 配置属性 ====================

	/** 预览 Actor 类 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Visual|Preview", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AVoxelPreviewActor> PreviewActorClass;

	/** 平移 Gizmo 类 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Visual|Gizmo|Translate", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AVoxel3DUIActor> TranslateGizmoClass;

	// ==================== Gizmo 网格配置 ====================

	/** 平移 Gizmo 网格（3个轴向箭头） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Visual|Gizmo|Translate", meta = (AllowPrivateAccess = "true"))
	UStaticMesh* TranslateGizmoMesh;

	/** 旋转 Gizmo 网格（3个轴向圆环） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Visual|Gizmo|Rotate", meta = (AllowPrivateAccess = "true"))
	UStaticMesh* RotateGizmoMesh;

	/** 面 Gizmo 网格（6个面方向） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Visual|Gizmo|Face", meta = (AllowPrivateAccess = "true"))
	UStaticMesh* FaceGizmoMesh;

	/** Gizmo 手柄大小 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Visual|Gizmo", meta = (AllowPrivateAccess = "true"))
	float GizmoHandleSize = 20.0f;

	// ==================== 核心实例 ====================

	/** 预览 Actor（核心，工具自身预览） */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Voxel|Visual|Preview")
	TObjectPtr<AVoxelPreviewActor> PreviewActor;

	/** 选区预览 Actor（独立，跨工具通用，渲染 CurrentSelection 高亮） */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Voxel|Visual|Preview")
	TObjectPtr<AVoxelSelectionPreviewActor> SelectionPreviewActor;

	/** 上一个被高亮的家具 Actor（用于差异更新，取消旧高亮） */
	UPROPERTY(Transient)
	TWeakObjectPtr<AUVoxelItemBaseInstance> LastHighlightedFurniture;

	/** 游标 Actor（变换原点可视化） */
	UPROPERTY(BlueprintReadOnly, Category = "Voxel|Visual|Preview")
	TObjectPtr<AVoxelPivotCursorActor> CursorActor;

	/**
	 * 地面标记 Actor（独立于建造模式的 CursorActor）
	 * - 始终显示地面点标记（PointMarker），位置 = 鼠标射线命中点
	 * - MMB 按住时显示旋转环标记（RotationRings）
	 * - 同时作为相机 OrbitCamera 的旋转中心点来源
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Voxel|Visual|GroundMarker")
	TObjectPtr<AVoxelPivotCursorActor> GroundMarkerActor;

	// ==================== Gizmo 实例 ====================

	/** 平移 Gizmo 数组（索引对应：0:X, 1:Y, 2:Z） */
	UPROPERTY(BlueprintReadOnly, Category = "Voxel|Visual|Gizmo")
	TArray<TObjectPtr<AVoxel3DUIActor>> TranslateGizmos;

	/** 旋转 Gizmo 数组（索引对应：0:X, 1:Y, 2:Z） */
	UPROPERTY(BlueprintReadOnly, Category = "Voxel|Visual|Gizmo")
	TArray<TObjectPtr<AVoxel3DUIActor>> RotateGizmos;

	/** 面 Gizmo 数组（索引对应：0:-X, 1:+X, 2:-Y, 3:+Y, 4:-Z, 5:+Z） */
	UPROPERTY(BlueprintReadOnly, Category = "Voxel|Visual|Gizmo")
	TArray<TObjectPtr<AVoxel3DUIActor>> FaceGizmos;

	/** 当前激活的 Gizmo 类型 */
	UPROPERTY(BlueprintReadOnly, Category = "Voxel|Visual|Gizmo")
	EInteractiveGizmoType ActiveGizmoType = EInteractiveGizmoType::None;

	/** 当前选中的 Gizmo 手柄 */
	UPROPERTY(BlueprintReadOnly, Category = "Voxel|Visual|Gizmo")
	EGizmoHandleID SelectedGizmoHandle = EGizmoHandleID::None;

	// ==================== 预览更新接口 ====================

	/** 更新单体素预览 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Preview")
	void UpdateSingleVoxelPreview(FIntVector PlaceIndex, FIntVector DeleteIndex, int32 BlockType, const FVoxelEditCommand& EditCommand);

	/** 更新批量编辑预览 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Preview")
	void UpdateBatchEditPreview(const struct FVoxelEditRequest& Request);

	/**
	 * 【已废弃】设置预览状态
	 * @note 保留以兼容旧蓝图调用，内部仅做视觉响应。请直接调用 StateComponent->SetBuildPhase()
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Preview", meta = (DeprecatedFunction, DeprecationMessage = "请直接调用 StateComponent->SetBuildPhase()"))
	void SetPreviewState(EPreviewInteractionState State);

	/** 清除预览 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Preview")
	void ClearPreview();

	// ==================== UI 元素控制 ====================

	/**
	 * 显示旋转环（由蓝图 IA_CameraOrbit Started 调用）
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|GroundMarker")
	void ShowRotationRings();

	/**
	 * 隐藏旋转环（由蓝图 IA_CameraOrbit Completed 调用）
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|GroundMarker")
	void HideRotationRings();

	/**
	 * 获取当前视野中心位置（兼容旧调用方）。
	 * 现轨道旋转直接绕 Pawn，不再使用此值做旋转中心，保留仅为过渡兼容。
	 * @return 视野中心（Pawn）世界坐标
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Visual|GroundMarker")
	FVector GetGroundHitLocation() const { return CachedGroundHitLocation; }

	/** 显示/隐藏平移 Gizmo */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Gizmo")
	void SetTranslateGizmosVisible(bool bVisible);

	/** 重置所有可视化元素 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual")
	void ResetBuildVisualItem();

	/** 取消当前编辑预览 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual")
	void CancelEdit();

	// ==================== Gizmo 控制接口 ====================

	/** 显示平移 Gizmo */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Gizmo")
	void ShowTranslateGizmo(const FVector& Center);

	/** 隐藏平移 Gizmo */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Gizmo")
	void HideTranslateGizmo();

	/** 显示旋转 Gizmo */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Gizmo")
	void ShowRotateGizmo(const FVector& Center);

	/** 隐藏旋转 Gizmo */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Gizmo")
	void HideRotateGizmo();

	/** 显示缩放 Gizmo */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Gizmo")
	void ShowScaleGizmo(const FVector& Center);

	/** 隐藏缩放 Gizmo */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Gizmo")
	void HideScaleGizmo();

	/** 隐藏所有 Gizmo */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Gizmo")
	void HideAllGizmos();

	// ==================== 工具驱动预览 API（供 Tool->UpdatePreview 等调用） ====================

	/** 获取当前激活的工具实例 */
	class UVoxelToolBase* GetCurrentTool() const;

	/** 获取缓存的状态组件 */
	UVoxelPlayerStateCacheComponent* GetStateComponent() const { return CachedStateComponent; }

	/** 获取体素边长 */
	float GetBlockLength() const;

	/** 重置所有预览（切工具时清场：清 PreviewActor + 清选区高亮 + 藏 Gizmo + 藏 Cursor） */
	void ResetAllPreview();

	/** 清除选区高亮 */
	void ClearSelectionHighlight();

	/** 显示当前选区（CurrentSelection）高亮 */
	void ShowCurrentSelection();

	/** 显示当前选区 + 计算的临时选区（CurrentSelection ∪ CalculateSelection）高亮 */
	void ShowSelectionWithCalculated();

	/** 转发状态到 PreviewActor */
	void SetPreviewActorState(EPreviewInteractionState Phase);

	/** 配置 Gizmo 布局（三模块独立开关：整体移动/旋转/面移动） */
	void ConfigureGizmoLayout(EPreviewInteractionState Phase, bool bTranslate, bool bRotate, bool bFace);

	/** 更新家具放置预览（从 RefreshToolPreview 提取） */
	void UpdateFurniturePreview();

	/** 更新模板放置预览（从 RefreshToolPreview 提取） */
	void UpdateTemplatePreview();

	/**
	 * 对新生成的 Actor 应用当前切面参数（spawn hook）
	 * 供 UVoxelItemWorldSubsystem::SpawnItemAtGrid 和 UVoxelCreatureWorldSubsystem::SpawnCreature 调用
	 * 内部读取 StateComponent 当前切面状态并应用到目标 Actor 的所有 MeshComponent 材质
	 * @param TargetActor 新生成的家具/生物 Actor
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Slice")
	void ApplySliceToNewActor(AActor* TargetActor);

	/** 获取当前激活的 Gizmo 类型 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Visual|Gizmo")
	EInteractiveGizmoType GetActiveGizmoType() const { return ActiveGizmoType; }

	/** 获取当前选中的 Gizmo 手柄 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Visual|Gizmo")
	EGizmoHandleID GetSelectedGizmoHandle() const { return SelectedGizmoHandle; }

private:
	// ==================== 组件引用缓存 ====================

	/** 缓存的状态组件 */
	UPROPERTY()
	TObjectPtr<UVoxelPlayerStateCacheComponent> CachedStateComponent;

	// ==================== 视野中心缓存 ====================

	/** 当前视野中心位置（=Pawn 位置，每帧 Tick 更新）。旧名残留，保留兼容。 */
	FVector CachedGroundHitLocation = FVector::ZeroVector;

	// ==================== 初始化方法 ====================

	/** 初始化所有视觉元素 */
	void InitializeVisualElements();

	/** 初始化预览 Actor */
	void InitPreviewActor();

	/** 初始化选区预览 Actor */
	void InitSelectionPreviewActor();

	/** 更新家具选中高亮（差异更新：取消旧高亮，设置新高亮） */
	void UpdateFurnitureSelectionHighlight();

	// ==================== 描边管理 ====================

	/** 设置当前描边 Actor（差异更新：取消旧描边，设置新描边） */
	void SetOutlinedActor(AActor* Actor);

	/** 对 Actor 的所有 MeshComponent 应用 CustomDepth 描边 */
	void ApplyOutlineToActor(AActor* Actor);

	/** 移除 Actor 的所有 MeshComponent 的 CustomDepth 描边 */
	void RemoveOutlineFromActor(AActor* Actor);

	/** 当前被描边的 Actor（CustomDepth + Stencil，用于差异更新） */
	TWeakObjectPtr<AActor> OutlinedActor;

	/** 初始化平移 Gizmo */
	void InitTranslateGizmos();

	/** 初始化旋转 Gizmo */
	void InitRotateGizmos();

	/** 初始化面 Gizmo */
	void InitFaceGizmos();

	/** 绑定事件委托 */
	void BindDelegates(UVoxelPlayerStateCacheComponent* SC);

	// ==================== 辅助工厂与批量操作方法 ====================

	/** 通用 Gizmo 生成函数 */
	AVoxel3DUIActor* CreateGizmoActor(FString NameSuffix, UStaticMesh* Mesh, FRotator InitialRot = FRotator::ZeroRotator);

	/** 批量设置 Gizmo 可见性 */
	void SetGizmoGroupVisibility(TArray<TObjectPtr<AVoxel3DUIActor>>& Group, bool bVisible);

	/** 批量更新 Gizmo 位置 */
	void UpdateGizmoGroupLocation(TArray<TObjectPtr<AVoxel3DUIActor>>& Group, FVector Location);

	// ==================== 事件处理函数 ====================

	/** 全量刷新（仅初始化/模式切换时调用一次；日常变更走按域小函数） */
	UFUNCTION()
	void HandleBuildModeParamsUpdated();

	/** 工具变更：重建模式相关预览 + Gizmo 显隐 + 选区高亮 */
	UFUNCTION()
	void HandleToolChanged();

	/** 几何变更：只重建几何相关预览与选区临时形状渲染 */
	UFUNCTION()
	void HandleGeometryChanged();

	/** 建造阶段变更：只切 Gizmo 显隐 */
	UFUNCTION()
	void HandleBuildPhaseChanged();

	/** 笔刷参数变更 */
	UFUNCTION()
	void HandleBrushChanged();

	/** 选中家具变更：只更新选中描边 */
	UFUNCTION()
	void HandleSelectedFurnitureChanged();

	/** 手持物变更：影响家具/模板/单体素预览 */
	UFUNCTION()
	void HandleItemInHandChanged();

	/** 选区变更（SelectionComponent::OnSelectionChanged） */
	UFUNCTION()
	void HandleSelectionChanged();

	/** 切面高度变化回调（只更新材质，不触发工具逻辑） */
	UFUNCTION()
	void HandleSliceHeightChanged();

	// ==================== 按域渲染刷新 ====================

	/** 全量刷新三件套：RefreshToolPreview + SetPreviewState + UpdateSelectionHighlight（ToolChanged/GeometryChanged 共用） */
	void RefreshAllPreview();

	/** 工具自身预览（单体素/几何/家具/模板），不含选区高亮与 Gizmo 显隐 */
	void RefreshToolPreview();

	/** 选区高亮刷新（本地离散化临时形状，纯只读不回写） */
	void UpdateSelectionHighlight();

	/** 构建模式变更（编辑器模式相关） */
	UFUNCTION()
	void HandleBuildModeChanged(UVoxelInteractionComponent* Interaction);

	/** 当前区域变更 */
	UFUNCTION()
	void HandleCurrentAreaChanged(AVoxelAreaBaseInstance* OldArea, AVoxelAreaBaseInstance* NewArea);

	// ==================== Geometry 操作回调处理 ====================

	/** 处理几何移动 */
	UFUNCTION()
	void HandleMoveGEO(FIntVector MoveDelta);

	/** 处理几何旋转 */
	UFUNCTION()
	void HandleRotateGEO(FRotator RotateDelta);

	/** 处理覆盖范围修改 */
	UFUNCTION()
	void HandleModifyCoverage(EAxis::Type Axis, bool bMaxSide, int32 Delta, float BlockLength);

	/** 处理重新居中 */
	UFUNCTION()
	void HandleRecenter(float BlockLength);

	// ==================== Gizmo 位置更新方法 ====================

	/** 更新平移 Gizmo 位置 */
	void UpdateTranslateGizmosPosition(FVector Center);

	/** 更新旋转 Gizmo 位置和旋转 */
	void UpdateRotateGizmosPosition(FVector Center, FRotator GeometryRotation);

	/** 更新面 Gizmo 位置和旋转 */
	void UpdateFaceGizmosPosition(const FBox& Bounds, FRotator GeometryRotation, FVector GeometryCenter);

	/** 更新所有 Gizmo 位置和旋转 */
	void UpdateAllGizmosPosition(FVector Center, const FBox& Bounds, FRotator GeometryRotation);

	// ==================== 游标 Actor 管理 ====================

	void EnsureCursorActor();

	void UpdateCursorActor(const FVector& PivotWorld, bool bVisible);

	void DestroyCursorActor();

	// ==================== 地面标记 Actor 管理 ====================

	/** 确保地面标记 Actor 已生成 */
	void EnsureGroundMarkerActor();

	/** 销毁地面标记 Actor */
	void DestroyGroundMarkerActor();

	/** 每帧更新视野中心标记（跟随 Pawn 位置 + 浮空落地竖线） */
	void UpdateGroundMarker();

	/** 从 FromWorld 正下方射线找最近地面点（体素地形，ECC_GameTraceChannel1）。
	 *  仅当地面确实在 Pawn 下方（Pawn 浮空）时返回 true。
	 *  @return true=找到下方地面，OutGroundPoint 为命中点 */
	bool FindGroundBelow(const FVector& FromWorld, FVector& OutGroundPoint) const;

	/** 应用切面虚化参数到体素/家具/生物材质 */
	void ApplySliceVisuals();

	/** 遍历家具/生物材质实例，应用切面参数（含区域过滤） */
	void ApplySliceToFurnitureAndCreatures(const FSliceAreaConfig& AreaConfig);

	/**
	 * 对单个 Actor 应用切面参数（用于新生成的家具/生物）
	 * @param Actor 目标 Actor
	 * @param AreaConfig 区域配置
	 * @param SliceHeight 切面世界 Z 高度
	 * @param bEnabled 是否启用切面
	 */
	void ApplySliceToActor(AActor* Actor, const FSliceAreaConfig& AreaConfig, float SliceHeight, bool bEnabled);

	/**
	 * 计算当前切面区域配置
	 * 优先级：1.当前选中区域 -> 2.玩家无父级区域 -> 3.排除别人的区域
	 */
	FSliceAreaConfig ComputeSliceAreaConfig() const;

	// ==================== 透明区域框 ====================

	/** 应用透明框：推有效框集到 BlockSys（地形 Ghost）+ 同步边框 Actor */
	void ApplyCutoutVisuals();

	/** OnCutoutChanged 回调 */
	UFUNCTION()
	void HandleCutoutChanged();

	// ==================== 辅助方法 ====================

	/** 每帧更新 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ==================== 切面动态更新 ====================

	/** 上次切面区域更新时的玩家位置（用于检测移动距离） */
	FVector LastSliceUpdatePos = FVector::ZeroVector;

	/** 上次切面区域更新时间（秒，用于节流） */
	float LastSliceUpdateTime = 0.0f;

	/** 玩家移动超过此距离（cm）时重新计算区域集合 */
	static constexpr float SliceUpdateMoveThreshold = 320.0f; // 约 1 个 ChunkLength

	/** 区域更新节流间隔（秒） */
	static constexpr float SliceUpdateInterval = 0.5f;

	/** Tick 中检查是否需要重新计算区域集合 */
	void CheckSliceAreaUpdate(float DeltaTime);

	// ==================== 透明框边框 Actor 管理 ====================

	/** 框 Id -> 边框 Actor（本玩家私有，不复制） */
	UPROPERTY()
	TMap<int32, AVoxelCutoutBoxActor*> CutoutBoxActors;

	/** 同步边框 Actor 到 BuildStateComp->CutoutBoxes（增/改/删/编辑模式/生效态） */
	void RefreshCutoutBorderActors();
};
