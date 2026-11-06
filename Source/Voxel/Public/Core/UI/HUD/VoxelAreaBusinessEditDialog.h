#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/UI/Base/VoxelDialogBase.h"
#include "VoxelAreaBusinessEditDialog.generated.h"

class AVoxelAreaBaseInstance;
class UButton;
class UComboBoxString;
class UEditableTextBox;
class UVoxelCreaturePickerWidget;
struct FAreaTypeBase;

/**
 * 区域业务数据编辑弹窗（Dialog 层）
 *
 * 职责：编辑区域的业务数据（名称 / 类型 / 营业状态 / 所有人 / 经营人）
 * - 由实体详情面板「编辑业务」按钮通过 PrimaryGameLayout.PushModal 打开，
 *   Context 传入待编辑的 AVoxelAreaBaseInstance*
 * - 打开时按当前玩家权限逐字段禁用：
 *   · 所有人/经营人（Owner 或 Manager）可改：名称 / 类型 / 状态 / 经营人
 *   · 仅所有人（Owner）可改：所有人字段（经营人不可转让所有权）
 * - 类型候选：同一直接父 Tag 且自身无子类型的叶子类型（层级节点不可直接选）
 * - 提交走 UVoxelInteractionComponent::ModifyArea 业务方法，不直改数据
 * - C++ 只做数据填充与交互路由，样式全部在 WBP_AreaBusinessEditDialog
 *
 * 设计文档：/VoxelDesign/02-系统设计/区域业务数据编辑表单-设计.md
 */
UCLASS()
class VOXEL_API UVoxelAreaBusinessEditDialog : public UVoxelDialogBase
{
	GENERATED_BODY()

public:
	/** Context = 待编辑的 AVoxelAreaBaseInstance（不合法则记录错误并保持空表单） */
	virtual void OnDialogOpened(UObject* Context) override;

	/** 默认焦点：取消按钮（安全选项） */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

protected:
	virtual void NativeConstruct() override;

	// === BindWidget 控件（WBP_AreaBusinessEditDialog 必须提供，缺失时编译报错） ===

	/** 区域名称输入框 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> NameInput;

	/** 区域类型下拉（候选 = 同直接父 Tag 的叶子类型） */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> TypeCombo;

	/** 营业状态下拉（EAreaStatus 全枚举） */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> StatusCombo;

	/** 所有人选择器（规范组件；仅 Owner 可改） */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVoxelCreaturePickerWidget> OwnerPicker;

	/** 经营人选择器（规范组件） */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVoxelCreaturePickerWidget> ManagerPicker;

	/** 确认保存按钮 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;

	/** 取消按钮 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CancelButton;

	// === 事件处理 ===

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnCancelClicked();

private:
	/** 用目标区域当前数据填充表单并应用字段级权限 */
	void RefreshFromArea();

	/** 填充类型下拉：同直接父 Tag 的叶子类型；当前类型为层级节点时附加保底项 */
	void FillTypeOptions();

	/** 填充营业状态下拉（EAreaStatus 全枚举 DisplayName） */
	void FillStatusOptions();

	/** Toast 提示（经 PrimaryGameLayout） */
	void ShowToast(const FText& Message, bool bIsError) const;

	/** 待编辑区域（OnDialogOpened 注入） */
	UPROPERTY()
	TObjectPtr<AVoxelAreaBaseInstance> TargetArea;

	// === 下拉选项 → 数据映射（并行数组，按 Option 索引对齐） ===

	TArray<FGameplayTag> TypeTags;
	TArray<int32> StatusValues;
};
