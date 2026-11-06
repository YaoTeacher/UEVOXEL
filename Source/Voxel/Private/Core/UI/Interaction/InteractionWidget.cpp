// Fill out your copyright notice in the Description page of Project Settings.


#include "InteractionWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Core/VoxelGameInstance.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Voxel/VoxelGameMode.h"


void UInteractionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ControllerReference = Cast<AVoxelPlayerController>(GetOwningPlayer());
	InteractionProgressBar->PercentDelegate.BindUFunction(this, "UpdateInteractionProgress");
	
}

void UInteractionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	KeyPressText->SetText(FText::FromString("Press"));
	CurrentInteractionDuration = 0.0f;
}

void UInteractionWidget::InitializeWidget()
{
	if (!ControllerReference || !ControllerReference->StateCacheComp)
	{
		return;
	}

	switch (ControllerReference->StateCacheComp->GetControllerMode())
	{
	case EVoxelControllerMode::AdventureMode:

		break;

	case EVoxelControllerMode::SceneBuildMode:
#if WITH_EDITOR
		if (GetWorld()->WorldType != EWorldType::PIE)
		{
			if (GEditor && GEditor->bIsSimulatingInEditor)
			{
				SetItemDisplay(ControllerReference->StateCacheComp->GetInteractableDataInHand());
			}
		}
#endif
		KeyPressText->SetText(FText::FromString("Click"));
		InteractionProgressBar->SetVisibility(ESlateVisibility::Collapsed);
		ActionText->SetText(FText::FromString("Place"));
		break;
	case EVoxelControllerMode::FilmMode:
		break;

	case EVoxelControllerMode::ManageMode:
		break;

	default:;
	}
}

void UInteractionWidget::SetItemDisplay(const FInteractableData& InteractableData) const
{
	UConfigDataSubsystem* ConfigData = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UConfigDataSubsystem>()
		: nullptr;
	const FVoxelItemBase* ItemData = ConfigData
		? ConfigData->GetItemTypeById<FVoxelItemBase>(InteractableData.HandleId)
		: nullptr;
	NameText->SetText(ItemData ? ItemData->ItemName : FText::GetEmpty());
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

void UInteractionWidget::UpdateWidget(const FInteractableData InteractableData) const
{
	if (!ControllerReference || !ControllerReference->StateCacheComp)
	{
		return;
	}

	switch (ControllerReference->StateCacheComp->GetControllerMode())
	{
	case EVoxelControllerMode::AdventureMode:
		KeyPressText->SetText(FText::FromString("Press"));
		InteractionProgressBar->SetVisibility(ESlateVisibility::Collapsed);
		SetItemDisplay(InteractableData);
		ActionText->SetText(FText::FromString("Pick Up"));
		break;

	case EVoxelControllerMode::SceneBuildMode:
		KeyPressText->SetText(FText::FromString("Click"));
		InteractionProgressBar->SetVisibility(ESlateVisibility::Collapsed);
		SetItemDisplay(InteractableData);
		ActionText->SetText(FText::FromString("Place"));
		break;
	case EVoxelControllerMode::FilmMode:
		break;

	case EVoxelControllerMode::ManageMode:
		break;

	default:;
	}
}

float UInteractionWidget::UpdateInteractionProgress()
{
	return 0.0f;
}
