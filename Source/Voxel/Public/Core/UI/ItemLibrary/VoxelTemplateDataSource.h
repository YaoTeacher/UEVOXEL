// Copyright (c) 2026 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/UI/ItemLibrary/ItemLibraryDataSource.h"
#include "VoxelTemplateDataSource.generated.h"

/**
 * 模板库数据源（M1b 专用）。
 *
 * 读：ConfigDataSubsystem::GetAllTemplates()（独立 RuntimeTemplates 仓库，不走 ItemMap 全表扫描）
 * 写：PC->InteractionComp->SetItemInHand(Place, EntryId, 1)
 */
UCLASS(EditInlineNew)
class VOXEL_API UVoxelTemplateDataSource : public UItemLibraryDataSource
{
	GENERATED_BODY()

public:
	virtual TArray<FItemLibraryEntry> GetAllEntries_Implementation() const override;
	virtual void OnEntryActivated_Implementation(const FItemLibraryEntry& Entry, AVoxelPlayerController* PC) override;
};
