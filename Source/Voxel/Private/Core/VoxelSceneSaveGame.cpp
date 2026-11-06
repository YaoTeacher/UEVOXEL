// Fill out your copyright notice in the Description page of Project Settings.
#include "Core/VoxelSceneSaveGame.h"

#include "VoxelGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/CoreTags.h"


UVoxelSceneSaveGame* UVoxelSceneSaveGame::LoadSaveScene(FString InputName)
{
		if (UVoxelSceneSaveGame* loadedGameScene = Cast<UVoxelSceneSaveGame>(UGameplayStatics::LoadGameFromSlot(InputName,0)))
		{
			return  loadedGameScene;
		}
		return nullptr;

}

void UVoxelSceneSaveGame::SaveSceneData
	(UVoxelGameInstance* Instance,
		FString SaveSlotName,
		FDateTime Time,UWorld* thisWorld)
{
	if (!Instance || !thisWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveFailed: 无效参数"));
		return;
	}
    
	UVoxelBlockWorldSubsystem* BlockSys = thisWorld->GetSubsystem<UVoxelBlockWorldSubsystem>();
	UVoxelAreaWorldSubsystem* AreaSys = thisWorld->GetSubsystem<UVoxelAreaWorldSubsystem>();
    
	if (!BlockSys || !AreaSys) 
	{
		UE_LOG(LogTemp, Error, TEXT("SaveFailed: 缺少子系统"));
		return;
	}
    
	int32 UserIndex = 0;
	UVoxelSceneSaveGame* SaveObj = nullptr;
    
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, UserIndex))
	{
		SaveObj = Cast<UVoxelSceneSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, UserIndex));
	}
	else
	{
		SaveObj = Cast<UVoxelSceneSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UVoxelSceneSaveGame::StaticClass())
		);
	}
    
	if (!SaveObj)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveFailed: 无法创建存档对象"));
		return;
	}
    
	SaveObj->SaveData.BaseData = Instance->CurSaveData;
	SaveObj->SaveData.BaseData.LastSaveTime = Time;
    
	// 获取或创建场景条目
	auto& SceneDataEntry = SaveObj->SaveData.Scenes.FindOrAdd(Instance->SceneData.BaseData.SceneId);
	SceneDataEntry = Instance->SceneData;
	// 持久化层：把运行时 Handle（int32）转成 FullId（FString）写入存档
	// Air（Handle=0）固定写 "core:block.air"，提升可读性
	{
		UConfigDataSubsystem* CDS = Instance->GetSubsystem<UConfigDataSubsystem>();
		SceneDataEntry.ModifiedBlockData.Empty(SceneDataEntry.ModifiedBlockData.Num());
		SceneDataEntry.ModifiedBlockData.Reserve(BlockSys->CurSceneBlockTypeMask.Num());
		for (const auto& KV : BlockSys->CurSceneBlockTypeMask)
		{
			FString FullId;
			if (KV.Value == EBlockId::Air)
			{
				FullId = TEXT("core:block.air");
			}
			else if (CDS)
			{
				FullId = CDS->GetFullIdByHandle(KV.Value);
			}
			if (FullId.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("[SaveScene] BlockId=%d Handle=%d not found in ItemMap, skip"),
					KV.Key, KV.Value);
				continue;
			}
			SceneDataEntry.ModifiedBlockData.Add(KV.Key, MoveTemp(FullId));
		}
	}
	SceneDataEntry.Areas.Empty();
	// 保存区域
	for (auto& AreaPair : AreaSys->AreaIndex)
	{
		if (AreaPair.Value)
		{
			SceneDataEntry.Areas.Add(AreaPair.Key, AreaPair.Value->AreaBase);
		}
	}

	// 保存透明区域框（从本地玩家 BuildStateComp 收集，带 OwnerPlayerId）
	SceneDataEntry.CutoutBoxes.Empty();
	if (AVoxelPlayerController* PC = thisWorld->GetFirstPlayerController<AVoxelPlayerController>())
	{
		if (UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>())
		{
			TArray<FCutoutBoxData> Boxes = SC->GetCutoutBoxesForSave();
			for (const FCutoutBoxData& B : Boxes)
			{
				SceneDataEntry.CutoutBoxes.Add(B.Id, B);
			}
		}
	}

	// 保存玩家相机 Pawn（视野中心）位置
	if (APlayerController* PC = thisWorld->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			SceneDataEntry.PlayerPawnLocation = Pawn->GetActorLocation();
		}
	}
    
	UE_LOG(LogTemp, Log, TEXT("已保存场景 %d，包含 %d 个区域,%d 个方块"), 
		Instance->SceneData.BaseData.SceneId, 
		SceneDataEntry.Areas.Num(),
		SceneDataEntry.ModifiedBlockData.Num()
		);
    
	if (!UGameplayStatics::SaveGameToSlot(SaveObj, SaveSlotName, UserIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("SaveFailed: 保存到插槽 %s 失败"), *SaveSlotName);
	}
	
}

void UVoxelSceneSaveGame::ContinueTheGame()
{
	bIsContinuingGame = true;
}
