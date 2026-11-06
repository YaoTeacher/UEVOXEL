// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Voxel/Editor/VoxelPreviewActor.h"
#include "Voxel/Editor/VoxelPlacementTypes.h"  // FPlacementHandle / FPlacementModification
#include "Core/Enum/GameStateEnum.h"  // EActorDirection
#include "VoxelToolBase.generated.h"

class UVoxelPlayerStateCacheComponent;
class UVoxelControllerHistoryComponent;
class UVoxelPlacementWorldSubsystem;
class UTexture2D;
class UVoxelPreviewComponent;

struct FVoxelToolContext;
struct FHitResult;
class AVoxel3DUIActor;

/**
 * 工具分类（用于 UI 分组）
 */
UENUM(BlueprintType)
enum class EVoxelToolCategory : uint8
{
	/** 编辑工具：添加、删除、替换方块 */
	Edit UMETA(DisplayName = "Edit Tools"),

	/** 选择工具：框选、刷选等 */
	Selection UMETA(DisplayName = "Selection Tools"),

	/** 变换工具：移动、旋转、缩放 */
	Transform UMETA(DisplayName = "Transform Tools"),

	/** 高级工具：复制、粘贴、镜像等 */
	Advanced UMETA(DisplayName = "Advanced Tools"),

	/** 其他工具 */
	Misc UMETA(DisplayName = "Misc Tools")
};

/**
 * 体素编辑工具基类
 *
 * 设计原则：
 * - 提供统一的工具接口和管理系统
 * - 支持工具元数据（图标、本地化、描述）
 * - 分离编辑逻辑和预览逻辑
 * - 支持工具生命周期管理
 *
 * 使用方式：
 * 1. 继承此类创建新工具
 * 2. 重写虚函数实现工具逻辑
 * 3. 在 UVoxelInteractionComponent::RegisterTools 中按 GameplayTag 注册
 *
 * @note 所有工具类都应该是无状态的，或状态存储在 StateComponent 中
 */
UCLASS(BlueprintType, Abstract, DefaultToInstanced, EditInlineNew)
class VOXEL_API UVoxelToolBase : public UObject
{
	GENERATED_BODY()

public:
	// ==================== 元数据接口（UI 使用） ====================

	/**
	 * 获取工具的显示名称（本地化）
	 * @return 工具显示名称，如 "批量编辑"、"选择工具" 等
	 */
	virtual FText GetToolDisplayName() const
	{
		return FText::FromString(TEXT("Unnamed Tool"));
	}

	/**
	 * 获取工具的详细描述（本地化）
	 * @return 工具描述，解释工具的用途和使用方法
	 */
	virtual FText GetToolDescription() const
	{
		return FText::FromString(TEXT("No description available."));
	}

	/**
	 * 获取工具的 UI 图标
	 * @return 图标资源，如果没有图标则返回 nullptr
	 */
	virtual UTexture2D* GetToolIcon() const
	{
		return nullptr;
	}

	/**
	 * 获取工具的分类
	 * @return 工具所属的分类，用于 UI 分组
	 */
	virtual EVoxelToolCategory GetToolCategory() const
	{
		return EVoxelToolCategory::Edit;
	}

	/**
	 * 获取工具的唯一标识符
	 * @return 工具 ID，用于保存设置和快捷键绑定
	 */
	virtual FName GetToolID() const
	{
		return FName(*GetClass()->GetName());
	}

	/**
	 * 获取工具的快捷键提示文本
	 * @return 快捷键提示，如 "Ctrl+B"
	 */
	virtual FText GetShortcutHint() const
	{
		return FText::GetEmpty();
	}

	// ==================== 面板需求声明（工具驱动 UI 显隐） ====================

	/** 工具是否需要库面板（ToolLibraryPanel：建造库/模板库/区域类型库）。默认 false，需要库的工具在构造函数中开启 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool|UI")
	bool bNeedsLibraryPanel = false;

	/** 工具是否需要详情面板（ToolDetailPanel：参数配置模块容器）。默认 false，需要配置的工具在构造函数中开启 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool|UI")
	bool bNeedsDetailPanel = false;

	// ==================== 工具生命周期 ====================

	/**
	 * 工具初始化
	 * 当工具被激活时调用
	 * @param StateComponent 控制器状态组件（可读取当前编辑状态）
	 */
	virtual void InitializeTool(UVoxelPlayerStateCacheComponent* StateComponent)
	{
		// 子类可重写以实现自定义初始化逻辑
	}

	/**
	 * 工具清理
	 * 当工具被停用时调用
	 */
	virtual void ShutdownTool()
	{
	}

	// ==================== 编辑逻辑（核心业务） ====================

	/**
	 * 执行编辑操作
	 * @param Request 编辑请求（包含几何信息、编辑模式、方块类型等）
	 * @return true 成功执行，false 执行失败
	 */
	virtual bool ExecuteEdit(const FVoxelEditRequest& Request) PURE_VIRTUAL(UVoxelToolBase::ExecuteEdit, return false;);

