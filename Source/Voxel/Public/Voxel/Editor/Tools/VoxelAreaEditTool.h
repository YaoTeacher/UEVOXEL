// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "Core/Enum/GameStateEnum.h"
#include "VoxelAreaEditTool.generated.h"

class AVoxel3DUIActor;
class AVoxelAreaBaseInstance;
struct FHitResult;
struct FVoxelToolContext;

/**
 * 区域编辑工具（Tool_AreaEdit）
 *
 * 职责：
 * - 鼠标输入：Gizmo 优先；射线命中已有区域（跳过相机所在父区域的贯穿命中）→ 选中该区域
 *   （含切换选中，CurArea 非空时同样生效）；否则进入 HIDDEN→BASE→EDIT 流程定义新区域包围盒
 * - 鼠标取消：有选中区域 → 清空 state 选中区域；无选中 IDLE → 回退区域层级；编辑中 → 基类统一取消
 * - Gizmo 平移：移动临时区域覆盖范围（MoveTmpAreaCoverage）
 * - 提供区域操作方法：SelectArea / CommitArea / EnterSubArea / ReturnToParentArea
 *
 * 迁移来源：VoxelControllerInteractComponent 中
 *           HandleMouseInput / HandleMouseCancel / HandleGizmo_Translate 的 Tool_AreaEdit 分支
 *           及公有方法 SelectArea / CommitArea / EnterSubArea / ReturnToParentArea
 *           （墙壁生成能力已收编进几何体编辑的壳体面选择，区域侧仅保留 M9 桥接按钮入口）
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelAreaEditTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UVoxelAreaEditTool();

	virtual FText GetToolDisplayName() const override;
	virtual EVoxelToolCategory GetToolCategory() const override { return EVoxelToolCategory::Edit; }
	virtual FName GetToolID() const override { return FName("AreaEdit"); }

	virtual void OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor) override;
	virtual void OnMouseRelease(const FVoxelToolContext& Context) override;
	virtual void OnMouseCancel(const FVoxelToolContext& Context) override;
	virtual void OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset) override;
	virtual void OnConfirmEdit(const FVoxelToolContext& Context) override;

	// ==================== 区域操作 ====================

	/**
	 * 射线检测已有区域（Area 通道多命中遍历）
	 * - 跳过相机所在父区域的贯穿命中（bStartPenetrating），取后方真实目标
	 * @param Context 工具上下文
	 * @param GroundHit 地面射线命中结果（复用其 TraceStart/TraceEnd 线段）
	 * @return 命中的区域实例，未命中返回 nullptr
	 */
	AVoxelAreaBaseInstance* TraceExistingArea(const FVoxelToolContext& Context, const FHitResult& GroundHit) const;

	/**
	 * 选中区域（将目标区域传给状态组件）
	 * @param Context 工具上下文
	 * @param Area 目标区域（nullptr 表示取消选中）
	 */
	void SelectArea(const FVoxelToolContext& Context, AVoxelAreaBaseInstance* Area);

	/**
	 * 提交区域创建（生成区域实例）
	 * - 检查修改成本与 CurArea 是否为空
	 * - 同步 CurParentArea 的 ID 到 TmpAreaInfo.ParentAreaId
	 * - 调用 AreaWorldSubsystem->GenerateAreaInstance 生成区域
	 * @param Context 工具上下文
	 * @return true 创建成功，false 创建失败
	 */
	bool CommitArea(const FVoxelToolContext& Context);

	/**
	 * 进入子区域（当前选中区域成为新的父级）
	 * @param Context 工具上下文
	 */
	void EnterSubArea(const FVoxelToolContext& Context);

	/**
	 * 返回父区域层级
	 * - 有选中区域 → 取消选中
	 * - 无选中区域且有父区域 → 将父区域变为选中区域
	 * - 已是最顶层 → 保持空
	 * @param Context 工具上下文
	 */
	void ReturnToParentArea(const FVoxelToolContext& Context);

	virtual void UpdatePreview(class UVoxelPreviewComponent* PC) override;

	virtual void ConfigureGizmos(class UVoxelPreviewComponent* PC, EPreviewInteractionState Phase) override;

};
