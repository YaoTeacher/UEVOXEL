#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "GameplayTagContainer.h"
#include "Core/UI/Item/ItemLibraryWidgetBase.h"
#include "Core/UI/ItemLibrary/ItemLibraryEntry.h"
#include "Core/UI/ItemLibrary/AreaTypeDataSource.h"
#include "Core/UI/ItemLibrary/ItemLibraryCategoryProvider.h"
#include "M12b_AreaTypeLibraryModule.generated.h"

class UTextBlock;
class UPanelWidget;

/**
 * M12b: 区域类型库模块
 * 显示条件: Tool.AreaEdit
 *
 * 改造后：完全复用 UItemLibraryWidgetBase + UAreaTypeDataSource + UTagHierarchyCategoryProvider。
 */
UCLASS()
class VOXEL_API UM12b_AreaTypeLibraryModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UTextBlock* SelectedAreaTypeNameText;

	/** 道具列表组件实例（蓝图内直接放置，BindWidget 绑定） */
	UPROPERTY(meta = (BindWidget))
	UItemLibraryWidgetBase* ItemLibrary;

private:
	UPROPERTY(EditDefaultsOnly, Category = "M12b|Data")
	TSubclassOf<UAreaTypeDataSource> DataSourceClass = UAreaTypeDataSource::StaticClass();

	UPROPERTY(EditDefaultsOnly, Category = "M12b|Data")
	TSubclassOf<UItemLibraryCategoryProvider> CategoryProviderClass;

	/** 区域分类根 Tag（默认 Area.Type） */
	UPROPERTY(EditDefaultsOnly, Category = "M12b|Data")
	FGameplayTag AreaRootTag;

	// ==================== 事件处理函数 ====================

	/** 处理建造模式参数更新 */
	UFUNCTION()
	void HandleBuildModeParamsUpdated();

	/** 从 SC 读取区域类型并更新选中区域类型名称显示 */
	void UpdateSelectedAreaTypeInfo(const FGameplayTag& AreaTypeTag);

	/** 委托是否已绑定 */
	bool bDelegatesBound = false;
};
