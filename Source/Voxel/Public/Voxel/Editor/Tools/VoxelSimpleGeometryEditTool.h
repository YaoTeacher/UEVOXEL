// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "VoxelSimpleGeometryEditTool.generated.h"

class AVoxel3DUIActor;
struct FHitResult;
struct FVoxelToolContext;

/**
 * 简单几何体编辑工具
 *
 * 职责：
 * - HIDDEN→BASE：第一点点击后进入 BASE 状态
 * - BASE：拖拽定义区域（更新 SecondPoint）
 * - 释放时进入 EDIT 状态（Gizmo 调整由 InteractionComp 保留的 HandleGizmo_Rotate/Face 处理）
 * - 确认编辑时构造历史记录并应用到世界
 *
 * 迁移来源：VoxelControllerInteractComponent 中 HandleGizmoInteraction /
 *           HandleMouseRelease / HandleMouseCancel / RequestConfirmVoxelEdit 的默认几何体分支
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelSimpleGeometryEditTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UVoxelSimpleGeometryEditTool();

	virtual FText GetToolDisplayName() const override;
	virtual EVoxelToolCategory GetToolCategory() const override { return EVoxelToolCategory::Edit; }
	virtual FName GetToolID() const override { return FName("SimpleGeometryEdit"); }

	virtual void OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor) override;
	virtual void OnMouseRelease(const FVoxelToolContext& Context) override;
	virtual void OnConfirmEdit(const FVoxelToolContext& Context) override;

	/** Gizmo 平移：将临时几何整体偏移（EDIT 阶段拖拽 TranslateGizmo） */
	virtual void OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset) override;
	virtual void UpdatePreview(class UVoxelPreviewComponent* PC) override;

	virtual void ConfigureGizmos(class UVoxelPreviewComponent* PC, EPreviewInteractionState Phase) override;

};
