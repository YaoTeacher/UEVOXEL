// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Core/UI/ItemLibrary/ItemLibraryEntry.h"
#include "ItemLibraryWidgetBase.generated.h"

class UPanelWidget;
class UWrapBox;
class UEditableTextBox;
class UButton;
class UTextBlock;
class UItemLibraryDataSource;
class UItemLibraryCategoryProvider;
class UItemLibraryTabButtonBase;
class UItemLibrarySlotBase;
class AVoxelPlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemLibraryEntryClicked, const FItemLibraryEntry&, Entry);

/**
 * 通用列表组件（唯一，不拆分子组件 BP）。
 *
 * 设计：搜索框 / Tab 容器 / 列表容器 / 翻页按钮全部内联为 BindWidget 原子控件，
 *      在 WBP_BuildItemLib 蓝图内直接可视化排版。
 *      运行时只有 TabHost 内的 TabButton 列表与 ListHost 内的 Slot 列表是动态生成的。
 *      DataSource / CategoryProvider 由父模块运行时注入（因各模块 QueryTag 不同）。
 */
UCLASS()
class VOXEL_API UItemLibraryWidgetBase : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// === 配置 ===

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "ItemLibrary|Config")
	TObjectPtr<UItemLibraryDataSource> DataSource;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "ItemLibrary|Config")
	TObjectPtr<UItemLibraryCategoryProvider> CategoryProvider;

	/**
	 * Tab 按钮类（运行时在 TabHost 内生成每个分类按钮用）
	 * 用 EditAnywhere 是为了让每个 Module 在自己的蓝图里直接对 ItemLibrary 子实例配置不同的 TabButton 外观，
	 * 不用统一改 WBP_BuildItemLib 的 ClassDefaults。
	 */
	UPROPERTY(EditAnywhere, Category = "ItemLibrary|Widgets")
	TSubclassOf<UItemLibraryTabButtonBase> TabButtonClass;

	/**
	 * Slot 类（运行时在 ListHost 内生成每个条目 Slot 用）
	 * 与 TabButtonClass 同理，使用 EditAnywhere，让每个 Module 实例独立配置（建造方块用 BuildSlot，模板/区域可换专用 Slot）。
	 */
	UPROPERTY(EditAnywhere, Category = "ItemLibrary|Widgets")
	TSubclassOf<UItemLibrarySlotBase> SlotClass;

	UPROPERTY(EditAnywhere, Category = "ItemLibrary|Config")
	int32 ItemsPerPage = 20;

	/**
	 * WrapBox 的行宽阈值（px）。
	 * 当一行子项累计宽度超过此值时换行。建议设置为列数 × Slot宽度（例如 8 × 128 = 1024）。
	 * 0 表示不主动设置（使用 WBP 中 ListHost 的 WrapSize 配置或父容器宽度）。
	 */
	UPROPERTY(EditAnywhere, Category = "ItemLibrary|Config")
	float WrapSize = 1024.0f;

	/** 点击条目时是否执行 DataSource::OnEntryActivated。普通物品库为 true；纯选择器（如 M15 材质替换）应设为 false。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemLibrary|Config")
	bool bActivateDataSourceOnClick = true;

	/**
	 * 是否显示分类 Tab 并允许库组件内部切换类型。
	 * false 时 TabHost 被清空、不响应分类切换，列表显示当前 DataSource 的全部条目；
	 * 库展示什么类型完全由外部模块/工具通过设置 DataSource 的 QueryTag 决定。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemLibrary|Config")
	bool bShowCategoryTabs = true;

	// === BindWidget 原子控件（蓝图内直接放置，非运行时创建） ===

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> SearchTextBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> TabHost;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> ListHost;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PrevPageButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> NextPageButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PageNumberText;

	// === API ===

	UFUNCTION(BlueprintCallable, Category = "ItemLibrary")
	void SetDataSource(UItemLibraryDataSource* InSource);

	UFUNCTION(BlueprintCallable, Category = "ItemLibrary")
	void SetCategoryProvider(UItemLibraryCategoryProvider* InProvider);

	UFUNCTION(BlueprintCallable, Category = "ItemLibrary")
	void InitializeLibrary();

	UFUNCTION(BlueprintCallable, Category = "ItemLibrary")
	void RefreshList();

	UFUNCTION(BlueprintCallable, Category = "ItemLibrary")
	void SwitchToCategory(const FString& CategoryId);

	UPROPERTY(BlueprintAssignable, Category = "ItemLibrary")
	FOnItemLibraryEntryClicked OnEntryClicked;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UItemLibraryTabButtonBase>> TabButtons;

	UPROPERTY(Transient)
	TArray<FItemLibraryEntry> AllEntries;

	UPROPERTY(Transient)
	TArray<FItemLibraryEntry> FilteredEntries;

	UPROPERTY(Transient)
	TArray<FItemLibraryCategory> Categories;

	UPROPERTY(Transient)
	FString SelectedCategoryId;

	UPROPERTY(Transient)
	FString CurrentSearch;

	UPROPERTY(Transient)
	int32 CurrentPage = 0;

	UPROPERTY(Transient)
	int32 TotalPages = 1;

	bool bDataSourceChangedBound = false;
	FDelegateHandle DataSourceChangedHandle;

	void BindSubWidgetDelegates();
	void RebuildTabs();
	void ReloadFromDataSource();
	void ApplyFiltersAndRender();
	void RenderCurrentPage();
	int32 GetTotalPages(int32 Count) const;

	UFUNCTION()
	void HandleSearchChanged(const FText& Text);

	UFUNCTION()
	void HandlePrevPageClicked();

	UFUNCTION()
	void HandleNextPageClicked();

	UFUNCTION()
	void HandleTabButtonClicked(const FString& CategoryId);

	UFUNCTION()
	void HandleSlotClicked(const FItemLibraryEntry& Entry);

	void OnDataSourceChanged();
};
