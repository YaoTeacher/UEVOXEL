// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelIntelligentCreatureInstance.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Components/ActorComponent.h"
#include "Data/Creature/CreatureBase.h"
#include "ConsumComponent.generated.h"


UCLASS( Blueprintable,ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UConsumComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UConsumComponent();

	AVoxelIntelligentCreatureInstance* ParentInstance;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
