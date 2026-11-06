#include "Core/UI/HUD/Cutout/CutoutBoxListEntry.h"
#include "Core/UI/HUD/Cutout/CutoutBoxListItemObject.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/Engine.h"

void UCutoutBoxListEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (EnabledCheckBox) EnabledCheckBox->OnCheckStateChanged.AddDynamic(this, &UCutoutBoxListEntry::OnEnabledCheckStateChanged);
	if (DeleteButton) DeleteButton->OnClicked.AddDynamic(this, &UCutoutBoxListEntry::OnDeleteClicked);
	if (LocateButton) LocateButton->OnClicked.AddDynamic(this, &UCutoutBoxListEntry::OnLocateClicked);
}

void UCutoutBoxListEntry::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	ListItem = ListItemObject;
	UCutoutBoxListItemObject* Item = Cast<UCutoutBoxListItemObject>(ListItemObject);
	if (!Item) return;

	const FCutoutBoxData& D = Item->Data;
	if (LabelText) LabelText->SetText(FText::FromName(D.Label));
	if (SizeText)
	{
		const FIntVector Sz = D.MaxIndex - D.MinIndex + FIntVector(1);
		SizeText->SetText(FText::FromString(FString::Printf(TEXT("%dx%dx%d"), Sz.X, Sz.Y, Sz.Z)));
	}
	if (PositionText)
	{
		PositionText->SetText(FText::FromString(FString::Printf(TEXT("X%d Y%d Z%d"), D.MinIndex.X, D.MinIndex.Y, D.MinIndex.Z)));
	}
	if (EnabledCheckBox)
	{
		// SetIsChecked 会广播，但 OnEnabledCheckStateChanged -> SetBoxEnabled 在值未变时早退，无副作用
		EnabledCheckBox->SetIsChecked(D.bEnabled);
	}
}

void UCutoutBoxListEntry::OnEnabledCheckStateChanged(bool bChecked)
{
	UCutoutBoxListItemObject* Item = Cast<UCutoutBoxListItemObject>(ListItem);
	if (!Item) return;
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->SetBoxEnabled(Item->Data.Id, bChecked);
		}
	}
}

void UCutoutBoxListEntry::OnDeleteClicked()
{
	UCutoutBoxListItemObject* Item = Cast<UCutoutBoxListItemObject>(ListItem);
	if (!Item) return;
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->RemoveCutoutBox(Item->Data.Id);
		}
	}
}

void UCutoutBoxListEntry::OnLocateClicked()
{
	UCutoutBoxListItemObject* Item = Cast<UCutoutBoxListItemObject>(ListItem);
	if (!Item) return;
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->FocusCutoutBox(Item->Data.Id);
		}
	}
}
