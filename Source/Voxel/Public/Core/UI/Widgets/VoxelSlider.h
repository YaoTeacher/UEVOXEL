// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Slider.h"
#include "VoxelSlider.generated.h"

/**
 * 统一滑条：自动应用主题 SliderStyle（轨道厚度/颜色、把手颜色/贴图）
 * IS-A USlider：存量 WBP 换装只需把控件替换为本类（控件名不变），C++ BindWidget 不受影响。
 */
UCLASS()
class VOXEL_API UVoxelSlider : public USlider
{
	GENERATED_BODY()

public:
	/** false 时不应用主题样式（保留手工设置，用于特例） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Style")
	bool bUseThemeStyle = true;

protected:
	virtual void SynchronizeProperties() override;

	/** 从主题应用滑条样式 */
	void ApplyThemeStyle();
};
