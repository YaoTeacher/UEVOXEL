#include "Core/UI/Build/Modules/M16_ActionButtonsModule.h"
#include "Components/Button.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"
#include "Core/CoreTags.h"

void UM16_ActionButtonsModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ConfirmButton)
		ConfirmButton->OnClicked.AddDynamic(this, &UM16_ActionButtonsModule::OnConfirmClicked);
	if (ResetButton)
		ResetButton->OnClicked.AddDynamic(this, &UM16_ActionButtonsModule::OnResetClicked);
	if (UndoButton)
		UndoButton->OnClicked.AddDynamic(this, &UM16_ActionButtonsModule::OnUndoClicked);
	if (RedoButton)
		RedoButton->OnClicked.AddDynamic(this, &UM16_ActionButtonsModule::OnRedoClicked);
}

void UM16_ActionButtonsModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM16_ActionButtonsModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
}

void UM16_ActionButtonsModule::OnConfirmClicked()
{
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->RequestConfirmVoxelEdit();
		}
	}
}

void UM16_ActionButtonsModule::OnResetClicked()
{
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->RequestCancelVoxelEdit();
		}
	}
}

void UM16_ActionButtonsModule::OnUndoClicked()
{
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->Undo();
		}
	}
}

void UM16_ActionButtonsModule::OnRedoClicked()
{
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->Redo();
		}
	}
}
