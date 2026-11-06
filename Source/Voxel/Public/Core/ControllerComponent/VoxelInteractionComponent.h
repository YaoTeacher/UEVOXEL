// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "Core/Enum/GameStateEnum.h"
#include "Core/Enum/EditCommandEnums.h"
#include "Core/CoreTags.h"
#include "Voxel/Editor/VoxelToolContext.h"
#include "VoxelInteractionComponent.generated.h"

struct FVoxelGeometryDesc;

class UVoxelPreviewComponent;
class UVoxelControllerHistoryComponent;
class UVoxelPlayerStateCacheComponent;
class UVoxelToolBase;
class UVoxelToolSubsystem;
class AVoxelAreaBaseInstance;
class AVoxelCharacter;
class AVoxel3DUIActor;
class AUVoxelItemBaseInstance;
class UVoxelPrimaryGameLayout;
enum class EActorDirection : uint8;
struct FAreaModificationData;
struct FTemplateMeta;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionControllerModeChanged, UVoxelInteractionComponent*, Interaction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractionToolChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractionBuildPhaseChanged);

/**
 * 交互逻辑组件（挂在 PlayerController 上）——交互流程状态唯一持有者 + 业务门面
 *
 * 职责：
 *  - 交互流程状态：控制器模式（PlayerMode）/ 当前工具 Tag（CurrentToolTag）/ 建造流程阶段（BuildPhase）
 *  - 业务门面：提供业务接口供蓝图和外部调用（状态切换、编辑操作、道具操作、区域操作）
 *  - 统一输入分发：鼠标悬停/按下/拖拽/释放/取消 → 路由到当前工具
 *  - 家具直接操作（长按拾取/拖拽放置状态机）
 *  - Gizmo 交互（平移、旋转、面推拉）
 *  - 撤销/重做（委托 HistoryComponent）
 *
 * 依赖：
 *  - UVoxelPlayerStateCacheComponent：状态数据唯一持有者（本组件只持流程状态，副作用经其业务方法落地）
 *  - UVoxelPreviewComponent：预览可视化
 *  - UVoxelControllerHistoryComponent：编辑历史
 *  - UVoxelToolSubsystem（GameInstance）：全局工具注册表
 */
