// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/EditableTextBox.h"
#include "VoxelEditableTextBox.generated.h"

/**
 * 统一输入框：自动应用主题 InputStyle（底色/聚焦色/文字色/内边距）+ Body 角色字体
 * IS-A UEditableTextBox：存量 WBP 换装只需把控件替换为本类（控件名不变），C++ BindWidget 不受影响。
 */
UCLASS()
class VOXEL_API UVoxelEditableTextBox : public UEditableTextBox
{
	GENERATED_BODY()

public:
	/** false 时不应用主题样式（保留手工设置，用于特例） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Style")
	bool bUseThemeStyle = true;

protected:
	virtual void SynchronizeProperties() override;

	/** 从主题应用输入框样式 */
	void ApplyThemeStyle();
};
