// Fill out your copyright notice in the Description page of Project Settings.


#include "BuildInteractionWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/CoreTags.h"
#include "Core/VoxelGameInstance.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Voxel/VoxelGameMode.h"
#include "Voxel/VoxelPlayerController.h"

void UBuildInteractionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UBuildInteractionWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBuildInteractionWidget::UpdateWidget(FInteractableData InteractableData) const
{
	if(AVoxelPlayerController* controller =GetOwningPlayer<AVoxelPlayerController>())
	{
		if(controller->StateCacheComp->GetControllerMode()==EVoxelControllerMode::SceneBuildMode)
		{
			FVoxelItemBase* item;
			FGameplayTag CurrentToolTag = controller->StateCacheComp->GetCurrentToolTag();

			// 方块编辑工具
			if (CurrentToolTag == FCoreTags::Tool_Edit_SingleVoxel ||
			    CurrentToolTag == FCoreTags::Tool_Edit_SimpleGeometry)
			{
				item=GetGameInstance<UVoxelGameInstance>()->GetSubsystem<UConfigDataSubsystem>()->GetItemTypeById<FVoxelItemBase>(InteractableData.HandleId);
			if(item && item->ItemTag.MatchesTag(FCoreTags::Item_Type_Block))
			{
				//UE_LOG(LogTemp, Warning, TEXT("ItemInHand  not Block:%s"), *item->ItemTag.ToString());
				return;
			}
			}
			// 模板放置工具
			else if (CurrentToolTag == FCoreTags::Tool_Edit_Template)
			{
				item =GetGameInstance<UVoxelGameInstance>()->GetSubsystem<UConfigDataSubsystem>()->GetItemTypeById<FVoxelItemBase>(InteractableData.HandleId);
			}
			else
			{
				return;
			}
		
		
			NameText->SetText(FText::FromString(item->ItemName.ToString()));
			ItemIcon->SetBrushFromTexture(item->AssetFragment.Icon);
			DescriptionText->SetText(FText::FromString(item->ItemDescription.ToString()));
			//UE_LOG(LogTemp, Warning, TEXT("ItemInHand : %d"), InteractableData.ItemID);
			if (InteractableData.Quantity < 2)
			{
				QuantityText->SetVisibility(ESlateVisibility::Collapsed);
			}
			else
			{
				QuantityText->SetText(FText::Format(FText::FromString("x{0}"), InteractableData.Quantity));
				QuantityText->SetVisibility(ESlateVisibility::Visible);
			}
		
		}
	}
	;
	
}


