// Copyright (c) 2026 zolnoor. All rights reserved.

#include "Core/UI/ItemLibrary/VoxelItemDataSource.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"

#include "Data/Item/VoxelItemBase.h"
#include "Data/Item/InteractableData.h"
#include "Item/VoxelBlockItemBase.h"
#include "Core/CoreTags.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"

TArray<FItemLibraryEntry> UVoxelItemDataSource::GetAllEntries_Implementation() const
{
	TArray<FItemLibraryEntry> Entries;
	if (!QueryTag.IsValid())
	{
		return Entries;
	}

	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UConfigDataSubsystem* CDS = GI ? GI->GetSubsystem<UConfigDataSubsystem>() : nullptr;
	if (!CDS)
	{
		return Entries;
	}

	const TArray<FVoxelItemBase*> Items = CDS->GetItemTypeArrayByCategory<FVoxelItemBase>(QueryTag);
	Entries.Reserve(Items.Num());
	for (const FVoxelItemBase* Item : Items)
	{
		if (!Item)
		{
			continue;
		}
		FItemLibraryEntry Entry;
		Entry.EntryTag = Item->ItemTag;
		Entry.HandleId = Item->HandleId;
		Entry.DisplayName = Item->ItemName;
		Entry.Description = Item->ItemDescription;
		Entry.Icon = Item->AssetFragment.Icon;
		Entry.TintColor = Item->AssetFragment.Color;
		Entry.Price = static_cast<int32>(Item->CirculateDataFragment.Value);

		// 方块条目：Icon 未配置时用外观贴图（侧面 BaseColor）作为列表图标
		// （方块表行结构为 FVoxelBlockItemBase，QueryTag=Item.Type.Block 的来源即方块表，static_cast 安全）
		if (!Entry.Icon && Item->ItemTag.MatchesTag(FCoreTags::Item_Type_Block))
		{
			const FVoxelBlockItemBase* BlockItem = static_cast<const FVoxelBlockItemBase*>(Item);
			Entry.Icon = BlockItem->BlockFragment.TextureSetup.Side.BaseColorMap.LoadSynchronous();
			if (!Entry.Icon)
			{
				UE_LOG(LogTemp, Warning, TEXT("[ItemLibrary] 方块 '%s' 既未配置 AssetFragment.Icon，TextureSetup.Side.BaseColorMap 也为空，列表图标将为空白"),
					*Item->GetFullId());
			}
		}
		else if (!Entry.Icon)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ItemLibrary] 物品 '%s' 未配置 AssetFragment.Icon，列表图标将为空白"),
				*Item->GetFullId());
		}

		Entries.Add(Entry);
	}
	return Entries;
}

void UVoxelItemDataSource::OnEntryActivated_Implementation(const FItemLibraryEntry& Entry, AVoxelPlayerController* PC)
{
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[ItemLibrary] OnEntryActivated: OwningPlayer 为空或非 AVoxelPlayerController，条目 %s 无法设置手持"), *Entry.EntryTag.ToString());
		return;
	}
	if (!PC->InteractionComp)
	{
		UE_LOG(LogTemp, Error, TEXT("[ItemLibrary] OnEntryActivated: PlayerController 缺少 InteractionComp，条目 %s 无法设置手持"), *Entry.EntryTag.ToString());
		return;
	}
	PC->InteractionComp->SetItemInHand(EInteractableType::Place, Entry.HandleId, 1);
}
