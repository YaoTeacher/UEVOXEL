#include "Core/UI/Base/VoxelScreenBase.h"

UVoxelScreenBase::UVoxelScreenBase()
{
	// 不注册 CommonUI Back 绑定：项目 ESC 由 AVoxelPlayerController::OnEscapePressed
	// → Screen->HandleEscape() 自研链路处理（含防抖）。
	// bIsBackHandler=true 在未配置 CommonUI DefaultBackAction 时注册必失败并报
	// "Cannot create action binding - no action provided"，属无效残留。
	SetIsFocusable(false);
}

void UVoxelScreenBase::NativeOnActivated()
{
	Super::NativeOnActivated();
	SetIsFocusable(false);
	UE_LOG(LogTemp, Warning, TEXT("[Screen] NativeOnActivated: %s (IsFocusable=%d)"), *GetName(), IsFocusable() ? 1 : 0);
	OnEnterScreen(DataContext);
}

void UVoxelScreenBase::NativeOnDeactivated()
{
	UE_LOG(LogTemp, Warning, TEXT("[Screen] NativeOnDeactivated: %s"), *GetName());
	OnExitScreen();
	Super::NativeOnDeactivated();
}
