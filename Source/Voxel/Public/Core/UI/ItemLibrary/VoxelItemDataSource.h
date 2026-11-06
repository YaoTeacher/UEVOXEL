// Copyright (c) 2026 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/UI/ItemLibrary/ItemLibraryDataSource.h"
#include "VoxelItemDataSource.generated.h"

/**
 * 物品类数据源（M1 建造库 / M1b 模板库 共用）。
 *
 * 读：ConfigDataSubsystem::GetItemTypeArrayByCategory<FVoxelItemBase>(QueryTag)
 * 写：PC->InteractionComp->SetItemInHand(Place, EntryId, 1)
 */
UCLASS(EditInlineNew)
class VOXEL_API UVoxelItemDataSource : public UItemLibraryDataSource
{
	GENERATED_BODY()

public:
	/** 查询根 Tag（建造库 = Voxel.Item.Type.Block；模板库 = Voxel.Item.Type.BuildItem） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemLibrary|Source")
	FGameplayTag QueryTag;

	virtual TArray<FItemLibraryEntry> GetAllEntries_Implementation() const override;
	virtual void OnEntryActivated_Implementation(const FItemLibraryEntry& Entry, AVoxelPlayerController* PC) override;
};
