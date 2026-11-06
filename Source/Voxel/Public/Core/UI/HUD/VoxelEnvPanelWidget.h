#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelWidgetBase.h"
#include "VoxelEnvPanelWidget.generated.h"

class UButton;
class UTextBlock;
class USlider;
class UEditableTextBox;
class UBorder;
class ULayerSwitchSectionWidget;
class UVoxelPlayerStateCacheComponent;
class UVoxelInteractionComponent;

/**
 * 环境域面板（右上角常驻，对应原型 voxel-ui-v6 环境组）
 *
 * 结构：
 * - 时钟行       ☀ + ClockText + DateText（静态占位，时间系统未接入）
 * - 小地图       MinimapNameText / MinimapCoordText（静态占位，渲染未接入）
 * - 按钮行       时间抽屉(🕐) / 高度抽屉(⬒) / 定位(🎯占位) / 图层弹窗(🗜) / 剖切弹窗(▣)
 * - TimeDrawer   时间倍速（占位禁用态）
 * - HeightDrawer PAWN 高度 / 渲染高度滑杆（从 TopLeftBar 迁入，业务走 InteractionComp）
 * - LayerPop     渲染图层切换区（复用 WBP_LayerSwitchSection）
 * - CutoutPop    剖切框列表（占位提示）
 *
 * 抽屉/弹窗各自独立开合，不互斥（对应原型 v6-edg/belowpops 行为）。
 * 数据流：UI 操作 -> InteractionComp Setter -> SC 广播 -> 本控件刷新（单向，无 Tick）
 */
UCLASS(Abstract)
class VOXEL_API UVoxelEnvPanelWidget : public UVoxelWidgetBase
{
	GENERATED_BODY()

public:
	UVoxelEnvPanelWidget();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// === BindWidget 子控件（蓝图必须同名绑定，缺失时编译报错）===

	// ---- 时钟行（占位显示）----
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ClockText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DateText;

	// ---- 小地图（占位显示）----
	UPROPERTY(meta = (BindWidget))
	UTextBlock* MinimapNameText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* MinimapCoordText;

	// ---- 按钮行 ----
	/** 时间控制抽屉拉片 */
	UPROPERTY(meta = (BindWidget))
	UButton* TimeDrawerButton;

	/** 高度控制抽屉拉片 */
	UPROPERTY(meta = (BindWidget))
	UButton* HeightDrawerButton;

	/** 镜头回到当前位置（占位，未接入） */
	UPROPERTY(meta = (BindWidget))
	UButton* LocateButton;

	/** 渲染图层弹窗开关 */
	UPROPERTY(meta = (BindWidget))
	UButton* LayerPopButton;

	/** 剖切框弹窗开关 */
	UPROPERTY(meta = (BindWidget))
	UButton* CutoutPopButton;

	// ---- 时间抽屉（占位：时间系统未接入，仅展示禁用态）----
	UPROPERTY(meta = (BindWidget))
	UBorder* TimeDrawer;

	// ---- 高度抽屉（从 TopLeftBar 迁入）----
	UPROPERTY(meta = (BindWidget))
	UBorder* HeightDrawer;

	UPROPERTY(meta = (BindWidget))
	USlider* PawnSlider;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* PawnInput;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* PawnValueText;

	UPROPERTY(meta = (BindWidget))
	USlider* RenderSlider;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* RenderInput;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* RenderValueText;

	/** 切面开关（渲染高度行尾部小按钮） */
	UPROPERTY(meta = (BindWidget))
	UButton* SliceToggleButton;

	/** 过滤模式按钮（全局/仅自己区域） */
	UPROPERTY(meta = (BindWidget))
	UButton* FilterToggleButton;

	// ---- 图层/剖切弹窗 ----
	UPROPERTY(meta = (BindWidget))
	UBorder* LayerPop;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	ULayerSwitchSectionWidget* LayerSection;

	UPROPERTY(meta = (BindWidget))
	UBorder* CutoutPop;

private:
	// === 抽屉/弹窗开合（独立，不互斥）===
	bool bTimeDrawerOpen = false;
	bool bHeightDrawerOpen = false;
	bool bLayerPopOpen = false;
	bool bCutoutPopOpen = false;

	void SyncDrawerVisibility() const;

	// === 高度控制事件回调（迁移自 TopLeftBar）===
	UFUNCTION()
	void OnPawnSliderChanged(float Value);

	UFUNCTION()
	void OnPawnInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnRenderSliderChanged(float Value);

	UFUNCTION()
	void OnRenderInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnSliceToggleClicked();

	UFUNCTION()
	void OnFilterToggleClicked();

	UFUNCTION()
	void OnTimeDrawerClicked();

	UFUNCTION()
	void OnHeightDrawerClicked();

	UFUNCTION()
	void OnLocateClicked();

	UFUNCTION()
	void OnLayerPopClicked();

	UFUNCTION()
	void OnCutoutPopClicked();

	// === 状态刷新 ===
	UFUNCTION()
	void OnSliceChanged();

	UFUNCTION()
	void OnPawnHeightChanged();

	UVoxelPlayerStateCacheComponent* GetStateComponent() const;

	UVoxelInteractionComponent* GetToolComponent() const;

	void RefreshPawnRow();

	void RefreshRenderRow();

	void SyncSlider(USlider* Slider, int32 MinLayer, int32 MaxLayer, int32 CurLayer);

	/** 防滑杆回调环 */
	bool bIsUpdatingSlider = false;

	UPROPERTY()
	TObjectPtr<UVoxelPlayerStateCacheComponent> CachedSC;
};
