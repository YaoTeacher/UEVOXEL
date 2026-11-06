// Fill out your copyright notice in the Description page of Project Settings.


#include "Area/VoxelSpawnAreaComponent.h"

#include "Utils/ChunkUtilsFunction.h"

// Sets default values for this component's properties
UVoxelSpawnAreaComponent::UVoxelSpawnAreaComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


FRotator UVoxelSpawnAreaComponent::GetSpawnDirection() const
{
	return UChunkUtilsFunction::GetSpawnDirection(SpawnDirection);
}

// Called when the game starts
void UVoxelSpawnAreaComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UVoxelSpawnAreaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

