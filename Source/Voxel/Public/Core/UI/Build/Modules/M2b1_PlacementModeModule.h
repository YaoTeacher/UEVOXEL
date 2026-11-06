// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "Core/Enum/EditCommandEnums.h"
#include "M2b1_PlacementModeModule.generated.h"

class UCheckBox;

/**
 * M2b-1: 放置模式模块
 * 职责：控制方块放置规则（覆盖/填充/替换/指定替换）
 *
 * 显示条件: Tool.Edit.* (编辑工具)
 *
 * UI控件:
 * - OverwriteModeCheckBox: 覆盖模式
 * - FillModeCheckBox: 填充模式
 * - ReplaceModeCheckBox: 替换模式
 * - SpecifiedReplaceModeCheckBox: 指定替换模式
 */
UCLASS()
class VOXEL_API UM2b1_PlacementModeModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	// === 模块接口 ===

	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 广播回调：后端模式变更后刷新 UI */
	UFUNCTION()
	void HandlePlacementModeChanged();
	

	// === UI 控件绑定 ===

	/** 覆盖模式复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* OverwriteModeCheckBox;

	/** 填充模式复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* FillModeCheckBox;

	/** 替换模式复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* ReplaceModeCheckBox;

	/** 指定替换模式复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* SpecifiedReplaceModeCheckBox;

	// === 事件处理 ===

	/** 覆盖模式改变 */
	UFUNCTION()
	void OnOverwriteModeChanged(bool bChecked);

	/** 填充模式改变 */
	UFUNCTION()
	void OnFillModeChanged(bool bChecked);

	/** 替换模式改变 */
	UFUNCTION()
	void OnReplaceModeChanged(bool bChecked);

	/** 指定替换模式改变 */
	UFUNCTION()
	void OnSpecifiedReplaceModeChanged(bool bChecked);

private:
	/** 设置放置模式 */
	void SetPlacementMode(EVoxelPlacementMode NewMode);

	/** 更新模式复选框状态 */
	void UpdateModeCheckBoxes(EVoxelPlacementMode CurrentMode);

	/** 广播订阅守卫 */
	bool bDelegatesBound = false;
};
