// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "Core/Enum/GameStateEnum.h"
#include "M3_GeometryShapeModule.generated.h"

class UCheckBox;

/**
 * M3: 几何形状模块
 * 职责：选择批量编辑的几何形状（立方体/球体/圆柱/金字塔）
 *
 * 显示条件: Tool.Selection.SimpleGeometry 或 Tool.Edit.SimpleGeometry
 *
 * UI控件:
 * - BoxShapeCheckBox: 立方体形状
 * - SphereShapeCheckBox: 球体形状
 * - CylinderShapeCheckBox: 圆柱形状
 * - PyramidShapeCheckBox: 金字塔形状
 */
UCLASS()
class VOXEL_API UM3_GeometryShapeModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	// === 模块接口 ===

	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;
	virtual void NativeDestruct() override;

	/** 状态变化回调 */
	UFUNCTION()
	void HandleBuildModeParamsUpdated();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	

	bool bDelegatesBound = false;

	// === UI 控件绑定 ===

	/** 立方体形状复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* BoxShapeCheckBox;

	/** 球体形状复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* SphereShapeCheckBox;

	/** 圆柱形状复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* CylinderShapeCheckBox;

	/** 金字塔形状复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* PyramidShapeCheckBox;

	// === 事件处理 ===

	/** 立方体形状改变 */
	UFUNCTION()
	void OnBoxShapeChanged(bool bChecked);

	/** 球体形状改变 */
	UFUNCTION()
	void OnSphereShapeChanged(bool bChecked);

	/** 圆柱形状改变 */
	UFUNCTION()
	void OnCylinderShapeChanged(bool bChecked);

	/** 金字塔形状改变 */
	UFUNCTION()
	void OnPyramidShapeChanged(bool bChecked);

private:
	/** 设置几何形状 */
	void SetGeometryShape(EBatchShapeType NewShape);

	/** 更新形状复选框状态 */
	void UpdateShapeCheckBoxes(EBatchShapeType CurrentShape);
};
