#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelScreenBase.h"
#include "VoxelSettingsWidget.generated.h"

class USlider;
class UComboBoxString;
class UButton;

/**
 * 设置面板（全屏 Screen）。
 * 可见范围（LoadDistance 1~8）+ 画质预设（低/中/高/极致）。
 * 主菜单与暂停菜单共用，push 到 GameMenuStack。
 * BP 只需放置同名控件（BindWidget 自动绑定），无需手写事件图表。
 */
UCLASS(Abstract)
class VOXEL_API UVoxelSettingsWidget : public UVoxelScreenBase
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	/** 当前可见范围（供 BP 进入时填充滑条/分档） */
	UFUNCTION(BlueprintPure, Category = "Voxel|Settings")
	int32 GetInitialLoadDistance() const;

	/** 当前画质预设 0~3（供 BP 填充下拉） */
	UFUNCTION(BlueprintPure, Category = "Voxel|Settings")
	int32 GetInitialQualityPreset() const;

	/** 应用并保存：写入可见范围+画质预设，生效+持久化，关闭面板 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Settings")
	void ApplyAndSave(int32 LoadDistance, int32 QualityPreset);

	/** 取消：关闭面板，不保存 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Settings")
	void Cancel();

	virtual bool HandleEscape() override { Cancel(); return true; }

protected:
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	USlider* LoadDistanceSlider;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UComboBoxString* QualityPresetCombo;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* ApplyButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* CancelButton;

	UFUNCTION() void HandleApplyClicked();
	UFUNCTION() void HandleCancelClicked();
};
