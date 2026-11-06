// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Voxel/Editor/VoxelHistoryData.h"
#include "Voxel/Editor/VoxelClipboardData.h"
#include "VoxelSelectionEditTool.generated.h"

class UVoxelBlockWorldSubsystem;
class UVoxelItemWorldSubsystem;
class UVoxelPlayerStateCacheComponent;
class UVoxelControllerHistoryComponent;

/**
 * 选区编辑工具（无状态方法类）
 *
 * 设计原则：
 * - 只提供静态方法，不存储任何状态
 * - 所有运行时状态通过 StateComponent 读取/写入
 * - 历史记录通过 HistoryComponent 写入
 *
 * 职责：
 * - 在已有选区内执行编辑操作（填充/删除/替换，类似 PS 选区蒙版）
 * - 选区剪切板操作（复制/剪切/粘贴）
 * - 移动选区内的实际数据（方块 + 家具）
 *
 * 触发入口：
 * - Tool_Edit_Selection 工具激活时，由 InteractComponent 委托调用
 * - 也可由其他编辑工具（Tool_Edit_*）在有选区时委托调用（选区约束模式）
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelSelectionEditTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UVoxelSelectionEditTool();

	// ==================== UVoxelToolBase 接口实现 ====================

	virtual FText GetToolDisplayName() const override;
	virtual FText GetToolDescription() const override;
	virtual EVoxelToolCategory GetToolCategory() const override;
	virtual FName GetToolID() const override;

	// ==================== 输入生命周期（override） ====================

	/**
	 * 鼠标按下：EDIT 阶段 Gizmo 拖拽管理
	 * 迁移自 HandleGizmoInteraction 的 EDIT 分支：
	 * - 激活/更新 ActiveDragGizmo、FirstPoint、SecondPoint
	 * - 平移 Gizmo 命中时计算单轴偏移并调用 OnGizmoTranslate（最终调 MoveSelectionContent）
	 */
	virtual void OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor) override;

	/**
	 * 确认编辑：在已有选区内执行编辑（选区约束模式）
	 * 迁移自 RequestConfirmVoxelEdit 中 Tool_Edit + HasSelection 分支：
	 * - 调用 ExecuteWithinSelection
	 * - 成功后重置编辑状态（清几何变换 + 回到 HIDDEN）
	 */
	virtual void OnConfirmEdit(const FVoxelToolContext& Context) override;

	/**
	 * Gizmo 平移：移动选区内的实际数据（方块 + 家具）
	 * 迁移自 HandleGizmo_Translate 中 Tool_Edit_Selection 分支
	 */
	virtual void OnGizmoTranslate(const FVoxelToolContext& Context, const FIntVector& Offset) override;

	// ==================== 选区内编辑（PS 选区蒙版模式） ====================

	/**
	 * 在已有选区内执行编辑操作（选区约束模式）
	 *
	 * 设计参考 PS：
	 * - 选区建立后，用编辑工具操作时仅影响选区内方块
	 * - 根据 PlacementMode 过滤：
	 *   - Complement（填充）：只在空气位置写入
	 *   - Replace（替换）：只替换非空方块
	 *   - Normal（覆盖）：不跳过
	 *
	 * @param EditRequest 编辑请求（包含 BlockType、PlacementMode 等）
	 * @param BuildStateComp 控制器状态组件（读取 CurrentSelection）
	 * @param HistoryComp 历史记录组件（写入撤销/重做记录）
	 * @param BlockWorld 方块世界子系统（执行实际修改）
	 * @return true 成功执行，false 没有可修改的体素
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Tool|SelectionEdit")
	static bool ExecuteWithinSelection(
		const FVoxelEditRequest& EditRequest,
		UVoxelPlayerStateCacheComponent* BuildStateComp,
		UVoxelControllerHistoryComponent* HistoryComp,
		UVoxelBlockWorldSubsystem* BlockWorld
	);

	// ==================== 剪切板操作 ====================

	/**
	 * 复制选区内容到剪切板
	 * - 读取选区内每个体素的当前类型
	 * - 转换为相对位置（相对于选区 Min）
	 * - 存入 BuildStateComp->ClipboardData
	 *
	 * @param BuildStateComp 状态组件（读取选区、写入剪切板）
	 * @param BlockWorld 方块世界（读取方块类型）
	 * @return true 复制成功，false 选区为空
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Tool|SelectionEdit")
	static bool CopySelection(
		UVoxelPlayerStateCacheComponent* BuildStateComp,
		UVoxelBlockWorldSubsystem* BlockWorld
	);

	/**
	 * 剪切选区（复制 + 删除原内容）
	 * - 先调用 CopySelection
	 * - 再用 Air 填充原选区
	 *
	 * @param BuildStateComp 状态组件
	 * @param BlockWorld 方块世界
	 * @param HistoryComp 历史记录组件
	 * @return true 剪切成功
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Tool|SelectionEdit")
	static bool CutSelection(
		UVoxelPlayerStateCacheComponent* BuildStateComp,
		UVoxelBlockWorldSubsystem* BlockWorld,
		UVoxelControllerHistoryComponent* HistoryComp
	);

	/**
	 * 粘贴剪切板内容到指定位置
	 * - 按方块类型分组批量写入（性能优化）
	 * - 写入历史记录（含 OldType 读取）
	 *
	 * @param BuildStateComp 状态组件（读取 ClipboardData）
	 * @param BlockWorld 方块世界（执行批量写入）
	 * @param HistoryComp 历史记录组件
	 * @param TargetPosition 目标位置（世界体素索引）
	 * @return true 粘贴成功，false 剪切板为空
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Tool|SelectionEdit")
	static bool PasteSelection(
		UVoxelPlayerStateCacheComponent* BuildStateComp,
		UVoxelBlockWorldSubsystem* BlockWorld,
		UVoxelControllerHistoryComponent* HistoryComp,
		const FIntVector& TargetPosition
	);

	// ==================== 移动选区内实际数据 ====================

	/**
	 * 移动选区内的实际数据（方块 + 家具）
	 *
	 * 双模式移动之一（MoveContent）：
	 * - 与 MoveSelection（仅移动选区边框索引）不同
	 * - 此方法会实际搬移选区内的方块数据
	 * - 同时搬移选区内已放置的家具 Actor
	 *
	 * 流程：
	 * 1. 读取选区内方块的当前类型
	 * 2. 在新位置写入相同类型
	 * 3. 删除原位置的方块（设为 Air）
	 * 4. 查找选区内所有家具，调用 ItemWorld 重新 Spawn 到新位置并 Remove 原位置
	 * 5. 平移选区索引本身（保持选区与内容同步）
	 * 6. 写入历史记录
	 *
	 * @param BuildStateComp 状态组件（读取选区、写入新选区索引）
	 * @param BlockWorld 方块世界（读写方块）
	 * @param ItemWorld 物品世界（移动家具）
	 * @param HistoryComp 历史记录组件
	 * @param Offset 体素索引偏移量
	 * @return true 移动成功
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Tool|SelectionEdit")
	static bool MoveSelectionContent(
		UVoxelPlayerStateCacheComponent* BuildStateComp,
		UVoxelBlockWorldSubsystem* BlockWorld,
		UVoxelItemWorldSubsystem* ItemWorld,
		UVoxelControllerHistoryComponent* HistoryComp,
		const FIntVector& Offset
	);

private:
	/**
	 * 计算选区包围盒 Min/Max
	 * @param Selection 选区数组
	 * @param OutMin 输出最小索引
	 * @param OutMax 输出最大索引
	 * @return true 成功计算，false 选区为空
	 */
	static bool GetSelectionBounds(
		const TArray<FIntVector>& Selection,
		FIntVector& OutMin,
		FIntVector& OutMax
	);
	virtual void ConfigureGizmos(class UVoxelPreviewComponent* PC, EPreviewInteractionState Phase) override;

};
