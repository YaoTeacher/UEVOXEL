// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Voxel/VoxelCharacter.h"
#include "VoxelPlayerState.generated.h"


/**
 * 
 */
UCLASS()
class VOXEL_API AVoxelPlayerState : public APlayerState
{
	GENERATED_BODY()

public :

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerSate")
	TArray<AVoxelCharacter*> ControllableCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerSate")
	AVoxelCharacter* CharacterInControl;
	
};
