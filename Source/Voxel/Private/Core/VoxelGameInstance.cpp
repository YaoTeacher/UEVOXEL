// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelGameInstance.h"

#include "VoxelSceneSaveGame.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "GameManager/WorldManager/VoxelCreatureWorldSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Core/VoxelControllerPawn.h"
#include "Core/VoxelGameUserSettings.h"

void UVoxelGameInstance::Init()
{
	UE_LOG(LogTemp, Warning, TEXT("START!!!"));

	Super::Init();

	// 加载用户设置并应用（可见范围+画质）。
	// 此时 GameInstance 已存在但世界未加载，ApplySettings 内的世界上下文同步可能找不到 GI，
	// 因此显式把 LoadDistance 写入 WorldConfig，保证后续子系统 Initialize 拿到用户值。
	if (UVoxelGameUserSettings* Settings = UVoxelGameUserSettings::Get())
	{
		Settings->LoadSettings();
		Settings->ApplySettings();
		WorldConfig.LoadDistance = Settings->LoadDistance;
		UE_LOG(LogTemp, Log, TEXT("[GameInstance] User settings applied: LoadDistance=%d"), WorldConfig.LoadDistance);
	}
}

void UVoxelGameInstance::Shutdown()
{
	Super::Shutdown();
}

const FSceneConfig* UVoxelGameInstance::FindSceneConfig(EScenceTypeEnum SceneType) const
{
	if (!WorldSceneDataTable) return nullptr;
	TArray<FSceneConfig*> Rows;
	WorldSceneDataTable->GetAllRows<FSceneConfig>(TEXT("FindSceneConfig"), Rows);
	for (const FSceneConfig* Row : Rows)
	{
		if (Row && Row->SceneType == SceneType)
		{
			return Row;
		}
	}
	return nullptr;
}

int32 UVoxelGameInstance::GetMainCharacterIdSafe(const UObject* WorldContextObject)
{
	if (GEngine)
	{
		if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
		{
			if (const UVoxelGameInstance* GI = World->GetGameInstance<UVoxelGameInstance>())
			{
				return GI->GetMainCharacterId();
			}
		}
	}
	return -1; // 未指派：无归属，权限判定一律失败
}


void UVoxelGameInstance::SaveCurScene()
{
	if(UVoxelSceneSaveGame* SaveGameInstance =
		Cast<UVoxelSceneSaveGame>(UGameplayStatics::CreateSaveGameObject(
			UVoxelSceneSaveGame::StaticClass())))
	{
		UE_LOG(LogTemp,Warning, TEXT("Save CurScene"));
		FDateTime CurrentTime = FDateTime::Now();
		SaveGameInstance->SaveSceneData(this, CurSaveData.SaveSlotName, CurrentTime,GetWorld());
	};
}

void UVoxelGameInstance::LoadSaveData(FString SaveSlotName)
{
    // 加载存档
    UVoxelSceneSaveGame* CurSceneSaveInstance = UVoxelSceneSaveGame::LoadSaveScene(SaveSlotName);
    
    // 情况1：存档不存在，创建默认数据
    if (!CurSceneSaveInstance)
    {
        UE_LOG(LogTemp, Warning, TEXT("LoadSaveData: 未找到存档 '%s'，创建默认数据"), *SaveSlotName);
        
        CurSaveData = FSaveBaseData();
        CurSaveData.SaveSlotName = SaveSlotName;
    	CurSaveData.SaveSlotId = 0;
    	CurSaveData.CurSceneId=0;
        LoadSceneData(0);  // 使用有效ID加载默认场景
        return;  // 重要：立即返回，避免后续逻辑
    }
    
    // 情况2：存档存在，从存档加载数据
    CurSaveData = CurSceneSaveInstance->SaveData.BaseData;

    // 存档版本校验：版本不匹配则作废旧档（不写迁移）
    if (CurSaveData.SaveVersion != FSaveBaseData::CurrentSaveVersion)
    {
        UE_LOG(LogTemp, Warning, TEXT("LoadSaveData: 存档版本不匹配 (存档=%d, 当前=%d)，作废旧档"),
            CurSaveData.SaveVersion, FSaveBaseData::CurrentSaveVersion);
        CurSaveData = FSaveBaseData();
        CurSaveData.SaveSlotName = SaveSlotName;
        CurSaveData.CurSceneId = 0;
        LoadSceneData(0);
        return;
    }

    int32 LoadId = CurSaveData.CurSceneId;  // 从存档获取当前场景ID
    
    // 检查场景ID是否有效
    if (!CurSceneSaveInstance->SaveData.Scenes.Contains(LoadId))
    {
        UE_LOG(LogTemp, Error, TEXT("LoadSaveData: 场景ID %d 在存档中不存在，回退到默认"), LoadId);
        LoadId = 0;  // 使用默认ID
    	
    }
    else
    {
        // 正确加载场景数据
        SceneData = CurSceneSaveInstance->SaveData.Scenes[LoadId];
    }
    
    // 加载场景内容
    LoadSceneData(LoadId);
}

