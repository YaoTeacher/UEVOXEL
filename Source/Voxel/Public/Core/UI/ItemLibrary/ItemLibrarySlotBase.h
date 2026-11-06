// Copyright (c) 2026 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Core/UI/ItemLibrary/ItemLibraryEntry.h"
#include "ItemLibrarySlotBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemLibrarySlotClicked, const FItemLibraryEntry&, Entry);

/**
 * 通用列表 Slot 基类。
 * 子类负责具体视觉呈现（Icon/数量/价格...）。
 */
UCLASS(Abstract, Blueprintable)
class VOXEL_API UItemLibrarySlotBase : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 由 ItemLibraryWidgetBase 在创建后立即调用 */
	UFUNCTION(BlueprintNativeEvent, Category = "ItemLibrary|Slot")
	void InitFromEntry(const FItemLibraryEntry& Entry);
	virtual void InitFromEntry_Implementation(const FItemLibraryEntry& Entry) { CurrentEntry = Entry; }

	/** 选中态视觉（可选） */
	UFUNCTION(BlueprintNativeEvent, Category = "ItemLibrary|Slot")
	void SetSelected(bool bSelected);
	virtual void SetSelected_Implementation(bool bSelected) {}

	/** 由子类点击时广播 */
	UPROPERTY(BlueprintAssignable, Category = "ItemLibrary|Slot")
	FOnItemLibrarySlotClicked OnSlotClicked;

	/** 当前条目快照 */
	UPROPERTY(BlueprintReadOnly, Category = "ItemLibrary|Slot")
	FItemLibraryEntry CurrentEntry;
};
