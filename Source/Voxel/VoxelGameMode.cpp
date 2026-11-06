// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelGameMode.h"
#include "VoxelPlayerController.h"
#include "Core/VoxelGameInstance.h"
#include "Core/VoxelSceneSaveGame.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"


void AVoxelGameMode::BeginPlay()
{
	Super::BeginPlay();
	//FurnitureNum= GetFurnitureNum();


}
AVoxelGameMode::AVoxelGameMode()
{
	// 配置自定义 PlayerController
	static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(TEXT("/Game/TopDown/Blueprints/BP_PlayerController.BP_PlayerController_C"));
	if (PlayerControllerBPClass.Class)
	{
		PlayerControllerClass = PlayerControllerBPClass.Class;
	}
}

// void AVoxelGameMode::WorldGenerate()
// {
// }


// void AVoxelGameMode::LoadScene()
// {
// 	
// }

// void AVoxelGameMode::SaveCurScene()
// {
// 	UE_LOG(LogTemp,Warning, TEXT("SaveCurScene"));
// 	SaveScene();
// }
//
// void AVoxelGameMode::SaveScene()
// {
// 	
// 	if(UVoxelSceneSaveGame* SaveGameInstance =Cast<UVoxelSceneSaveGame>(UGameplayStatics::CreateSaveGameObject(UVoxelSceneSaveGame::StaticClass())))
// 	{
// 		UE_LOG(LogTemp,Warning, TEXT("Initialized CurSceneSave"));
// 		SaveGameInstance->SaveSceneData();
// 	};
// 	
// }

AVoxelGameState* AVoxelGameMode::GetVoxelGameState()
{
	return GetGameState<AVoxelGameState>();
}

// void AVoxelGameMode::LoadGameWorld()
// {
// 	GameState = GetGameState<AVoxelGameState>();
// 	if (GameState)
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("START"));
// 		// LoadBlockType();
// 		// LoadSceneData();
// 		//GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>()->GenerateScencePFNav();
// 		// GameState->NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
// 		// GameState->NavMesh =Cast<ARecastNavMesh> (UGameplayStatics::GetActorOfClass(GetWorld(), ARecastNavMesh::StaticClass()));
// 		// ChangeScene(0);
// 		
// 	}
// 	else
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("无法获取 AVoxelGameState 实例！"));
// 	}
//
//
// }



// void AVoxelGameMode::GenerateScene(UVoxelSceneSaveGame* CurSceneSaveInstance)
// {
// 	auto transform = FTransform(
// FRotator::ZeroRotator,
// FVector::ZeroVector,
// FVector::OneVector
// );
	// const auto SceneManager = GetWorld()->SpawnActorDeferred<AVoxelSceneBaseInstance>(SceneType, transform, this);
	// if (CurSceneSaveInstance==nullptr)
	// {
	// 		UE_LOG(LogTemp, Log, TEXT("Generate Scene Without Save!"));
	// 	
	// 		SceneManager->SetActorLabel(SceneDataTable[GameState->curScence]->SceneName);
	// 		SceneManager->SceneBaseData.SceneId = SceneDataTable[GameState->curScence]->SceneId;
	// 		SceneManager->SceneBaseData.SceneName = SceneDataTable[GameState->curScence]->SceneName;
	// 		CurSceneSaveInstance=Cast<UVoxelSceneSaveGame>(UGameplayStatics::CreateSaveGameObject(UVoxelSceneSaveGame::StaticClass()));
	//
	// 	
	// }
	// else
	// {
	// 	UE_LOG(LogTemp, Log, TEXT("Find SceneSave,Generate Scene"));
	// 	SceneManager->SetActorLabel(CurSceneSaveInstance->SceneBaseData.SceneName);
	// 	SceneManager->SceneBaseData= CurSceneSaveInstance->SceneBaseData;
	// 	
	// }
	//
	// SceneManager->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
	// SceneManager->SetGameModeReference(this);
	// SceneManager->SetGameStateReference(GameState);
	// // CurSceneManagerInstance= SceneManager;
	// UGameplayStatics::FinishSpawningActor(SceneManager, FTransform());
	// SceneManager->InitialAllScene(CurSceneSaveInstance);;
// }


