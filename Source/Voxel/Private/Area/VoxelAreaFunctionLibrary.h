// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoxelAreaFunctionLibrary.generated.h"

class  AVoxelGameState;
/**
 * 
 */
UCLASS()
class UVoxelAreaFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	UFUNCTION()
	TArray<FIntVector> GetNeighbourGround(FIntVector CurGround);

	
};
