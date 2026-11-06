// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Image.h"
#include "Core/UI/ItemLibrary/ItemLibrarySlotBase.h"
#include "BuildSlotWidget.generated.h"

class UButton;

/**
 * 建造/区域选择插槽控件（项目内默认 Slot 实现）。
 * 继承 UItemLibrarySlotBase，点击后只广播 OnSlotClicked，不再持有 PlayerController。
 * 副作用（SetItemInHand / SetAreaType）由 DataSource::OnEntryActivated 处理。
 */
UCLASS()
class VOXEL_API UBuildSlotWidget : public UItemLibrarySlotBase
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeOnInitialized() override;

	/** [兼容] 旧 API：仅设 Tag 与图标，会构造一个最小 Entry 转入 InitFromEntry */
	UFUNCTION(BlueprintCallable, Category = "Build")
	void InitSlotData(FGameplayTag InTag, TSoftObjectPtr<UTexture2D> InIcon);

	// === 基类重写 ===
	virtual void InitFromEntry_Implementation(const FItemLibraryEntry& Entry) override;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ClickBtn;

	/** [兼容] 旧字段，等价于 CurrentEntry.EntryTag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	FGameplayTag ItemTag;

	UFUNCTION()
	void OnClickButton();

	/** 输入诊断：按下（Slate 命中即触发，与 OnClicked 无关） */
	UFUNCTION()
	void OnPressedDebug();

	/** 输入诊断：释放 */
	UFUNCTION()
	void OnReleasedDebug();
};
