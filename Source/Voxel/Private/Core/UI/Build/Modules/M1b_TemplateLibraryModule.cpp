// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/Modules/M1b_TemplateLibraryModule.h"

#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/CoreTags.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Voxel/Private/Core/VoxelGameInstance.h"
#include "Voxel/VoxelPlayerController.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Data/Item/VoxelItemBase.h"
#include "Data/Item/InteractableData.h"
#include "Core/UI/ItemLibrary/ItemLibraryCategoryProvider.h"
#include "Core/UI/ItemLibrary/VoxelTemplateDataSource.h"
#include "UObject/ConstructorHelpers.h"
#include "Core/UI/ItemLibrary/ItemLibrarySlotBase.h"
#include "Core/UI/ItemLibrary/ItemLibraryTabButtonBase.h"

bool UM1b_TemplateLibraryModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	return ToolTag == FCoreTags::Tool_Edit_Template;
}

void UM1b_TemplateLibraryModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!ItemLibrary) return;

	// 模板库类型固定为 Template，不允许库组件内部切换分类
	ItemLibrary->bShowCategoryTabs = false;

	UVoxelTemplateDataSource* DS = NewObject<UVoxelTemplateDataSource>(ItemLibrary, UVoxelTemplateDataSource::StaticClass());
	ItemLibrary->SetDataSource(DS);

	UTagHierarchyCategoryProvider* CP = NewObject<UTagHierarchyCategoryProvider>(ItemLibrary, UTagHierarchyCategoryProvider::StaticClass());
	CP->RootTag = FCoreTags::Item_Type_Template;
	ItemLibrary->SetCategoryProvider(CP);

	ItemLibrary->InitializeLibrary();
}

void UM1b_TemplateLibraryModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM1b_TemplateLibraryModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC)
	{
		return;
	}
	StateComponent = SC;

	if (!bDelegatesBound)
	{
		SC->BindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM1b_TemplateLibraryModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = true;
	}

	const FInteractableData HandData = SC->GetInteractableDataInHand();
	UpdateSelectedTemplateInfo(HandData);
	// 列表数据在 NativeConstruct 初始化，模板保存后由 OnDataChanged 刷新。
	// 这里是工具状态刷新入口，会在 hover / 手持 / phase 变化时频繁触发，不能重复 RefreshList。
}

void UM1b_TemplateLibraryModule::UpdateSelectedTemplateInfo(const FInteractableData& HandData)
{
	if (HandData.HandleId == EBlockId::Invalid)
	{
		if (SelectedTemplateNameText)
		{
			SelectedTemplateNameText->SetText(FText::FromString(TEXT("未选择")));
		}
		if (MaterialRequirementsText)
		{
			MaterialRequirementsText->SetText(FText::FromString(TEXT("-")));
		}
		return;
	}
	if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
	{
		if (UConfigDataSubsystem* CDS = GI->GetSubsystem<UConfigDataSubsystem>())
		{
			// 模板库选中的是模板，按句柄从模板仓库查
			if (const FVoxelTemplate* Template = CDS->GetTemplateById(HandData.HandleId))
			{
				if (SelectedTemplateNameText)
				{
					SelectedTemplateNameText->SetText(Template->ItemName);
				}
				if (MaterialRequirementsText)
				{
					const bool bOK = CheckMaterialRequirements(Template->ItemTag);
					MaterialRequirementsText->SetText(FText::FromString(bOK ? TEXT("素材充足") : TEXT("缺少素材")));
				}
			}
		}
	}
}

bool UM1b_TemplateLibraryModule::CheckMaterialRequirements(FGameplayTag /*TemplateTag*/)
{
	// TODO: 背包系统就绪后接入真实校验
	return true;
}

void UM1b_TemplateLibraryModule::HandleBuildModeParamsUpdated()
{
	if (!StateComponent)
	{
		return;
	}
	const FInteractableData HandData = StateComponent->GetInteractableDataInHand();
	UpdateSelectedTemplateInfo(HandData);

	// 模板保存后刷新列表（替代旧的 UISubsystem.OnDataChanged("Template")）
	if (ItemLibrary)
	{
		ItemLibrary->RefreshList();
	}
}

void UM1b_TemplateLibraryModule::NativeDestruct()
{
	Super::NativeDestruct();
	if (StateComponent)
	{
		StateComponent->UnbindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM1b_TemplateLibraryModule, HandleBuildModeParamsUpdated));
	}
}
