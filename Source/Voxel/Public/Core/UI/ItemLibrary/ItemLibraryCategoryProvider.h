// Copyright (c) 2026 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Core/UI/ItemLibrary/ItemLibraryEntry.h"
#include "ItemLibraryCategoryProvider.generated.h"

/**
 * 分类提供者基类。
 * 由 CategoryProvider 决定列表分类规则与每个 Entry 的归属。
 */
UCLASS(Abstract, EditInlineNew, Blueprintable, BlueprintType)
class VOXEL_API UItemLibraryCategoryProvider : public UObject
{
	GENERATED_BODY()

public:
	/** 分类根 Tag（决定 Common 与动态 Tab 的语义） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemLibrary|Category")
	FGameplayTag RootTag;

	/** 生成分类列表 */
	UFUNCTION(BlueprintNativeEvent, Category = "ItemLibrary|Category")
	TArray<FItemLibraryCategory> GenerateCategories(const TArray<FItemLibraryEntry>& AllEntries) const;
	virtual TArray<FItemLibraryCategory> GenerateCategories_Implementation(const TArray<FItemLibraryEntry>& AllEntries) const { return {}; }

	/** 判断 Entry 是否属于某分类 */
	UFUNCTION(BlueprintNativeEvent, Category = "ItemLibrary|Category")
	bool BelongsToCategory(const FItemLibraryEntry& Entry, const FItemLibraryCategory& Category) const;
	virtual bool BelongsToCategory_Implementation(const FItemLibraryEntry& Entry, const FItemLibraryCategory& Category) const { return false; }
};

/**
 * Tag 层级分类：
 *   Common = EntryTag 是 RootTag 的直接子级
 *   动态 Tab = RootTag 的所有直接子标签（去重）
 *   归属 = EntryTag.MatchesTag(Category.CategoryTag)
 *
 * 适用：M1 / M1b 建造库与模板库。
 */
UCLASS()
class VOXEL_API UTagHierarchyCategoryProvider : public UItemLibraryCategoryProvider
{
	GENERATED_BODY()

public:
	virtual TArray<FItemLibraryCategory> GenerateCategories_Implementation(const TArray<FItemLibraryEntry>& AllEntries) const override;
	virtual bool BelongsToCategory_Implementation(const FItemLibraryEntry& Entry, const FItemLibraryCategory& Category) const override;
};
