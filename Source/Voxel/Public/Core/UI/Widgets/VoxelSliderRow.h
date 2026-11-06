// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelWidgetBase.h"
#include "VoxelSliderRow.generated.h"

class UVoxelTextBlock;
class UVoxelSlider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVoxelSliderRowValueChangedSignature, float, Value);

/**
 * 统一滑条行：标签 + 滑条 + 数值显示（组合控件）
 *
 * 纯展示/转发，不含业务：外部订阅 OnValueChanged 后自行调业务逻辑层方法。
 * WBP 中放置同名控件（BindWidget 自动绑定，全部可选）：
 * - LabelText (UVoxelTextBlock)：行标签，ItemLabel 角色
 * - Slider (UVoxelSlider)
 * - ValueText (UVoxelTextBlock)：数值，DataValue 角色
 */
UCLASS()
class VOXEL_API UVoxelSliderRow : public UVoxelWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	// === 配置（设计期可见） ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Row")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Row")
	float MinValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Row")
	float MaxValue = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Row")
	bool bShowValueText = true;

	// === API ===

	UFUNCTION(BlueprintCallable, Category = "Voxel|Row")
	void SetValue(float InValue);

	UFUNCTION(BlueprintPure, Category = "Voxel|Row")
	float GetValue() const;

	UFUNCTION(BlueprintCallable, Category = "Voxel|Row")
	void SetLabelText(const FText& InText);

	/** 滑条值改变（转发内部 Slider 事件） */
	UPROPERTY(BlueprintAssignable, Category = "Voxel|Row")
	FVoxelSliderRowValueChangedSignature OnValueChanged;

protected:
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UVoxelTextBlock> LabelText;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UVoxelSlider> Slider;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UVoxelTextBlock> ValueText;

	UFUNCTION()
	void HandleSliderChanged(float Value);

private:
	void RefreshValueText(float Value);

	bool bEventsBound = false;
};
