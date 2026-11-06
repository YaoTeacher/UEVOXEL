// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Item/VoxelBlockItemBase.h"
#include "NavigationSystem.h"
#include "Core/Enum/GameStateEnum.h"
#include "NavMesh/RecastNavMesh.h"
#include "VoxelGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FChangeBuildStateDelegate, FGameplayTag, ToolTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FChangeControllerModeDelegate, EVoxelControllerMode,VoxelControllerMode);
/**
 * 
 */


UCLASS()
class VOXEL_API AVoxelGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	AVoxelGameState();
    UPROPERTY(Replicated, BlueprintReadWrite, Category = "GameState")
    int32 PlayerScore;

    UPROPERTY(Replicated, BlueprintReadWrite, Category = "GameState")
    int32 CurrentMap;
	

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	UNavigationSystemV1* NavSystem;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	ARecastNavMesh* NavMesh;
	// UPROPERTY(EditAnywhere,Replicated, BlueprintReadWrite, Category = "PlayerSate")
	// EVoxelGameAuthority CurGameAuthority=EVoxelGameAuthority::Developer;
	// UPROPERTY(EditAnywhere,Replicated, BlueprintReadWrite, Category = "PlayerSate")
	// EVoxelControllerBuildState BuildState = EVoxelControllerBuildState::EditConstruction;
	// UPROPERTY(EditAnywhere,Replicated, BlueprintReadWrite, Category = "PlayerSate")
	// EVoxelControllerEditState EditState = EVoxelControllerEditState::Select;
	UPROPERTY(EditAnywhere,Replicated, BlueprintReadWrite, Category = "PlayerSate")
	bool bIsContinuing = false;//用于框选等持续性输入判断
	UPROPERTY(EditAnywhere,Replicated, BlueprintReadWrite, Category = "PlayerSate")
	EVoxelControllerGameState PlayerGameState = EVoxelControllerGameState::InGame;
	


	// UFUNCTION(BlueprintCallable, Category = "GameState")
	// EVoxelControllerBuildState GetBuildState();
	//
	// UFUNCTION(BlueprintCallable, Category = "GameState")
	// EVoxelControllerMode GetControllerMode();
	// UFUNCTION(BlueprintCallable, Category = "GameState")
	// void ChangeBuildState(EVoxelControllerBuildState Input);
	//
	// UFUNCTION(BlueprintCallable, Category = "GameState")
	// void ChangeControllerMode(EVoxelControllerMode Input);
	//
	// UFUNCTION(BlueprintCallable, Category = "GameState")
	// void ChangeEditState(EVoxelControllerEditState Input);
	
	// FVoxelBlockItemBase* GetItemInstanceById(int Input)
	// {
	// 	if(!WorldItemType.IsValidIndex(Input))
	// 	{
	// 		return WorldItemType[0];
	// 	}
	// 	return WorldItemType[Input];
	// };
	
	
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// UFUNCTION(BlueprintCallable, Category = "GameState")
	// void ChangeBatchMinPosition(FIntVector add);
	

	// TMap<FIntVector,int32>GroundMap;

	TMap<FName,int>CurSceneSpawnArea;
	
	//TArray<FVoxelBlockItemBase*> WorldItemType;

	TArray<int> NpcList;
	//
	// TArray<FAreaInstance*> WorldAreas;
	
};
