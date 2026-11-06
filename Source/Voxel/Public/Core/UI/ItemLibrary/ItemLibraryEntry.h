// Copyright (c) 2026 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ItemLibraryEntry.generated.h"

/**
 * 通用列表条目（视图模型）
 * 三种业务（建造/模板/区域）共用此结构，UI 层不直接持有 FVoxelItemBase / FAreaTypeBase。
 */
USTRUCT(BlueprintType)
struct VOXEL_API FItemLibraryEntry
{
	GENERATED_BODY()

	/** 身份与分类合一的 Tag（沿用项目惯例） */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	FGameplayTag EntryTag;

	/** 物品运行时句柄（对应 FVoxelItemBase::HandleId） */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	int32 HandleId = INDEX_NONE;

	/** 显示名称 */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	FText DisplayName;

	/** 描述（可选） */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	FText Description;

	/** 图标（硬指针，与项目 FVoxelVisualFragment 对齐） */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 调色 */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	FLinearColor TintColor = FLinearColor::White;

	/** 数量（背包用，建造库置 -1） */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	int32 Count = INDEX_NONE;

	/** 价格（建造库用，其它置 -1） */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	int32 Price = INDEX_NONE;

	/** 灰显（材料不足等） */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	bool bDisabled = false;
};

/**
 * 列表分类
 */
USTRUCT(BlueprintType)
struct VOXEL_API FItemLibraryCategory
{
	GENERATED_BODY()

	/** 唯一 ID（字符串，便于序列化与对外切换） */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	FString CategoryId;

	/** 显示名称 */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	FText DisplayName;

	/** 分类对应 Tag（用于 TagUIDef 取图标/颜色，及匹配规则） */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	FGameplayTag CategoryTag;

	/** 排序（小→前） */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	int32 SortOrder = 0;

	/** 是否默认选中 */
	UPROPERTY(BlueprintReadWrite, Category = "ItemLibrary")
	bool bIsDefault = false;
};
