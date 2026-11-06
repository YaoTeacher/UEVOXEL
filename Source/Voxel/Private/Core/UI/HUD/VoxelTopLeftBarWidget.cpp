#include "Core/UI/HUD/VoxelTopLeftBarWidget.h"
#include "Core/UI/Build/AreaTree/VoxelAreaNavigatorWidget.h"
#include "Core/UI/VoxelAreaItemWidget.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"

UVoxelTopLeftBarWidget::UVoxelTopLeftBarWidget()
{
}

void UVoxelTopLeftBarWidget::SetBreadcrumbItemClass(TSubclassOf<UVoxelAreaItemWidget> InItemClass)
{
	if (AreaNavigator)
	{
		AreaNavigator->SetBreadcrumbItemClass(InItemClass);
	}
}

void UVoxelTopLeftBarWidget::RefreshNavigator(UVoxelPlayerStateCacheComponent* SC)
{
	if (AreaNavigator && SC)
	{
		AreaNavigator->Refresh(SC);
	}
}
