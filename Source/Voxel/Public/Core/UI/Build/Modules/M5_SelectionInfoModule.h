// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "M5_SelectionInfoModule.generated.h"

class UTextBlock;
class UButton;
class UVoxelDialogBase;

/**
 * M5: 选区信息模块
 * 职责：显示当前选区统计信息，提供清空和保存功能
 *
 * 显示条件: Tool.Selection.* (选区工具)
 *
 * UI控件:
 * - SelectionStatusText: 选区状态文本
 * - VoxelCountText: 体素数量
 * - SizeText: 选区尺寸
 * - CenterText: 中心点坐标
 * - MinCoordText: 最小坐标
 * - MaxCoordText: 最大坐标
 * - ClearSelectionButton: 清空选区按钮
 * - SaveAsTemplateButton: 保存为模板按钮（点击后通过 AVoxelHUD.PushDialog 弹出模板保存弹窗）
 */
UCLASS()
class VOXEL_API UM5_SelectionInfoModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	UM5_SelectionInfoModule();

	// === 模块接口 ===

	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleBuildModeParamsUpdated();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	

	// === UI 控件绑定 ===

	/** 选区状态文本 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* SelectionStatusText;

	/** 体素数量 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* VoxelCountText;

	/** 选区尺寸 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* SizeText;

	/** 中心点坐标 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* CenterText;

	/** 最小坐标 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* MinCoordText;

	/** 最大坐标 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* MaxCoordText;

	/** 清空选区按钮 */
	UPROPERTY(meta = (BindWidget))
	UButton* ClearSelectionButton;

	/** 保存为模板按钮 */
	UPROPERTY(meta = (BindWidget))
	UButton* SaveAsTemplateButton;

	// === 弹窗配置 ===

	/** 模板保存弹窗类（由蓝图配置，将通过 AVoxelHUD.PushDialog 打开） */
	UPROPERTY(EditDefaultsOnly, Category = "M5|Dialog")
	TSubclassOf<UVoxelDialogBase> TemplateSaveDialogClass;

	// === 事件处理 ===

	/** 清空选区按钮点击 */
	UFUNCTION()
	void OnClearSelectionClicked();

	/** 保存为模板按钮点击 */
	UFUNCTION()
	void OnSaveAsTemplateClicked();

	/** 选区变更回调 */
	UFUNCTION()
	void OnSelectionChangedHandler();

private:
	/** 标记是否已绑定选区委托 */
	bool bSelectionDelegateBound = false;
	bool bBuildModeDelegateBound = false;
};
