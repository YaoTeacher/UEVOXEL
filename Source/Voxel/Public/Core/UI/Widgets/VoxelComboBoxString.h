// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ComboBoxString.h"
#include "VoxelComboBoxString.generated.h"

/**
 * 统一下拉选项：自动应用主题 ComboStyle（行底色/悬停色/文字色/内边距/列表高度）+ Body 角色字体
 * IS-A UComboBoxString：存量 WBP 换装只需把控件替换为本类（控件名不变），C++ BindWidget 不受影响。
 */
UCLASS()
class VOXEL_API UVoxelComboBoxString : public UComboBoxString
{
	GENERATED_BODY()

public:
	/** false 时不应用主题样式（保留手工设置，用于特例） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Style")
	bool bUseThemeStyle = true;

protected:
	/** SComboBox 仅在 RebuildWidget 时消费 Font/ItemStyle 等成员，主题必须在构建前的 PreConstruct 应用 */
	virtual void NativePreConstruct() override;

	/** 从主题应用下拉样式 */
	void ApplyThemeStyle();
};
