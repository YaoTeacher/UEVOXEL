// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "Core/UI/Item/ItemLibraryWidgetBase.h"
#include "Core/UI/ItemLibrary/ItemLibraryEntry.h"
#include "Core/UI/ItemLibrary/ItemLibraryDataSource.h"
#include "Core/UI/ItemLibrary/ItemLibraryCategoryProvider.h"
#include "M1b_TemplateLibraryModule.generated.h"

struct FInteractableData;
class UTextBlock;
class UPanelWidget;

/**
 * M1b: 模板库模块
 * 显示条件: Tool.Edit.Template
 */
UCLASS()
class VOXEL_API UM1b_TemplateLibraryModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* SelectedTemplateNameText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* MaterialRequirementsText;

	/** 道具列表组件实例（蓝图内直接放置，BindWidget 绑定） */
	UPROPERTY(meta = (BindWidget))
	UItemLibraryWidgetBase* ItemLibrary;

private:
	UPROPERTY(EditDefaultsOnly, Category = "M1b|Data")
	TSubclassOf<UItemLibraryDataSource> DataSourceClass;

	UPROPERTY(EditDefaultsOnly, Category = "M1b|Data")
	TSubclassOf<UItemLibraryCategoryProvider> CategoryProviderClass;

	UPROPERTY(EditDefaultsOnly, Category = "M1b|Data")
	FGameplayTag QueryTag;

	void UpdateSelectedTemplateInfo(const FInteractableData& HandData);

	bool CheckMaterialRequirements(FGameplayTag TemplateTag);

	// ==================== 事件处理函数 ====================

	/** 处理建造模式参数更新 */
	UFUNCTION()
	void HandleBuildModeParamsUpdated();

	/** 委托是否已绑定 */
	bool bDelegatesBound = false;
};
