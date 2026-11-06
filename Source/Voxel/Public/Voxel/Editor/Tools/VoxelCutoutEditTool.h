#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "VoxelCutoutEditTool.generated.h"

class AVoxel3DUIActor;
struct FHitResult;
struct FVoxelToolContext;

/**
 * 透明区域框编辑工具（Tool.Cutout）
 *
 * - 激活时 CutoutSys 进入编辑模式（边框 FillBox+碰撞显隐）
 * - IDLE：Cutout 通道射线选中已有框 -> 载入 TmpGeometry 进 Gizmo 编辑；否则两点定义新框
 * - 确认（M16 Confirm）：新建->AddCutoutBox；编辑已有->UpdateCutoutBox（带历史）
 */
UCLASS(BlueprintType)
class UVoxelCutoutEditTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UVoxelCutoutEditTool();

	virtual FText GetToolDisplayName() const override;
	virtual EVoxelToolCategory GetToolCategory() const override { return EVoxelToolCategory::Edit; }
	virtual FName GetToolID() const override { return FName("Cutout"); }

	virtual void OnToolActivated() override;
	virtual void OnToolDeactivated() override;

	virtual void OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor) override;
	virtual void OnMouseRelease(const FVoxelToolContext& Context) override;
	virtual void OnMouseCancel(const FVoxelToolContext& Context) override;
	virtual void OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset) override;
	virtual void OnConfirmEdit(const FVoxelToolContext& Context) override;

	// ==================== 预览驱动 ====================

	virtual void UpdatePreview(class UVoxelPreviewComponent* PC) override;
	virtual void UpdateSelectionHighlight(class UVoxelPreviewComponent* PC) override;

private:
	/** 当前选中编辑的框 Id（INDEX_NONE=新建态） */
	int32 SelectedBoxId = INDEX_NONE;

	/** 载入指定框到 TmpGeometry 并进入 EDITING（供 Gizmo 编辑） */
	void BeginEditExisting(const FVoxelToolContext& Context, int32 BoxId);

	/** 重置到 IDLE 新建态 */
	void ResetToIdle(const FVoxelToolContext& Context);
	virtual void ConfigureGizmos(class UVoxelPreviewComponent* PC, EPreviewInteractionState Phase) override;

};
