// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/Enum/GameStateEnum.h"
#include "Data/Area/VoxelAreaBase.h"
#include "Data/Item/VoxelItemBase.h"
#include "Data/VoxelCutoutData.h"
#include "Engine/DataTable.h"
#include "SceneData.generated.h"

/**
 * 场景种子策略
 */
UENUM(BlueprintType)
enum class ESeedStrategy : uint8
{
	Fixed UMETA(DisplayName = "固定种子"),
	Random UMETA(DisplayName = "随机种子"),
};

/**
 * 场景类型配置（DataTable 行），按 EScenceTypeEnum 查表。
 * 新场景创建时从此配置读取维度与种子策略，创建后维度锁定不变。
 */
USTRUCT()
struct FSceneConfig : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config")
	EScenceTypeEnum SceneType = EScenceTypeEnum::TestScene;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config")
	FIntVector WorldDimensionsInChunk = FIntVector(2, 2, 1);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config")
	ESeedStrategy SeedStrategy = ESeedStrategy::Random;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config")
	int32 DefaultSeed = 0;
};

USTRUCT(BlueprintType)
struct FSceneBaseData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Scene")
	FString SceneName; // 场景显示名称

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Scene")
	EScenceTypeEnum SceneType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Scene")
	int SceneId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Scene")
	int SceneSeed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config")
	FIntVector WorldDimensionsInChunk = FIntVector(2, 2, 1);
	
	FSceneBaseData()
	{
		SceneName ="test_-1";
		SceneType = EScenceTypeEnum::TestScene;
		SceneId = -1;
		SceneSeed = 0;
	}

	FSceneBaseData(int InputSceneId)
	{
		SceneName = FString::Printf(TEXT("Scene_%d"), InputSceneId);
		SceneType = EScenceTypeEnum::TestScene;
		SceneId = InputSceneId;
		SceneSeed = 0;
	}

	FSceneBaseData(FString sceneName,EScenceTypeEnum sceneType,int sceneId,int sceneSeed)
	{
		SceneName = sceneName;
		SceneType = sceneType;
		SceneId = sceneId;
		SceneSeed = sceneSeed;
		
	}

	FORCEINLINE FBox GetWorldDimensionsInChunk() const
	{
		auto MakeAxis = [](int32 Dim) -> TPair<int32, int32>
		{
			return (Dim == 0) ? TPair<int32, int32>(0, 0)
							  : TPair<int32, int32>(-Dim, Dim - 1);
		};
		const auto X = MakeAxis(WorldDimensionsInChunk.X);
		const auto Y = MakeAxis(WorldDimensionsInChunk.Y);
		const auto Z = MakeAxis(WorldDimensionsInChunk.Z);
		return FBox(FVector(X.Key,  Y.Key,  Z.Key),
					FVector(X.Value, Y.Value, Z.Value));
	}
	
	FORCEINLINE bool IsChunkIndexInScene(FIntVector ChunkIndex) const
	{
		auto InRange = [](int32 Idx, int32 Dim) -> bool
		{
			// 维度为 0 时，仅允许 0
			if (Dim == 0) return Idx == 0;
			// 否则检查 [-Dim, Dim-1]
			const int32 Shifted = Idx + Dim;        // 区间平移到 [0, 2*Dim-1]
			return static_cast<uint32>(Shifted) < static_cast<uint32>(Dim * 2);
		};
		return InRange(ChunkIndex.X, WorldDimensionsInChunk.X) &&
			   InRange(ChunkIndex.Y, WorldDimensionsInChunk.Y) &&
			   InRange(ChunkIndex.Z, WorldDimensionsInChunk.Z);
	}
	
};


USTRUCT(BlueprintType)
struct FSceneData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

public:
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Scene")
	FSceneBaseData BaseData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Scene")
	TMap<int,FString> ModifiedBlockData;

	// 区域数据 (领地、建筑区等)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Scene")
	TMap<int32, FAreaInstance> Areas;

	// 透明区域框数据（穿过墙/屋顶查看编辑内部；含生效态，随场景持久化）
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Scene")
	TMap<int32, FCutoutBoxData> CutoutBoxes;

	// 玩家相机 Pawn（视野中心）位置，存档时保存、读档时恢复。
	// 新场景无此值（默认 ZeroVector），由 Pawn 贴地逻辑处理。
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Scene")
	FVector PlayerPawnLocation = FVector::ZeroVector;

	// 还可以加：实体(NPC/怪物)数据
	// UPROPERTY()
	// TArray<FNPCData> NPCs;

	FSceneData()
	{
		BaseData = FSceneBaseData();
		ModifiedBlockData.Empty();
		Areas.Empty();
		CutoutBoxes.Empty();
	}
	
	FSceneData(int InputSceneId)
	{
		BaseData = FSceneBaseData(InputSceneId);
		ModifiedBlockData.Empty();
		Areas.Empty();
		CutoutBoxes.Empty();
	}
};
USTRUCT(BlueprintType)
struct FSaveBaseData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int SaveSlotId; // 存档文件ID
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FString SaveSlotName; // 存档文件名

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int CurSceneId;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int CurPlayerId; // 玩家主角业务 CreatureId：当前测试阶段 = 0（系统角色身份）；角色系统上线后单机固定 1、联机 = 玩家 ID（1-99）

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FDateTime LastSaveTime; //最后游玩时间

	static constexpr int32 CurrentSaveVersion = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int32 SaveVersion = CurrentSaveVersion;

	FSaveBaseData()
	{
		SaveSlotId = 0;
		CurSceneId = 0;
		CurPlayerId = 0; // 角色系统未上线，测试用 0（系统角色身份）
		SaveSlotName="TEST";
	}
	
};
USTRUCT(BlueprintType)
struct FSaveData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FSaveBaseData BaseData;
	
	UPROPERTY(BlueprintReadWrite)
	TMap<int,FSceneData> Scenes;
	
};