// void AVoxelGameMode::LoadSceneData()
//  {
//  
//  	if (SceneDataTable.Num() == 0)
//  	{
//  		if (TempSceneData == nullptr) {
//  			TempSceneData = LoadObject<UDataTable>(this, TEXT("/Script/Engine.DataTable'/Game/Resource/Data/WorldSceneDataTable.WorldSceneDataTable'"));
//  		}
//  		
//  		if (TempSceneData)
//  		{
//  			TempSceneData->GetAllRows(TEXT("Load BaseData Miss!"), SceneDataTable);
//  			UE_LOG(LogTemp, Log, TEXT("SUCCESS LOAD SCENE: %d"), SceneDataTable.Num());
//  			UE_LOG(LogTemp, Log, TEXT("===========SUCCESS LOAD========="));
//  		}
//  		
//  
//  	}
//  }




// void AVoxelGameMode::LoadBlockType()
// {
// 	// UVoxelGameInstance* GameInstance=GetGameInstance<UVoxelGameInstance>();
// 	// if (GameInstance->WorldItemType.Num() == 0)
// 	// {
// 	// 	if (TempBlockType == nullptr) 
// 	// 	{
// 	// 		TempBlockType = LoadObject<UDataTable>(this, TEXT("/Script/Engine.DataTable'/Game/Resource/DataRresource/ItemData/BlockType_DB.BlockType_DB'"));
// 	// 	}
// 	// 	
// 	// 	if (TempBlockType)
// 	// 	{
// 	// 		TempBlockType->GetAllRows(TEXT("Load WorldItemType!"), GameInstance->WorldItemType);
// 	// 		if (GameInstance== nullptr|| GameInstance->WorldItemType.Num()==0) {
// 	// 			UE_LOG(LogTemp, Log, TEXT("===========Failed========="));
// 	// 			return;
// 	// 		}
// 	// 		UE_LOG(LogTemp, Log, TEXT("SUCCESS LOAD : %s"), *TempBlockType->GetName());
// 	// 		UE_LOG(LogTemp, Log, TEXT("===========SUCCESS LOAD BlockType========="));
// 	// 		// for (int x = 0; x < GameState->WorldItemType.Num(); x++)
// 	// 		// {
// 	// 		// 	UE_LOG(LogTemp, Warning, TEXT("WorldItemType :%d  : %s"),x, *GameState->WorldItemType[x]->ItemName.ToString());
// 	// 		// }
// 	//
// 	// 	}
// 	//
// 	// }
// }

// FString AVoxelGameMode::LoadBlockTypeName()
// {
// 	FVoxelBlockItemBase* A =LoadBlockTypeById(1);
// 	if (A != nullptr)
// 	{
// 		return FString::FromInt(A->BlockFragment.BottomFaceTexture);
// 	}
// 	return "Not A Block";
// 	
// }

// FVoxelBlockItemBase* AVoxelGameMode::LoadBlockTypeById(int InputId) const
// {
// 	// 1. 前置校验
// 	if (!ItemDataRegistry || InputId < 0)
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("Invalid registry or ID: %d"), InputId);
// 		return nullptr;
// 	}
// 	// 2. 构造查询ID
// 	const FDataRegistryId ItemId("DR_Item", *FString::FromInt(InputId));
// 	// 3. 安全获取数据
// 	const FVoxelItemBase* BaseItem = ItemDataRegistry->GetCachedItem<FVoxelItemBase>(ItemId);
// 	if (!BaseItem)
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("Item not found: %s"), *ItemId.ItemName.ToString());
// 		return nullptr;
// 	}
// 	// 4. 类型安全转换
// 	if (BaseItem->ItemType!= EVoxelItemType::BuildBlock)
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("Item %d is not a block type"), InputId);
// 		return nullptr;
// 	}
// 	// 5. 二次验证（可选）
// 	const FVoxelBlockItemBase* BlockItem = static_cast<const FVoxelBlockItemBase*>(BaseItem);
// 	if (!BlockItem)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("Corrupted block data: %d"), InputId);
// 		return nullptr;
// 	}
// 	// 6. 返回可变引用（需确保设计上允许修改）
// 	return const_cast<FVoxelBlockItemBase*>(BlockItem);
//
// }

