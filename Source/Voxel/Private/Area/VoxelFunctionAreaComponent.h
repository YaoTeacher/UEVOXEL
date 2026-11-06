// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Time/VoxelTimeDataBase.h"
#include "VoxelFunctionAreaComponent.generated.h"


class AUVoxelItemBaseInstance;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UVoxelFunctionAreaComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UVoxelFunctionAreaComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="Area/WorkingData")
	int MaxUser =20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="Area/WorkingData")
	TMap<FIntVector,int> WaitSeat;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	TArray<TSubclassOf<AUVoxelItemBaseInstance>> ManagerBarList;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="Area/WorkingData")
	FBusinessSchedule BusinessSchedule;
		
};
