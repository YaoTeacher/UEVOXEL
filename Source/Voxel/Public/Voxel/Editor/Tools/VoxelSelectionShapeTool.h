// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "VoxelSelectionShapeTool.generated.h"

class AVoxel3DUIActor;
struct FHitResult;
struct FVoxelToolContext;

/**
 * 选区形状工具（Tool_Selection_SimpleGeometry）
 *
 * 职责：
 * - IDLE→PLACING：第一点点击后进入 PLACING 状态（拖拽定义选区形状）
 * - PLACING：拖拽更新 SecondPoint，调用 SC->SetEditPoints 同步选区预览
 * - 释放时立即应用形状到 CurrentSelection（MagicaVoxel 松手即用模式）
 * - 确认编辑时按当前选择算法（New/Add/Subtract/Intersect）应用临时形状
 * - Gizmo 平移时仅移动选区边框索引（不搬移实际数据）
 *
 * 迁移来源：VoxelControllerInteractComponent 中
 *           HandleGizmoInteraction / HandleMouseRelease /
 *           HandleMouseCancel / RequestConfirmVoxelEdit /
 *           HandleGizmo_Translate 的 Tool_Selection 分支
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelSelectionShapeTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UVoxelSelectionShapeTool();

	virtual FText GetToolDisplayName() const override;
	virtual EVoxelToolCategory GetToolCategory() const override { return EVoxelToolCategory::Selection; }
	virtual FName GetToolID() const override { return FName("Selection_SimpleGeometry"); }

	virtual void OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor) override;
	virtual void OnMouseRelease(const FVoxelToolContext& Context) override;
	virtual void OnMouseCancel(const FVoxelToolContext& Context) override;
	virtual void OnConfirmEdit(const FVoxelToolContext& Context) override;
	virtual void OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset) override;

private:
	/**
	 * 立即应用临时形状到 CurrentSelection
	 * - 计算当前临时形状（只算形状，不应用算法）
	 * - 按当前选择算法（New/Add/Subtract/Intersect）应用到 CurrentSelection
	 * - 重置编辑状态，准备下一次选择
	 */
	void ApplySelectionShapeImmediately(const FVoxelToolContext& Context);
	virtual void UpdatePreview(class UVoxelPreviewComponent* PC) override;
	virtual void UpdateSelectionHighlight(class UVoxelPreviewComponent* PC) override;

	virtual void ConfigureGizmos(class UVoxelPreviewComponent* PC, EPreviewInteractionState Phase) override;

};