	/**
	 * 验证编辑参数是否有效
	 * @param Request 待验证的编辑请求
	 * @return true 参数有效，false 参数无效
	 */
	virtual bool ValidateEditRequest(const FVoxelEditRequest& Request) const
	{
		return Request.Geometry.IsValid();
	}

	/**
	 * 检查工具是否可以执行编辑
	 * @return true 可以执行，false 不可以执行
	 */
	virtual bool CanExecuteEdit() const
	{
		return true;
	}

	/**
	 * 检查编辑是否需要用户确认
	 * @return true 需要确认对话框，false 立即执行
	 */
	virtual bool RequiresConfirmation() const
	{
		return false;
	}

	/**
	 * 获取确认对话框的提示文本
	 * @param Request 待确认的编辑请求
	 * @return 确认对话框的提示文本
	 */
	virtual FText GetConfirmationText(const FVoxelEditRequest& Request) const
	{
		return FText::Format(
			NSLOCTEXT("VoxelTools", "ConfirmEdit", "确定要执行 {0} 吗？"),
			Request.GetDescription()
		);
	}

	// ==================== 输入生命周期（工具子类重写） ====================

	/**
	 * 鼠标悬停处理（每帧由 InteractionComp Tick 调用）
	 * 仅更新视觉预览，不执行编辑操作
	 * @param Context 工具上下文
	 * @param GroundHit 地面射线命中结果（已由 InteractionComp 计算）
	 */
	virtual void OnMouseHover(const FVoxelToolContext& Context, const FHitResult& GroundHit) {}

	/**
	 * 鼠标按下处理（按下时触发一次）
	 * 用于设置初始状态、开始拖拽、执行单次编辑
	 * @param Context 工具上下文
	 * @param GroundHit 地面命中结果（可能无效，bBlockingHit=false）
	 * @param GizmoHitActor Gizmo 命中的 Actor（可为 nullptr）
	 */
	virtual void OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor) {}

	/**
	 * 鼠标按住拖拽处理（按住期间由 IA Triggered 每帧调用）
	 * 用于两点编辑 PLACING 阶段持续更新第二点。单体素/笔刷工具不实现，基类默认
	 * 调用 HandleTwoPointDragInput（非 PLACING 阶段为空操作）。
	 * @param Context 工具上下文
	 * @param GroundHit 地面命中结果（可能无效，bBlockingHit=false）
	 * @param GizmoHitActor Gizmo 命中的 Actor（可为 nullptr）
	 */
	virtual void OnMouseDrag(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor);

	/**
	 * 鼠标释放处理
	 */
	virtual void OnMouseRelease(const FVoxelToolContext& Context) {}

	/**
	 * 鼠标右键取消处理
	 * 默认实现：EDIT→BASE→HIDDEN 逐级回退
	 * - EDIT：CancelCurrentEdit + SetBuildPhase(BASE)
	 * - BASE：CancelCurrentEdit + SetBuildPhase(HIDDEN)
	 * - HIDDEN：无操作（子类可 override 实现工具特定行为，如清空选区）
	 * 无状态机的工具（SingleVoxel/Brush）应 override 跳过此逻辑。
	 */
	virtual void OnMouseCancel(const FVoxelToolContext& Context);

	/**
	 * 确认编辑（蓝图调用 RequestConfirmVoxelEdit 时触发）
	 */
	virtual void OnConfirmEdit(const FVoxelToolContext& Context) {}

	/**
	 * 键盘按键处理（R=旋转, Delete=删除选中, Escape=退出工具）
	 * @param Context 工具上下文
	 * @param KeyName 按键名称（"R"/"Delete"/"Escape" 等）
	 * @return true 如果工具处理了该按键，false 如果未处理
	 */
	virtual bool OnKeyDown(const FVoxelToolContext& Context, const FName& KeyName) { return false; }

	/**
	 * Gizmo 平移处理（EDIT 状态下拖拽平移 Gizmo）
	 * @param Context 工具上下文
	 * @param Offset 体素索引偏移量（单轴）
	 */
	virtual void OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset) {}

	// ==================== 预览驱动（工具主动配置 PreviewComponent） ====================

	/** 配置 PreviewActor 内容（预览构建/清空）。由 PreviewComponent::RefreshToolPreview 委托。 */
	virtual void UpdatePreview(UVoxelPreviewComponent* PreviewComp);

	/** 配置选区高亮（显示/清除）。由 PreviewComponent::UpdateSelectionHighlight 委托。 */
	virtual void UpdateSelectionHighlight(UVoxelPreviewComponent* PreviewComp);

	/** 配置 Gizmo（translate/rotate/face 显隐）。由 PreviewComponent::SetPreviewState 委托。 */
	virtual void ConfigureGizmos(UVoxelPreviewComponent* PreviewComp, EPreviewInteractionState Phase);

	// ==================== 状态机（两点编辑流程收编） ====================

	/**
	 * 两点编辑状态机（IDLE→PLACING→EDITING）模板方法
	 * 收编原 SimpleGeometryEditTool/SelectionShapeTool/AreaEditTool 各自重复实现的流程：
	 * - EDITING：命中 Gizmo → 转发 InteractionComp::HandleGizmoInteraction（激活拖拽）
	 * - IDLE：记录 FirstPoint/SecondPoint + SetEditPoints → SetBuildPhase(PLACING)
	 * - PLACING：拖拽更新 SecondPoint + SetEditPoints
	 * @param Context 工具上下文
	 * @param GroundHit 地面命中结果
	 * @param GizmoHitActor Gizmo 命中的 Actor（可为 nullptr）
	 * @return true=已处理（调用方不再继续），false=未处理
	 */
	bool HandleTwoPointPhaseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor);

	/**
	 * 两点编辑 PLACING 阶段拖拽更新（由 OnMouseDrag 调用）
	 * 仅在 BuildPhase==PLACING 时更新 SecondPoint，其它阶段为空操作。
	 * @param Context 工具上下文
	 * @param GroundHit 地面命中结果
	 * @return true=已处理（PLACING 阶段），false=未处理
	 */
	bool HandleTwoPointDragInput(const FVoxelToolContext& Context, const FHitResult& GroundHit);

	/**
	 * 工具是否能处理该 ItemTag（用于 UseItemInHand 路由）
	 * 默认 false，由具体工具重写
	 */
	virtual bool CanHandleItem(const FGameplayTag& ItemTag) const { return false; }

	/**
	 * 使用手持物品（按 ItemTag 路由入口）
	 * @param Context 工具上下文
	 * @param TargetWorldPosition 目标世界坐标
	 * @param TargetNormal 目标表面法线
	 * @return true 使用成功
	 */
	virtual bool UseItemInHand(const FVoxelToolContext& Context, FVector TargetWorldPosition, FVector TargetNormal) { return false; }

	// ==================== 工具状态 ====================

	/**
	 * 检查工具当前是否激活
	 * @return true 工具激活，false 工具未激活
	 */
	bool IsToolActive() const
	{
		return bIsActive;
	}

	/**
	 * 设置工具的激活状态
	 * @param bActive 是否激活
	 */
	void SetToolActive(bool bActive)
	{
		if (bIsActive != bActive)
		{
			bIsActive = bActive;

			if (bActive)
			{
				OnToolActivated();
			}
			else
			{
				OnToolDeactivated();
			}
		}
	}

	/**
	 * 工具激活回调
	 * 当工具被激活时调用（虚函数，子类可重写）
	 */
	virtual void OnToolActivated()
	{
		UE_LOG(LogTemp, Log, TEXT("Tool '%s' activated"), *GetToolDisplayName().ToString());
	}

	/**
	 * 工具停用回调
	 * 当工具被停用时调用（虚函数，子类可重写）
	 */
	virtual void OnToolDeactivated()
	{
		UE_LOG(LogTemp, Log, TEXT("Tool '%s' deactivated"), *GetToolDisplayName().ToString());
		ShutdownTool();
	}

	// ==================== 辅助方法 ====================

	/**
	 * 获取工具的调试信息
	 * @return 调试信息字符串
	 */
	virtual FString GetDebugInfo() const
	{
		return FString::Printf(TEXT("Tool: %s (ID: %s)"),
			*GetToolDisplayName().ToString(),
			*GetToolID().ToString());
	}

