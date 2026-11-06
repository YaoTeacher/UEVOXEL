// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "ToolDetailModuleBase.generated.h"

class UVoxelPlayerStateCacheComponent;

/**
 * 工具详情页模块基类
 * 所有工具参数控制模块的统一接口
 *
 * 设计模式:
 * - 策略模式: 每个子类实现特定工具的显示逻辑
 * - 模板方法模式: 定义生命周期钩子，子类实现具体行为
 *
 * 数据流:
 * BuildPanelWidget → ShouldShowForTool() → SetVisibility()
 * StateComponent → UpdateFromToolState() → UI控件更新
 *
 * 使用示例:
 * 1. 创建子类 (如 UShapeDetailModule)
 * 2. 重写 ShouldShowForTool() 返回对应的工具标签
 * 3. 重写 UpdateFromToolState() 从 StateComponent 读取数据并更新UI
 * 4. 在 UMG 蓝图中绑定控件 (BindWidget)
 */
UCLASS()
class VOXEL_API UToolDetailModuleBase : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// === 模块生命周期 ===

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// === 模块接口 ===

	/**
	 * 判断模块是否应该在指定工具下显示
	 *
	 * @param ToolTag 当前工具标签 (如 Voxel.Tool.Block)
	 * @return true 表示显示, false 表示隐藏
	 *
	 * @note 子类必须重写此方法
	 * @see BuildPanelWidget::UpdateDetailsPanel() 调用此方法
	 */
	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const;

	/**
	 * 从 StateComponent 更新模块显示
	 *
	 * @param StateComponent 状态组件指针 (由 BuildPanelWidget 传入)
	 *
	 * @note 子类必须重写此方法
	 * @note 在此方法中读取 StateComponent 的状态并更新 UI 控件
	 * @note StateComponent 由 BuildPanelWidget 在调用前设置
	 *
	 * 示例:
	 * void UShapeDetailModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
	 * {
	 *     if (!SC) return;
	 *     FVoxelGeometryDesc Geometry = SC->GetTmpGeometry();
	 *     RotationSlider->SetValue(Geometry.Rotation.Pitch);
	 * }
	 */
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent);

	/**
	 * 模块显示顺序权重（在 DEFAULT 中配置）
	 * 数值越小，在工具详情页中越靠前显示
	 * 建议步长为 10，方便未来插入新模块
	 *
	 * 示例：
	 * - M1: DisplayOrder = 10   (建造库，最靠前)
	 * - M2: DisplayOrder = 20   (放置模式)
	 * - M3: DisplayOrder = 30   (几何形状)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module|Config")
	int32 DisplayOrder = 100;

	/** StateComponent 引用 (由 BuildPanelWidget 设置) */
	UPROPERTY()
	TObjectPtr<UVoxelPlayerStateCacheComponent> StateComponent;

protected:
	// 子类可以在这里添加额外的辅助方法
};
