// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "ToolIconButtonWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class AVoxelPlayerController;
class UVoxelInteractionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToolButtonClicked, UToolIconButtonWidget*, Button);

/**
 * 工具图标按钮 Widget
 * 用于工具栏的主工具和子工具显示
 */
UCLASS()
class VOXEL_API UToolIconButtonWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UToolIconButtonWidget(const FObjectInitializer& ObjectInitializer);

	// ==================== 初始化 ====================

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ==================== 工具配置 ====================

	/**
	 * 设置工具标签
	 * @param InToolTag 工具标签（Tool.*）
	 */
	UFUNCTION(BlueprintCallable, Category = "Tool|Button")
	void SetToolTag(FGameplayTag InToolTag);

	/**
	 * 获取工具标签
	 */
	FGameplayTag GetToolTag() const { return ToolTag; }

	// ==================== 选中状态 ====================

	/**
	 * 设置选中状态
	 * @param bIsSelected 是否选中
	 */
	UFUNCTION(BlueprintCallable, Category = "Tool|Button")
	void SetSelected(bool bIsSelected);

	/**
	 * 获取选中状态
	 */
	bool IsSelected() const { return bSelected; }

	// ==================== 委托 ====================

	/** 按钮点击时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Tool|Button|Events")
	FOnToolButtonClicked OnToolButtonClicked;

protected:
	// ==================== UI 组件绑定 ====================

	/** 底层按钮组件 */
	UPROPERTY(meta = (BindWidget))
	UButton* Button;

	/** 图标显示 */
	UPROPERTY(meta = (BindWidget))
	UImage* IconImage;

	/** 工具名称文本 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ToolNameText;

	// /** 选中边框（可选） */
	// UPROPERTY(meta = (BindWidget, OptionalWidget))
	// UWidget* SelectedBorder;

private:
	// ==================== 数据成员 ====================

	/** 工具标签 */
	FGameplayTag ToolTag;

	/** 选中状态 */
	bool bSelected=false;

	/** 帧计数：第 2 帧才做几何诊断（首帧子控件 CachedGeometry 尚未由 paint 填充，读了必为 0） */
	int32 TickFrameCount = 0;

	// ==================== 事件处理 ====================

	/**
	 * 按钮点击处理
	 * 实现层级导航：
	 * - 如果按钮未被选中：切换到按钮的标签
	 * - 如果按钮已被选中：
	 *   - 获取按钮标签的父级标签
	 *   - 如果父级是 Tool（根标签）：切换到 EmptyTag（取消选中）
	 *   - 如果父级不是 Tool：切换到父级标签（返回上一级）
	 */
	UFUNCTION()
	void OnButtonClicked();

	/**
	 * 更新视觉状态
	 */
	void UpdateVisualState();

	/**
	 * 获取标签的直接父标签
	 * @param ChildTag 子标签
	 * @return 父标签，如果没有则返回 EmptyTag
	 */
	FGameplayTag GetParentTag(FGameplayTag ChildTag) const;
};
