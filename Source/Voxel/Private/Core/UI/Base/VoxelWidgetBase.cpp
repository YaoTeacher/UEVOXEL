#include "Core/UI/Base/VoxelWidgetBase.h"
#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "GameFramework/PlayerController.h"

UVoxelPrimaryGameLayout* UVoxelWidgetBase::GetLayout() const
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		return UVoxelPrimaryGameLayout::GetFromPC(PC);
	}
	return nullptr;
}
