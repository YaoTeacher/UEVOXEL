// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "VoxelIntelligentCreatureInstance.h"
#include "Components/StateTreeAIComponent.h"
#include "CreatureAIController.generated.h"

/**
 * 
 */
UCLASS()
class ACreatureAIController : public AAIController
{
	GENERATED_BODY()
public:
	ACreatureAIController();
	// UPROPERTY(EditDefaultsOnly)
	// UBehaviorTree* BehaviourTree;

	UPROPERTY(EditInstanceOnly,Blueprintable)
	AVoxelIntelligentCreatureInstance* CreatureInstance;
	

	
	
protected:
	virtual void BeginPlay() override;
};
