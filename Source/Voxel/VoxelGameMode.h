// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataRegistry.h"
#include "Data/Scene/SceneData.h"
#include "Item/VoxelBlockItemBase.h"
#include "GameFramework/GameModeBase.h"
#include "Private/Utils/ChunkBase.h"
#include "VoxelGameState.h"
#include "VoxelGameMode.generated.h"



UCLASS(minimalapi)
class AVoxelGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AVoxelGameMode();
	
	// UPROPERTY(BlueprintReadWrite, Category = "World")
	// TArray<FVoxelBlockItemBase> BlockType;

	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	// TSubclassOf<AVoxelSceneBaseInstance> SceneType;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	TObjectPtr<UDataRegistry> ItemDataRegistry;

	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	// UDataTable* TempSceneData;
	//
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	// UDataTable* TempCreatureData;
	//
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	// UDataTable* TempItemType;
	//
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	// UDataTable* TempBlockType;
	//
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	// UDataTable* TempAreaData;

	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	// UDataTable* ActionFlowConfig;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	float ItemInterFaceDUration;

	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	// int FurnitureNum;
	
	// UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	// void LoadGameWorld();
	// UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	// void LoadScenceArea();
	// UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	// void LoadSceneData();
	// UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	// void GenerateScene(UVoxelSceneSaveGame* CurSceneSaveInstance);
	// UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	// void LoadBlockType();
	// UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	// FString LoadBlockTypeName();
	//
	//
	// FVoxelBlockItemBase* LoadBlockTypeById(int InputId) const;
	// UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	// TArray<FVoxelItemBase>LoadFurnitureListById(int StartId,int ListNum);
	// UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	// int GetFurnitureNum();
	// UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	// FVoxelItemBase LoadFurnitureById(int InputId);
	// FVoxelNormalItemBase* LoadItemById(int InputId);
	
	// UFUNCTION(BlueprintCallable, Category = "WorldEdit")
	// void WorldGenerate();

	// UFUNCTION(BlueprintCallable, Category = "场景管理")
	// void ChangeScene(int InputSceneId);
	// UFUNCTION(BlueprintCallable, Category = "场景管理")
	// void LoadScene();
	// UFUNCTION(BlueprintCallable, Category = "场景管理")
	// void SaveCurScene();
	// UFUNCTION(BlueprintCallable, Category = "场景管理")
	// void SaveScene();
	
	// UFUNCTION(BlueprintCallable, Category = "WorldEdit")
	// AVoxelSceneBaseInstance* GetCurSceneManager();
	UFUNCTION(BlueprintCallable, Category = "WorldEdit")
	AVoxelGameState* GetVoxelGameState();

	
	

protected:

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:

	//AVoxelSceneBaseInstance* CurSceneManagerInstance;

};



