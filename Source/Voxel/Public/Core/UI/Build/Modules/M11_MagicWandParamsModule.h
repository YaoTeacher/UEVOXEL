#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "M11_MagicWandParamsModule.generated.h"

class UCheckBox;
class UEditableTextBox;

/**
 * M11: 魔棒参数控制模块
 * 职责：控制魔棒工具的选择模式、最大体素数和最大深度
 *
 * 显示条件: Tool.Selection.Magic
 *
 * UI控件:
 * - ConnectedRegionModeCheckBox: 连通区域模式
 * - GlobalSimilarModeCheckBox: 全局相似模式
 * - MaxVoxelCountInput: 最大体素数输入
 * - MaxDepthInput: 最大深度输入
 */
UCLASS()
class VOXEL_API UM11_MagicWandParamsModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectionModeChanged, bool, bIsConnected);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMaxVoxelCountChanged, int32, MaxVoxelCount);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMaxDepthChanged, int32, MaxDepth);

	// === 模块接口 ===
	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;

	/** 从参数更新（新接口） */
	UFUNCTION(BlueprintCallable, Category = "M11")
	void UpdateParams(bool bInIsConnected, int32 InMaxVoxelCount, int32 InMaxDepth);

	/** 选择模式变更委托 */
	UPROPERTY(BlueprintAssignable, Category = "M11|Events")
	FOnSelectionModeChanged OnSelectionModeChanged;

	/** 最大体素数变更委托 */
	UPROPERTY(BlueprintAssignable, Category = "M11|Events")
	FOnMaxVoxelCountChanged OnMaxVoxelCountChanged;

	/** 最大深度变更委托 */
	UPROPERTY(BlueprintAssignable, Category = "M11|Events")
	FOnMaxDepthChanged OnMaxDepthChanged;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	

	// === UI 控件绑定 ===

	/** 连通区域模式复选框 */
UPROPERTY(meta = (BindWidget))
UCheckBox* ConnectedRegionModeCheckBox;

/** 全局相似模式复选框 */
UPROPERTY(meta = (BindWidget))
UCheckBox* GlobalSimilarModeCheckBox;

/** 最大体素数输入框 */
UPROPERTY(meta = (BindWidget))
UEditableTextBox* MaxVoxelCountInput;

/** 最大深度输入框 */
UPROPERTY(meta = (BindWidget))
UEditableTextBox* MaxDepthInput;

	// === 事件处理 ===

	/** 连通区域模式改变 */
	UFUNCTION()
	void OnConnectedRegionModeChanged(bool bChecked);

	/** 全局相似模式改变 */
	UFUNCTION()
	void OnGlobalSimilarModeChanged(bool bChecked);

	/** 最大体素数文本改变 */
	UFUNCTION()
	void OnMaxVoxelCountTextChanged(const FText& Text);

	/** 最大深度文本改变 */
	UFUNCTION()
	void OnMaxDepthTextChanged(const FText& Text);

private:
	/** 当前选择模式 */
	UPROPERTY(VisibleInstanceOnly, Category = "M11|State")
	bool bIsConnected = true;

	/** 当前最大体素数 */
	UPROPERTY(VisibleInstanceOnly, Category = "M11|State")
	int32 MaxVoxelCount = 10000;

	/** 当前最大深度 */
	UPROPERTY(VisibleInstanceOnly, Category = "M11|State")
	int32 MaxDepth = 100;

	/** 设置选择模式 */
	void SetSelectionMode(bool bInIsConnected);

	/** 更新选择模式复选框状态 */
	void UpdateSelectionModeCheckBoxes(bool bInIsConnected);
};