protected:
	/** 当前预览 Actor */
	UPROPERTY(Transient)
	TObjectPtr<AVoxelPreviewActor> CurrentPreviewActor = nullptr;

	/** 工具是否激活 */
	UPROPERTY(Transient)
	bool bIsActive = false;

	/** 工具初始化标志 */
	UPROPERTY(Transient)
	bool bIsInitialized = false;

	// ==================== Placement 薄包装（工具层便捷访问） ====================
	// 这些方法封装了 PlacementSubsystem + HistoryComp 的调用，工具子类直接用

	/** 放置物体并自动记录历史 */
	FPlacementHandle PlaceItemWithHistory(
		const FVoxelToolContext& Context,
		int32 ItemId,
		FIntVector Pos,
		EPlacementDirection Direction = EPlacementDirection::Forward);

	/** 放置物体（EActorDirection 重载，工具层常用） */
	FPlacementHandle PlaceItemWithHistory(
		const FVoxelToolContext& Context,
		int32 ItemId,
		FIntVector Pos,
		EActorDirection Direction);

	/** 删除物体并自动记录历史 */
	bool RemoveItemWithHistory(
		const FVoxelToolContext& Context,
		const FPlacementHandle& Handle);

	/** 修改物体并自动记录历史 */
	bool ModifyItemWithHistory(
		const FVoxelToolContext& Context,
		const FPlacementHandle& Handle,
		const FPlacementModification& Modification);

	/** 查询某格的占用者 */
	FPlacementHandle QueryItemAt(
		const FVoxelToolContext& Context,
		FIntVector Pos) const;

	/** 能否放置 */
	bool CanPlaceItem(
		const FVoxelToolContext& Context,
		int32 ItemId,
		FIntVector Pos,
		EPlacementDirection Direction = EPlacementDirection::Forward) const;

	/** 获取 PlacementSubsystem */
	UVoxelPlacementWorldSubsystem* GetPlacementSubsystem(const FVoxelToolContext& Context) const;
};
