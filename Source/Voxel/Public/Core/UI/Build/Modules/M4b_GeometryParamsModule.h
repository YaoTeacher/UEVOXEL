// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "M4b_GeometryParamsModule.generated.h"

struct FVoxelGeometryDesc;

class USlider;
class UCheckBox;
class UTextBlock;
class UButton;
class UComboBoxString;
class UPanelWidget;

/**
 * M4b: 几何参数控制模块
 * 职责：控制几何体的旋转、厚度和正方体模式
 *
 * 显示条件: Tool.Selection.SimpleGeometry 或 Tool.Edit.SimpleGeometry
 *
 * UI控件:
 * - PivotModeComboBox: 变换原点模式切换
 * - TransformSpaceComboBox: 坐标空间切换
 * - PivotCoordText: 游标坐标显示
 * - CursorToCenterButton: 游标吸附到物体中心
 * - CenterToCursorButton: 物体移到游标位置
 * - RotationPitchSlider: Pitch 旋转滑块
 * - RotationYawSlider: Yaw 旋转滑块
 * - RotationRollSlider: Roll 旋转滑块
 * - ThicknessInput: 厚度滑条（正=向内壳/负=向外壳，0=实心）
 * - [ThicknessValueText]: 厚度值显示文本
 * - RegularModeCheckBox: 正方体模式复选框
 * - ShellFacePanel + ShellFaceUp/Down/Left/Right/Front/BackCheckBox: 壳体面选择（仅壳模式生效，全不勾=全部面）
 * - [RotationPitchText]: Pitch 值显示文本
 * - [RotationYawText]: Yaw 值显示文本
 * - [RotationRollText]: Roll 值显示文本
 */
UCLASS()
class VOXEL_API UM4b_GeometryParamsModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	// === 模块接口 ===

	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleBuildModeParamsUpdated();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	

	bool bDelegatesBound = false;

	/** 从 TmpGeometry 刷新 UI */
	void RefreshFromGeometry();

	// === UI 控件绑定 ===

	/** Pitch 旋转滑块 */
	UPROPERTY(meta = (BindWidget))
	USlider* RotationPitchSlider;

	/** Yaw 旋转滑块 */
	UPROPERTY(meta = (BindWidget))
	USlider* RotationYawSlider;

	/** Roll 旋转滑块 */
	UPROPERTY(meta = (BindWidget))
	USlider* RotationRollSlider;

	/** Pitch 值显示文本 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* RotationPitchText;

	/** Yaw 值显示文本 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* RotationYawText;

	/** Roll 值显示文本 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* RotationRollText;

	/** 厚度滑条（正=向内壳/负=向外壳，0=实心） */
	UPROPERTY(meta = (BindWidget))
	USlider* ThicknessInput;

	/** 厚度值显示文本 */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* ThicknessValueText;

	/** 正方体模式复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* RegularModeCheckBox;

	// === 壳体面选择（仅壳模式 Thickness != 0 生效；全不勾=全部面） ===

	/** 壳体面面板（Thickness == 0 时禁用） */
	UPROPERTY(meta = (BindWidget))
	UPanelWidget* ShellFacePanel;

	/** 壳体面：上 (+Z) */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* ShellFaceUpCheckBox;

	/** 壳体面：下 (-Z) */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* ShellFaceDownCheckBox;

	/** 壳体面：左 (-Y) */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* ShellFaceLeftCheckBox;

	/** 壳体面：右 (+Y) */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* ShellFaceRightCheckBox;

	/** 壳体面：前 (+X) */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* ShellFaceFrontCheckBox;

	/** 壳体面：后 (-X) */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* ShellFaceBackCheckBox;

	// === 变换原点控制 ===

	/** 原点模式切换 */
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* PivotModeComboBox;

	/** 坐标空间切换 */
	UPROPERTY(meta = (BindWidget))
	UComboBoxString* TransformSpaceComboBox;

	/** 游标坐标显示 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* PivotCoordText;

	/** 游标吸附到物体中心 */
	UPROPERTY(meta = (BindWidget))
	UButton* CursorToCenterButton;

	/** 物体移到游标位置 */
	UPROPERTY(meta = (BindWidget))
	UButton* CenterToCursorButton;

	// === 事件处理 ===

	/** Pitch 旋转改变 */
	UFUNCTION()
	void OnRotationPitchChanged(float Value);

	/** Yaw 旋转改变 */
	UFUNCTION()
	void OnRotationYawChanged(float Value);

	/** Roll 旋转改变 */
	UFUNCTION()
	void OnRotationRollChanged(float Value);

	/** 厚度滑条改变 */
	UFUNCTION()
	void OnThicknessSliderChanged(float Value);

	/** 正方体模式改变 */
	UFUNCTION()
	void OnRegularModeCheckChanged(bool bChecked);

	// === 变换原点事件 ===

	UFUNCTION()
	void OnPivotModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnTransformSpaceChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnCursorToCenterClicked();

	UFUNCTION()
	void OnCenterToCursorClicked();

	// === 壳体面事件 ===

	UFUNCTION()
	void OnShellFaceChanged(bool bChecked);

private:
	/** 交互层统一出口：经 InteractionComp 写回 StateComponent（触发广播），UI 不直改 SC */
	void ApplyGeometryToState(const FVoxelGeometryDesc& Geo);

	/** 收集 6 个勾选状态 → 写回 TmpGeometry.ShellFaces（走 ApplyGeometryToState） */
	void PushShellFaces();

	/** UI 回写守卫（RefreshFromGeometry 回读勾选时不触发 PushShellFaces） */
	bool bIsUpdatingShellFaces = false;
};