UCLASS(ClassGroup=(Voxel), BlueprintType, meta=(BlueprintSpawnableComponent))
class VOXEL_API UVoxelInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVoxelInteractionComponent();

	// ==================== 控制器模式 ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|Interact|State")
	EVoxelControllerMode GetControllerMode() const { return PlayerMode; }

	/** 设置控制器模式（建造模式联动默认工具 Tag，变化才广播） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
	void SetControllerMode(EVoxelControllerMode Mode);

	// ==================== 工具 Tag ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|Interact|State")
	FGameplayTag GetCurrentToolTag() const { return CurrentToolTag; }

	/**
	 * 设置当前工具 Tag（含切换业务规则）：
	 *  - 离开选区工具族时清空体素选区（切到选区/编辑工具不清空）
	 *  - 进入 Brush/单体素工具且无形状时初始化默认 Box
	 *  - 重置 BuildPhase 为 IDLE，清空 Rotation（Box 保留供 M4 读取）
	 *  - 进入区域编辑工具且未选中区域时初始化临时区域默认值
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
	void SetCurrentToolTag(FGameplayTag ToolTag);

	// ==================== 建造流程阶段 ====================

	UFUNCTION(BlueprintPure, Category = "Voxel|Interact|State")
	EPreviewInteractionState GetBuildPhase() const { return BuildPhase; }

	/** 设置建造流程阶段（变化才广播 OnBuildPhaseChanged） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
	void SetBuildPhase(EPreviewInteractionState NewPhase);

	// ==================== 事件 ====================

	UPROPERTY(BlueprintAssignable, Category = "Events|Interact")
	FOnInteractionControllerModeChanged OnControllerModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events|Interact")
	FOnInteractionToolChanged OnToolChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events|Interact")
	FOnInteractionBuildPhaseChanged OnBuildPhaseChanged;

	// ==================== 状态切换 ====================

	/** 切换工具类型 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
	void SwitchTool(FGameplayTag NewToolTag);

	/** 设置批量形状 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
	void SetBatchShape(EBatchShapeType NewShape);

	/** 设置笔刷大小 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
	void SetBrushSize(int32 NewSize);

	/** 设置笔刷维度（平面/立体） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
	void SetBrushDimension(EBrushDimension NewDimension);

	/** 设置放置规则 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
	void SetPlacementMode(EVoxelPlacementMode NewMode);

	/** 设置选区运算模式 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
	void SetSelectionMode(ESelectionOpMode NewMode);

	/**
	 * 平移整个选区数据（Gizmo 拖拽调用）
	 * @param Offset 体素索引偏移量
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Selection")
	void MoveSelection(const FIntVector& Offset);

	/**
	 * 选区工具松手时立即应用临时形状到 CurrentSelection（跳过 EDIT/CONFIRM）
	 * 类似 MagicaVoxel 的松手即用模式
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Selection")
	void ApplySelectionShapeImmediately();

	/**
	 * 在已有选区内执行编辑操作（选区约束模式）
	 * 委托给 UVoxelSelectionEditTool::ExecuteWithinSelection
	 * @param EditRequest 编辑请求（包含 BlockType、PlacementMode 等）
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Selection")
	void ApplyEditWithinSelection(const struct FVoxelEditRequest& EditRequest);

	// ==================== 剪切板操作（委托给 UVoxelSelectionEditTool） ====================

	/** 复制选区内容到剪切板 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Clipboard")
	void CopySelection();

	/** 剪切选区（复制 + 删除原内容） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Clipboard")
	void CutSelection();

	/** 粘贴剪切板内容到指定位置 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Clipboard")
	void PasteSelection(const FIntVector& TargetPosition);

	/** 粘贴剪切板内容到鼠标命中位置 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Clipboard")
	void PasteAtMousePosition();

	/**
	 * 移动选区内的实际数据（方块 + 家具）
	 * 委托给 UVoxelSelectionEditTool::MoveSelectionContent
	 * @param Offset 体素索引偏移量
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Selection")
	void MoveSelectionContent(const FIntVector& Offset);

	/** 设置玩家模式 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
	void SetPlayerMode(EVoxelControllerMode NewMode);

	/** 设置游戏权限 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
	void SetGameAuthority(EVoxelGameAuthority NewAuthority);

	// ==================== 编辑操作 ====================

	/** 释放批量编辑预览 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Edit")
	void ReleaseBatchEditPreview();

	/** 取消当前编辑 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Edit")
	void CancelCurrentEdit();

	// ==================== 道具操作 ====================

	/** 设置手持道具 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Item")
	void SetItemInHand(EInteractableType Type, int32 Id, int Quantity);

	/** 设置视线中的道具 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Item")
	void SetItemInSight(EInteractableType Type, int32 Id, int Quantity);

	/** 选中/取消选中家具（nullptr 取消；写入 State 并广播，R/Delete 快捷键依赖此选中态） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Furniture")
	void SelectFurniture(AUVoxelItemBaseInstance* Furniture);

	/** 更新家具放置位置 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Item")
	void UpdateFurniturePosition(FIntVector Position);

	/** 旋转家具 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Item")
	void RotateFurniture(EActorDirection Direction);

	/** 获取家具边界 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Item")
	FBox GetFurnitureBounds(FIntVector Position, EActorDirection Direction);

	/**
	 * 统一使用手持物品入口（战斗/通用使用管线）
	 *
	 * 根据手持物品的 ItemTag 路由到对应的放置/使用逻辑，不依赖 ToolTag。
	 * 用于战斗模式、热键快速使用等无编辑工具场景；编辑器内亦可调用。
	 *
	 * 路由规则（按 ItemTag）：
	 * - Item.Type.Block     → 单方块放置（含历史记录，支持撤销）
	 * - Item.Type.BuildItem → 家具放置（委托 UVoxelFurnitureEditTool::PlaceFurniture）
	 * - Item.Type.Template  → 模板放置（委托 UVoxelTemplateEditTool::PlaceTemplate）
	 *
	 * @param TargetWorldPosition 目标世界坐标（通常为射线命中点）
	 * @param TargetNormal 目标表面法线（用于计算放置偏移，方块沿法线方向放置到相邻格）
	 * @return true 使用成功
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Item")
	bool UseItemInHand(FVector TargetWorldPosition, FVector TargetNormal);

	// ==================== 区域操作 ====================

	/** 选择区域（nullptr 表示取消选择） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
	void SelectArea(AVoxelAreaBaseInstance* Area);

	/** 清空 state 当前选中区域（真清空不回退层级；UI 关闭详情等入口调用） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
	void DeselectArea();

	/** 生成或更新当前区域 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
	bool CommitArea();

	/** 设置区域类型 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
	void SetAreaType(FGameplayTag AreaTypeTag);

	/** 进入子区域 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
	void EnterSubArea();

	/** 返回父区域 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
	void ReturnToParentArea();

	// ==================== 编辑流程控制 ====================

	/** 回退一步（EDIT→BASE→HIDDEN） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Edit")
	void StepBack();

	// ==================== 统一输入接口（蓝图唯一调用点） ====================

	/**
	 * 统一鼠标悬停处理 - 每帧由蓝图 Tick 调用
	 * 不执行任何编辑操作，仅更新视觉预览
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Input")
	void HandleMouseHover();

	/**
	 * 统一鼠标输入处理 - 按下时触发一次
	 * 用于设置初始状态（单体素放置、框选起点、Gizmo 开始拖拽）
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Input")
	void HandleMouseInput();

	/**
	 * 统一鼠标拖拽处理 - 按住期间由 IA Triggered 每帧调用
	 * 用于两点编辑 PLACING 阶段持续更新第二点（单体素/笔刷不响应）
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Input")
	void HandleMouseDrag();

	/**
	 * 公共鼠标场景射线：Deproject + 地面射线 + PLACING 相机平面回退 + EDITING Gizmo 射线
	 * HandleMouseInput / HandleMouseDrag 共用，消除 45 行重复代码块。
	 * @param PC 玩家控制器
	 * @param OutGroundHit 输出的地面命中（PLACING 回退时填充投影点）
	 * @param OutGizmoActor 输出的 Gizmo 命中 Actor（EDITING 阶段可能非空）
	 * @return 是否有有效命中（地面命中 || Gizmo 命中 || 正在拖拽 Gizmo）
	 */
	bool TraceMouseScene(APlayerController* PC, FHitResult& OutGroundHit, AVoxel3DUIActor*& OutGizmoActor);

	/**
	 * 统一鼠标输入处理 - 释放
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Input")
	void HandleMouseRelease();

	/**
	 * 统一鼠标右键处理 - 取消当前操作
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Input")
	void HandleMouseCancel();

	/**
	 * 键盘按键处理 - 透传给当前工具的 OnKeyDown
	 * 业务逻辑由蓝图通过单独的方法调用（DeleteSelectedFurniture/Undo/Redo/HandleMouseCancel）
	 * @param Key 按键
	 * @return true 如果工具处理了该按键
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Input")
	bool HandleKeyDown(const FKey& Key);

	/**
	 * 删除当前选中的家具（记录历史，可撤销）
	 * @return true 如果成功删除
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Input")
	bool DeleteSelectedFurniture();

	// ==================== Gizmo 操作（内部使用） ====================

	/**
	 * 处理 Gizmo 交互（统一的 Gizmo 操作入口）
	 * @param Input 3D UI Actor 指针（用于识别是哪个 Gizmo，可为 nullptr）
	 * @param WorldPosition 世界坐标位置
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Gizmo")
	void HandleGizmoInteraction(AVoxel3DUIActor* Input, FVector WorldPosition);

	/** 释放 Gizmo 交互（清空 FirstPoint/SecondPoint） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Gizmo")
	void ReleaseGizmoInteraction();

	/** 设置临时区域位置 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
	void SetTempAreaPosition(FBox Bounds);

	/** 清除区域信息 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
	void ClearAreaInfo();

	// ==================== 几何操作 ====================

	/** 设置几何旋转 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Geometry")
	void SetGeometryRotation(FRotator NewRotation);

	/** 设置几何厚度 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Geometry")
	void SetGeometryThickness(int32 NewThickness);

	/** 设置规则几何模式 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Geometry")
	void SetRegularGeometry(bool bEnable);

	/**
	 * 设置完整临时几何信息（用于 UI 模块批量更新 Box/Rotation/Thickness 等字段）
	 * @param NewGeometry 新的几何描述
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Geometry")
	void SetTmpGeometry(const FVoxelGeometryDesc& NewGeometry);

	/** 设置单个模板素材替换（NewFullId == OriginFullId 时移除该项），触发 OnGeometryChanged 刷新预览 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Template")
	void SetTemplateRemapEntry(const FString& OriginFullId, const FString& NewFullId);

	// ==================== 区域修改 ====================

	/** 修改当前区域 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
	void ModifyCurrentArea(FAreaModificationData& NewData);

	/** 修改区域（优先当前区域，否则更新临时区域） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
	void ModifyArea(FAreaModificationData& NewData);

	/** 更新 STATE 暂存数据（不触碰 AREA 实例） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
	void UpdateTmpAreaInfo(FAreaModificationData& NewData);

	// ==================== 模板操作 ====================

	/**
	 * 将当前选区捕获为模板并落盘注册（模板保存弹窗的唯一后端入口）
	 * 内部：捕获选区 → 存 Saved/Templates/<Guid>.tmpl → RegisterRuntimeTemplate → 广播刷新模板库
	 * @param Meta 模板元信息（名称/描述/作者/分类 Tag）
	 * @return true 捕获并注册成功
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Template")
	bool SaveSelectionAsTemplate(const FTemplateMeta& Meta);

	// ==================== 切面 / Pawn 高度 / 渲染图层（UI 参数写入） ====================

	/** 设置编辑切面层数（Clamp + 变化才广播 OnSliceChanged） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Slice")
	void SetEditSliceLayer(int32 NewLayer);

	/** 启用/禁用切面虚化 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Slice")
	void SetSliceEnabled(bool bEnabled);

	/** 设置切面过滤模式 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Slice")
	void SetSliceFilterMode(ESliceFilterMode NewMode);

	/** 设置 Pawn 高度（体素层，Clamp + 变化才广播 OnPawnHeightChanged） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Pawn")
	void SetPawnHeightLayer(int32 NewLayer);

	/** 开关渲染图层（多选，变化才广播 OnLayersChanged） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Layer")
	void SetLayerActive(FGameplayTag LayerTag, bool bActive);

	// ==================== 透明区域框（UI 参数写入） ====================

	/** 切换单个框生效/失效（不入历史，视图偏好） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Cutout")
	void SetBoxEnabled(int32 BoxId, bool bEnabled);

	/** 全部框生效/失效（不入历史，一次广播） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Cutout")
	void SetAllCutoutBoxesEnabled(bool bEnabled);

	/** 删除框（HistoryComponent 由本组件内部查找并记录历史） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Cutout")
	void RemoveCutoutBox(int32 BoxId);

	/** 定位到框：Pawn 移到框中心上方。返回框是否存在 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Cutout")
	bool FocusCutoutBox(int32 BoxId);

	// ==================== 历史记录操作 ====================

	/** 撤销上一次编辑 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|History")
	bool Undo();

	/** 重做上次撤销的操作 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|History")
	bool Redo();

	/** 清空编辑历史 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|History")
	void ClearHistory();

	// ==================== 编辑器接口 ====================

	/** 确认编辑（鼠标左键点击时调用） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Edit", meta = (AllowPrivateAccess = "true"))
	void RequestConfirmVoxelEdit();

	/** 取消编辑（鼠标右键点击时调用） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Edit", meta = (AllowPrivateAccess = "true"))
	void RequestCancelVoxelEdit();

	// ==================== 区域生成 ====================

	/**
	 * 区域建墙桥接（M9 桥接按钮触发）
	 * 把当前选中区域的包围盒装配为几何体编辑请求（盒体+内壳+默认四面墙），
	 * 并切换到几何体编辑工具；墙壁厚度/面/方块类型在几何体编辑侧调整与确认
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
	void BeginWallShellFromArea();

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==================== 工具系统（公共访问） ====================

	/** 获取当前激活的工具实例（按 CurrentToolTag 查全局注册表） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Tool")
	UVoxelToolBase* GetCurrentTool() const;

	/** 组装当前工具上下文（供工具访问各组件与输入状态） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Tool")
	FVoxelToolContext BuildToolContext();

	/** 获取输入状态引用（工具通过 Context 间接访问，此处供内部使用） */
	FVoxelInputState& GetInputState() { return InputState; }
	const FVoxelInputState& GetInputState() const { return InputState; }

	/**
	 * 获取最近一次 Hover 地面射线命中（由 50ms 统一射线定时器缓存）。
	 * VisualUIComponent 的 GroundMarker 复用此结果，避免重复追踪同一通道。
	 */
	bool GetLatestGroundHit(FVector& OutLocation) const
	{
		OutLocation = LatestGroundHitLocation;
		return bLatestGroundHitValid;
	}

	/** 获取最近一次地面命中法线 */
	bool GetLatestGroundHitNormal(FVector& OutNormal) const
	{
		OutNormal = LatestGroundHitNormal;
		return bLatestGroundHitValid;
	}

	/**
	 * 将鼠标射线投影到面向相机的虚拟平面上（经过几何体中心）。
	 * 替代 HeightCollisionVisual 的碰撞检测职能，纯数学计算无物理碰撞体。
	 * @param RayOrigin  射线起点（鼠标世界位置）
	 * @param RayDir     射线方向（鼠标世界方向）
	 * @param OutPoint   输出交点
	 * @return true=成功求交
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Input", meta = (AllowPrivateAccess = "true"))
	bool ProjectMouseToCameraPlane(const FVector& RayOrigin, const FVector& RayDir, FVector& OutPoint) const;

	/**
	 * 家具放置成功回调（由 VoxelFurnitureEditTool 放置成功后调用）
	 * 如果当前是从拾取的家具放置，销毁原Actor + 清空手持 + 回到IDLE
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Furniture", meta = (AllowPrivateAccess = "true"))
	void OnFurniturePickedUpPlaced();

protected:
	virtual void BeginPlay() override;

	// ==================== 组件引用缓存 ====================

	/** 缓存的状态缓存组件引用 */
	UPROPERTY()
	TObjectPtr<UVoxelPlayerStateCacheComponent> CachedStateCache;

	/** 缓存的可视化 UI 组件引用 */
	UPROPERTY()
	TObjectPtr<UVoxelPreviewComponent> CachedPreviewComponent;

	/** 缓存的历史记录组件引用 */
	UPROPERTY()
	TObjectPtr<UVoxelControllerHistoryComponent> CachedHistoryComponent;

	// ==================== 组件访问器 ====================

	/** 获取状态缓存组件（状态数据唯一持有者） */
	UVoxelPlayerStateCacheComponent* GetStateCache();
	UVoxelPlayerStateCacheComponent* GetStateCache() const;

	/** 获取可视化 UI 组件 */
	class UVoxelPreviewComponent* GetPreviewComponent() const;

	/** 获取历史记录组件 */
	class UVoxelControllerHistoryComponent* GetHistoryComponent() const;

