// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelWidgetBase.h"
#include "VoxelSearchBox.generated.h"

class UImage;
class UVoxelEditableTextBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVoxelSearchBoxTextChangedSignature, const FText&, Text);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVoxelSearchBoxCommittedSignature, const FText&, Text, TEnumAsByte<ETextCommit::Type>, CommitMethod);

/**
 * 统一搜索框：搜索图标 + 输入框（组合控件）
 *
 * 纯展示/转发，不含业务：外部订阅 OnSearchTextChanged 后自行调业务逻辑层方法做过滤。
 * WBP 中放置同名控件（BindWidget 自动绑定，全部可选）：
 * - SearchIcon (UImage)：图标与尺寸取主题 SearchIconTexture/SearchIconSize，主题无贴图时自动隐藏
 * - InputBox (UVoxelEditableTextBox)：HintText 缺省为「搜索...」
 */
UCLASS()
class VOXEL_API UVoxelSearchBox : public UVoxelWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	// === 配置（设计期可见） ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Search")
	FText HintText;

	// === API ===

	UFUNCTION(BlueprintCallable, Category = "Voxel|Search")
	void SetText(const FText& InText);

	UFUNCTION(BlueprintPure, Category = "Voxel|Search")
	FText GetText() const;

	/** 搜索词实时变化（转发内部 InputBox 事件，用于即时过滤） */
	UPROPERTY(BlueprintAssignable, Category = "Voxel|Search")
	FVoxelSearchBoxTextChangedSignature OnSearchTextChanged;

	/** 搜索词提交（回车/失焦） */
	UPROPERTY(BlueprintAssignable, Category = "Voxel|Search")
	FVoxelSearchBoxCommittedSignature OnSearchTextCommitted;

protected:
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UImage> SearchIcon;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UVoxelEditableTextBox> InputBox;

	UFUNCTION()
	void HandleTextChanged(const FText& Text);

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

private:
	bool bEventsBound = false;
};
