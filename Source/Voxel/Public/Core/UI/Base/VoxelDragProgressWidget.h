#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelWidgetBase.h"
#include "VoxelDragProgressWidget.generated.h"

class UTextBlock;

/**
 * 拖拽进度圈 Widget 基类（非模态，跟随鼠标位置）
 *
 * 用途：长按家具时显示进度，视觉反馈长按进度
 * - 非模态、无遮罩、不暂停游戏
 * - 跟随鼠标屏幕位置
 * - 进度 0.0~1.0，由 AVoxelHUD.Tick 每帧调用 UpdateProgress
 *
 * C++ 提供默认实现（设置 ProgressText 百分比文本），蓝图可 override UpdateProgress 实现环形效果
 *
 * 蓝图控件（必需，缺失则编译失败）：
 * - ProgressText: 进度文本（如 "75%"）
 */
UCLASS(Abstract)
class VOXEL_API UVoxelDragProgressWidget : public UVoxelWidgetBase
{
	GENERATED_BODY()

public:
	/**
	 * 设置进度（0~1），由 AVoxelHUD.Tick 调用
	 * BlueprintNativeEvent: C++ 提供默认实现（更新 ProgressText），蓝图可 override
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Voxel|UI|DragProgress")
	void UpdateProgress(float Progress);

	/**
	 * 更新屏幕位置（由 AVoxelHUD.Tick 调用）
	 * @param MousePosition 鼠标屏幕坐标
	 * @param ViewportSize 视口尺寸
	 */
	void UpdatePosition(const FVector2D& MousePosition, const FVector2D& ViewportSize);

protected:
	/** 进度文本控件（显示百分比，如 "75%"）- 必需 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ProgressText;

	/** 相对鼠标的居中偏移（进度圈中心对齐鼠标位置） */
	FVector2D CenterOffset = FVector2D(0.0f, 0.0f);
};
