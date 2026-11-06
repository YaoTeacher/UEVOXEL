#include "Core/UI/Base/VoxelToastWidget.h"
#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Input/CommonUIInputTypes.h"

UVoxelPrimaryGameLayout* UVoxelToastWidget::GetLayout() const
{
	return UVoxelPrimaryGameLayout::GetFromPC(GetOwningPlayer());
}

TOptional<FUIInputConfig> UVoxelToastWidget::GetDesiredInputConfig() const
{
	// Toast 非交互：与 PrimaryLayout 全局输入配置保持一致（三个参数全对齐），不改变输入模式。
	// 若沿用 CommonActivatableWidget 默认值，保存弹窗关闭瞬间 Toast 成为 leaf-most
	// node 会把 Menu/NoCapture 延续下去，Toast 退场后无人恢复 → 场景点击被吞。
	// [第二坑] 第三参 bHideCursorDuringViewportCapture 引擎默认 true：Toast 显示期间
	// 点击场景视口会捕获并隐藏光标，框选等拖拽交互失效，且退场后无人恢复 → 必须显式传 false
	return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::CaptureDuringMouseDown, false);
}

void UVoxelToastWidget::InitializeToast(const FText& Message, float InDuration, EToastType Type)
{
	ToastMessage = Message;
	Duration = InDuration;
	ToastType = Type;
}

void UVoxelToastWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	OnToastShown();

	// 启动自动消失定时器
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ExpiryTimerHandle,
			[this]()
			{
				OnToastExpired();
				DeactivateWidget();
			},
			FMath::Max(Duration, 0.1f),
			false);
	}
}

void UVoxelToastWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpiryTimerHandle);
	}
}
