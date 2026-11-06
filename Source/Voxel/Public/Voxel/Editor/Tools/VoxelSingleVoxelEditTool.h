// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "VoxelSingleVoxelEditTool.generated.h"

class AVoxel3DUIActor;
struct FHitResult;
struct FVoxelToolContext;

/**
 * 单体素编辑笔刷工具
 *
 * 职责：
 * - 鼠标悬停时显示笔刷预览（红色=删除，绿色=放置）
 * - 点击/拖拽时批量编辑地形（基于笔刷形状）
 * - 手持方块物品时支持单方块放置（含历史记录，支持撤销）
 *
 * 迁移来源：VoxelControllerInteractComponent 中的 SingleVoxel 分支
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelSingleVoxelEditTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UVoxelSingleVoxelEditTool();

	virtual FText GetToolDisplayName() const override;
	virtual EVoxelToolCategory GetToolCategory() const override { return EVoxelToolCategory::Edit; }
	virtual FName GetToolID() const override { return FName("SingleVoxelEdit"); }

	virtual void OnMouseHover(const FVoxelToolContext& Context, const FHitResult& GroundHit) override;
	virtual void OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor) override;
	virtual void OnMouseCancel(const FVoxelToolContext& Context) override;

	virtual bool CanHandleItem(const FGameplayTag& ItemTag) const override;
	virtual bool UseItemInHand(const FVoxelToolContext& Context, FVector TargetWorldPosition, FVector TargetNormal) override;

private:
	/** 应用编辑笔刷 stamp（基于笔刷形状批量编辑体素，含历史记录） */
	void ApplyEditBrushStamp(const FVoxelToolContext& Context, const FIntVector& CenterIndex);
	virtual void UpdatePreview(class UVoxelPreviewComponent* PC) override;

	virtual void ConfigureGizmos(class UVoxelPreviewComponent* PC, EPreviewInteractionState Phase) override;

};
