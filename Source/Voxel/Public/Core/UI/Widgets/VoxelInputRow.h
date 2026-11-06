// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelWidgetBase.h"
#include "VoxelInputRow.generated.h"

class UVoxelTextBlock;
class UVoxelEditableTextBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVoxelInputRowCommittedSignature, const FText&, Text, TEnumAsByte<ETextCommit::Type>, CommitMethod);

/**
 * 统一输入框行：标签 + 输入框（组合控件）
 *
 * 纯展示/转发，不含业务：外部订阅 OnTextCommitted 后自行调业务逻辑层方法。
 * WBP 中放置同名控件（BindWidget 自动绑定，全部可选）：
 * - LabelText (UVoxelTextBlock)：行标签，ItemLabel 角色
 * - InputBox (UVoxelEditableTextBox)
 */
UCLASS()
class VOXEL_API UVoxelInputRow : public UVoxelWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	// === 配置（设计期可见） ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Row")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Row")
	FText HintText;

	// === API ===

	UFUNCTION(BlueprintCallable, Category = "Voxel|Row")
	void SetText(const FText& InText);

	UFUNCTION(BlueprintPure, Category = "Voxel|Row")
	FText GetText() const;

	UFUNCTION(BlueprintCallable, Category = "Voxel|Row")
	void SetLabelText(const FText& InText);

	/** 输入提交（转发内部 InputBox 事件） */
	UPROPERTY(BlueprintAssignable, Category = "Voxel|Row")
	FVoxelInputRowCommittedSignature OnTextCommitted;

protected:
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UVoxelTextBlock> LabelText;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UVoxelEditableTextBox> InputBox;

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

private:
	bool bEventsBound = false;
};
