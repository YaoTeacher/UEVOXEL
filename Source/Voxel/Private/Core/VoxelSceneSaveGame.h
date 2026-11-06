// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/Area/VoxelAreaBase.h"
#include "Data/Item/VoxelItemBase.h"
#include "GameFramework/SaveGame.h"
#include "Data/Scene/SceneData.h"

#include "VoxelSceneSaveGame.generated.h"
/**
 * 
 */

class UVoxelGameInstance;
class AChunkBase;

UCLASS(Blueprintable)
class UVoxelSceneSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(visibleAnywhere, Blueprintable,category="SaveGame/Scene")
	FSaveData SaveData;
	
	
	UFUNCTION(Blueprintable)
	static UVoxelSceneSaveGame* LoadSaveScene(FString InputName);
	
	UFUNCTION(Blueprintable)
	void SaveSceneData(UVoxelGameInstance* Instance, FString SaveSlotName, FDateTime Time, UWorld* thisWorld);

	UFUNCTION(BlueprintCallable)
	static void ContinueTheGame();
	UFUNCTION(BlueprintCallable)
	static bool IsContinuingGame(){return bIsContinuingGame;};
	UFUNCTION(BlueprintCallable)
	static void ConsumeContinuingGame(){bIsContinuingGame = false;};
	

	// UPROPERTY(visibleAnywhere, Blueprintable,category="SaveGame")
private:
	inline static bool bIsContinuingGame=false;
};