private:
	// ==================== 工具系统 ====================

	/**
	 * 取全局工具注册表（UVoxelToolSubsystem，GameInstance 子系统）
	 * 工具实例由 GameInstance 子系统统一持有（Initialize 先于一切 World/Controller），
	 * 组件不再自建注册表
	 */
	class UVoxelToolSubsystem* GetToolSubsystem() const;

	// ==================== 输入状态存储 ====================

	/** 射线检测距离（cm），蓝图可配置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Input", meta = (AllowPrivateAccess = "true"))
	float TraceDistance = 5000.0f;

	/** 统一输入状态（替代原 FirstPoint/SecondPoint/bIsModyfing 等散落成员） */
	FVoxelInputState InputState;

	/** 最近一次地面射线命中点缓存（HandleMouseHover 写入，供 GroundMarker/Brush 复用） */
	FVector LatestGroundHitLocation = FVector::ZeroVector;
	FVector LatestGroundHitNormal = FVector::UpVector;
	bool bLatestGroundHitValid = false;

	/** 统一鼠标射线定时器（50ms 驱动 HandleMouseHover，替代蓝图 Tick 调用） */
	FTimerHandle MouseRayTimerHandle;

	/** 统一鼠标射线间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Input", meta = (AllowPrivateAccess = "true"))
	float MouseRayInterval = 0.05f;

	/** 是否显示交互射线调试 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Debug", meta = (AllowPrivateAccess = "true"))
	bool bShowDebugTrace = false;

	// ==================== 家具直接操作状态 ====================

	/**
	 * 家具拾取/选中状态（编辑模式下直接操作已放置家具）
	 *
	 * 流程（无状态机）：
	 *   1. 鼠标按下命中家具 → bPressed=true，记录 PressTime
	 *   2. 长按达到 FurnitureDragDelay → PickupFurniture()：设手持数据 + 隐藏原Actor + bPickedUp=true + bPressed=false
	 *   3. 之后走正常 PLACING 流程（OnMouseHover 跟随鼠标，点击放置）
	 *   4. 放置成功 → OnFurniturePickedUpPlaced()：销毁原Actor + 清空手持 + 回到 IDLE
	 *   5. 右键取消 → 恢复原Actor显示 + 清空手持 + 回到 IDLE
	 */
	struct FFurnitureDragState
	{
		/** 被拾取的原始 Actor（放置时销毁，取消时恢复显示） */
		TWeakObjectPtr<AUVoxelItemBaseInstance> Actor;
		/** 鼠标按下时间（用于长按判定） */
		float PressTime = 0.f;
		/** 鼠标是否按下（长按检测期间为 true，拾取后置 false） */
		bool bPressed = false;
		/** 家具是否已拾取（手持数据已设置，原Actor已隐藏，走正常PLACING流程） */
		bool bPickedUp = false;
	} FurnitureDragState;

	/** 长按阈值（秒），超过此时间未松手则拾取家具 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Input", meta = (AllowPrivateAccess = "true"))
	float FurnitureDragDelay = 0.5f;

	/** 长按进度更新定时器（FTimerHandle 驱动，替代 Tick 轮询） */
	FTimerHandle FurnitureDragTimerHandle;

	/** 获取家具长按进度（0~1），供UI显示环形进度条 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Input")
	float GetFurnitureDragProgress() const;

	/** 是否正在长按家具（用于UI显示环形进度条） */
	UFUNCTION(BlueprintPure, Category = "Voxel|Input")
	bool IsFurnitureDragPressing() const;

	// ==================== 内部辅助方法 ====================

	/** 获取主 UI 布局（内部处理 PC 获取与判空），消除 8 处 PC+Layout 双判空模板 */
	UVoxelPrimaryGameLayout* GetPrimaryLayout() const;

	/** 射线检测家具 Actor（BuildItem 通道） */
	AUVoxelItemBaseInstance* TraceFurniture();

	/** 处理家具单击查看（短按松手） */
	void HandleFurnitureInspect(AUVoxelItemBaseInstance* Furniture);

	/** 长按达到阈值时拾取家具：设手持数据 + 隐藏原Actor + bPickedUp=true */
	void PickupFurniture();

	/** 家具长按定时器回调（长按检测 → 拾取），由 FTimerHandle 驱动 */
	void HandleFurnitureDragTimer();

	/** 启动家具长按定时器（按下命中家具时调用） */
	void StartFurnitureDragTimer();

	/** 停止家具长按定时器（松手/拾取/取消时调用） */
	void StopFurnitureDragTimer();

	/** 取消家具拾取（右键）：恢复原Actor显示 + 清空手持 + 回到IDLE */
	void CancelFurnitureDrag();

	/** Gizmo 操作：平移（薄分发到当前工具的 OnGizmoTranslate） */
	void HandleGizmo_Translate(AVoxel3DUIActor* Input, int AxisIndex);

	/** Gizmo 操作：旋转（纯几何数学，与工具无关，保留） */
	void HandleGizmo_Rotate(AVoxel3DUIActor* Input, int32 AxisIndex);

	/** Gizmo 操作：面推拉（纯几何数学，与工具无关，保留） */
	void HandleGizmo_Face(AVoxel3DUIActor* Input, int FaceIndex);

	/**
	 * 按 Gizmo 所属数组分发到对应的 HandleGizmo_Translate/Rotate/Face
	 * HandleGizmoInteraction 的持续拖拽与激活首帧共用。
	 * @return 是否分发成功（gizmo 属于三数组之一）
	 */
	bool DispatchGizmoAction(AVoxel3DUIActor* Gizmo, int32 Index);

	// ==================== 交互流程状态（唯一持有） ====================

	/** 控制器模式（SceneBuildMode / GamePlayMode 等） */
	UPROPERTY(VisibleInstanceOnly, Category = "State")
	EVoxelControllerMode PlayerMode = EVoxelControllerMode::SceneBuildMode;

	/** 当前工具 Tag（空 = 非建造态无工具） */
	UPROPERTY(VisibleInstanceOnly, Category = "State")
	FGameplayTag CurrentToolTag;

	/** 当前建造流程阶段（IDLE/EDITING/CONFIRM） */
	UPROPERTY(VisibleInstanceOnly, Category = "State|Edit|Preview")
	EPreviewInteractionState BuildPhase = EPreviewInteractionState::IDLE;
};
