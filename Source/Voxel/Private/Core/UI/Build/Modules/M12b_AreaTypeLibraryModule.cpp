#include "Core/UI/Build/Modules/M12b_AreaTypeLibraryModule.h"

#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/CoreTags.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Voxel/Private/Core/VoxelGameInstance.h"
#include "Voxel/VoxelPlayerController.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Data/Area/AreaEnums.h"
#include "Core/UI/ItemLibrary/ItemLibraryCategoryProvider.h"

bool UM12b_AreaTypeLibraryModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	return ToolTag.MatchesTag(FCoreTags::Tool_AreaEdit);
}

void UM12b_AreaTypeLibraryModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC)
	{
		return;
	}
	StateComponent = SC;

	if (!bDelegatesBound)
	{
		SC->BindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM12b_AreaTypeLibraryModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = true;
	}

	// 从 SC 读取当前区域类型并更新显示
	UpdateSelectedAreaTypeInfo(SC->TmpAreaInfo.AreaType);

	if (ItemLibrary)
	{
		ItemLibrary->RefreshList();
	}
}

void UM12b_AreaTypeLibraryModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!ItemLibrary) return;

	// 区域类型库类型固定为 Area.Type，不允许库组件内部切换分类
	ItemLibrary->bShowCategoryTabs = false;

	UAreaTypeDataSource* DS = NewObject<UAreaTypeDataSource>(ItemLibrary, UAreaTypeDataSource::StaticClass());
	ItemLibrary->SetDataSource(DS);

	UTagHierarchyCategoryProvider* CP = NewObject<UTagHierarchyCategoryProvider>(ItemLibrary, UTagHierarchyCategoryProvider::StaticClass());
	CP->RootTag = FCoreTags::Area_Type;
	ItemLibrary->SetCategoryProvider(CP);

	ItemLibrary->InitializeLibrary();
}

void UM12b_AreaTypeLibraryModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM12b_AreaTypeLibraryModule::HandleBuildModeParamsUpdated()
{
	if (!StateComponent)
	{
		return;
	}
	UpdateSelectedAreaTypeInfo(StateComponent->TmpAreaInfo.AreaType);
}

void UM12b_AreaTypeLibraryModule::UpdateSelectedAreaTypeInfo(const FGameplayTag& AreaTypeTag)
{
	if (!SelectedAreaTypeNameText)
	{
		return;
	}
	if (!AreaTypeTag.IsValid())
	{
		SelectedAreaTypeNameText->SetText(FText::FromString(TEXT("未选择")));
		return;
	}
	if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
	{
		if (UConfigDataSubsystem* CDS = GI->GetSubsystem<UConfigDataSubsystem>())
		{
			if (const FAreaTypeBase* AreaType = CDS->GetAreaTypeByTag(AreaTypeTag))
			{
				SelectedAreaTypeNameText->SetText(AreaType->DisplayName);
			}
			else
			{
				SelectedAreaTypeNameText->SetText(FText::FromString(TEXT("未选择")));
			}
		}
	}
}

void UM12b_AreaTypeLibraryModule::NativeDestruct()
{
	Super::NativeDestruct();
	if (StateComponent)
	{
		StateComponent->UnbindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM12b_AreaTypeLibraryModule, HandleBuildModeParamsUpdated));
	}
}
