#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "VoxelDialogBase.generated.h"

UCLASS(Abstract)
class VOXEL_API UVoxelDialogBase : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UVoxelDialogBase();

	virtual void OnDialogOpened(UObject* Context) {}
	virtual void OnDialogClosed() {}

	UFUNCTION(BlueprintCallable, Category = "Voxel|UI|Dialog")
	void Close() { DeactivateWidget(); }

	virtual bool HandleEscape() { Close(); return true; }

	/** 模态弹窗不改全局输入配置：与 PrimaryLayout/Screen/Toast 全局基准一致
	 *  （All + CaptureDuringMouseDown）。
	 *  阻断游戏世界交互由全屏 Backdrop 遮罩承担（点击落在遮罩上，不穿透场景），
	 *  禁止在此切换 Menu/NoCapture——弹窗关闭时 ActionRouter 恢复链可能断裂，
	 *  造成场景点击被吞（与 Toast 修复同源，详见 VoxelToastWidget.cpp 注释） */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override
	{
		return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::CaptureDuringMouseDown, false);
	}

	/** 模态弹窗默认焦点：子类应覆盖返回具体控件 */
	virtual UWidget* NativeGetDesiredFocusTarget() const override
	{
		return nullptr;
	}

	UPROPERTY(Transient) UObject* DataContext;

	UPROPERTY(EditDefaultsOnly, Category = "Voxel|UI|Dialog")
	FVector2D DesiredDialogSize = FVector2D(600, 400);

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
};
