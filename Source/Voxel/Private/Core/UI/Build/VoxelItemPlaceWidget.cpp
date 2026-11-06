// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/UI/Build/VoxelItemPlaceWidget.h"

#include "Core/VoxelGameInstance.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/UI/ItemSlotWidget.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Voxel/VoxelPlayerController.h"

void UVoxelItemPlaceWidget::UpdateItemIcon() const
{
	if (AVoxelPlayerController* PC= GetOwningPlayer<AVoxelPlayerController>())
	{
		const int32 ItemId = PC->StateCacheComp->GetInteractableDataInHand().HandleId;
		const FVoxelItemBase* i=GetGameInstance<UVoxelGameInstance>()->GetSubsystem<UConfigDataSubsystem>()->GetItemTypeById<FVoxelItemBase>(ItemId);
		//UE_LOG(LogTemp, Warning, TEXT("UpdateItemIcon : %d"), ItemId);
		ItemInHand->UpdateVisual(i->AssetFragment.Icon,0);
	}
}

void UVoxelItemPlaceWidget::UpdateCurItemDir(const bool IsTurnLeft) const
{
	if (AVoxelPlayerController* PC= GetOwningPlayer<AVoxelPlayerController>())
	{
		const EActorDirection NewDir= UChunkUtilsFunction::UpdateDir
		(IsTurnLeft,PC->StateCacheComp->GetInteractableDataInHand().Direction);

		PC->InteractionComp->RotateFurniture(NewDir);
		DirTextBox->SetText(FText::FromString(UEnum::GetValueAsString(NewDir)));
	}

}