void UVoxelGameInstance::LoadSceneData(int32 InputSceneId)
{
	
	// 1. 构造局部变量，准备加载
	FSceneData Scene = FSceneData(InputSceneId);
    
	// 尝试读取存档文件
	UVoxelSceneSaveGame* CurSceneSaveInstance = UVoxelSceneSaveGame::LoadSaveScene(CurSaveData.SaveSlotName);

	// 2. 判断是否需要新建场景
	// (存档文件不存在 OR 存档文件里没有这个场景ID)
	if (!CurSceneSaveInstance || !CurSceneSaveInstance->SaveData.Scenes.Contains(InputSceneId))
	{
		UE_LOG(LogTemp, Log, TEXT("LoadSceneData: 场景ID %d 无存档，准备生成新场景"), InputSceneId);

		// 按场景类型查配置表得维度与种子策略；无配置则用默认维度+随机种子
		const FSceneConfig* Config = FindSceneConfig(Scene.BaseData.SceneType);
		if (Config)
		{
			Scene.BaseData.WorldDimensionsInChunk = Config->WorldDimensionsInChunk;
			Scene.BaseData.SceneSeed = (Config->SeedStrategy == ESeedStrategy::Fixed)
				? Config->DefaultSeed
				: FMath::Rand();
			UE_LOG(LogTemp, Log, TEXT("LoadSceneData: 命中场景配置 Dim=%s SeedStrategy=%s"),
				*Config->WorldDimensionsInChunk.ToString(),
				Config->SeedStrategy == ESeedStrategy::Fixed ? TEXT("Fixed") : TEXT("Random"));
		}
		else
		{
			Scene.BaseData.SceneSeed = FMath::Rand();
		}
		Scene.ModifiedBlockData.Empty();
		Scene.Areas.Empty();
		Scene.PlayerPawnLocation = FVector::ZeroVector;
		bPendingNewScenePawnSnap = true;
	}
	else
	{
		// 3. 读取现有存档
		// 使用引用获取，减少一次 Map 查找，但后面赋值给 Scene 会发生拷贝
		// (考虑到 Scene 随后要传给子系统，这里的拷贝是可以接受的)
		Scene = CurSceneSaveInstance->SaveData.Scenes[InputSceneId];
		UE_LOG(LogTemp, Warning, TEXT("LoadSceneData: 成功从磁盘读取场景 %d, WorldDimensionsInChunk=%s"),
			InputSceneId, *Scene.BaseData.WorldDimensionsInChunk.ToString());
		// 读档：用存档里的 Pawn 位置
		bPendingNewScenePawnSnap = false;
	}
	
    // 获取世界实例
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("LoadSceneData: 无效的世界实例"));
        return;
    }
    
    // 获取子系统（带空指针检查）
    UVoxelBlockWorldSubsystem* BlockSys = World->GetSubsystem<UVoxelBlockWorldSubsystem>();
    if (!BlockSys)
    {
        UE_LOG(LogTemp, Error, TEXT("[BlockSys] LoadSceneData: BlockWorldSubsystem NOT READY yet! World=%s"), *World->GetName());
        return;
    }
	UE_LOG(LogTemp, Error, TEXT("[BlockSys] LoadSceneData: BlockSys OK, calling GenerateWorldBlock"));

	// 暂存完整场景数据（供后续生成区域/生物使用）
	SceneData = Scene;

	// 3. 绑定委托 (先移除防止重复绑定)
	BlockSys->OnWorldBuildFinished.RemoveDynamic(this, &UVoxelGameInstance::OnWorldGenerationComplete);
	BlockSys->OnWorldBuildFinished.AddDynamic(this, &UVoxelGameInstance::OnWorldGenerationComplete);

	// 4. 启动地形生成 (传入引用，内部会 MoveTemp 走方块数据)
	BlockSys->GenerateWorldBlock(this->SceneData);
    
	// 5. 更新部分基础数据
	this->CurSaveData.CurSceneId = InputSceneId;
    
	// 注意：此时不要调用 AreaSys 或 CreatureSys！
	UE_LOG(LogTemp, Log, TEXT("LoadSceneData: 已启动异步生成，等待回调..."));
}

