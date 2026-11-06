// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "VoxelTemplateEditTool.generated.h"

class UVoxelBlockWorldSubsystem;
class UVoxelItemWorldSubsystem;
class UConfigDataSubsystem;

/**
 * 模板放置工具（无状态方法类）
 *
 * 设计原则：
 * - 只提供静态方法，不存储任何状态
 *
 * 职责：
 * - 从配置表读取模板数据
 * - 调用 UVoxelTemplateUtils::PlaceTemplate 批量生成方块和家具
 *
 * 触发入口：
 * - Tool_Edit_Template 工具激活时，由 InteractComponent 委托调用
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelTemplateEditTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UVoxelTemplateEditTool();

	// ==================== UVoxelToolBase 接口实现 ====================

	virtual FText GetToolDisplayName() const override;
	virtual FText GetToolDescription() const override;
	virtual EVoxelToolCategory GetToolCategory() const override;
	virtual FName GetToolID() const override;

	// ==================== 输入生命周期（override） ====================

	/**
	 * 鼠标悬停：模板预览状态机
	 * 迁移自 HandleMouseHover 中 Tool_Edit_Template 分支：
	 * - HIDDEN + 有效手持 → 自动进入 BASE（跟随鼠标）
	 * - BASE  → 跟随鼠标更新 TmpGeometry.Box.MinIndex
	 * - EDIT  → 不更新（由 Gizmo / DetailPanel 控制）
	 */
	virtual void OnMouseHover(const FVoxelToolContext& Context, const FHitResult& GroundHit) override;

	/**
	 * 鼠标按下：模板状态机推进
	 * 迁移自 HandleMouseInput 中 Tool_Edit_Template 分支：
	 * - BASE  → 点击地面确认位置 → 进入 EDIT
	 * - EDIT  → Gizmo 交互（平移 Gizmo 命中时计算偏移并调 OnGizmoTranslate）
	 */
	virtual void OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor) override;

	/**
	 * 确认编辑：调用 PlaceTemplate 批量生成方块和家具
	 * 迁移自 RequestConfirmVoxelEdit 中 Tool_Edit_Template 分支
	 * 放置后保持 EDIT 状态（预览保留，可连续放置）
	 */
	virtual void OnConfirmEdit(const FVoxelToolContext& Context) override;

	/**
	 * Gizmo 平移：平移模板实例位置（TmpGeometry.Box.MinIndex 即模板中心点）
	 * 迁移自 HandleGizmo_Translate 中 Tool_Edit_Template 分支
	 */
	virtual void OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset) override;

	/**
	 * 是否能处理该 ItemTag（模板类物品路由入口）
	 * @return true 当 ItemTag 匹配 Item.Type.Template
	 */
	virtual bool CanHandleItem(const FGameplayTag& ItemTag) const override;

	/**
	 * 使用手持模板物品：模板不允许直接落地
	 * 迁移自 UseItemInHand 中 Template 分支（返回 false + 警告日志）
	 * 模板使用状态机：BASE 点击只进入 EDIT；最终放置统一由 M15/Confirm 调 OnConfirmEdit
	 */
	virtual bool UseItemInHand(const FVoxelToolContext& Context, FVector TargetWorldPosition, FVector TargetNormal) override;

	// ==================== 模板放置 ====================

	/**
	 * 在指定位置放置模板
	 *
	 * 流程：
	 * 1. 从 EditRequest.BlockType 读取模板 ItemId
	 * 2. 从 ConfigSys 查询 FVoxelTemplate 配置
	 * 3. 从 EditRequest.Geometry 读取放置位置和旋转
	 * 4. 调用 UVoxelTemplateUtils::PlaceTemplate 批量生成方块和家具
	 *
	 * @param EditRequest 编辑请求（BlockType=模板 ItemId, Geometry.Box.MinIndex=放置位置）
	 * @param BlockWorld 方块世界
	 * @param ItemWorld 物品世界
	 * @param ConfigSys 配置数据子系统
	 * @return true 放置成功，false 失败
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Tool|Template")
	static bool PlaceTemplate(
		const FVoxelEditRequest& EditRequest,
		UVoxelBlockWorldSubsystem* BlockWorld,
		UVoxelItemWorldSubsystem* ItemWorld,
		UConfigDataSubsystem* ConfigSys,
		const TMap<FString, FString>& Remap
	);

private:
	/** 标记：刚从 BASE 转入 EDIT，本次按下事件不抓取 Gizmo（防止落地瞬间误抓） */
	bool bJustEnteredEdit = false;
	virtual void UpdatePreview(class UVoxelPreviewComponent* PC) override;

	virtual void ConfigureGizmos(class UVoxelPreviewComponent* PC, EPreviewInteractionState Phase) override;

};
