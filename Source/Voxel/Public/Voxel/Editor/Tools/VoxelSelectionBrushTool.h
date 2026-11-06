// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "VoxelSelectionBrushTool.generated.h"

class AVoxel3DUIActor;
struct FHitResult;
struct FVoxelToolContext;

/**
 * 选区笔刷工具（Tool_Selection_Brush）
 *
 * 职责：
 * - 鼠标悬停时显示笔刷预览（青色，区分于编辑笔刷的绿/红）
 * - 点击/拖拽时按笔刷形状涂抹选区（New 模式首 stamp 清空，之后按 Add 累加；Subtract 擦除）
 * - 释放时结束拖拽（保留已应用选区）；若为 Gizmo 拖拽则结束平移模式
 * - 取消时根据状态分支：Gizmo 拖拽中/笔刷涂抹中保留选区，否则清空选区
 * - 有选区 + 点击位移 Gizmo 时拖拽整个选区（保持 IDLE）
 *
 * 迁移来源：VoxelControllerInteractComponent 中
 *           HandleMouseHover / HandleGizmoInteraction /
 *           HandleMouseRelease / HandleMouseCancel 的 Tool_Selection_Brush 分支
 *           及私有方法 ApplyBrushStamp
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelSelectionBrushTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UVoxelSelectionBrushTool();

	virtual FText GetToolDisplayName() const override;
	virtual EVoxelToolCategory GetToolCategory() const override { return EVoxelToolCategory::Selection; }
	virtual FName GetToolID() const override { return FName("Selection_Brush"); }

	virtual void OnMouseHover(const FVoxelToolContext& Context, const FHitResult& GroundHit) override;
	virtual void OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor) override;
	virtual void OnMouseRelease(const FVoxelToolContext& Context) override;
	virtual void OnMouseCancel(const FVoxelToolContext& Context) override;

private:
	/**
	 * 应用选区笔刷 stamp
	 * - 计算当前鼠标位置对应的体素索引，跳过同一体素（拖拽中未移到新体素）
	 * - 根据 SelectionMode 决定首次 stamp 是否清空选区（New 模式清空）
	 * - 调用 UVoxelSelectionTool::CalculateBrushSelection 计算笔刷覆盖体素
	 * - 调用 SC->ApplySelectionOp 应用选区操作
	 *
	 * @param Context 工具上下文
	 * @param WorldPosition 鼠标世界坐标
	 */
	void ApplyBrushStamp(const FVoxelToolContext& Context, const FVector& WorldPosition);
	virtual void ConfigureGizmos(class UVoxelPreviewComponent* PC, EPreviewInteractionState Phase) override;

};