// 【新增】回调函数：当地形生成完毕后自动执行
void UVoxelGameInstance::OnWorldGenerationComplete()
{
	UWorld* World = GetWorld();
	if (!World) return;

	UVoxelBlockWorldSubsystem* BlockSys = World->GetSubsystem<UVoxelBlockWorldSubsystem>();
	UVoxelAreaWorldSubsystem* AreaSys = World->GetSubsystem<UVoxelAreaWorldSubsystem>();
	UVoxelCreatureWorldSubsystem* CreatureSys = World->GetSubsystem<UVoxelCreatureWorldSubsystem>();

	if (BlockSys)
	{
		// 解除绑定，避免下次其他操作误触发
		BlockSys->OnWorldBuildFinished.RemoveDynamic(this, &UVoxelGameInstance::OnWorldGenerationComplete);
	}

	if (AreaSys && CreatureSys)
	{
		UE_LOG(LogTemp, Log, TEXT("地形生成完毕，开始生成区域和生物..."));

		// 1. 生成区域 (使用之前暂存的数据)
		AreaSys->GenerateAreas(this->SceneData);

		// 2. 生成生物 (此时有地形了，射线检测或位置修正可以正常工作)
		CreatureSys->StartSpawnTimer();
	}

	// 透明区域框恢复：加载本玩家的框到 BuildStateComp（PreviewComp 监听 OnCutoutChanged 自动应用）
	if (AVoxelPlayerController* PC = World->GetFirstPlayerController<AVoxelPlayerController>())
	{
		if (UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>())
		{
			TArray<FCutoutBoxData> Boxes;
			this->SceneData.CutoutBoxes.GenerateValueArray(Boxes);
			SC->LoadCutoutBoxes(Boxes);
		}
	}

	// 3. 恢复/贴地玩家相机 Pawn（视野中心）
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (bPendingNewScenePawnSnap)
			{
				// 新游戏：贴地（地形已就绪，仍可能需重试）
				if (AVoxelControllerPawn* ControllerPawn = Cast<AVoxelControllerPawn>(Pawn))
				{
					ControllerPawn->RequestSnapToGround();
				}
				bPendingNewScenePawnSnap = false;
			}
			else
			{
				// 读档：恢复存档位置
				const FVector& SavedLoc = this->SceneData.PlayerPawnLocation;
				if (!SavedLoc.IsNearlyZero())
				{
					Pawn->SetActorLocation(SavedLoc);
					UE_LOG(LogTemp, Log, TEXT("[Pawn] 读档恢复位置 %s"), *SavedLoc.ToString());
				}
			}
		}
	}
}

void UVoxelGameInstance::ChangeScene(int InputSceneId)
{
	if (InputSceneId < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ChangeScene: 非法场景ID %d，忽略"), InputSceneId);
		return;
	}

	if (InputSceneId != SceneData.BaseData.SceneId)
	{
		SaveCurScene();
		LoadSceneData(InputSceneId);
	}
	
}

// =============================================================
// 模板编辑场景切换
// =============================================================
void UVoxelGameInstance::EnterTemplateEditMode(int32 HandleId)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 暂存主场景信息
	StashedMainLevelName = World->GetFName();

	PendingEditHandleId = HandleId;

	// 切换到模板编辑场景
	// 注意：TemplateEditLevel 需要在 UE 编辑器中创建
	UGameplayStatics::OpenLevel(World, TEXT("TemplateEditLevel"));

	UE_LOG(LogTemp, Log, TEXT("[Template] EnterTemplateEditMode: HandleId=%d"), HandleId);
}

void UVoxelGameInstance::ExitTemplateEditMode(bool bSave)
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (bSave)
	{
		// 通知 TemplateEditWorldSubsystem 保存
		// 实际保存逻辑在 TemplateEditWorldSubsystem 中处理
		UE_LOG(LogTemp, Log, TEXT("[Template] Save requested before exit"));
	}

	// 切回主场景
	if (StashedMainLevelName != NAME_None)
	{
		UGameplayStatics::OpenLevel(World, StashedMainLevelName);
	}
	
	PendingEditHandleId = -2;
	
	UE_LOG(LogTemp, Log, TEXT("[Template] ExitTemplateEditMode: bSave=%d"), bSave);
}