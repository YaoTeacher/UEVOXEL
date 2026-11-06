// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Math/Box.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "VoxelFurnitureEditTool.generated.h"

class UVoxelBlockWorldSubsystem;
class UVoxelItemWorldSubsystem;
class UVoxelPlayerStateCacheComponent;
class UConfigDataSubsystem;
class UVoxelPreviewComponent;
struct FVoxelToolContext;

/** 家具朝向（前向声明，定义于 GameStateEnum.h） */
enum class EActorDirection : uint8;

/**
 * 家具放置工具（无状态方法类）
 *
 * 设计原则：
 * - 只提供静态方法，不存储任何状态
 * - 所有运行时状态通过 StateComponent 读取
 *
 * 职责：
 * - 在指定网格位置生成家具 Actor
 * - 旋转家具方向（更新 StateComponent 中的手持家具数据）
 *
 * 触发入口：
 * - Tool_Edit_Furniture 工具激活时，由 InteractComponent 委托调用
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelFurnitureEditTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UVoxelFurnitureEditTool();

	// ==================== UVoxelToolBase 接口实现 ====================

	virtual FText GetToolDisplayName() const override;
	virtual FText GetToolDescription() const override;
	virtual EVoxelToolCategory GetToolCategory() const override;
	virtual FName GetToolID() const override;

	// ==================== 输入生命周期（override） ====================

	/**
	 * 鼠标悬停：家具预览跟随鼠标，每帧更新 TmpGeometry.Box 到当前体素
	 * 迁移自 HandleMouseHover 中 Tool_Edit_Furniture 分支
	 */
	virtual void OnMouseHover(const FVoxelToolContext& Context, const FHitResult& GroundHit) override;

	/**
	 * 鼠标按下：将命中体素写入 TmpGeometry.Box（供后续确认放置使用）
	 * 迁移自 HandleMouseInput 中 Tool_Edit_Furniture 分支
	 */
	virtual void OnMouseInput(const FVoxelToolContext& Context, const FHitResult& GroundHit, AVoxel3DUIActor* GizmoHitActor) override;

	/**
	 * 确认编辑：调用 PlaceFurniture 在 TmpGeometry 指定位置生成家具
	 * 迁移自 RequestConfirmVoxelEdit 中 Tool_Edit_Furniture 分支
	 */
	virtual void OnConfirmEdit(const FVoxelToolContext& Context) override;

	/**
	 * 是否能处理该 ItemTag（家具类物品路由入口）
	 * @return true 当 ItemTag 匹配 Item.Type.BuildItem
	 */
	virtual bool CanHandleItem(const FGameplayTag& ItemTag) const override;

	/**
	 * 使用手持家具物品：构建 EditRequest 并调用 PlaceFurniture
	 * 迁移自 UseItemInHand 中 BuildItem 分支
	 */
	virtual bool UseItemInHand(const FVoxelToolContext& Context, FVector TargetWorldPosition, FVector TargetNormal) override;

	// ==================== 家具相关工具方法（迁移自 InteractionComp） ====================

	/**
	 * 更新家具预览位置（迁移自 UVoxelInteractionComponent::UpdateFurniturePosition）
	 * - 更新预览 Actor 的单体素预览
	 * - 同步写入 TmpGeometry.Box
	 * @param Context 工具上下文
	 * @param Position 目标体素索引
	 */
	void UpdateFurniturePosition(const FVoxelToolContext& Context, FIntVector Position);

	/**
	 * 旋转手持家具（迁移自 UVoxelInteractionComponent::RotateFurniture）
	 * - 更新手持道具的 Direction 字段
	 * - 同步 TmpGeometry.Rotation
	 * - 旋转预览 Actor
	 * @param Context 工具上下文
	 * @param Direction 新朝向
	 */
	void RotateFurniture(const FVoxelToolContext& Context, EActorDirection Direction);

	/**
	 * 计算家具包围盒（迁移自 UVoxelInteractionComponent::GetFurnitureBounds）
	 * @param Context 工具上下文
	 * @param Position 放置体素索引
	 * @param Direction 朝向
	 * @return 家具包围盒（世界坐标）
	 */
	FBox GetFurnitureBounds(const FVoxelToolContext& Context, FIntVector Position, EActorDirection Direction);

	// ==================== 家具放置 ====================

	/**
	 * 在指定位置放置家具
	 *
	 * 流程：
	 * 1. 从 EditRequest.Geometry.Box.MinIndex 获取放置位置
	 * 2. 从 EditRequest.Geometry.Rotation 计算朝向
	 * 3. 从 BuildStateComp 读取手持道具数据
	 * 4. 更新手持道具的 Direction 字段
	 * 5. 调用 ItemWorld->SpawnItemAtGrid 生成家具 Actor
	 *
	 * @param EditRequest 编辑请求（包含几何位置和旋转）
	 * @param BuildStateComp 状态组件（读取/写入手持道具数据）
	 * @param ItemWorld 物品世界子系统（执行家具生成）
	 * @return true 放置成功，false 失败
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Tool|Furniture")
	static bool PlaceFurniture(
		const FVoxelEditRequest& EditRequest,
		UVoxelPlayerStateCacheComponent* BuildStateComp,
		UVoxelItemWorldSubsystem* ItemWorld
	);
	virtual void UpdatePreview(class UVoxelPreviewComponent* PC) override;

	virtual void ConfigureGizmos(class UVoxelPreviewComponent* PC, EPreviewInteractionState Phase) override;

};
