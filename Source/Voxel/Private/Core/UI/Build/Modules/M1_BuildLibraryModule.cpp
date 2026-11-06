// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/Modules/M1_BuildLibraryModule.h"

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

bool UM1_BuildLibraryModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	// 匹配 Tool.Edit.* 但排除 Tool.Edit.Template（模板工具下只显示 M1b）
	return ToolTag.MatchesTag(FCoreTags::Tool_Edit) && ToolTag != FCoreTags::Tool_Edit_Template;
}

FGameplayTag UM1_BuildLibraryModule::GetQueryTagForTool(FGameplayTag ToolTag) const
{
	// 家具工具 → 家具列表；其他编辑工具 → 方块列表
	if (ToolTag == FCoreTags::Tool_Edit_Furniture)
	{
		return FCoreTags::Item_Type_BuildItem;
	}
	return FCoreTags::Item_Type_Block;
}

void UM1_BuildLibraryModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC)
	{
		return;
	}
	StateComponent = SC;

	if (!bDelegatesBound)
	{
		SC->BindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM1_BuildLibraryModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = true;
	}

	// 根据当前工具切换查询 Tag（方块工具 ↔ 家具工具）
	const FGameplayTag DesiredQueryTag = GetQueryTagForTool(SC->GetCurrentToolTag());
	if (CachedDataSource && CachedDataSource->QueryTag != DesiredQueryTag)
	{
		CachedDataSource->QueryTag = DesiredQueryTag;
	}
	if (CachedCategoryProvider && CachedCategoryProvider->RootTag != DesiredQueryTag)
	{
		CachedCategoryProvider->RootTag = DesiredQueryTag;
	}

	const FInteractableData HandData = SC->GetInteractableDataInHand();

	if (HandItemSlot)
	{
		if (HandData.HandleId != EBlockId::Invalid)
		{
			if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
			{
				if (UConfigDataSubsystem* CDS = GI->GetSubsystem<UConfigDataSubsystem>())
				{
					if (const FVoxelItemBase* Item = CDS->GetItemTypeById<FVoxelItemBase>(HandData.HandleId))
					{
						HandItemSlot->UpdateVisual(Item->AssetFragment.Icon, HandData.Quantity);
					}
				}
			}
		}
		else
		{
			HandItemSlot->UpdateVisual(nullptr, 0);
		}
	}

	UpdateSelectedItemInfo(HandData);
	if (ItemLibrary)
	{
		ItemLibrary->RefreshList();
	}
}

void UM1_BuildLibraryModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!ItemLibrary) return;

	// 建造库的类型切换不由库组件自己决定，而由当前工具通过 QueryTag 驱动
	ItemLibrary->bShowCategoryTabs = false;

	UVoxelItemDataSource* DS = NewObject<UVoxelItemDataSource>(ItemLibrary, UVoxelItemDataSource::StaticClass());
	DS->QueryTag = FCoreTags::Item_Type_Block;
	CachedDataSource = DS;
	ItemLibrary->SetDataSource(DS);

	UTagHierarchyCategoryProvider* CP = NewObject<UTagHierarchyCategoryProvider>(ItemLibrary, UTagHierarchyCategoryProvider::StaticClass());
	CP->RootTag = FCoreTags::Item_Type_Block;
	CachedCategoryProvider = CP;
	ItemLibrary->SetCategoryProvider(CP);

	ItemLibrary->InitializeLibrary();
}

void UM1_BuildLibraryModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM1_BuildLibraryModule::HandleBuildModeParamsUpdated()
{
	if (!StateComponent)
	{
		return;
	}
	const FInteractableData HandData = StateComponent->GetInteractableDataInHand();
	if (HandItemSlot)
	{
		if (HandData.HandleId != EBlockId::Invalid)
		{
			if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
			{
				if (UConfigDataSubsystem* CDS = GI->GetSubsystem<UConfigDataSubsystem>())
				{
					if (const FVoxelItemBase* Item = CDS->GetItemTypeById<FVoxelItemBase>(HandData.HandleId))
					{
						HandItemSlot->UpdateVisual(Item->AssetFragment.Icon, HandData.Quantity);
					}
				}
			}
		}
		else
		{
			HandItemSlot->UpdateVisual(nullptr, 0);
		}
	}
	UpdateSelectedItemInfo(HandData);
}

void UM1_BuildLibraryModule::NativeDestruct()
{
	Super::NativeDestruct();
	if (StateComponent)
	{
		StateComponent->UnbindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM1_BuildLibraryModule, HandleBuildModeParamsUpdated));
	}
}

void UM1_BuildLibraryModule::UpdateSelectedItemInfo(const FInteractableData& HandData)
{
	if (HandData.HandleId == EBlockId::Invalid)
	{
		if (SelectedItemNameText)     SelectedItemNameText->SetText(FText::FromString(TEXT("未选择")));
		if (SelectedItemPriceText)    SelectedItemPriceText->SetText(FText::FromString(TEXT("-")));
		if (SelectedItemQuantityText) SelectedItemQuantityText->SetText(FText::FromString(TEXT("0")));
		return;
	}
	if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
	{
		if (UConfigDataSubsystem* CDS = GI->GetSubsystem<UConfigDataSubsystem>())
		{
			if (const FVoxelItemBase* Item = CDS->GetItemTypeById<FVoxelItemBase>(HandData.HandleId))
			{
				if (SelectedItemNameText)
				{
					SelectedItemNameText->SetText(Item->ItemName);
				}
				if (SelectedItemPriceText)
				{
					SelectedItemPriceText->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), Item->CirculateDataFragment.Value)));
				}
				if (SelectedItemQuantityText)
				{
					SelectedItemQuantityText->SetText(FText::FromString(FString::Printf(TEXT("%d"), HandData.Quantity)));
				}
			}
		}
	}
}
