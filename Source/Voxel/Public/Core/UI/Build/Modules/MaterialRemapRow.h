// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "MaterialRemapRow.generated.h"

class UImage;
class UTextBlock;
class UButton;
class UConfigDataSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMaterialRemapPickRequested, FString, OriginFullId);

/**
 * M15 素材替换列表的单行控件。
 * 显示：原素材（图标+名） → 当前替换目标（图标+名），点击弹出同类选择器。
 *
 * 数据流：M15 调用 InitRow 填充视觉；点击 PickButton 广播 OnPickRequested(OriginFullId)，
 *        由 M15 弹出 ItemLibrary 选择器（QueryTag=该素材同类）。
 */
UCLASS()
class VOXEL_API UMaterialRemapRow : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 初始化行显示。
	 * @param InOriginFullId 原素材 FullId（模板内定义）
	 * @param InCurrentFullId 当前生效的素材 FullId（未替换时 == OriginFullId）
	 * @param ConfigSys 配置子系统（查图标/名称）
	 */
	UFUNCTION(BlueprintCallable, Category = "M15|MaterialRemap")
	void InitRow(const FString& InOriginFullId, const FString& InCurrentFullId, UConfigDataSubsystem* ConfigSys);

	/** 点击本行请求替换时广播（携带原素材 FullId） */
	UPROPERTY(BlueprintAssignable, Category = "M15|MaterialRemap")
	FOnMaterialRemapPickRequested OnPickRequested;

	/** 当前行对应的原素材 FullId */
	UPROPERTY(BlueprintReadOnly, Category = "M15|MaterialRemap")
	FString OriginFullId;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	

	// === UI 控件绑定（必填，与项目主流 Slot 组件一致） ===

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> OriginIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OriginNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> CurrentIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CurrentNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PickButton;

	UFUNCTION()
	void HandlePickClicked();

private:
	bool bBound = false;
};
