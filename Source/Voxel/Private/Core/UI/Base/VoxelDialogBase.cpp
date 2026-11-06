#include "Core/UI/Base/VoxelDialogBase.h"

UVoxelDialogBase::UVoxelDialogBase()
{
	bIsModal = true;
	// 不注册 CommonUI Back 绑定（同 VoxelScreenBase）：ESC 走自研 HandleEscape 链，
	// 未配置 DefaultBackAction 时注册必失败并报错
}

void UVoxelDialogBase::NativeOnActivated()
{
	Super::NativeOnActivated();
	OnDialogOpened(DataContext);
}

void UVoxelDialogBase::NativeOnDeactivated()
{
	OnDialogClosed();
	Super::NativeOnDeactivated();
}
