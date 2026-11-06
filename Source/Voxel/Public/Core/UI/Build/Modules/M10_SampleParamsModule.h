#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "M10_SampleParamsModule.generated.h"

class UCheckBox;
class UTextBlock;

/**
 * M10: 采样参数控制模块
 * 职责：控制采样模式和预览采样结果
 *
 * 显示条件: Tool.Sample
 *
 * UI控件:
 * - SingleVoxelModeCheckBox: 单体素采样模式
 * - AreaSampleModeCheckBox: 区域采样模式
 * - PreviewTextBlock: 采样预览显示
 */
UCLASS()
class VOXEL_API UM10_SampleParamsModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSampleModeChanged, bool, bIsAreaMode);

	// === 模块接口 ===
	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;

	/** 从参数更新（新接口，替代 UpdateFromToolState） */
	UFUNCTION(BlueprintCallable, Category = "M10")
	void UpdateParams(bool bInIsAreaMode);

	/** 采样模式变更委托 */
	UPROPERTY(BlueprintAssignable, Category = "M10|Events")
	FOnSampleModeChanged OnSampleModeChanged;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	

	// === UI 控件绑定 ===

	/** 单体素采样模式复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* SingleVoxelModeCheckBox;

	/** 区域采样模式复选框 */
	UPROPERTY(meta = (BindWidget))
	UCheckBox* AreaSampleModeCheckBox;

	/** 采样预览显示 */
UPROPERTY(meta = (BindWidget))
UTextBlock* PreviewTextBlock;

	// === 事件处理 ===

	/** 单体素采样模式改变 */
	UFUNCTION()
	void OnSingleVoxelModeChanged(bool bChecked);

	/** 区域采样模式改变 */
	UFUNCTION()
	void OnAreaSampleModeChanged(bool bChecked);

private:
	/** 当前采样模式 */
	UPROPERTY(VisibleInstanceOnly, Category = "M10|State")
	bool bIsAreaMode = false;

	/** 设置采样模式 */
	void SetSampleMode(bool bInIsAreaMode);

	/** 更新采样预览 */
	void UpdateSamplePreview();

	/** 更新采样模式复选框状态 */
	void UpdateSampleModeCheckBoxes(bool bInIsAreaMode);
};