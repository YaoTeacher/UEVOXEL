// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "TendencyRowWidget.generated.h"

/**
 * 
 */

class UTextBlock;
class USlider;
class UConfigDataSubsystem;


UCLASS()
class UTendencyRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// --- UI 绑定 ---
	
	// 显示名称 (如 "开放性")
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LabelText;

	// 显示倾向条 (建议使用 Slider 并禁用交互，因为 Slider 天然支持从左到右滑块)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> TendencySlider;

	// 显示数值文本 (可选，如 "+0.5")
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

	// --- 核心逻辑 ---

	/**
	 * 更新倾向数据
	 * @param Tag - 数据的 Tag (用于查名字)
	 * @param Value - 范围 [-1.0, 1.0]
	 * @param ConfigSubsystem - 用于查找 Tag 的本地化名称
	 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateTendencyData(FGameplayTag Tag, float Value, UConfigDataSubsystem* ConfigSubsystem);

	/**
	 * 直接设置数据 (重载版本，用于非Tag数据)
	 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateTendencyRaw(FText InName, float Value, FLinearColor Color);
	
};
