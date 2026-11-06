#include "Core/UI/Build/Modules/M13a_AreaBasicInfoEditModule.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/CoreTags.h"
#include "Core/VoxelGameInstance.h"
#include "Components/Button.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Core/UI/Build/AreaTree/VoxelAreaTreeWidget.h"
#include "Voxel/VoxelPlayerController.h"

bool UM13a_AreaBasicInfoEditModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	return ToolTag.MatchesTag(FCoreTags::Tool_AreaEdit);
}

void UM13a_AreaBasicInfoEditModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	Super::UpdateFromToolState(SC);  // 保存 StateComponent 引用

	if (!SC)
	{
		return;
	}

	// 首次激活时绑定全部建造事件，CurArea 变化即时刷新导航树
	if (!bDelegatesBound)
	{
		SC->BindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM13a_AreaBasicInfoEditModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = true;
	}

	UpdateSubAreaList();
}

void UM13a_AreaBasicInfoEditModule::HandleBuildModeParamsUpdated()
{
	// 全量刷新导航树（UpdateFromToolState 内绑定有守卫，不会重复绑定/递归）
	if (StateComponent)
	{
		UpdateSubAreaList();
	}
}

void UM13a_AreaBasicInfoEditModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (EnterAreaButton)
		EnterAreaButton->OnClicked.AddDynamic(this, &UM13a_AreaBasicInfoEditModule::OnEnterAreaClicked);
}

void UM13a_AreaBasicInfoEditModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM13a_AreaBasicInfoEditModule::NativeDestruct()
{
	if (StateComponent && bDelegatesBound)
	{
		StateComponent->UnbindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM13a_AreaBasicInfoEditModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = false;
	}
	Super::NativeDestruct();
}

void UM13a_AreaBasicInfoEditModule::OnEnterAreaClicked()
{
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp)
		{
			// 将当前选中区域设为 CurParentArea，进入下一层
			PC->InteractionComp->EnterSubArea();
		}
	}
}

void UM13a_AreaBasicInfoEditModule::UpdateSubAreaList()
{
    if (!SubAreaTreeWidget || !StateComponent)
    {
        return;
    }

    // M13 只展示当前选中区域（CurArea）及下属区域的区域树
    // CurArea 未选中时隐藏树
    AVoxelAreaBaseInstance* RootArea = StateComponent->GetCurrentArea();
    if (!RootArea)
    {
        SubAreaTreeWidget->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    SubAreaTreeWidget->SetVisibility(ESlateVisibility::Visible);

    // 获取当前角色 ID 用于权限判定（统一入口，取不到时 -1 = 未指派，整树只读）
    const int32 CharId = UVoxelGameInstance::GetMainCharacterIdSafe(this);

    // 以 CurArea 为根，展示其及下属区域
    SubAreaTreeWidget->InitializeTree(RootArea, CharId);
    SubAreaTreeWidget->SetSelectedArea(RootArea);
}
