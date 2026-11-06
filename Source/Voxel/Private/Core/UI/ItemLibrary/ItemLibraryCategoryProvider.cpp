// Copyright (c) 2026 zolnoor. All rights reserved.

#include "Core/UI/ItemLibrary/ItemLibraryCategoryProvider.h"

namespace
{
	const FString CommonCategoryId = TEXT("Common");
	const FString AllCategoryId = TEXT("All");

	FItemLibraryCategory MakeCommonCategory()
	{
		FItemLibraryCategory C;
		C.CategoryId = CommonCategoryId;
		C.DisplayName = NSLOCTEXT("ItemLibrary", "Common", "通用");
		C.SortOrder = 0;
		C.bIsDefault = true;
		return C;
	}

	FItemLibraryCategory MakeAllCategory()
	{
		FItemLibraryCategory C;
		C.CategoryId = AllCategoryId;
		C.DisplayName = NSLOCTEXT("ItemLibrary", "All", "全部");
		C.SortOrder = 0;
		C.bIsDefault = true;
		return C;
	}

	FItemLibraryCategory MakeTagCategory(const FGameplayTag& Tag, int32 Order)
	{
		FItemLibraryCategory C;
		C.CategoryId = Tag.ToString();
		C.DisplayName = FText::FromName(Tag.GetTagName());
		C.CategoryTag = Tag;
		C.SortOrder = Order;
		C.bIsDefault = false;
		return C;
	}
}

// ===== UTagHierarchyCategoryProvider =====

TArray<FItemLibraryCategory> UTagHierarchyCategoryProvider::GenerateCategories_Implementation(const TArray<FItemLibraryEntry>& AllEntries) const
{
	TArray<FItemLibraryCategory> Categories;
	// 第一个 Tab 为"全部"：不过滤，展示所有 Entry
	Categories.Add(MakeAllCategory());

	if (!RootTag.IsValid())
	{
		return Categories;
	}

	// 收集 RootTag 的所有直接子级 Tag（来自 Entries 自身存在的 Tag 路径）
	TSet<FGameplayTag> DirectChildren;
	for (const FItemLibraryEntry& Entry : AllEntries)
	{
		if (!Entry.EntryTag.IsValid() || !Entry.EntryTag.MatchesTag(RootTag) || Entry.EntryTag == RootTag)
		{
			continue;
		}
		// 截取 EntryTag 中 "RootTag.X" 这一段作为分类 Tag
		const FString EntryStr = Entry.EntryTag.ToString();
		const FString RootStr = RootTag.ToString();
		if (!EntryStr.StartsWith(RootStr + TEXT(".")))
		{
			continue;
		}
		const FString Remainder = EntryStr.Mid(RootStr.Len() + 1);
		FString FirstSegment;
		if (!Remainder.Split(TEXT("."), &FirstSegment, nullptr))
		{
			FirstSegment = Remainder;
		}
		const FString CategoryTagStr = RootStr + TEXT(".") + FirstSegment;
		const FGameplayTag CategoryTag = FGameplayTag::RequestGameplayTag(FName(*CategoryTagStr), false);
		if (CategoryTag.IsValid())
		{
			DirectChildren.Add(CategoryTag);
		}
	}

	int32 Order = 1;
	TArray<FGameplayTag> Sorted = DirectChildren.Array();
	Sorted.Sort([](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.ToString() < B.ToString();
	});
	for (const FGameplayTag& Tag : Sorted)
	{
		Categories.Add(MakeTagCategory(Tag, Order++));
	}
	return Categories;
}

bool UTagHierarchyCategoryProvider::BelongsToCategory_Implementation(const FItemLibraryEntry& Entry, const FItemLibraryCategory& Category) const
{
	if (Category.CategoryId == AllCategoryId)
	{
		// "全部" Tab：展示所有 Entry，不做任何过滤
		return true;
	}
	if (!Category.CategoryTag.IsValid())
	{
		return false;
	}
	return Entry.EntryTag.MatchesTag(Category.CategoryTag);
}
