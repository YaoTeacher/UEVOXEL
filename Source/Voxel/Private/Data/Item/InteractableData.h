// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/Item/VoxelItemBase.h"
#include "Core/CoreTags.h"
#include "InteractableData.generated.h"

/**
 * 
 */
UENUM()
enum class EInteractableType : uint8
{
	Pickup						UMETA(DisplayName = "Pickup"),
	Place						UMETA(DisplayName = "Place"),
	NonPlayerCharacter			UMETA(DisplayName = "NonPlayerCharacter"),
	Device						UMETA(DisplayName = "Device"),
	Toggle						UMETA(DisplayName = "Toggle"),
	Container					UMETA(DisplayName = "Container")
};


USTRUCT(Blueprintable)
struct FInteractableData:public FVoxelItemInstanceBase
{
	GENERATED_USTRUCT_BODY()

	public:

	FInteractableData(EInteractableType t,int32 id,int8 q)
	{InteractableType=t;
		HandleId=id;
		Quantity=q;
	};

	FInteractableData() :
		InteractableType(EInteractableType::Pickup)
	{
		HandleId=EBlockId::Invalid;
		Quantity=0;
	};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EInteractableType InteractableType;

	
	// used for things like valves, doors, etc. that require an interaction timer
	UPROPERTY(EditInstanceOnly)
	float InteractionDuration = 0.0f;
};
