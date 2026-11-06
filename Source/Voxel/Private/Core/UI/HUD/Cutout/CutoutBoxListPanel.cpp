#include "Core/UI/HUD/Cutout/CutoutBoxListPanel.h"
#include "Core/UI/HUD/Cutout/CutoutBoxListItemObject.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/CoreTags.h"
#include "Components/ListView.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"

void UCutoutBoxListPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PC = GetOwningPlayer())
	{
		CachedSC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
	}
	if (CachedSC)
	{
		CachedSC->OnCutoutChanged.AddDynamic(this, &UCutoutBoxListPanel::OnBoxListChanged);
	}
	if (NewButton)
	{
		NewButton->OnClicked.AddDynamic(this, &UCutoutBoxListPanel::OnNewButtonClicked);
	}
	if (AllShowButton)
	{
		AllShowButton->OnClicked.AddDynamic(this, &UCutoutBoxListPanel::OnAllShowClicked);
	}
	if (AllHideButton)
	{
		AllHideButton->OnClicked.AddDynamic(this, &UCutoutBoxListPanel::OnAllHideClicked);
	}
	RefreshList();
}

void UCutoutBoxListPanel::NativeDestruct()
{
	if (CachedSC)
	{
		CachedSC->OnCutoutChanged.RemoveDynamic(this, &UCutoutBoxListPanel::OnBoxListChanged);
	}
	Super::NativeDestruct();
}

void UCutoutBoxListPanel::OnNewButtonClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UVoxelInteractionComponent* InteractionComp = PC->FindComponentByClass<UVoxelInteractionComponent>())
		{
			InteractionComp->SwitchTool(FCoreTags::Tool_Cutout);
		}
	}
}

void UCutoutBoxListPanel::OnAllShowClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UVoxelInteractionComponent* InteractionComp = PC->FindComponentByClass<UVoxelInteractionComponent>())
		{
			InteractionComp->SetAllCutoutBoxesEnabled(true);
		}
	}
}

void UCutoutBoxListPanel::OnAllHideClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UVoxelInteractionComponent* InteractionComp = PC->FindComponentByClass<UVoxelInteractionComponent>())
		{
			InteractionComp->SetAllCutoutBoxesEnabled(false);
		}
	}
}

void UCutoutBoxListPanel::OnBoxListChanged()
{
	RefreshList();
}

void UCutoutBoxListPanel::RefreshList()
{
	if (!CachedSC || !BoxListView) return;

	TArray<FCutoutBoxData> Boxes = CachedSC->GetCutoutBoxes();

	TArray<UObject*> Items;
	Items.Reserve(Boxes.Num());
	for (const FCutoutBoxData& D : Boxes)
	{
		UCutoutBoxListItemObject* Item = NewObject<UCutoutBoxListItemObject>(this);
		Item->Data = D;
		Items.Add(Item);
	}
	BoxListView->SetListItems(Items);

	if (CountText)
	{
		CountText->SetText(FText::FromString(FString::Printf(TEXT("(%d)"), Boxes.Num())));
	}
	if (EmptyHint)
	{
		EmptyHint->SetVisibility(Boxes.Num() == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
