#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "VoxelScreenBase.generated.h"

UCLASS(Abstract)
class VOXEL_API UVoxelScreenBase : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UVoxelScreenBase();

	virtual void OnEnterScreen(UObject* Context) {}
	virtual void OnExitScreen() {}

	virtual bool HandleEscape() { return false; }

	UPROPERTY(Transient) UObject* DataContext;

	/** 全屏面板：Game+UI 同时输入（不阻断游戏世界） */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override
	{
		return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::CaptureDuringMouseDown, false);
	}

	/** Screen 默认焦点：子类应覆盖返回具体控件 */
	virtual UWidget* NativeGetDesiredFocusTarget() const override
	{
		return nullptr;
	}

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
};
