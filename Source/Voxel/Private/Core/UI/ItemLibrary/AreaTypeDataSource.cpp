// Copyright (c) 2026 zolnoor. All rights reserved.

#include "Core/UI/ItemLibrary/AreaTypeDataSource.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"

#include "Data/Area/AreaEnums.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"

TArray<FItemLibraryEntry> UAreaTypeDataSource::GetAllEntries_Implementation() const
{
	TArray<FItemLibraryEntry> Entries;

	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UConfigDataSubsystem* CDS = GI ? GI->GetSubsystem<UConfigDataSubsystem>() : nullptr;
	if (!CDS)
	{
		return Entries;
	}

	const TArray<const FAreaTypeBase*> AreaTypes = CDS->GetAllAreaTypes();
	Entries.Reserve(AreaTypes.Num());
	for (const FAreaTypeBase* AreaType : AreaTypes)
	{
		if (!AreaType)
		{
			continue;
		}
		FItemLibraryEntry Entry;
		Entry.EntryTag = AreaType->AreaTypeTag;
		Entry.DisplayName = AreaType->DisplayName;
		Entry.Description = AreaType->Description;
		Entry.Icon = AreaType->AssetFragment.Icon;
		Entry.TintColor = AreaType->AssetFragment.Color;
		Entries.Add(Entry);
	}
	return Entries;
}

void UAreaTypeDataSource::OnEntryActivated_Implementation(const FItemLibraryEntry& Entry, AVoxelPlayerController* PC)
{
	if (!PC || !PC->InteractionComp)
	{
		return;
	}
	PC->InteractionComp->SetAreaType(Entry.EntryTag);
}
