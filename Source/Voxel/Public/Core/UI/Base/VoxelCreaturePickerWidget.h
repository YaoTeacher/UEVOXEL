#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VoxelCreaturePickerWidget.generated.h"

class UComboBoxString;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCreaturePicked, int32, CreatureId);

/**
 * 角色选择器（规范组件）
 *
 * 职责：CreatureId 下拉选择的唯一实现，替代各表单自行拼装 ComboBox 选项
 * - 选项构成：当前主角快捷项（bIncludeSelf，显示名走统一解析）+ 可管理在场生物；
 *   CurrentId 不在列表时补显「xxx（当前）」避免空白
 * - 名称解析走 UVoxelCreatureWorldSubsystem::ResolveCreatureName（分区语义唯一实现处）
 * - C++ 只做数据填充与选中回调路由，样式全部在 WBP_CreaturePicker
 */
UCLASS(Abstract)
class VOXEL_API UVoxelCreaturePickerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 选中项变化时广播（CreatureId；清空选择不广播） */
	UPROPERTY(BlueprintAssignable, Category = "Voxel|UI|Picker")
	FOnCreaturePicked OnCreaturePicked;

	/** 填充选项并选中 CurrentId；bIncludeSelf = 是否含当前主角快捷项 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|UI|Picker")
	void InitPicker(int32 CurrentId, bool bIncludeSelf = true);

	/** 当前选中 CreatureId；未选中返回 -1（调用方按保持原值处理） */
	UFUNCTION(BlueprintPure, Category = "Voxel|UI|Picker")
	int32 GetSelectedCreatureId() const;

	/** 只读开关（转发到 ComboBox，权限控制用） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|UI|Picker")
	void SetPickerEnabled(bool bEnabled);

protected:
	virtual void NativeConstruct() override;

	// === BindWidget 控件（WBP_CreaturePicker 必须提供，缺失时编译报错） ===

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> CreatureCombo;

private:
	/** 选项文本 → CreatureId（按 Option 索引对齐） */
	TArray<int32> OptionIds;

	UFUNCTION()
	void HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	/** 生物选项显示文本：RealName #Id */
	static FString MakeCreatureOptionText(const FText& DisplayName, int32 CreatureId);
};
