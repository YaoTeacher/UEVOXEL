// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Enum/GameStateEnum.h"
#include "VoxelSpawnAreaComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UVoxelSpawnAreaComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UVoxelSpawnAreaComponent();
	UPROPERTY(editAnywhere, BlueprintReadWrite)
	EActorDirection SpawnDirection;

	UFUNCTION(BlueprintCallable)
	FRotator GetSpawnDirection() const;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