// FVoxelNormalItemBase* AVoxelGameMode::LoadItemById(int InputId)
// {
// 	if (!TempItemType)
// 	{
// 		TempItemType = LoadObject<UDataTable>(this, TEXT("/Script/Engine.DataTable'/Game/Resource/DataRresource/ItemData/Furniture_DB.Furniture_DB'"));
// 	}
//
// 	FName RowName =FName(FString::FromInt(InputId)) ; // 匹配数据表格式
// 	FVoxelNormalItemBase* ItemData = TempItemType->FindRow<FVoxelNormalItemBase>(RowName, TEXT(""));
//     
// 	// 添加内存有效性校验
// 	if (!ItemData)
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("Invalid item data for ID: %d"), InputId);
// 		return nullptr;
// 	}
//
// 	return ItemData;
// }

// TArray<FVoxelItemBase> AVoxelGameMode::LoadFurnitureListById(int32 StartId, int32 ListNum)
// {
// 	TArray<FVoxelItemBase> ResultList;
//
// 	// 1. 校验输入参数
// 	if (!TempItemType || ListNum <= 0)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("Invalid input: DataTable not set or ListNum <= 0!"));
// 		return ResultList;
// 	}
//
// 	// 2. 预加载并缓存有效家具数据（按ItemId排序）
// 	static TArray<FVoxelItemBase*> SortedFurnitureList;
// 	if (SortedFurnitureList.IsEmpty())
// 	{
// 		TArray<FVoxelItemBase*> AllRows;
// 		TempItemType->GetAllRows<FVoxelItemBase>(TEXT("LoadFurnitureListById"), AllRows);
//
// 		// 过滤并排序有效家具
// 		for (auto* Row : AllRows)
// 		{
// 			if (Row && Row->ItemType == EVoxelItemType::BuildItems)
// 			{
// 				SortedFurnitureList.Add(Row);
// 			}
// 		}
// 		// 按ItemId升序排序
// 		SortedFurnitureList.Sort([](const FVoxelItemBase& A, const FVoxelItemBase& B)
// 		{
// 			return A.ItemId < B.ItemId;
// 		});
// 	}
//
// 	// 3. 查找起始位置
// 	int32 StartIndex = 0;
// 	for (; StartIndex < SortedFurnitureList.Num(); ++StartIndex)
// 	{
// 		if (SortedFurnitureList[StartIndex]->ItemId >= StartId)
// 		{
// 			break;
// 		}
// 	}
//
// 	// 4. 提取连续条目
// 	for (int32 i = StartIndex; 
// 		 i < SortedFurnitureList.Num() && ResultList.Num() < ListNum; 
// 		 ++i)
// 	{
// 		ResultList.Add(*SortedFurnitureList[i]);
// 	}
//
// 	// 5. 越界警告
// 	if (ResultList.Num() < ListNum)
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("Requested %d items, but only found %d valid items starting from ID %d"), 
// 			ListNum, ResultList.Num(), StartId);
// 	}
//
// 	return ResultList;
// }

// int AVoxelGameMode::GetFurnitureNum()
// {
//
// 	if (TempItemType!=nullptr)
// 	{
// 		TArray<FVoxelItemBase*> AllRows;
// 		TempItemType->GetAllRows<FVoxelItemBase>(TEXT("LoadFurnitureListById"), AllRows);
// 	
// 		return AllRows.Num();
// 	}
// 	return 0;
//
// }
//
//
// FVoxelItemBase AVoxelGameMode::LoadFurnitureById(int InputId)
// {
// 	FVoxelItemBase DefaultItem;
// 	UE_LOG(LogTemp, Warning, TEXT("ItemDataTable : %s"), *TempItemType->GetName());
// 	if (TempItemType==nullptr)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("ItemDataTable is not initialized!"));
// 		return DefaultItem;
// 	}
//
// 	FName RowName = FName(FString::FromInt(InputId));
// 	FVoxelItemBase* ItemData = TempItemType->FindRow<FVoxelItemBase>(RowName, TEXT(""));
// 	if (!ItemData||ItemData->ItemType==EVoxelItemType::BuildItems)
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("Item ID %d not found!"), InputId);
// 		return DefaultItem;
// 	}
//
// 	return *ItemData;
// }
//
