#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelWidgetBase.h"
#include "VoxelInfoTooltip.generated.h"

class UTextBlock;
class UBorder;
class UButton;

/**
 * L2.5 信息浮窗基类（非模态，固定位置）
 *
 * 用途：MMO式 Tooltip，点击物体时在物体附近弹出信息浮窗
 * - 非模态、无遮罩、不暂停游戏
 * - 位置基于目标 Actor 世界坐标投影到屏幕（一次性定位，不跟随鼠标）
 * - 同时只允许 1 个 InfoTooltip
 * - 有关闭按钮（X）由子类蓝图必需配置
 *
 * 蓝图子类 BindWidget 控件（全部必需，缺失则编译失败）：
 * - TitleText: 标题（如家具名称、NPC名称）
 * - ContentText: 多行内容（"标签: 值" 格式）
 * - CloseButton: 关闭按钮（X），点击后调用 Close()
 *
 * 由 AVoxelHUD::ShowInfoTooltip 打开，HideInfoTooltip 关闭
 */
UCLASS(Abstract)
class VOXEL_API UVoxelInfoTooltip : public UVoxelWidgetBase
{
	GENERATED_BODY()

public:
	/** 浮窗打开时调用（子类重写：从 Context 填充内容） */
	virtual void OnTooltipShown(UObject* Context) {}

	/** 浮窗关闭时调用（子类重写：清理） */
	virtual void OnTooltipHidden() {}

	/** 关闭浮窗（调用 AVoxelHUD.HideInfoTooltip） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|UI|Tooltip")
	void Close();

	/**
	 * 基于世界坐标定位浮窗（将世界坐标投影到屏幕，在附近定位）
	 * 在 OnTooltipShown 中调用，一次性设置位置
	 * @param WorldLocation 目标世界坐标（如 Actor 位置）
	 * @param ViewportSize 视口尺寸（用于边界检测）
	 */
	void UpdatePositionNearWorldLocation(const FVector& WorldLocation, const FVector2D& ViewportSize);

protected:
	/** NativeOnInitialized: 一次性绑定 CloseButton 点击事件 */
	virtual void NativeOnInitialized() override;

	/** NativeConstruct: 每次激活时的刷新（空，保留给子类） */
	virtual void NativeConstruct() override;

	/** CloseButton 点击回调 */
	UFUNCTION()
	void HandleCloseClicked();

	/** 标题文本（如物体名称）- 必需 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	/** 内容文本（多行，"标签: 值\n" 格式）- 必需 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ContentText;

	/** 关闭按钮（X），点击后关闭浮窗 - 必需 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	/** 相对目标位置的偏移（避免遮挡物体） */
	FVector2D TargetOffset = FVector2D(16.0f, -50.0f);
};
