// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "Core/UI/Item/ItemLibraryWidgetBase.h"
#include "Core/UI/ItemSlotWidget.h"
#include "Core/UI/ItemLibrary/ItemLibraryEntry.h"
#include "Core/UI/ItemLibrary/VoxelItemDataSource.h"
#include "Core/UI/ItemLibrary/ItemLibraryCategoryProvider.h"
#include "M1_BuildLibraryModule.generated.h"

struct FInteractableData;
class UTextBlock;
class UPanelWidget;

/**
 * M1: 建造库模块（方块库）
 *
 * 职责：显示手持物品、方块列表、搜索和分页功能
 * 显示条件: Tool.Edit.* (编辑工具，排除 Template)
 *
 * 架构：
 * - 继承 UToolDetailModuleBase：作为工具详情模块显示
 * - 包含 ItemLibrary：通用列表组件实例（蓝图内置 BindWidget）
 *
 * UI控件:
 * - HandItemSlot: 手持物品槽（蓝图内置，Optional）
 * - ItemLibrary: 通用列表组件实例（蓝图内置，BindWidget）
 * - 选中物品信息显示（工具模块专属）
 */
UCLASS()
class VOXEL_API UM1_BuildLibraryModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	// ==================== UToolDetailModuleBase 接口 ====================

	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	
	virtual void NativeDestruct() override;

	// ==================== 工具模块专属 UI ====================

	/** 手持物品槽实例（蓝图内置，BindWidget） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UItemSlotWidget* HandItemSlot;

	/** 【新增】当前选中方块的名字显示 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* SelectedItemNameText;

	/** 【新增】当前选中方块的价格显示 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* SelectedItemPriceText;

	/** 【新增】当前选中方块的拥有数量显示 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* SelectedItemQuantityText;

	// ==================== 道具列表组件（蓝图内置实例）====================

	/**
	 * 道具列表组件实例（蓝图内直接放置，BindWidget 绑定）
	 * 不再运行时 CreateWidget，布局在蓝图内可视化编辑
	 */
	UPROPERTY(meta = (BindWidget))
	UItemLibraryWidgetBase* ItemLibrary;


private:
	/** 数据源类（默认 UVoxelItemDataSource） */
	UPROPERTY(EditDefaultsOnly, Category = "M1|Data")
	TSubclassOf<UVoxelItemDataSource> DataSourceClass = UVoxelItemDataSource::StaticClass();

	/** 分类提供者类（默认 UTagHierarchyCategoryProvider） */
	UPROPERTY(EditDefaultsOnly, Category = "M1|Data")
	TSubclassOf<UItemLibraryCategoryProvider> CategoryProviderClass;

	/** 查询根 Tag（仅作为初始值；运行时由当前工具决定，见 UpdateFromToolState） */
	UPROPERTY(EditDefaultsOnly, Category = "M1|Data")
	FGameplayTag QueryTag;

	/** 缓存的数据源实例（NativeConstruct 创建，UpdateFromToolState 更新其 QueryTag） */
	UPROPERTY(Transient)
	UVoxelItemDataSource* CachedDataSource = nullptr;

	/** 缓存的分类提供者实例 */
	UPROPERTY(Transient)
	UItemLibraryCategoryProvider* CachedCategoryProvider = nullptr;

	/** 根据当前工具返回应当使用的查询 Tag */
	FGameplayTag GetQueryTagForTool(FGameplayTag ToolTag) const;

	/** 【新增】更新当前选中方块的信息显示 */
	void UpdateSelectedItemInfo(const FInteractableData& HandData);

	// ==================== 事件处理函数 ====================

	/** 处理建造模式参数更新 */
	UFUNCTION()
	void HandleBuildModeParamsUpdated();

private:
	/** 委托是否已绑定 */
	bool bDelegatesBound = false;
};
