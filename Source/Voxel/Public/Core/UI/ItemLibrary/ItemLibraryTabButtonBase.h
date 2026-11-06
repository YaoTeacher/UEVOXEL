// Copyright (c) 2026 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Core/UI/ItemLibrary/ItemLibraryEntry.h"
#include "ItemLibraryTabButtonBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemLibraryTabClicked, const FString&, CategoryId);

/**
 * 通用列表 Tab 按钮基类。
 * 子类负责按下视觉。
 */
UCLASS(Abstract, Blueprintable)
class VOXEL_API UItemLibraryTabButtonBase : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 由 TabContainer 调用 */
	UFUNCTION(BlueprintNativeEvent, Category = "ItemLibrary|Tab")
	void SetCategory(const FItemLibraryCategory& InCategory);
	virtual void SetCategory_Implementation(const FItemLibraryCategory& InCategory) { Category = InCategory; }

	UFUNCTION(BlueprintNativeEvent, Category = "ItemLibrary|Tab")
	void SetSelected(bool bSelected);
	virtual void SetSelected_Implementation(bool bSelected) { bIsSelected = bSelected; }

	UFUNCTION(BlueprintPure, Category = "ItemLibrary|Tab")
	const FString& GetCategoryId() const { return Category.CategoryId; }

	UFUNCTION(BlueprintPure, Category = "ItemLibrary|Tab")
	bool IsSelected() const { return bIsSelected; }

	/** 子类点击时广播 CategoryId */
	UPROPERTY(BlueprintAssignable, Category = "ItemLibrary|Tab")
	FOnItemLibraryTabClicked OnTabClicked;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "ItemLibrary|Tab")
	FItemLibraryCategory Category;

	UPROPERTY(BlueprintReadOnly, Category = "ItemLibrary|Tab")
	bool bIsSelected = false;
};
