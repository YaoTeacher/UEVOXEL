// Copyright (c) 2026 zolnoor. All rights reserved.

#include "Core/UI/ItemLibrary/VoxelTemplateDataSource.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"

#include "Data/Item/VoxelItemBase.h"
#include "Data/Item/InteractableData.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"

TArray<FItemLibraryEntry> UVoxelTemplateDataSource::GetAllEntries_Implementation() const
{
	TArray<FItemLibraryEntry> Entries;

	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UConfigDataSubsystem* CDS = GI ? GI->GetSubsystem<UConfigDataSubsystem>() : nullptr;

	UE_LOG(LogTemp, Warning, TEXT("[M1b] VoxelTemplateDataSource::GetAllEntries: World=%s, GI=%s, CDS=%s"),
		World ? TEXT("VALID") : TEXT("NULL"),
		GI ? TEXT("VALID") : TEXT("NULL"),
		CDS ? TEXT("VALID") : TEXT("NULL"));

	if (!CDS)
	{
		return Entries;
	}

	const TArray<FVoxelTemplate*> Templates = CDS->GetAllTemplates();
	UE_LOG(LogTemp, Warning, TEXT("[M1b] GetAllEntries: Templates.Num()=%d"), Templates.Num());

	Entries.Reserve(Templates.Num());
	for (const FVoxelTemplate* T : Templates)
	{
		if (!T)
		{
			continue;
		}
		FItemLibraryEntry Entry;
		Entry.EntryTag = T->ItemTag;
		Entry.HandleId = T->HandleId;
		Entry.DisplayName = T->ItemName;
		Entry.Description = T->ItemDescription;
		Entry.Icon = T->AssetFragment.Icon;
		Entry.TintColor = T->AssetFragment.Color;
		Entry.Price = static_cast<int32>(T->CirculateDataFragment.Value);
		Entries.Add(Entry);
	}
	return Entries;
}

void UVoxelTemplateDataSource::OnEntryActivated_Implementation(const FItemLibraryEntry& Entry, AVoxelPlayerController* PC)
{
	if (!PC || !PC->InteractionComp)
	{
		return;
	}
	PC->InteractionComp->SetItemInHand(EInteractableType::Place, Entry.HandleId, 1);
}
