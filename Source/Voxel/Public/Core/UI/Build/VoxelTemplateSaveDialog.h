#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelDialogBase.h"
#include "Components/Button.h"
#include "VoxelTemplateSaveDialog.generated.h"

class UTextBlock;
class UButton;
class UEditableTextBox;
class UVoxelPlayerStateCacheComponent;

/**
 * 模板保存弹窗（L3 Dialog 层）
 * 职责：从选区捕获为模板前，让玩家编辑模板元信息并预览选区统计
 *
 * 通过 AVoxelHUD::PushDialog 打开，Context 传入 UVoxelPlayerStateCacheComponent*
 *
 * UI控件:
 * - SelectionStatsText: 选区统计文本（体素数、尺寸、家具数等）
 * - TemplateNameInput: 模板名称输入框
 * - TemplateAuthorInput: 作者输入框
 * - TemplateDescInput: 描述输入框
 * - ConfirmButton: 确认保存按钮
 * - CancelButton: 取消按钮
 */
UCLASS()
class VOXEL_API UVoxelTemplateSaveDialog : public UVoxelDialogBase
{
	GENERATED_BODY()

public:
	// 弹窗打开时调用：从 Context 获取 StateComponent 并刷新统计
	virtual void OnDialogOpened(UObject* Context) override;

	/** 弹窗默认焦点：返回取消按钮（安全选项） */
	virtual UWidget* NativeGetDesiredFocusTarget() const override
	{
		return CancelButton;
	}

protected:
	virtual void NativeConstruct() override;

	// === BindWidget 控件 ===

	/** 选区统计文本 */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UTextBlock* SelectionStatsText;

	/** 模板名称输入框 */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UEditableTextBox* TemplateNameInput;

	/** 作者输入框 */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UEditableTextBox* TemplateAuthorInput;

	/** 描述输入框 */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UEditableTextBox* TemplateDescInput;

	/** 确认保存按钮 */
	UPROPERTY(meta = (BindWidget))
	UButton* ConfirmButton;

	/** 取消按钮 */
	UPROPERTY(meta = (BindWidget))
	UButton* CancelButton;

	// === 事件处理 ===

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnCancelClicked();

private:
	/** 计算并填充选区统计文本 */
	void RefreshSelectionStats();

	/** 缓存的选区状态组件（由 OnDialogOpened 从 Context 获取） */
	UPROPERTY()
	UVoxelPlayerStateCacheComponent* StateComponent;
};
