// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "StatRowWidget.generated.h"

struct FGameplayTag;
class UImage;
class UTextBlock;
/**
 * 
 */
UCLASS()
class UStatRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

	// =============================================================
	// [UI 组件] 需要在 UMG Designer 中创建同名控件
	// =============================================================
public:
	// 1. 显示属性名称 (例如 "Strength")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatNameText;

	// 2. 显示属性数值 (例如 "100.0")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatValueText;

	// 3. (可选) 显示属性图标
	// OptionalWidget 表示如果 UMG 里没放这个 Image 也不会报错
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UImage> StatIcon;

	// =============================================================
	// [逻辑接口]
	// =============================================================

	/**
	 * 初始化行数据
	 * @param StatTag - 属性的 Tag (用于去 Subsystem 查名字/颜色)
	 * @param Value   - 具体数值
	 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateStat(FGameplayTag StatTag, float Value);
	
};
