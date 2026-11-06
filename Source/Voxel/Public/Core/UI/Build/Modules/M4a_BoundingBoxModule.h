#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "M4a_BoundingBoxModule.generated.h"

struct FVoxelGeometryDesc;

class UEditableTextBox;
class UTextBlock;
class UButton;
class UComboBoxString;

/**
 * M4a 包围盒模块
 *
 * 数据流（遵循框架）：
 * - 交互层：按钮/文本提交 → 从SC读Geometry → 修改对应字段 → SC->SetTmpGeometry（触发广播）
 * - 显示层：UpdateFromToolState(SC) → 从SC读Geometry → 更新所有UI控件
 *
 * MultiNum 为纯 UI 状态（步长倍数），不入 SC，仅在交互层使用。
 */
UCLASS()
class VOXEL_API UM4a_BoundingBoxModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;

	/** NativeDestruct：解绑广播 */
	virtual void NativeDestruct() override;

	/** 状态变化回调（TmpGeometry 变化时刷新 UI） */
	UFUNCTION()
	void HandleBuildModeParamsUpdated();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	

	/** 是否已订阅广播 */
	bool bDelegatesBound = false;

	// ====== 包围盒尺寸 —— 坐标输入 (UEditableTextBox) ======

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* Input_MinX;
	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* Input_MinY;
	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* Input_MinZ;
	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* Input_MaxX;
	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* Input_MaxY;
	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* Input_MaxZ;

	// ====== 包围盒尺寸 —— 增减按钮 ======

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_MinX_Dec;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_MinX_Inc;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_MinY_Dec;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_MinY_Inc;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_MinZ_Dec;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_MinZ_Inc;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_MaxX_Dec;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_MaxX_Inc;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_MaxY_Dec;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_MaxY_Inc;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_MaxZ_Dec;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_MaxZ_Inc;

	// ====== 步长下拉框 + 体积 ======

	UPROPERTY(meta = (BindWidget))
	UComboBoxString* StepSizeComboBox;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* VolumeText;

	// ====== 整体位置偏移 —— 数值输入 (UEditableTextBox) ======

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* OffsetX_Value;
	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* OffsetY_Value;
	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* OffsetZ_Value;

	// ====== 整体位置偏移 —— 增减按钮 ======

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_OffsetX_Dec;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_OffsetX_Inc;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_OffsetY_Dec;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_OffsetY_Inc;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_OffsetZ_Dec;
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_OffsetZ_Inc;

	// ====== 事件处理（交互层：只改SC数据，不改UI） ======

	UFUNCTION()
	void OnMinXTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	UFUNCTION()
	void OnMinYTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	UFUNCTION()
	void OnMinZTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	UFUNCTION()
	void OnMaxXTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	UFUNCTION()
	void OnMaxYTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	UFUNCTION()
	void OnMaxZTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void OnCoreXTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	UFUNCTION()
	void OnCoreYTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	UFUNCTION()
	void OnCoreZTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void OnBtn_MinX_Dec();
	UFUNCTION()
	void OnBtn_MinX_Inc();
	UFUNCTION()
	void OnBtn_MinY_Dec();
	UFUNCTION()
	void OnBtn_MinY_Inc();
	UFUNCTION()
	void OnBtn_MinZ_Dec();
	UFUNCTION()
	void OnBtn_MinZ_Inc();
	UFUNCTION()
	void OnBtn_MaxX_Dec();
	UFUNCTION()
	void OnBtn_MaxX_Inc();
	UFUNCTION()
	void OnBtn_MaxY_Dec();
	UFUNCTION()
	void OnBtn_MaxY_Inc();
	UFUNCTION()
	void OnBtn_MaxZ_Dec();
	UFUNCTION()
	void OnBtn_MaxZ_Inc();

	UFUNCTION()
	void OnBtn_OffsetX_Dec();
	UFUNCTION()
	void OnBtn_OffsetX_Inc();
	UFUNCTION()
	void OnBtn_OffsetY_Dec();
	UFUNCTION()
	void OnBtn_OffsetY_Inc();
	UFUNCTION()
	void OnBtn_OffsetZ_Dec();
	UFUNCTION()
	void OnBtn_OffsetZ_Inc();

	UFUNCTION()
	void OnStepSizeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

private:
	/** 交互层核心：从SC读Geometry→改字段→写回SC（触发广播） */
	void ApplyCoordinateDelta(bool bIsMin, int32 Axis, bool bIncrease);
	void ApplyOffsetDelta(int32 Axis, bool bIncrease);
	void ApplyTextToCoord(bool bIsMin, int32 Axis, const FText& Text);
	void ApplyTextToOffset(int32 Axis, const FText& Text);

	/** 交互层统一出口：经 InteractionComp 写回 StateComponent（触发广播），UI 不直改 SC */
	void ApplyGeometryToState(const FVoxelGeometryDesc& Geo);

	/** 纯UI状态：步长倍数，不写入SC */
	int32 MultiNum = 1;
};
