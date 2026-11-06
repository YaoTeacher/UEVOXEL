// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelWidgetBase.h"
#include "VoxelComboRow.generated.h"

class UVoxelTextBlock;
class UVoxelComboBoxString;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVoxelComboRowSelectionChangedSignature, FString, SelectedItem, TEnumAsByte<ESelectInfo::Type>, SelectionType);

/**
 * 统一下拉行：标签 + 下拉选项（组合控件）
 *
 * 纯展示/转发，不含业务：外部订阅 OnSelectionChanged 后自行调业务逻辑层方法。
 * WBP 中放置同名控件（BindWidget 自动绑定，全部可选）：
 * - LabelText (UVoxelTextBlock)：行标签，ItemLabel 角色
 * - ComboBox (UVoxelComboBoxString)
 */
UCLASS()
class VOXEL_API UVoxelComboRow : public UVoxelWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	// === 配置（设计期可见） ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Row")
	FText Label;

	/** 设计期预置选项（运行时可用 SetOptions 替换） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Row")
	TArray<FString> DefaultOptions;

	// === API ===

	UFUNCTION(BlueprintCallable, Category = "Voxel|Row")
	void SetOptions(const TArray<FString>& InOptions);

	UFUNCTION(BlueprintCallable, Category = "Voxel|Row")
	void SetSelectedOption(FString Option);

	UFUNCTION(BlueprintPure, Category = "Voxel|Row")
	FString GetSelectedOption() const;

	UFUNCTION(BlueprintCallable, Category = "Voxel|Row")
	void SetLabelText(const FText& InText);

	/** 选中项改变（转发内部 ComboBox 事件） */
	UPROPERTY(BlueprintAssignable, Category = "Voxel|Row")
	FVoxelComboRowSelectionChangedSignature OnSelectionChanged;

protected:
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UVoxelTextBlock> LabelText;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UVoxelComboBoxString> ComboBox;

	UFUNCTION()
	void HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

private:
	bool bEventsBound = false;
};
