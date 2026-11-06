// Copyright (c) 2026 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/UI/ItemLibrary/ItemLibraryDataSource.h"
#include "AreaTypeDataSource.generated.h"

/**
 * 区域类型数据源（M12b 区域类型库）。
 *
 * 读：ConfigDataSubsystem::GetAllAreaTypes()
 * 写：PC->InteractionComp->SetAreaType(EntryTag)
 */
UCLASS(EditInlineNew)
class VOXEL_API UAreaTypeDataSource : public UItemLibraryDataSource
{
	GENERATED_BODY()

public:
	virtual TArray<FItemLibraryEntry> GetAllEntries_Implementation() const override;
	virtual void OnEntryActivated_Implementation(const FItemLibraryEntry& Entry, AVoxelPlayerController* PC) override;
};
