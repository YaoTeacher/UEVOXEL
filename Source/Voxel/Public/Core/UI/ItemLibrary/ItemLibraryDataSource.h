// Copyright (c) 2026 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/UI/ItemLibrary/ItemLibraryEntry.h"
#include "ItemLibraryDataSource.generated.h"

class AVoxelPlayerController;

/**
 * 通用列表数据源基类。
 *
 * 设计：
 * - 用 UObject 抽象基类（而非 UInterface），便于 UPROPERTY(Instanced) 直接挂在容器上。
 * - 子类必须重写 GetAllEntries / OnEntryActivated。
 * - 可变源（如背包）通过 NotifyDataChanged() 上抛 OnDataSourceChanged，UI 自动重拉。
 */
UCLASS(Abstract, EditInlineNew, Blueprintable, BlueprintType)
class VOXEL_API UItemLibraryDataSource : public UObject
{
	GENERATED_BODY()

public:
	/** 当数据源内容发生变化时广播（无参） */
	DECLARE_MULTICAST_DELEGATE(FOnDataSourceChanged);
	FOnDataSourceChanged OnDataSourceChanged;

	/** 拉取全部条目（一次性快照） */
	UFUNCTION(BlueprintNativeEvent, Category = "ItemLibrary|DataSource")
	TArray<FItemLibraryEntry> GetAllEntries() const;
	virtual TArray<FItemLibraryEntry> GetAllEntries_Implementation() const { return {}; }

	/** 搜索关键字匹配（默认按 DisplayName + EntryTag.ToString 大小写不敏感包含） */
	UFUNCTION(BlueprintNativeEvent, Category = "ItemLibrary|DataSource")
	bool MatchesSearch(const FItemLibraryEntry& Entry, const FString& Keyword) const;
	virtual bool MatchesSearch_Implementation(const FItemLibraryEntry& Entry, const FString& Keyword) const;

	/** 点击副作用入口；建造库->SetItemInHand，区域库->SetAreaType，背包->库存操作 */
	UFUNCTION(BlueprintNativeEvent, Category = "ItemLibrary|DataSource")
	void OnEntryActivated(const FItemLibraryEntry& Entry, AVoxelPlayerController* PC);
	virtual void OnEntryActivated_Implementation(const FItemLibraryEntry& Entry, AVoxelPlayerController* PC) {}

protected:
	/** 子类在数据变化时调用 */
	void NotifyDataChanged() { OnDataSourceChanged.Broadcast(); }
};
