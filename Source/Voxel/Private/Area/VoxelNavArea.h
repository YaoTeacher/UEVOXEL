// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/Area/AreaEnums.h"
#include "NavAreas/NavArea_Default.h"
#include "VoxelNavArea.generated.h"

/**
 * 
 */
UCLASS(Config=Engine, MinimalAPI)
class UVoxelNavArea : public UNavArea_Default
{
	GENERATED_UCLASS_BODY()

	public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="NavArea/AreaAuthor")
	bool bIsPrivate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="NavArea/WorkingData")
	FGameplayTag  AreaType;

	
	
};
