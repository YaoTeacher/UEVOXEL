// Copyright (c) 2025 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/UI/ItemLibrary/ItemLibraryTabButtonBase.h"
#include "CategoryTabButtonWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

/**
 * 通用分类 TAB 按钮（项目内默认实现）。
 * 现继承 UItemLibraryTabButtonBase，统一通过 OnTabClicked 上抛 CategoryId。
 *
 * 兼容旧 API：SetCategoryData / SetSelected(已被基类版本覆盖) / OnSelectedStateChanged / CategoryId / GetRootButton。
 */
UCLASS(Abstract, Blueprintable)
class VOXEL_API UCategoryTabButtonWidget : public UItemLibraryTabButtonBase
{
	GENERATED_BODY()

public:
	/** [兼容] 直接设置文本与图标（会同步到 base.Category 的 DisplayName） */
	UFUNCTION(BlueprintCallable, Category = "CategoryTab")
	void SetCategoryData(const FText& InLabel, UTexture2D* InIcon = nullptr);

	/** [兼容] 旧字段：与 base.Category.CategoryId 自动同步，仍允许外部代码直接读写 */
	UPROPERTY(BlueprintReadWrite, Category = "CategoryTab")
	FString CategoryId;

	/** [兼容] 旧字段直接读 base CategoryId */
	UFUNCTION(BlueprintPure, Category = "CategoryTab")
	const FString& GetLegacyCategoryId() const { return Category.CategoryId; }

	/** 获取根按钮（蓝图常用） */
	UFUNCTION(BlueprintPure, Category = "CategoryTab")
	UButton* GetRootButton() const { return RootButton; }

	/** 选中态变化（蓝图可重写做视觉反馈） */
	UFUNCTION(BlueprintNativeEvent, Category = "CategoryTab")
	void OnSelectedStateChanged(bool bNewSelected);

	// === 基类重写 ===
	virtual void SetCategory_Implementation(const FItemLibraryCategory& InCategory) override;
	virtual void SetSelected_Implementation(bool bSelected) override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RootButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LabelText;

private:
	UFUNCTION()
	void HandleRootClicked();

	void RefreshVisualFromCategory();
};
