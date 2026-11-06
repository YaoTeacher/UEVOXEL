// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "Core/Enum/EditCommandEnums.h"
#include "M2b2_SelectionOpModeModule.generated.h"

class UCheckBox;

/**
 * M2b-2: 选区运算模式模块
 * 职责：控制选区运算模式（新选/加选/减选/交选）
 *
 * 显示条件: Tool.Edit.Selection (选区编辑工具)
 *
 * UI控件:
 * - NewModeCheckBox: 新选模式
 * - AddModeCheckBox: 加选模式
 * - SubtractModeCheckBox: 减选模式
 * - IntersectModeCheckBox: 交选模式
 */
UCLASS()
class VOXEL_API UM2b2_SelectionOpModeModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	// === 模块接口 ===

	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	

	// === UI 控件绑定 ===

	/** 新选模式复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* NewModeCheckBox;

	/** 加选模式复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* AddModeCheckBox;

	/** 减选模式复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* SubtractModeCheckBox;

	/** 交选模式复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* IntersectModeCheckBox;

	// === 事件处理 ===

	/** 新选模式改变 */
	UFUNCTION()
	void OnNewModeChanged(bool bChecked);

	/** 加选模式改变 */
	UFUNCTION()
	void OnAddModeChanged(bool bChecked);

	/** 减选模式改变 */
	UFUNCTION()
	void OnSubtractModeChanged(bool bChecked);

	/** 交选模式改变 */
	UFUNCTION()
	void OnIntersectModeChanged(bool bChecked);

private:
	/** 设置选区运算模式 */
	void SetSelectionMode(ESelectionOpMode NewMode);

	/** 更新模式复选框状态 */
	void UpdateModeCheckBoxes(ESelectionOpMode CurrentMode);
};
