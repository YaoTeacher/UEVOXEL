// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "Core/Enum/GameStateEnum.h"
#include "Core/Enum/EditCommandEnums.h"
#include "M7_BrushParamsModule.generated.h"

class USlider;
class UCheckBox;
class UTextBlock;

/**
 * M7: 笔刷参数控制模块
 * 职责：控制笔刷形状和大小
 *
 * 显示条件: Tool.Selection.Brush (笔刷选区工具)
 *
 * 交互模型：点击即应用 → 拖拽连续涂抹 → 松开结束（统一涂抹模式，无点击/绘制切换）
 *
 * UI控件:
 * - CircleBrushCheckBox: 圆形笔刷（Sphere）
 * - SquareBrushCheckBox: 方形笔刷（Box）
 * - FlatBrushCheckBox: 平面笔刷（Flat，单层Z）
 * - VolumetricBrushCheckBox: 立体笔刷（Volumetric，3D体积）
 * - BrushSizeSlider: 笔刷大小滑块
 * - BrushSizeText: 笔刷大小数字显示
 */
UCLASS()
class VOXEL_API UM7_BrushParamsModule : public UToolDetailModuleBase
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
	

	// === UI 控件绑定 ===

	/** 圆形笔刷复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* CircleBrushCheckBox;

	/** 方形笔刷复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* SquareBrushCheckBox;

	/** 平面笔刷复选框（Flat，单层Z） */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* FlatBrushCheckBox;

	/** 立体笔刷复选框（Volumetric，3D体积） */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* VolumetricBrushCheckBox;

	/** 笔刷大小滑块 */
	UPROPERTY(meta = (BindWidget))
	USlider* BrushSizeSlider;

	/** 笔刷大小数字显示 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* BrushSizeText;

	// === 事件处理 ===

	/** 圆形笔刷改变 */
	UFUNCTION()
	void OnCircleBrushChanged(bool bChecked);

	/** 方形笔刷改变 */
	UFUNCTION()
	void OnSquareBrushChanged(bool bChecked);

	/** 平面笔刷改变 */
	UFUNCTION()
	void OnFlatBrushChanged(bool bChecked);

	/** 立体笔刷改变 */
	UFUNCTION()
	void OnVolumetricBrushChanged(bool bChecked);

	/** 笔刷大小改变 */
	UFUNCTION()
	void OnBrushSizeChanged(float Value);

private:
	/** 设置笔刷形状 */
	void SetBrushShape(EBatchShapeType NewShape);

	/** 设置笔刷维度 */
	void SetBrushDimension(EBrushDimension NewDimension);

	/** 更新笔刷形状复选框状态 */
	void UpdateBrushShapeCheckBoxes(EBatchShapeType CurrentShape);

	/** 更新笔刷维度复选框状态 */
	void UpdateBrushDimensionCheckBoxes(EBrushDimension CurrentDimension);

	/** 本地缓存（仅用于滑块显示，权威值在 StateComponent.BrushSize） */
	float CurrentBrushSize = 1.0f;

	/** 标记是否已绑定 OnBuildModeParamsUpdated 委托 */
	bool bDelegatesBound = false;
};
