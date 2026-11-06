// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/CoreTags.h"
#include "Engine/DataTable.h"
#include "Data/Item/VoxelItemBase.h"
#include "VoxelBlockItemBase.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FVoxelBlockItemBase : public FVoxelItemBase
{
	GENERATED_BODY()

public:

	FVoxelBlockItemBase() {ItemTag=FCoreTags::Item_Type_Block;}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BlockType/Data")
	FVoxelBlockItemFragment BlockFragment;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BlockType/Data")
	FVoxelPhysicFragment PhysicFragment;

};
	
