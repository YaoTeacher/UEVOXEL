#include "VoxelBlockWorldSubsystem.h"
#include "Core/VoxelGameInstance.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "GameManager/GameInstanceManager/VoxelTextureRegistry.h"
#include "Kismet/GameplayStatics.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Utils/ChunkBase.h"
#include "WorldGenerate/GreedyChunk.h"
#include "GameManager/WorldManager/WorldObjectPoolSubsystem.h"
#include "GameManager/WorldManager/VoxelItemWorldSubsystem.h"
#include "Item/VoxelItemBaseInstance.h"
#include "Core/CoreTags.h"
#include "Async/Async.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Data/Item/VoxelItemBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Engine/Texture2DArray.h"

void UVoxelBlockWorldSubsystem::GenerateWorldBase()
{
	GameInstance = Cast<UVoxelGameInstance>(GetWorld()->GetGameInstance());
	if(!GameInstance) return;

	ConfigSubsystemCache = GameInstance->GetSubsystem<UConfigDataSubsystem>();
	TextureRegistryCache = GameInstance->GetSubsystem<UVoxelTextureRegistry>();
	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UVoxelBlockWorldSubsystem::OnPostWorldInit);

	WorldDimension = GameInstance->WorldConfig.GetWorldDimensions(GameInstance->GetCurScene().WorldDimensionsInChunk);
	ChunkSize = GameInstance->WorldConfig.GetChunkSize();
	BlockLength = GameInstance->WorldConfig.BlockLength;

	UE_LOG(LogTemp, Warning, TEXT("[BlockSys] GenerateWorldBase: ChunkActorClass=%s, ChunkLength=%d, ChunkHeight=%d"),
		GameInstance->WorldConfig.ChunkActorClass ? *GameInstance->WorldConfig.ChunkActorClass->GetName() : TEXT("NULL"),
		GameInstance->WorldConfig.ChunkLength, GameInstance->WorldConfig.ChunkHeight);

	// 通过身份字符串查地表默认方块句柄（四元身份体系：core:block.stone → hash → Handle）
	// 失败时保持 EBlockId::Air，地表将不渲染（安全降级）
	if (ConfigSubsystemCache)
	{
		GroundBlockHandleId = ConfigSubsystemCache->GetHandleByFullId(TEXT("core:block.stone"));
		if (GroundBlockHandleId == EBlockId::Invalid)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BlockSys] GroundBlockHandleId not found for 'core:block.stone', terrain will be empty."));
		}
	}
    
	// 优化：根据 CPU 核心数动态设置并发度 (保留 2 个核心给主线程和渲染线程)
	int32 NumCores = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
	MaxConcurrentTasks = FMath::Max(1, NumCores - 2);
	UE_LOG(LogTemp, Log, TEXT("Voxel System Initialized. MaxConcurrentTasks: %d"), MaxConcurrentTasks);

	PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubsystem>();
	if (PoolSubsystem)
	{
		PoolSubsystem->RegisterPool<AChunkBase>(
			"ChunkPool",
			[this]() -> AChunkBase* {
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				return GetWorld()->SpawnActor<AChunkBase>(
					GameInstance->WorldConfig.ChunkActorClass, 
					FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams
				);
			},
			[](AChunkBase* Chunk) { if (IsValid(Chunk)) Chunk->ResetForPool(); },
			GameInstance->WorldConfig.InitialPoolSize()
		);
	}
}

void UVoxelBlockWorldSubsystem::GenerateWorldBlock(FSceneData& CurScene)
{
	UE_LOG(LogTemp, Error, TEXT("[BlockSys] GenerateWorldBlock START"));

	// 0. 刷新世界参数快照：维度只在切场景时变，在此用传入的真实场景数据重算，
	//    确保 WorldDimension/ChunkSize/BlockLength 与 GetWorldBorder 永远同源。
	//    （消除 P1：原仅在 Initialize 快照一次，切场景后不重算）
	if (GameInstance)
	{
		WorldDimension = GameInstance->WorldConfig.GetWorldDimensions(CurScene.BaseData.WorldDimensionsInChunk);
		ChunkSize = GameInstance->WorldConfig.GetChunkSize();
		BlockLength = GameInstance->WorldConfig.BlockLength;
	}

	// 透明区域框有效集由 CutoutWorldSubsystem::GenerateCutouts 按 OwnerPlayerId 过滤后推入
	// （此处早于本地 PC 就绪，无法判定本地玩家；初始 CurrentCutoutBoxes 留空）

    // 1. 硬同步：彻底停止并等待所有后台任务完成
    bShouldStopAllTasks = true;
    
    // 等待所有异步计算任务完成（必须保证任务池为空）
    while (true)
    {
        FScopeLock Lock(&TaskMutex);
        if (PendingGenerationTasks.IsEmpty())
        {
            break;
        }
        FPlatformProcess::Sleep(0.01f); // 避免忙等待
    }

    // 2. 全面清理：在锁保护下清空所有状态和队列
    {
        FWriteScopeLock BlockLock(BlockMapLock);
        FWriteScopeLock EditLock(PendingEditsLock);
        FScopeLock TaskLock(&TaskMutex);

        // 清空核心数据（必须在锁内操作）
        // 加载层：把存档中的 FullId（FString）解析回运行时 Handle（int32）
        // Air（"core:block.air"）固定映射到 EBlockId::Air=0；解析失败的 FullId 跳过并警告
        {
            UConfigDataSubsystem* CDS = GetWorld()->GetGameInstance<UVoxelGameInstance>()->GetSubsystem<UConfigDataSubsystem>();
            CurSceneBlockTypeMask.Empty(CurScene.ModifiedBlockData.Num());
            for (const auto& KV : CurScene.ModifiedBlockData)
            {
                int32 Handle = EBlockId::Invalid;
                if (KV.Value == TEXT("core:block.air"))
                {
                    Handle = EBlockId::Air;
                }
                else if (CDS)
                {
                    Handle = CDS->GetHandleByFullId(KV.Value);
                }
                if (Handle == EBlockId::Invalid)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[BlockSys] LoadScene: BlockId=%d FullId='%s' not found in ItemMap, skip"),
                        KV.Key, *KV.Value);
                    continue;
                }
                CurSceneBlockTypeMask.Add(KV.Key, Handle);
            }
        }
        SceneBaseBlockTypeMask.Empty(); // 将在后台线程重建

        // 彻底清空所有队列
		FChunkLoadRequest TempReq;
		while(LoadingQueue.Dequeue(TempReq));
		while(GeneratingQueue.Dequeue(TempReq));
		while(CalculateQueue.Dequeue(TempReq));
		while(RenderingQueue.Dequeue(TempReq));
		while(UnloadingQueue.Dequeue(TempReq));

        // 清空状态集合
        ActiveRequestsSet.Empty();
        PendingGenerationTasks.Empty();
        
        // 释放所有已加载的区块Actor
        if (PoolSubsystem)
        {
            for (auto& Pair : LoadedChunks)
            {
                if (Pair.Value.IsValid())
                {
                    PoolSubsystem->Release("ChunkPool", Pair.Value.Get());
                }
            }
        }
        LoadedChunks.Empty();
        ChunkMetaDataMap.Empty();
    }

    UE_LOG(LogTemp, Error, TEXT("[BlockSys] States cleaned. CurSceneBlockTypeMask=%d, starting async terrain rebuild..."), CurSceneBlockTypeMask.Num());

    // 3. 异步重建：在后台线程执行耗时地形生成
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
    {
        // 重建整个世界的地形基础数据
        GenerateBlockTypeMasks();
    	UE_LOG(LogTemp, Error, TEXT("[BlockSys] GenerateBlockTypeMasks DONE. SceneBaseBlockTypeMask=%d"), SceneBaseBlockTypeMask.Num());

        // 返回主线程恢复系统
        AsyncTask(ENamedThreads::GameThread, [this]()
        {
            // 4. 安全恢复：数据重建完成后才开放任务处理
            bShouldStopAllTasks = false;
            
            // 强制触发流式加载
            bPlayersMoved = true;
            RequestStreamingUpdate();
        	bIsWorldRebuilding = true;
			InitialLoadStableTimer = 0.1f; // 重置计时器
            
			UE_LOG(LogTemp, Error, TEXT("[BlockSys] Terrain rebuild complete, streaming triggered. bShouldStopAllTasks=false"));
        });
    });
}

bool UVoxelBlockWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if(ReceiveShouldCreateSubsystem()) return Super::ShouldCreateSubsystem(Outer);
    return false;
}

void UVoxelBlockWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    
	GenerateWorldBase();
    Super::Initialize(Collection);
	
    ReceiveInitialize();
	UE_LOG(LogTemp, Error, TEXT("[BlockSys] Initialize DONE. WorldDim=%s, ChunkSize=%s, BlockLength=%d, PoolSubsystem=%s"),
		*WorldDimension.ToString(), *ChunkSize.ToString(), BlockLength, PoolSubsystem ? TEXT("OK") : TEXT("NULL"));
}

void UVoxelBlockWorldSubsystem::Deinitialize()
{
	StopAllBackgroundTasks();
	CleanupAllResources();
    Super::Deinitialize();
    ReceiveDeinitialize();
}

void UVoxelBlockWorldSubsystem::CleanupAllResources()
{
	// TQueue 没有 Empty()，需要手动清空
	FChunkLoadRequest Temp;
	while(LoadingQueue.Dequeue(Temp));
	while(GeneratingQueue.Dequeue(Temp));
	while(CalculateQueue.Dequeue(Temp));
	while(UnloadingQueue.Dequeue(Temp));
	while(RenderingQueue.Dequeue(Temp));
    
	{
		FWriteScopeLock Lock(PendingEditsLock);
		FVoxelEditModification TempMod;
		while (PendingModifications.Dequeue(TempMod));
	}
    
	ActiveRequestsSet.Empty();
	
	{
		FWriteScopeLock Lock(BlockMapLock);
		CurSceneBlockTypeMask.Empty();
	}
	ChunkMetaDataMap.Empty();
	LoadedChunks.Empty();
    
	bStreamingRequested = false;
	bIsWaitingToUpdateBlock = false;
	bShouldStopAllTasks = false;
}

void UVoxelBlockWorldSubsystem::StopAllBackgroundTasks()
{
	bShouldStopAllTasks = true;
	FPlatformProcess::Sleep(0.1f); 
	FScopeLock Lock(&TaskMutex);
	PendingGenerationTasks.Empty();
}

void UVoxelBlockWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if(!bShouldStopAllTasks)
	{
		CheckPlayersMovement();
    
		if (bIsWaitingToUpdateBlock)
		{
			ProcessBlockEdits();
		}
    
		if (bPlayersMoved)
		{
			RequestStreamingUpdate();
		}
    
		if (bStreamingRequested) 
		{
			// 优化：如果有大量任务正在排队，不需要频繁处理流式加载
			if (LoadingQueue.IsEmpty()) 
			{
				StreamingDelay += DeltaTime;
				if (StreamingDelay >= MaxStreamingDelay) 
				{
					ProcessChunkStreaming();
					bStreamingRequested = false;
					StreamingDelay = 0.0f;
				}
			}
		}
    
		// 总入口：处理各类队列
		ProcessChunkUpdates();

		if (bIsWorldRebuilding)
		{
			InitialLoadStableTimer += DeltaTime;

			// 1. 等待至少 0.5 秒，确保 LoadingQueue 已经被 RequestStreamingUpdate 填满了
			if (InitialLoadStableTimer > 5.f) 
			{
				// 2. 检查所有队列状态
				bool bQueuesEmpty = LoadingQueue.IsEmpty() && 
									GeneratingQueue.IsEmpty() && 
									CalculateQueue.IsEmpty() && 
									RenderingQueue.IsEmpty() &&
									UnloadingQueue.IsEmpty();

				// 3. 检查后台线程任务
				bool bTasksEmpty = false;
				{
					FScopeLock Lock(&TaskMutex);
					bTasksEmpty = PendingGenerationTasks.IsEmpty();
				}

				// 4. 如果所有任务都清空了，说明初始范围内的 Chunk 全部生成完毕
			if (bQueuesEmpty && bTasksEmpty)
			{
				UE_LOG(LogTemp, Error, TEXT("[BlockSys] === World Fully Built! Broadcasting OnWorldBuildFinished ==="));

				bIsWorldRebuilding = false; // 结束监控

				// 广播事件 -> 通知 GameInstance 生成生物
				if (OnWorldBuildFinished.IsBound())
				{
					OnWorldBuildFinished.Broadcast();
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[BlockSys] OnWorldBuildFinished NOT BOUND!"));
				}
			}
		}
	}
	}
}

bool UVoxelBlockWorldSubsystem::IsTickable() const { return Super::IsTickable(); }
TStatId UVoxelBlockWorldSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UVoxelBlockWorldSubsystem, STATGROUP_Tickables); return Super::GetStatId(); }

void UVoxelBlockWorldSubsystem::OnPostWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
    if (!World || !World->IsGameWorld()) return;
    GenerateScencePFNav();
}
void UVoxelBlockWorldSubsystem::GenerateScencePFNav()
{
    NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
    NavMesh = Cast<ARecastNavMesh>(UGameplayStatics::GetActorOfClass(GetWorld(), ARecastNavMesh::StaticClass()));
    NavData = Cast<ANavigationData>(UGameplayStatics::GetActorOfClass(GetWorld(), ANavigationData::StaticClass()));
    BoundsVolume = Cast<ANavMeshBoundsVolume>(UGameplayStatics::GetActorOfClass(GetWorld(), ANavMeshBoundsVolume::StaticClass()));
    
    if (BoundsVolume != nullptr)
    {
        auto transform = FTransform(
            FRotator::ZeroRotator,
            FVector::ZeroVector,
            FVector(WorldDimension) * BlockLength * 0.01f
        );
        BoundsVolume->SetActorTransform(transform);
        UE_LOG(LogTemp, Warning, TEXT("BoundsVolume initialize"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("none BoundsVolume"));
    }
}



void UVoxelBlockWorldSubsystem::GenerateBlockTypeMasks()
{
	// -----------------------------------------------------------------------
    // 1. 准备阶段：为每个并行任务分配独立的存储空间
    // -----------------------------------------------------------------------
    int SizeY = WorldDimension.Y * 2;
    
    // 创建一个数组，数组长度等于我们将要执行的任务数量（即 Y 轴的行数）
    // 每个元素都是一个 Map，对应每一行的计算结果
    TArray<TMap<int, int32>> ThreadResults;
    ThreadResults.SetNum(SizeY); 
	
    // -----------------------------------------------------------------------
    // 2. 并行计算阶段 (完全无锁 - Lock Free)
    // -----------------------------------------------------------------------
    ParallelFor(SizeY, [&](int YIndex) {
        
        // 获取当前线程/任务专属的 Map 引用
        // 因为每个线程操作数组中不同的 Index，所以这里绝对线程安全，不需要锁
        TMap<int, int32>& LocalRowMap = ThreadResults[YIndex];
        
        // 预估一下这一行大概有多少非空气方块，避免局部 Map 扩容
        // 假设 X 轴宽 * 6 层厚度 (Z从底座到+5)
        LocalRowMap.Reserve((WorldDimension.X * 2) * 6);

        int Y = YIndex - WorldDimension.Y;
        
        // 【优化】限制 Z 轴循环范围，不再无效空跑
        // 只循环需要生成地形的区间 (-WorldDimension.Z 到 5)
        int ZStart = -WorldDimension.Z;
        int ZEnd = WorldDimension.Z - 1; 
		int32 BlockTag = EBlockId::Air;
        for (int X = -WorldDimension.X; X < WorldDimension.X; X++)
        {
            for (int Z = ZStart; Z <= ZEnd; Z++)
            {
                FIntVector BlockIndex(X, Y, Z);
                int BlockId = UChunkUtilsFunction::GetBlockIdByIndex(BlockIndex, WorldDimension);
				
                // 简单的地形生成逻辑
            	if (Z <= 0)
            	{
					BlockTag = GroundBlockHandleId;
				}
				else
				{
					// 地上层：空气
					BlockTag = EBlockId::Air;
				}

    //         	if (BlockIndex == FIntVector(-2, 2, 3)) {
				// 	UE_LOG(LogTemp, Error, TEXT("DEBUG CTX: Pos(-2,2,3) -> ID(%d) -> Found(%s)"), 
				// 		BlockId, *BlockTag.ToString());
				// }

                    // 直接写入本地 Map，无锁
            	LocalRowMap.Add(BlockId, BlockTag);
            }
        }
    });

    // -----------------------------------------------------------------------
    // 3. 合并阶段 (主线程 - 只锁一次)
    // -----------------------------------------------------------------------
    {
        // 此时并行计算已结束，只有主线程在跑，但为了安全写入成员变量，加一次写锁
        FWriteScopeLock Lock(BlockMapLock);
        
        // 3.1 统计总数量，进行一次性精确内存分配
        int32 TotalElements = 0;
        for (const auto& Map : ThreadResults)
        {
            TotalElements += Map.Num();
        }
        
        // 3.2 清空并预留精确空间 (避免 Append 过程中的扩容拷贝)
        SceneBaseBlockTypeMask.Empty(TotalElements);

        // 3.3 快速合并
        for (auto& Map : ThreadResults)
        {
            if (Map.Num() > 0)
            {
                // 使用 MoveTemp 将临时 Map 的数据的所有权直接“移动”进去
                SceneBaseBlockTypeMask.Append(MoveTemp(Map));
            }
        }
    }
}
void UVoxelBlockWorldSubsystem::GenerateBlockTypeMaskInChunk(FIntVector ChunkIndex)
{
	// 计算区块的世界坐标范围
	FIntVector MinBlockIndex = ChunkIndex * ChunkSize;
	FIntVector MaxBlockIndex = MinBlockIndex + ChunkSize - FIntVector(1, 1, 1);
    
	// 只生成地面层附近的方块
	if (MinBlockIndex.Z > 5) return;
    
	for (int32 Z = MinBlockIndex.Z; Z <= MaxBlockIndex.Z; Z++)
	{
		for (int32 Y = MinBlockIndex.Y; Y <= MaxBlockIndex.Y; Y++)
		{
			for (int32 X = MinBlockIndex.X; X <= MaxBlockIndex.X; X++)
			{
				FIntVector BlockIndex(X, Y, Z);
				GenerateBlockTypeMask(BlockIndex);
			}
		}
	}
}

void UVoxelBlockWorldSubsystem::GenerateBlockTypeMask(FIntVector Input)
{
	// 1. 边界检查
	if (Input.Z > 5) return;

	// 2. 计算 BlockID (Key)
	int BlockId = UChunkUtilsFunction::GetBlockIdByIndex(Input, WorldDimension);

	// 3. 决定方块类型 (Value)：Z<=5 用 GroundBlockHandleId（来自身份字符串 core:block.stone），否则空气
	int32 BlockTag = (Input.Z <= 5) ? GroundBlockHandleId : EBlockId::Air;

	// 4. 线程安全写入
	FWriteScopeLock Lock(BlockMapLock);

	// [关键]：如果是有效方块，存入 Map；如果是空气/Empty，从 Map 移除以省内存
	if (BlockTag != EBlockId::Invalid)
	{
		SceneBaseBlockTypeMask.FindOrAdd(BlockId) = BlockTag;
	}
	else
	{
		// 如果本来就没有，Remove 操作也是安全的且开销很小
		SceneBaseBlockTypeMask.FindOrAdd(BlockId) = EBlockId::Air;
	}
}

int UVoxelBlockWorldSubsystem::GetTextureIndex(const int32 TypeID, const FVector Normal) const
{
	// Air 无纹理
	if (TypeID == EBlockId::Air)
	{
		return 0;
	}
	if (const FVoxelBlockItemBase* BlockInfo = GetBlockInform(TypeID))
	{
		if (TextureRegistryCache)
		{
			return TextureRegistryCache->GetTextureIndex(
				BlockInfo->BlockFragment.TextureSetup.GetFaceByNormal(Normal));
		}
	}
	return 0;
}

bool UVoxelBlockWorldSubsystem::CheckBlockOpaque(int32 InputId)
{
	// Air 透明
	if (InputId == EBlockId::Air)
	{
		return false;
	}
	if (const FVoxelBlockItemBase* BlockInfo = GetBlockInform(InputId))
	{
		return BlockInfo->BlockFragment.IsSolid && !BlockInfo->BlockFragment.IsRenderNeibor;

	}
	return false; // 未知方块视为透明，避免错误渲染
}

UMaterialInstanceDynamic* UVoxelBlockWorldSubsystem::GetSharedChunkMID()
{
	// 已创建则直接返回
	if (SharedChunkMID)
	{
		return SharedChunkMID;
	}

	if (!GameInstance || !GameInstance->WorldConfig.ChunkActorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlockSys] GetSharedChunkMID: ChunkActorClass not configured."));
		return nullptr;
	}

	// 从 ChunkActorClass 的 CDO 获取父材质（蓝图配置的 Material）
	AChunkBase* ChunkCDO = GameInstance->WorldConfig.ChunkActorClass->GetDefaultObject<AChunkBase>();
	if (!ChunkCDO || !ChunkCDO->Material)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlockSys] GetSharedChunkMID: Chunk CDO Material not configured."));
		return nullptr;
	}

	// 创建共享 MID
	SharedChunkMID = UMaterialInstanceDynamic::Create(ChunkCDO->Material, this);
	if (!SharedChunkMID)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlockSys] GetSharedChunkMID: Failed to create MID."));
		return nullptr;
	}

	// 设置 Texture2DArray 参数
	if (TextureRegistryCache)
	{
		if (UTexture2DArray* BaseColorArray = TextureRegistryCache->GetBaseColorArray())
		{
			SharedChunkMID->SetTextureParameterValue(FName(TEXT("BaseColorArray")), BaseColorArray);
		}
		if (UTexture2DArray* NormalArray = TextureRegistryCache->GetNormalArray())
		{
			SharedChunkMID->SetTextureParameterValue(FName(TEXT("NormalArray")), NormalArray);
		}
		if (UTexture2DArray* ORMArray = TextureRegistryCache->GetORMArray())
		{
			SharedChunkMID->SetTextureParameterValue(FName(TEXT("ORMArray")), ORMArray);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[BlockSys] GetSharedChunkMID: Created shared Chunk MID with Texture2DArray params."));
	return SharedChunkMID;
}

void UVoxelBlockWorldSubsystem::ApplySlice(float SliceHeight, bool bEnabled, const FSliceAreaConfig& AreaConfig)
{
	// 1. 计算新的 SliceLayer（INT32_MAX = 无切面）
	const int32 NewSliceLayer = bEnabled
		? FMath::RoundToInt(SliceHeight / static_cast<float>(BlockLength))
		: INT32_MAX;

	// 2. 缓存切面快照（区域盒从世界空间转体素索引空间，供 FChunkGenContext 逐体素判定）
	//    与 SetBoundingBox 消费约定一致：BoundingBox 生成时 Max=MaxIndex+1（半开，连续空间物理范围），
	//    体素索引空间需 -1 还原为闭区间 MaxIndex，使 IsInsideOrOn 逐体素判定正确
	//    （否则 +X/+Y/+Z 方向各多判定一格体素为区域内）
	const int32 NewAreaMode = AreaConfig.AreaMode;
	TArray<FBox> NewAreaBoxes;
	const float InvBL = 1.0f / static_cast<float>(BlockLength);
	for (const FBox& WorldBox : AreaConfig.AABBs)
	{
		const FVector IndexMin = WorldBox.Min * InvBL;
		const FVector IndexMax = WorldBox.Max * InvBL - FVector(1);
		NewAreaBoxes.Add(FBox(IndexMin, IndexMax));
	}

	// 3. 无变化则跳过
	if (NewSliceLayer == CurrentSliceLayer && NewAreaMode == CurrentSliceAreaMode && NewAreaBoxes == CurrentSliceAreaBoxes)
	{
		return;
	}

	// 快照旧配置（重建目标 = 旧影响范围 ∪ 新影响范围，避免旧影响区的 chunk 残留旧样式）
	const int32 OldSliceLayer = CurrentSliceLayer;
	const int32 OldAreaMode = CurrentSliceAreaMode;
	const TArray<FBox> OldAreaBoxes = CurrentSliceAreaBoxes;

	CurrentSliceLayer = NewSliceLayer;
	CurrentSliceAreaMode = NewAreaMode;
	CurrentSliceAreaBoxes = MoveTemp(NewAreaBoxes);

	UE_LOG(LogTemp, Log, TEXT("[Slice] ApplySlice: SliceLayer=%d (Height=%.0f), AreaMode=%d, Boxes=%d"),
		CurrentSliceLayer, SliceHeight, CurrentSliceAreaMode, CurrentSliceAreaBoxes.Num());

	// 4. 标记目标 chunk 重建（复用现有 WToCalculate 异步队列，天然节流）
	//    重建范围 = 旧配置影响范围 ∪ 新配置影响范围：
	//    - 旧/新配置任一为"全局生效"（SliceLayer!=INT32_MAX 且 AreaMode==0）-> 全部已加载 chunk
	//    - 否则 -> 与（旧盒 ∪ 新盒）相交的 chunk（盒为空侧自然无贡献）
	//    否则 All->PlayerManaged 或区域集合变化时，旧影响区外的 chunk 不重建，会残留另一种模式的样式
	const bool bOldGlobal = (OldSliceLayer != INT32_MAX) && (OldAreaMode == 0);
	const bool bNewGlobal = (CurrentSliceLayer != INT32_MAX) && (CurrentSliceAreaMode == 0);
	const bool bRebuildAll = bOldGlobal || bNewGlobal;

	TArray<FBox> TestBoxes;
	if (!bRebuildAll)
	{
		TestBoxes.Append(OldAreaBoxes);
		TestBoxes.Append(CurrentSliceAreaBoxes);
	}

	TArray<FIntVector> Targets;
	for (const auto& Pair : LoadedChunks)
	{
		if (!Pair.Value.IsValid()) continue;
		const FIntVector& CIdx = Pair.Key;
		if (!bRebuildAll)
		{
			// chunk 级预筛（仅性能优化，精确性由逐体素判定保证）
			if (TestBoxes.Num() == 0) continue; // 旧/新均无盒且非全局 -> 无 chunk 受影响
			const FBox ChunkWorldBox(
				FVector(CIdx * ChunkSize) * BlockLength,
				FVector((CIdx + FIntVector(1)) * ChunkSize) * BlockLength);
			const FBox ChunkIndexBox(ChunkWorldBox.Min * InvBL, ChunkWorldBox.Max * InvBL);
			bool bIntersect = false;
			for (const FBox& AreaBox : TestBoxes)
			{
				if (AreaBox.Intersect(ChunkIndexBox)) { bIntersect = true; break; }
			}
			if (!bIntersect) continue;
		}
		Targets.Add(CIdx);
	}

	int32 MarkedCount = 0;
	int32 SkippedCount = 0;
	int32 NotFoundCount = 0;
	for (const FIntVector& CIdx : Targets)
	{
		if (FChunkData* DataPtr = ChunkMetaDataMap.Find(CIdx))
		{
			// 仅 Active/WToRender 等已就绪状态才标脏重建；流水线中间态不回退
			if (DataPtr->State == EChunkState::Active || DataPtr->State == EChunkState::WToRender)
			{
				UpdateChunkState(CIdx, EChunkState::Any, EChunkState::WToCalculate);
				MarkedCount++;
			}
			else
			{
				SkippedCount++;
			}
		}
		else
		{
			NotFoundCount++;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[Slice] ApplySlice DONE: Targets=%d, Marked=%d, Skipped(state)=%d, NotFound=%d, SliceLayer=%d"),
		Targets.Num(), MarkedCount, SkippedCount, NotFoundCount, CurrentSliceLayer);
}

void UVoxelBlockWorldSubsystem::ApplyCutoutBoxes(const TArray<FBox>& EffectiveBoxes)
{
	// 1. 无变化则跳过（顺序无关比较：数量+逐个相等）
	if (EffectiveBoxes.Num() == CurrentCutoutBoxes.Num())
	{
		bool bSame = true;
		for (const FBox& B : EffectiveBoxes)
		{
			if (!CurrentCutoutBoxes.Contains(B)) { bSame = false; break; }
		}
		if (bSame) return;
	}

	// 2. 快照旧集，写入新集（闭盒 [Min,Max]）
	const TArray<FBox> OldBoxes = CurrentCutoutBoxes;
	CurrentCutoutBoxes = EffectiveBoxes;

	UE_LOG(LogTemp, Log, TEXT("[Cutout] ApplyCutoutBoxes: Old=%d New=%d"), OldBoxes.Num(), CurrentCutoutBoxes.Num());

	// 3. 标记受影响 chunk 重建 = 与（旧集∪新集）相交的 chunk
	const float InvBL = 1.0f / static_cast<float>(BlockLength);

	// 收集测试盒（旧∪新）
	TArray<FBox> TestBoxes;
	TestBoxes.Append(OldBoxes);
	TestBoxes.Append(CurrentCutoutBoxes);

	TArray<FIntVector> Targets;
	for (const auto& Pair : LoadedChunks)
	{
		if (!Pair.Value.IsValid()) continue;
		const FIntVector& CIdx = Pair.Key;
		if (TestBoxes.Num() == 0) continue;
		const FBox ChunkWorldBox(
			FVector(CIdx * ChunkSize) * BlockLength,
			FVector((CIdx + FIntVector(1)) * ChunkSize) * BlockLength);
		const FBox ChunkIndexBox(ChunkWorldBox.Min * InvBL, ChunkWorldBox.Max * InvBL);
		bool bIntersect = false;
		for (const FBox& AreaBox : TestBoxes)
		{
			if (AreaBox.Intersect(ChunkIndexBox)) { bIntersect = true; break; }
		}
		if (!bIntersect) continue;
		Targets.Add(CIdx);
	}

	int32 MarkedCount = 0;
	for (const FIntVector& CIdx : Targets)
	{
		if (FChunkData* DataPtr = ChunkMetaDataMap.Find(CIdx))
		{
			if (DataPtr->State == EChunkState::Active || DataPtr->State == EChunkState::WToRender)
			{
				UpdateChunkState(CIdx, EChunkState::Any, EChunkState::WToCalculate);
				MarkedCount++;
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[Cutout] ApplyCutoutBoxes DONE: Targets=%d, Marked=%d"), Targets.Num(), MarkedCount);
}

UMaterialInstanceDynamic* UVoxelBlockWorldSubsystem::GetSharedChunkTranslucentMID()
{
	if (SharedChunkTranslucentMID) return SharedChunkTranslucentMID;

	if (!GameInstance || !GameInstance->WorldConfig.ChunkActorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlockSys] GetSharedChunkTranslucentMID: ChunkActorClass not configured."));
		return nullptr;
	}

	// 从 Chunk CDO 获取半透明材质（BP_GreedChunk 上配置的 TranslucentMaterial = M_ChunkTranslucent）
	AChunkBase* ChunkCDO = GameInstance->WorldConfig.ChunkActorClass->GetDefaultObject<AChunkBase>();
	if (!ChunkCDO || !ChunkCDO->TranslucentMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlockSys] GetSharedChunkTranslucentMID: ChunkCDO TranslucentMaterial not configured."));
		return nullptr;
	}

	SharedChunkTranslucentMID = UMaterialInstanceDynamic::Create(ChunkCDO->TranslucentMaterial, this);
	if (!SharedChunkTranslucentMID)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlockSys] GetSharedChunkTranslucentMID: Failed to create MID."));
		return nullptr;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Slice] GetSharedChunkTranslucentMID: Created from '%s'"),
		*ChunkCDO->TranslucentMaterial->GetName());

	// 设置纹理数组参数（与 SharedChunkMID 一致）
	if (TextureRegistryCache)
	{
		if (UTexture2DArray* BaseColorArray = TextureRegistryCache->GetBaseColorArray())
		{
			SharedChunkTranslucentMID->SetTextureParameterValue(FName(TEXT("BaseColorArray")), BaseColorArray);
		}
		if (UTexture2DArray* NormalArray = TextureRegistryCache->GetNormalArray())
		{
			SharedChunkTranslucentMID->SetTextureParameterValue(FName(TEXT("NormalArray")), NormalArray);
		}
		if (UTexture2DArray* ORMArray = TextureRegistryCache->GetORMArray())
		{
			SharedChunkTranslucentMID->SetTextureParameterValue(FName(TEXT("ORMArray")), ORMArray);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[BlockSys] GetSharedChunkTranslucentMID: Created translucent Chunk MID."));
	return SharedChunkTranslucentMID;
}

FVoxelBlockItemBase* UVoxelBlockWorldSubsystem::GetBlockInform(int32 TypeId) const
{
	// Air 特殊处理：不查配置表
	if (TypeId == EBlockId::Air)
	{
		return nullptr;
	}
	if (ConfigSubsystemCache)
	{
		if (const FVoxelItemBase* const* Found = ConfigSubsystemCache->ItemMap.Find(TypeId))
		{
			// 类型检查：只返回方块类型物品
			if (*Found && (*Found)->ItemTag.MatchesTag(FCoreTags::Item_Type_Block))
			{
				return const_cast<FVoxelBlockItemBase*>(static_cast<const FVoxelBlockItemBase*>(*Found));
			}
		}
	}
	return nullptr;
}

int32 UVoxelBlockWorldSubsystem::GetDefaultWallBlockId() const
{
	if (ConfigSubsystemCache)
	{
		for (const auto& Pair : ConfigSubsystemCache->ItemMap)
		{
			// 首个可放置方块作为默认墙方块（区域围墙等几何功能空手时的回退）
			if (Pair.Value && Pair.Value->ItemTag.MatchesTag(FCoreTags::Item_Type_Block))
			{
				return Pair.Key;
			}
		}
	}
	return EBlockId::Invalid;
}

void UVoxelBlockWorldSubsystem::CalculateSelectionMaterials(const TArray<FIntVector>& Selection, TMap<int32, int32>& OutMaterialCounts, int32& OutFurnitureCount)
{
	OutMaterialCounts.Reset();
	OutFurnitureCount = 0;

	// 统计方块
	for (const FIntVector& Pos : Selection)
	{
		const int32 BlockId = GetBlockType(Pos);
		if (BlockId <= 0) continue; // 跳过 Air/Invalid
		++OutMaterialCounts.FindOrAdd(BlockId);
	}

	// 统计家具（去重，每个家具实例算 1 个；家具可跨多格，按实例 HandleId 计数）
	if (UVoxelItemWorldSubsystem* ItemWorld = GetWorld()->GetSubsystem<UVoxelItemWorldSubsystem>())
	{
		TSet<AUVoxelItemBaseInstance*> Visited;
		for (const FIntVector& Pos : Selection)
		{
			AUVoxelItemBaseInstance* Item = ItemWorld->GetItemAtGrid(Pos);
			if (!Item || Visited.Contains(Item)) continue;
			Visited.Add(Item);
			++OutFurnitureCount;
			++OutMaterialCounts.FindOrAdd(Item->ItemInstance.HandleId);
		}
	}
}

void UVoxelBlockWorldSubsystem::LoadChunkData(FIntVector InputIndex)
{
	// (这里通常是读取磁盘或生成地形数据)
	// 模拟：直接进入生成阶段
	for(int x =-1;x <= 1;x++) {
		for(int y =-1;y <= 1;y++) {
			for(int z =-1;z <=+1;z++) {
				FIntVector ChunkCheckIndex = InputIndex + FIntVector(x, y, z);
				if (!ChunkMetaDataMap.Contains(ChunkCheckIndex) && GameInstance->GetCurScene().IsChunkIndexInScene(ChunkCheckIndex)) {
					GenerateChunkData(ChunkCheckIndex);
				}
			}
		}
	}
	if(ChunkMetaDataMap.Contains(InputIndex)){
		UpdateChunkState(InputIndex, EChunkState::WToLoad, EChunkState::WToGenerate);
	}
}

void UVoxelBlockWorldSubsystem::GenerateChunkData(FIntVector InputIndex)
{
    FChunkData ChunkData;
    ChunkData.State = EChunkState::WToLoad;
    ChunkData.ChunkIndex = InputIndex;
    //GenerateBlockTypeMaskInChunk(InputIndex); // 地形生成
    ChunkMetaDataMap.Add(InputIndex, ChunkData);
}

void UVoxelBlockWorldSubsystem::GenerateChunk(FIntVector InputIndex)
{
    if (!PoolSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[BlockSys] GenerateChunk: PoolSubsystem is NULL! Chunk=%s"), *InputIndex.ToString());
        return;
    }

    auto Transform = FTransform(
        FRotator::ZeroRotator,
        FVector(InputIndex.X * ChunkSize.X * BlockLength, InputIndex.Y * ChunkSize.Y * BlockLength, InputIndex.Z * ChunkSize.Z * BlockLength),
        FVector::OneVector
    );
    
    AGreedyChunk* Chunk = PoolSubsystem->Acquire<AGreedyChunk>("ChunkPool");
    if (!Chunk)
    {
        UE_LOG(LogTemp, Error, TEXT("[BlockSys] GenerateChunk: Acquire returned NULL! Chunk=%s"), *InputIndex.ToString());
        return;
    }
    
    Chunk->PrepareForReuse();
    Chunk->SetActorTransform(Transform);
    #if WITH_EDITOR
    Chunk->SetActorLabel(FString::Printf(TEXT("Chunk_%d_%d_%d"), InputIndex.X, InputIndex.Y, InputIndex.Z));
    #endif
    Chunk->ChunkIndex = InputIndex;
    Chunk->ChunkSize = ChunkSize;
    Chunk->BlockLength = BlockLength;
    
    LoadedChunks.Add(InputIndex, Chunk);
    //UE_LOG(LogTemp, Error, TEXT("[BlockSys] GenerateChunk: Actor created & labeled Chunk_%d_%d_%d, state→WToCalculate"), InputIndex.X, InputIndex.Y, InputIndex.Z);
    UpdateChunkState(InputIndex, EChunkState::WToGenerate, EChunkState::WToCalculate);
}

void UVoxelBlockWorldSubsystem::CalculateChunk(FIntVector InputIndex, FChunkMeshDataPair MeshDataPair)
{
	//UE_LOG(LogTemp, Error, TEXT("[BlockSys] CalculateChunk: Chunk=%s, Vertices=%d"), *InputIndex.ToString(), MeshData.Vertices.Num());
	if (auto* WeakChunkPtr = LoadedChunks.Find(InputIndex))
	{
		// 弱指针指向的对象可能已经被销毁，必须检查！
		if (AChunkBase* ChunkActor = WeakChunkPtr->Get())
		{
			if (FChunkData* DataPtr = ChunkMetaDataMap.Find(InputIndex))
			{
				// 直接通过指针检查状态
				if (DataPtr->State == EChunkState::WToCalculate)
				{
					ChunkActor->ModifyChunkMesh(MeshDataPair);
					UpdateChunkState(InputIndex, EChunkState::WToCalculate, EChunkState::WToRender);
					UE_LOG(LogTemp, Log, TEXT("[Slice] CalculateChunk: Chunk=%s OpaqueV=%d TranslucentV=%d"),
						*InputIndex.ToString(), MeshDataPair.Opaque.Vertices.Num(), MeshDataPair.Translucent.Vertices.Num());
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[BlockSys] CalculateChunk: Chunk=%s state mismatch! Expected WToCalculate, got %d"), *InputIndex.ToString(), (int32)DataPtr->State);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[BlockSys] CalculateChunk: Chunk=%s not in ChunkMetaDataMap!"), *InputIndex.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[BlockSys] CalculateChunk: Chunk Actor is stale/null for %s"), *InputIndex.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BlockSys] CalculateChunk: Chunk=%s not in LoadedChunks!"), *InputIndex.ToString());
	}
}

void UVoxelBlockWorldSubsystem::ApplyChunk(FIntVector InputIndex)
{
	//UE_LOG(LogTemp, Error, TEXT("[BlockSys] ApplyChunk: Chunk=%s"), *InputIndex.ToString());
	if (auto* WeakChunkPtr = LoadedChunks.Find(InputIndex))
	{
		// 【关键】检查 Actor 是否有效
		if (AChunkBase* ChunkActor = WeakChunkPtr->Get())
		{
			ChunkActor->ApplyMeshRender();
			UpdateChunkState(InputIndex, EChunkState::WToRender, EChunkState::Active);
			//UE_LOG(LogTemp, Error, TEXT("[BlockSys] ApplyChunk: Chunk=%s render applied, state→Active"), *InputIndex.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[BlockSys] ApplyChunk: Chunk=%s Actor is null!"), *InputIndex.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BlockSys] ApplyChunk: Chunk=%s not in LoadedChunks!"), *InputIndex.ToString());
	}
}
void UVoxelBlockWorldSubsystem::ReleaseChunk(FIntVector InputChunkIndex)
{
    if (!PoolSubsystem) return;
    
    if (TWeakObjectPtr<AChunkBase>* ChunkPtr = LoadedChunks.Find(InputChunkIndex)) 
    {
        if (ChunkPtr->IsValid()) {
            PoolSubsystem->Release("ChunkPool", ChunkPtr->Get());
        }
        LoadedChunks.Remove(InputChunkIndex);
    }

	if (FChunkData* DataPtr = ChunkMetaDataMap.Find(InputChunkIndex))
	{
		DataPtr->State = EChunkState::Unloaded;
		ActiveRequestsSet.Remove(InputChunkIndex); 
	}
}

void UVoxelBlockWorldSubsystem::CreateQuad(
				const FMask& Mask,
				const FIntVector& AxisMask, 
				int UVAxis1,
				int UVAxis2,
				int TileSize, 
				const FIntVector& V1,
				const FIntVector& V2,
				const FIntVector& V3,
				const FIntVector& V4, 
				FChunkMeshDataPair& MeshDataPair, int& VertexCount, 
				const FChunkGenContext& Context
)
{
	const auto Normal = FVector(AxisMask * Mask.Normal);
	const int32 TexIndex = Context.GetTexture(Mask.TypeId, Normal);
	const auto Color = FColor(0, 0, 0, TexIndex);
    
    // 按 bTranslucent 分流到对应 MeshData
    FChunkMeshData& MeshData = Mask.bTranslucent ? MeshDataPair.Translucent : MeshDataPair.Opaque;

	MeshData.Vertices.Append({
		FVector(V1) * BlockLength,
		FVector(V2) * BlockLength,
		FVector(V3) * BlockLength,
		FVector(V4) * BlockLength,
	});

	MeshData.Triangles.Append({
		VertexCount,
		VertexCount + 2 + Mask.Normal,
		VertexCount + 2 - Mask.Normal,
		VertexCount + 3,
		VertexCount + 1 - Mask.Normal,
		VertexCount + 1 + Mask.Normal
	});

	MeshData.Normals.Append({
		Normal,
		Normal,
		Normal,
		Normal
	});

	MeshData.Colors.Append({
		Color,
		Color,
		Color,
		Color
	});
	
	// 确定UV轴

	float Utrans =Mask.maskId[UVAxis1]%TileSize;
	float Vtrans =Mask.maskId[UVAxis2]%TileSize;

	// 计算局部UV坐标
	float u0 = Utrans/ (float)TileSize;
	float v0 = Vtrans/ (float)TileSize;
	float u1 = (V2[UVAxis1]-V1[UVAxis1]+Utrans) / (float)TileSize;
	float v1 = (V3[UVAxis2]-V1[UVAxis2]+Vtrans) / (float)TileSize;
	
	if (Normal.X !=0)
	{
		MeshData.UV0.Append({

			FVector2D(u0, v0),
			FVector2D(u1, v0),
			FVector2D(u0, v1),
			FVector2D(u1, v1),
		});
	}
	else
	{
		MeshData.UV0.Append({

			FVector2D(v0, u0),
			FVector2D(v0, u1),
			FVector2D(v1, u0),
			FVector2D(v1, u1),
		});
	}
	VertexCount += 4;
}

void UVoxelBlockWorldSubsystem::BuildChunkContext(const FIntVector& ChunkIndex, FChunkGenContext& OutContext)
{
    // 设置上下文基础信息
    OutContext.ChunkIndex = ChunkIndex;
    OutContext.ChunkSize = ChunkSize;
    OutContext.PaddedSize = ChunkSize + FIntVector(2); // 每个轴向 +2 padding
    
    // 分配内存：(X+2)*(Y+2)*(Z+2)
    int32 TotalSize = OutContext.PaddedSize.X * OutContext.PaddedSize.Y * OutContext.PaddedSize.Z;
    OutContext.RawBlockData.SetNumUninitialized(TotalSize);

    // 计算世界坐标起点 (ChunkIndex * ChunkSize - 1)
    FIntVector WorldBase = ChunkIndex * ChunkSize - FIntVector(1);

    // 加锁，一次性批量读取数据
    FReadScopeLock Lock(BlockMapLock);
	if (ConfigSubsystemCache)
	{
		// 遍历配置表中所有的方块类型
		// 如果方块类型非常多(比如几万个)，可以只遍历 Chunk 里出现过的方块，但通常直接遍历 Map 更快且省逻辑
		for (const auto& Elem : ConfigSubsystemCache->ItemMap)
		{
			if (const FVoxelItemBase* ItemPtr = Elem.Value)
			{
				// 只处理方块类型物品（通过 ItemTag 匹配 Item.Type.Block）
				if (!ItemPtr->ItemTag.MatchesTag(FCoreTags::Item_Type_Block))
				{
					continue;
				}
				const FVoxelBlockItemBase* Item = static_cast<const FVoxelBlockItemBase*>(ItemPtr);
				FBlockRenderInfo Info;

				// 1. 缓存不透明度 (逻辑照搬 CheckBlockOpaque)
				Info.bIsOpaque = Item->BlockFragment.IsSolid && !Item->BlockFragment.IsRenderNeibor;

				// 2. 缓存纹理索引 (通过 TextureRegistry 查询，替代旧的6个int字段)
				const FVoxelBlockTextureSetup& TexSetup = Item->BlockFragment.TextureSetup;
				if (TextureRegistryCache)
				{
					Info.TextureIndices[0] = TextureRegistryCache->GetTextureIndex(TexSetup.Side);    // Backward (-X)
				Info.TextureIndices[1] = TextureRegistryCache->GetTextureIndex(TexSetup.Side);    // Forward (+X)
				Info.TextureIndices[2] = TextureRegistryCache->GetTextureIndex(TexSetup.Top);     // Up (+Z)
				Info.TextureIndices[3] = TextureRegistryCache->GetTextureIndex(TexSetup.Bottom);  // Down (-Z)
				Info.TextureIndices[4] = TextureRegistryCache->GetTextureIndex(TexSetup.Side);    // Left (-Y)
				Info.TextureIndices[5] = TextureRegistryCache->GetTextureIndex(TexSetup.Side);    // Right (+Y)
				}

				OutContext.BlockDefs.Add(Elem.Key, Info);

			#if !UE_BUILD_SHIPPING
			// 一次性诊断：打印每个方块的 Handle、identityName、TextureIndices
			UE_LOG(LogTemp, Log, TEXT("[BlockDefs] Handle=%d Name=%s Opaque=%d TexIdx=[%d,%d,%d,%d,%d,%d]"),
				Elem.Key, *Item->IdentityName, Info.bIsOpaque,
				Info.TextureIndices[0], Info.TextureIndices[1], Info.TextureIndices[2],
				Info.TextureIndices[3], Info.TextureIndices[4], Info.TextureIndices[5]);
			#endif
			}
		}
	}
    // 填充数据
    int32 Index = 0;
    for (int32 z = 0; z < OutContext.PaddedSize.Z; z++)
    {
        for (int32 y = 0; y < OutContext.PaddedSize.Y; y++)
        {
            for (int32 x = 0; x < OutContext.PaddedSize.X; x++)
            {
                if (!OutContext.RawBlockData.IsValidIndex(Index))
                {
                    break;
                }
                FIntVector BlockWorldPos = WorldBase + FIntVector(x, y, z);
            	int QueryId=UChunkUtilsFunction::GetBlockIdByIndex(BlockWorldPos, WorldDimension);
	            const int32 FoundType = GetBlockType(
		            QueryId);
            	//if (BlockWorldPos == FIntVector(-2, 2, 3)) {
            	//	UE_LOG(LogTemp, Error, TEXT("DEBUG CTX: Pos(-2,2,3) -> ID(%d) -> Found(%d)"),
            	//		QueryId, FoundType);
            	//}
            	OutContext.RawBlockData[Index++] =FoundType;
            }
        }
    }

    // 注入当前切面快照（所有重建路径统一经过此处 -> 挖/放/刷/粘贴后自动保持切面一致）
    OutContext.SliceLayer = CurrentSliceLayer;
    OutContext.SliceAreaMode = CurrentSliceAreaMode;
    OutContext.SliceAreaBoxes = CurrentSliceAreaBoxes;

    // 注入透明区域框快照（按本 chunk 过滤相交框，控"不限数量"时逐体素成本有界）
    {
        const float InvBL = 1.0f / static_cast<float>(BlockLength);
        const FBox ChunkIndexBox(
            FVector(ChunkIndex * ChunkSize),
            FVector((ChunkIndex + FIntVector(1)) * ChunkSize));
        OutContext.CutoutBoxes.Reset();
        for (const FBox& Box : CurrentCutoutBoxes)
        {
            if (Box.Intersect(ChunkIndexBox))
            {
                OutContext.CutoutBoxes.Add(Box);
            }
        }
    }
}

void UVoxelBlockWorldSubsystem::BuildSelectionContext(const TArray<FIntVector>& SelectedVoxels, FChunkGenContext& OutContext)
{
    if (SelectedVoxels.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MeshChain] BuildSelectionContext: Empty input"));
        return;
    }

    // 1. 计算选区包围盒
    FIntVector Min = SelectedVoxels[0];
    FIntVector Max = SelectedVoxels[0];
    for (const FIntVector& Pos : SelectedVoxels)
    {
        Min = FIntVector(FMath::Min(Min.X, Pos.X), FMath::Min(Min.Y, Pos.Y), FMath::Min(Min.Z, Pos.Z));
        Max = FIntVector(FMath::Max(Max.X, Pos.X), FMath::Max(Max.Y, Pos.Y), FMath::Max(Max.Z, Pos.Z));
    }

    // 2. 设置上下文：选区大小 + 2 padding（边界为 Air，使选区边缘面可见）
    FIntVector SelectionSize = Max - Min + FIntVector(1, 1, 1);
    OutContext.ChunkIndex = FIntVector::ZeroValue;
    OutContext.ChunkSize = SelectionSize;
    OutContext.PaddedSize = SelectionSize + FIntVector(2);

    int32 TotalSize = OutContext.PaddedSize.X * OutContext.PaddedSize.Y * OutContext.PaddedSize.Z;
    OutContext.RawBlockData.SetNumUninitialized(TotalSize);

    // 3. 全部初始化为 Air
    FMemory::Memset(OutContext.RawBlockData.GetData(), 0, TotalSize * sizeof(int32));

    // 4. 填充选区内体素的方块类型
    // 选区预览仅需"实心方块"占位以生成可见 Mesh 体积，不关心真实方块类型。
    // 用 GroundBlockHandleId（core:block.stone，已确认实心）作为占位句柄，
    // 让 BlockDefs.Find 命中并返回 bIsOpaque=true。
    // 注意：原硬编码 1 在四元身份重构后失效（BlockDefs key 是 hash handle）。
    int32 PlaceholderBlock = GroundBlockHandleId;
    if (!ConfigSubsystemCache || !ConfigSubsystemCache->ItemMap.Contains(PlaceholderBlock))
    {
        PlaceholderBlock = EBlockId::Invalid;
        if (ConfigSubsystemCache)
        {
            for (const auto& Elem : ConfigSubsystemCache->ItemMap)
            {
                if (const FVoxelItemBase* ItemPtr = Elem.Value)
                {
                    if (ItemPtr->ItemTag.MatchesTag(FCoreTags::Item_Type_Block))
                    {
                        PlaceholderBlock = Elem.Key;
                        break;
                    }
                }
            }
        }
    }
    if (PlaceholderBlock == EBlockId::Invalid)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MeshChain] BuildSelectionContext: no valid placeholder block found. GroundBlockHandleId=%d, ItemMap=%d"),
            GroundBlockHandleId, ConfigSubsystemCache ? ConfigSubsystemCache->ItemMap.Num() : -1);
    }
    int32 FilledCount = 0;
    for (const FIntVector& Pos : SelectedVoxels)
    {
        // 局部坐标（偏移 +1 是因为 PaddedSize 有 1 的边界 padding）
        int32 LocalX = Pos.X - Min.X + 1;
        int32 LocalY = Pos.Y - Min.Y + 1;
        int32 LocalZ = Pos.Z - Min.Z + 1;

        int32 Index = LocalZ * (OutContext.PaddedSize.X * OutContext.PaddedSize.Y) + LocalY * OutContext.PaddedSize.X + LocalX;
        if (OutContext.RawBlockData.IsValidIndex(Index))
        {
            OutContext.RawBlockData[Index] = PlaceholderBlock;
            ++FilledCount;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[MeshChain] BuildSelectionContext: Voxels=%d, Min=%s, Max=%s, Size=%s, Padded=%s, Filled=%d/%d"),
        SelectedVoxels.Num(), *Min.ToString(), *Max.ToString(),
        *SelectionSize.ToString(), *OutContext.PaddedSize.ToString(),
        FilledCount, TotalSize);

    // 5. 填充 BlockDefs（和 BuildChunkContext 完全相同）
    if (ConfigSubsystemCache)
    {
        for (const auto& Elem : ConfigSubsystemCache->ItemMap)
        {
            if (const FVoxelItemBase* ItemPtr = Elem.Value)
            {
                if (!ItemPtr->ItemTag.MatchesTag(FCoreTags::Item_Type_Block))
                {
                    continue;
                }
                const FVoxelBlockItemBase* Item = static_cast<const FVoxelBlockItemBase*>(ItemPtr);
                FBlockRenderInfo Info;

                Info.bIsOpaque = Item->BlockFragment.IsSolid && !Item->BlockFragment.IsRenderNeibor;

				const FVoxelBlockTextureSetup& TexSetup = Item->BlockFragment.TextureSetup;
				if (TextureRegistryCache)
				{
					Info.TextureIndices[0] = TextureRegistryCache->GetTextureIndex(TexSetup.Side);
				Info.TextureIndices[1] = TextureRegistryCache->GetTextureIndex(TexSetup.Side);
				Info.TextureIndices[2] = TextureRegistryCache->GetTextureIndex(TexSetup.Top);
				Info.TextureIndices[3] = TextureRegistryCache->GetTextureIndex(TexSetup.Bottom);
				Info.TextureIndices[4] = TextureRegistryCache->GetTextureIndex(TexSetup.Side);
				Info.TextureIndices[5] = TextureRegistryCache->GetTextureIndex(TexSetup.Side);
				}

                OutContext.BlockDefs.Add(Elem.Key, Info);
            }
        }
    }
}

void UVoxelBlockWorldSubsystem::BuildTemplateContext(const FVoxelTemplate& Template, FChunkGenContext& OutContext, const TMap<FString, FString>& Remap)
{
    if (Template.Entries.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MeshChain] BuildTemplateContext: Empty template entries"));
        return;
    }

    // 1. 计算模板包围盒（Entries 的 key 是相对坐标）
    FIntVector Min = FIntVector(INT32_MAX);
    FIntVector Max = FIntVector(INT32_MIN);
    for (const auto& Pair : Template.Entries)
    {
        const FIntVector& Pos = Pair.Key;
        Min = FIntVector(FMath::Min(Min.X, Pos.X), FMath::Min(Min.Y, Pos.Y), FMath::Min(Min.Z, Pos.Z));
        Max = FIntVector(FMath::Max(Max.X, Pos.X), FMath::Max(Max.Y, Pos.Y), FMath::Max(Max.Z, Pos.Z));
    }

    // 2. 设置上下文：模板大小 + 2 padding（边界为 Air，使边缘面可见）
    FIntVector TemplateSize = Max - Min + FIntVector(1, 1, 1);
    OutContext.ChunkIndex = FIntVector::ZeroValue;
    OutContext.ChunkSize = TemplateSize;
    OutContext.PaddedSize = TemplateSize + FIntVector(2);

    int32 TotalSize = OutContext.PaddedSize.X * OutContext.PaddedSize.Y * OutContext.PaddedSize.Z;
    OutContext.RawBlockData.SetNumUninitialized(TotalSize);

    // 3. 全部初始化为 Air
    FMemory::Memset(OutContext.RawBlockData.GetData(), 0, TotalSize * sizeof(int32));

    // 4. 填充模板内方块的真实句柄（区分不同方块纹理）
    int32 FilledCount = 0;
    for (const auto& Pair : Template.Entries)
    {
        const FIntVector& Pos = Pair.Key;
        const FVoxelInstanceEntry& Entry = Pair.Value;

        if (!Entry.IsValid() || Entry.IsAir()) continue;

        // 应用素材替换（原 FullId → 新 FullId），使预览所见即所得
        FString FinalFullId = Entry.FullId;
        if (const FString* NewFullId = Remap.Find(Entry.FullId))
        {
            FinalFullId = *NewFullId;
        }

        // 由 FullId 解析运行时句柄
        int32 FinalId = -1;
        if (ConfigSubsystemCache)
        {
            FinalId = ConfigSubsystemCache->GetHandleByFullId(FinalFullId);
        }
        if (FinalId == EBlockId::Invalid)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MeshChain] BuildTemplateContext: FullId='%s' not found in ItemMap, skip"), *FinalFullId);
            continue;
        }

        // 只处理 Block 类型（家具不在 Chunk Mesh 里渲染）
        if (ConfigSubsystemCache)
        {
            if (const FVoxelItemBase* const* ItemPtr = ConfigSubsystemCache->ItemMap.Find(FinalId))
            {
                if ((*ItemPtr)->ItemTag.MatchesTag(FCoreTags::Item_Type_Block))
                {
                    int32 LocalX = Pos.X - Min.X + 1;
                    int32 LocalY = Pos.Y - Min.Y + 1;
                    int32 LocalZ = Pos.Z - Min.Z + 1;
                    int32 Index = LocalZ * (OutContext.PaddedSize.X * OutContext.PaddedSize.Y) + LocalY * OutContext.PaddedSize.X + LocalX;
                    if (OutContext.RawBlockData.IsValidIndex(Index))
                    {
                        OutContext.RawBlockData[Index] = FinalId;
                        ++FilledCount;
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[MeshChain] BuildTemplateContext: Entries=%d, Min=%s, Max=%s, Size=%s, Padded=%s, Filled=%d/%d"),
        Template.Entries.Num(), *Min.ToString(), *Max.ToString(),
        *TemplateSize.ToString(), *OutContext.PaddedSize.ToString(),
        FilledCount, TotalSize);

    // 5. 填充 BlockDefs（和 BuildChunkContext / BuildSelectionContext 完全相同）
    if (ConfigSubsystemCache)
    {
        for (const auto& Elem : ConfigSubsystemCache->ItemMap)
        {
            if (const FVoxelItemBase* ItemPtr = Elem.Value)
            {
                if (!ItemPtr->ItemTag.MatchesTag(FCoreTags::Item_Type_Block))
                {
                    continue;
                }
                const FVoxelBlockItemBase* Item = static_cast<const FVoxelBlockItemBase*>(ItemPtr);
                FBlockRenderInfo Info;

                Info.bIsOpaque = Item->BlockFragment.IsSolid && !Item->BlockFragment.IsRenderNeibor;

				const FVoxelBlockTextureSetup& TexSetup = Item->BlockFragment.TextureSetup;
				if (TextureRegistryCache)
				{
					Info.TextureIndices[0] = TextureRegistryCache->GetTextureIndex(TexSetup.Side);
				Info.TextureIndices[1] = TextureRegistryCache->GetTextureIndex(TexSetup.Side);
				Info.TextureIndices[2] = TextureRegistryCache->GetTextureIndex(TexSetup.Top);
				Info.TextureIndices[3] = TextureRegistryCache->GetTextureIndex(TexSetup.Bottom);
				Info.TextureIndices[4] = TextureRegistryCache->GetTextureIndex(TexSetup.Side);
				Info.TextureIndices[5] = TextureRegistryCache->GetTextureIndex(TexSetup.Side);
				}

                OutContext.BlockDefs.Add(Elem.Key, Info);
            }
        }
    }
}

FChunkMeshDataPair UVoxelBlockWorldSubsystem::GenerateMeshData(const FChunkGenContext& Context)
{
    FChunkMeshDataPair OutPair;
    int VertexCountOpaque = 0;
    int VertexCountTranslucent = 0;

    // 内存预分配（使用 Context.ChunkSize 而非成员变量，支持选区预览）
    const int32 BlockCount = Context.ChunkSize.X * Context.ChunkSize.Y * Context.ChunkSize.Z;
    OutPair.Opaque.Vertices.Reserve(BlockCount * 2);
    OutPair.Opaque.Triangles.Reserve(BlockCount);
    OutPair.Translucent.Vertices.Reserve(BlockCount);
    OutPair.Translucent.Triangles.Reserve(BlockCount / 2);

	FIntVector WorldBlockIndex;
    // 贪婪网格算法（三域面分流：Opaque/Ghost/Air）
    for (int MainAxis = 0; MainAxis < 3; ++MainAxis)
    {
        const int UVAxis1 = (MainAxis + 1) % 3; 
        const int UVAxis2 = (MainAxis + 2) % 3; 

        const int MainAxisLimit = Context.ChunkSize[MainAxis];
        const int Axis1Limit = Context.ChunkSize[UVAxis1];
        const int Axis2Limit = Context.ChunkSize[UVAxis2];

        auto DeltaAxis1 = FIntVector::ZeroValue;
        auto DeltaAxis2 = FIntVector::ZeroValue;
        auto ChunkItr = FIntVector::ZeroValue;
        auto AxisMask = FIntVector::ZeroValue;
        AxisMask[MainAxis] = 1;

        TArray<FMask> Mask;
        Mask.SetNum(Axis1Limit * Axis2Limit);

        for (ChunkItr[MainAxis] = -1; ChunkItr[MainAxis] < MainAxisLimit; )
        {
            int N = 0;
            // 1. 计算 Mask (扫描当前层) - 三域判定
            for (ChunkItr[UVAxis2] = 0; ChunkItr[UVAxis2] < Axis2Limit; ++ChunkItr[UVAxis2])
            {
                for (ChunkItr[UVAxis1] = 0; ChunkItr[UVAxis1] < Axis1Limit; ++ChunkItr[UVAxis1])
                {
                	WorldBlockIndex = Context.ChunkIndex * Context.ChunkSize + ChunkItr;
                    const int32 CurrentBlockTypeId = Context.GetBlockId(ChunkItr.X, ChunkItr.Y, ChunkItr.Z);
                    const int32 CompareBlockTypeId = Context.GetBlockId(ChunkItr.X + AxisMask.X, ChunkItr.Y + AxisMask.Y, ChunkItr.Z + AxisMask.Z);

                    const FIntVector CompareWorldIndex = WorldBlockIndex + AxisMask;

                    // 三域：Air/Ghost/Opaque
                    const EVoxelFaceDom CurrentDom = Context.GetFaceDom(CurrentBlockTypeId, WorldBlockIndex);
                    const EVoxelFaceDom CompareDom = Context.GetFaceDom(CompareBlockTypeId, CompareWorldIndex);

                    if (CurrentDom == CompareDom)
                    {
                        // 同域：无面
                        Mask[N++] = FMask{ EBlockId::Air, 0, WorldBlockIndex, EVoxelFaceDom::Air, false };
                    }
                    else
                    {
                        // 异域：按三域规则表判定面归属
                        // Opaque 对任何异域 -> Opaque 方面 (Section0)
                        // Ghost 对 Air -> Ghost 方面 (Section1)
                        // Air 对 Ghost -> Ghost 反方面 (Section1)
                        // Ghost 对 Opaque -> Opaque 方面 (Section0)，法线反向
                        // Air 对 Opaque -> Opaque 方面 (Section0)，法线反向
                        if (CurrentDom == EVoxelFaceDom::Opaque)
                        {
                            // Opaque 方正向面
                            Mask[N++] = FMask{ CurrentBlockTypeId, 1, WorldBlockIndex, EVoxelFaceDom::Opaque, false };
                        }
                        else if (CompareDom == EVoxelFaceDom::Opaque)
                        {
                            // Opaque 方反向面（Compare 是 Opaque，法线 -1）
                            Mask[N++] = FMask{ CompareBlockTypeId, -1, WorldBlockIndex, EVoxelFaceDom::Opaque, false };
                        }
                        else if (CurrentDom == EVoxelFaceDom::Ghost)
                        {
                            // Ghost 方正向面（Compare 必为 Air）-> Section1
                            Mask[N++] = FMask{ CurrentBlockTypeId, 1, WorldBlockIndex, EVoxelFaceDom::Ghost, true };
                        }
                        else // CurrentDom == Air, CompareDom == Ghost
                        {
                            // Ghost 方反向面 -> Section1
                            Mask[N++] = FMask{ CompareBlockTypeId, -1, WorldBlockIndex, EVoxelFaceDom::Ghost, true };
                        }
                    }
                }
            }
            ++ChunkItr[MainAxis];
            N = 0;

            // 2. 根据 Mask 生成 Mesh (贪婪合并)
            for (int j = 0; j < Axis2Limit; ++j)
            {
                for (int i = 0; i < Axis1Limit;)
                {
                    if (Mask[N].Normal != 0)
                    {
                        const auto CurrentMask = Mask[N];
                        // 更新迭代器位置用于生成顶点
                        ChunkItr[UVAxis1] = i;
                        ChunkItr[UVAxis2] = j;

                        // 计算宽度（同 TypeId + 同 Normal + 同 Section 归属才合并）
                        int Width;
                        for (Width = 1; i + Width < Axis1Limit
                            && Mask[N + Width].TypeId == CurrentMask.TypeId
                            && Mask[N + Width].Normal == CurrentMask.Normal
                            && Mask[N + Width].bTranslucent == CurrentMask.bTranslucent; ++Width) {}

                        // 计算高度
                        int Height;
                        bool Done = false;
                        for (Height = 1; j + Height < Axis2Limit; ++Height)
                        {
                            for (int k = 0; k < Width; ++k)
                            {
                                const FMask& M = Mask[N + k + Height * Axis1Limit];
                                if (M.TypeId == CurrentMask.TypeId && M.Normal == CurrentMask.Normal && M.bTranslucent == CurrentMask.bTranslucent) continue;
                                Done = true; break;
                            }
                            if (Done) break;
                        }

                        DeltaAxis1[UVAxis1] = Width;
                        DeltaAxis2[UVAxis2] = Height;

                        // 创建四边形（按 bTranslucent 分流到对应 MeshData）
                        CreateQuad(
                            CurrentMask, AxisMask, UVAxis1, UVAxis2, 10,
                            ChunkItr,
                            ChunkItr + DeltaAxis1,
                            ChunkItr + DeltaAxis2,
                            ChunkItr + DeltaAxis1 + DeltaAxis2,
                            OutPair, CurrentMask.bTranslucent ? VertexCountTranslucent : VertexCountOpaque,
                            Context
                        );

                        DeltaAxis1 = FIntVector::ZeroValue;
                        DeltaAxis2 = FIntVector::ZeroValue;

                        // 清除已处理的 Mask
                        for (int l = 0; l < Height; ++l)
                        {
                            for (int k = 0; k < Width; ++k)
                            {
                                Mask[N + k + l * Axis1Limit] = FMask{ EBlockId::Air, 0, FIntVector::ZeroValue, EVoxelFaceDom::Air, false };
                            }
                        }
                        i += Width;
                        N += Width;
                    }
                    else
                    {
                        i++; N++;
                    }
                }
            }
        }
    }

    return OutPair;
}


void UVoxelBlockWorldSubsystem::IsBlockOnBorder(const FIntVector BlockIndex, const FIntVector ChunkIndex,TSet<FIntVector>& Result)
{

	FIntVector NeighborChunkIndex;
	
	if ((BlockIndex.X+WorldDimension.X)% ChunkSize.X == 0)
	{
		NeighborChunkIndex=ChunkIndex-FIntVector(1,0,0);
		Result.Add(NeighborChunkIndex);
	}
	else if ((BlockIndex.X+1+WorldDimension.X) % ChunkSize.X == 0)
	{
		NeighborChunkIndex=ChunkIndex+FIntVector(1,0,0);
		Result.Add(NeighborChunkIndex);
	}
	if ((BlockIndex.Y+WorldDimension.Y)% ChunkSize.Y == 0)
	{
		NeighborChunkIndex=ChunkIndex-FIntVector(0,1,0);
		Result.Add(NeighborChunkIndex);
	}
	else if ((BlockIndex.Y+1+WorldDimension.Y) % ChunkSize.Y == 0)
	{
		NeighborChunkIndex=ChunkIndex+FIntVector(0,1,0);
		Result.Add(NeighborChunkIndex);
	}
	if ((BlockIndex.Z+WorldDimension.Z)% ChunkSize.Z == 0)
	{
		NeighborChunkIndex=ChunkIndex-FIntVector(0,0,1);
		Result.Add(NeighborChunkIndex);
	}
	else if ((BlockIndex.Z+1+WorldDimension.Z) % ChunkSize.Z == 0)
	{
		NeighborChunkIndex=ChunkIndex+FIntVector(0,0,1);
		Result.Add(NeighborChunkIndex);
	}
}



void UVoxelBlockWorldSubsystem::ModifyVoxelDataByPosition(const FVector Position, const FVoxelEditCommand& EditCommand, const int32 TypeID)
{
	if (!IsPositionInScene(Position))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid BatchShapeType"));
		return;
	} 
	FIntVector BlockIndex = UChunkUtilsFunction::GetBlockIndexByPosition(Position, BlockLength);
	ModifyVoxelData(BlockIndex, EditCommand, TypeID);
	bIsWaitingToUpdateBlock = true;
}

void UVoxelBlockWorldSubsystem::ModifyVoxelDataByIndex(const FIntVector Position, const FVoxelEditCommand& EditCommand, const int32 TypeID)
{
	if (!IsBlockIndexInScene(Position))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid BatchShapeType"));
		return;
	}
	ModifyVoxelData(Position, EditCommand, TypeID);
	bIsWaitingToUpdateBlock = true;
}

void UVoxelBlockWorldSubsystem::BatchModifyVoxelDataByIndex(FBox InputBox, EBatchShapeType CurType,
                                                            const FVoxelEditCommand& EditCommand, int32 TypeID, int Thick)
{
	switch (CurType)
	{
	case EBatchShapeType::Box: BatchModifyBox(InputBox, EditCommand, TypeID); break;
	case EBatchShapeType::Sphere: BatchModifySphere(InputBox, EditCommand, TypeID,Thick); break;
	case EBatchShapeType::Cylinder: BatchModifyCylinder(InputBox, EditCommand, TypeID,Thick, true); break;
	case EBatchShapeType::HollowBox: BatchModifyBox(InputBox, EditCommand, TypeID, Thick); break;
	case EBatchShapeType::HollowSphere: BatchModifySphere(InputBox, EditCommand, TypeID, Thick); break;
	case EBatchShapeType::HollowCylinder: BatchModifyCylinder(InputBox, EditCommand, TypeID,Thick, false); break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("Invalid BatchShapeType"));
		BatchModifyBox(InputBox, EditCommand, TypeID);
		break;
	}
	bIsWaitingToUpdateBlock = true;
}

void UVoxelBlockWorldSubsystem::CheckBatchEditType(TArray<int> BlockIds, const FVoxelEditCommand& EditCommand,  int32 TypeID)
{
	const bool bIsDelete = TypeID == EBlockId::Air;

	ParallelFor(BlockIds.Num(), [&](int32 Index)
	{
		if (bIsDelete || EditCommand.PlacementMode == EVoxelPlacementMode::Normal)
		{
			// 删除或覆盖模式：无条件设置
			SetBlockType(BlockIds[Index], TypeID);
		}
		else if (EditCommand.PlacementMode == EVoxelPlacementMode::Complement)
		{
			// 补全模式：只填充空气
			if (GetBlockType(BlockIds[Index]) == EBlockId::Air)
				SetBlockType(BlockIds[Index], TypeID);
		}
		else if (EditCommand.PlacementMode == EVoxelPlacementMode::Replace)
		{
			// 替换模式：只替换非空方块，不填空气
			if (GetBlockType(BlockIds[Index]) != EBlockId::Air)
				SetBlockType(BlockIds[Index], TypeID);
		}
		else if (EditCommand.PlacementMode == EVoxelPlacementMode::ReplaceMask)
		{
			// 指定替换模式：TODO 需要目标类型参数；暂按 Replace 处理（只替换非空）
			if (GetBlockType(BlockIds[Index]) != EBlockId::Air)
				SetBlockType(BlockIds[Index], TypeID);
		}
	});
}

void UVoxelBlockWorldSubsystem::BatchModifyVoxelData(TArray<FIntVector> BlockIndexList, const FVoxelEditCommand& EditCommand,int32 TypeID)
{
	TSet<FIntVector> uniqueBlocks;
	TSet<FIntVector> uniqueChunks;
    
	// 预分配内存
	uniqueBlocks.Reserve(BlockIndexList.Num());
	uniqueChunks.Reserve(BlockIndexList.Num() * 2); // 平均每个方块影响1-2个区块
    
	for (const auto& BlockIndex : BlockIndexList) {
		if (!IsBlockIndexInScene(BlockIndex)) continue;
        
		uniqueBlocks.Add(BlockIndex);
        
		FIntVector ChunkIndex = UChunkUtilsFunction::Get3DChunkIndexByBlockIndex(
			BlockIndex, ChunkSize);
        
		uniqueChunks.Add(ChunkIndex);
		IsBlockOnBorder(BlockIndex, ChunkIndex, uniqueChunks);
	}
    
	// 转换为 FVoxelEditModification 并入队
	if (!uniqueBlocks.IsEmpty()) {
		FWriteScopeLock Lock(PendingEditsLock);

		FVoxelEditModification Modification;
		// 透传调用方的 EditCommand（PlacementMode 等），下游 ProcessBlockEdits 依赖它分发
		Modification.SetEditCommand(EditCommand);
		Modification.Reserve(uniqueBlocks.Num());

		// 将所有方块添加到 Modification
		for (const auto& BlockIndex : uniqueBlocks) {
			Modification.Add(BlockIndex, TypeID);
		}

		PendingModifications.Enqueue(Modification);
	}
	bIsWaitingToUpdateBlock = true;
}

void UVoxelBlockWorldSubsystem::BatchModifyVoxelData_Single(const FSingleEditRequest& SingleEditRequest)
{
	// 将 FSingleEditRequest 转换为 FVoxelEditModification
	if (!SingleEditRequest.BlockIndex.IsEmpty())
	{
		FWriteScopeLock Lock(PendingEditsLock);

		FVoxelEditModification Modification;
		Modification.SetEditCommand(SingleEditRequest.EditCommand);
		Modification.Reserve(SingleEditRequest.BlockIndex.Num());

		// 将所有方块添加到 Modification
		for (const auto& BlockIndex : SingleEditRequest.BlockIndex) {
			Modification.Add(BlockIndex, SingleEditRequest.NewHandleId);
		}

		PendingModifications.Enqueue(Modification);
	}

	bIsWaitingToUpdateBlock = true;
}

void UVoxelBlockWorldSubsystem::CalculateWallBlocks(FBox InputBox, const TSet<EActorDirection>& GenDir, int Thick, TSet<FIntVector>& OutBlocks) const
{
	OutBlocks.Reset();
	if (Thick == 0)
	{
		return;
	}

	// 统一按方向构建条带盒：方向分支数学对正负厚度均正确
	// （条带区间反转由 Min/Max 规范化修正：正=边界外扩、负=边界内缩）
	for (const EActorDirection Dir : GenDir)
	{
		FVector Min, Max;

		// 根据方向设置Min/Max
		switch (Dir)
		{
		case EActorDirection::Backward:
			Min = FVector(InputBox.Min.X, InputBox.Min.Y - Thick, InputBox.Min.Z);
			Max = FVector(InputBox.Min.X -Thick, InputBox.Max.Y + Thick, InputBox.Max.Z);
			break;
		case EActorDirection::Forward:
			Min = FVector(InputBox.Max.X + Thick, InputBox.Min.Y - Thick, InputBox.Min.Z);
			Max = FVector(InputBox.Max.X, InputBox.Max.Y + Thick, InputBox.Max.Z);
			break;
		case EActorDirection::Left:
			Min = FVector(InputBox.Min.X - Thick, InputBox.Min.Y, InputBox.Min.Z);
			Max = FVector(InputBox.Max.X + Thick, InputBox.Min.Y - Thick, InputBox.Max.Z);
			break;
		case EActorDirection::Right:
			Min = FVector(InputBox.Min.X - Thick, InputBox.Max.Y + Thick, InputBox.Min.Z);
			Max = FVector(InputBox.Max.X + Thick, InputBox.Max.Y, InputBox.Max.Z);
			break;
		case EActorDirection::Up:
			Min = FVector(InputBox.Min.X - Thick, InputBox.Min.Y-Thick, InputBox.Max.Z);
			Max = FVector(InputBox.Max.X + Thick, InputBox.Max.Y+Thick, InputBox.Max.Z+Thick);
			break;
		case EActorDirection::Down:
			Min = FVector(InputBox.Min.X - Thick, InputBox.Min.Y-Thick, InputBox.Min.Z);
			Max = FVector(InputBox.Max.X + Thick, InputBox.Max.Y+Thick, InputBox.Min.Z-Thick);
			break;
		default:
			continue;
		}
		// 确保Min小于Max（同时修正负厚度产生的反转区间）
		const FBox StripBox(FVector::Min(Min, Max), FVector::Max(Min, Max));

		for (int32 z = StripBox.Min.Z; z <= StripBox.Max.Z; ++z)
		{
			for (int32 y = StripBox.Min.Y; y <= StripBox.Max.Y; ++y)
			{
				for (int32 x = StripBox.Min.X; x <= StripBox.Max.X; ++x)
				{
					OutBlocks.Add(FIntVector(x, y, z));
				}
			}
		}
	}
}

void UVoxelBlockWorldSubsystem::BatchModifyWallByIndex(FBox InputBox, const FVoxelEditCommand& EditCommand,
	int32 TypeID, TArray<EActorDirection> GenDir, int Thick)
{
	UE_LOG(LogTemp, Warning, TEXT("Start Gen Wall! TypeID: %d"), TypeID);
	// 参数验证

	if (Thick == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Thick cannot be zero"));
		return;
	}

	if (GenDir.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("none wall dir"));
		return;
	}

	// 与工具层历史采集共用同一份方向条带数学（CalculateWallBlocks）
	TSet<EActorDirection> DirSet;
	DirSet.Append(GenDir);
	TSet<FIntVector> WallBlocks;
	CalculateWallBlocks(InputBox, DirSet, Thick, WallBlocks);

	if (WallBlocks.IsEmpty())
	{
		return;
	}

	BatchModifyVoxelData(WallBlocks.Array(), EditCommand, TypeID);

	bIsWaitingToUpdateBlock=true;

}

void UVoxelBlockWorldSubsystem::BatchModifyBox(FBox InputBox, const FVoxelEditCommand& EditCommand, int32 TypeID,
	int Thick)
{
	// 1. 参数验证
	if (Thick < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid thickness value: %d"), Thick);
		Thick = 0;
	}
	FBox AddBox;
	// 2. 计算扩展后的边界
	AddBox.Min= InputBox.Min-FVector(Thick);
	AddBox.Max = InputBox.Max + FVector(Thick);
	UE_LOG(LogTemp, Warning, TEXT("Box value: %s,%s"), *AddBox.Min.ToString(), *AddBox.Max.ToString());
	// 3. 优化后的遍历逻辑
	TArray<FIntVector> block;
	for (int32 z = AddBox.Min.Z; z <= AddBox.Max.Z; ++z)
	{
		const bool bIsInnerZ = (z > InputBox.Min.Z && z < InputBox.Max.Z);
        
		for (int32 y = AddBox.Min.Y; y <= AddBox.Max.Y; ++y)
		{
			const bool bIsInnerY = (y > InputBox.Min.Y && y < InputBox.Max.Y);
            
			for (int32 x = AddBox.Min.X; x <= AddBox.Max.X; ++x)
			{
				const bool bIsInnerX = (x > InputBox.Min.X && x < InputBox.Max.X);
                
				// 跳过内部方块（当thick>0时只处理外壳）
				if (Thick!=0&&bIsInnerX && bIsInnerY && bIsInnerZ)
				{
					continue;
				}
				block.Add(FIntVector(x, y, z));
			}
		}
	}
	BatchModifyVoxelData(block, EditCommand, TypeID);
	// 4. 标记需要更新
	//bIsWaitingToUpdateChunk = true;
	UE_LOG(LogTemp, Log, TEXT("Prepared chunks for update"));
}

void UVoxelBlockWorldSubsystem::BatchModifyCylinder(FBox InputBox, const FVoxelEditCommand& EditCommand, int32 TypeID,
	int Thick, bool IsThrough)
{
	// 计算半径

	const float BaseRadius = FVector::DistSquaredXY(InputBox.Min, InputBox.Max);
	const float AddRadius =  FVector::DistSquaredXY(InputBox.Min+Thick, InputBox.Max+Thick);
	const float MaxRadius = FMath::Max(BaseRadius, AddRadius);  // 取较大值
	const float MinRadius = FMath::Min(BaseRadius, AddRadius);  // 取较小值
	
	
	const int IntRadius = FMath::CeilToInt(FMath::Sqrt(MaxRadius));
    TArray<FIntVector> block;
	for (int y = -IntRadius; y <= +IntRadius; y++)
	{
		for (int x = -IntRadius; x <= +IntRadius; x++)
		{
			int CurDist = x*x+y*y;
			const bool bIsInner = (Thick!=0&&CurDist>=MinRadius);
			if (!bIsInner&&CurDist<=MaxRadius)
			{
				if(IsThrough)
				{
					for (int z = 0; z <= InputBox.Max.Z-InputBox.Min.Z; z++)
					{
						FIntVector BlockIndex=FIntVector(x,y,z)+FIntVector(InputBox.Min);
						block.Add(BlockIndex);
					}
				}
				else
				{
					for (int z = Thick; z <= InputBox.Max.Z-InputBox.Min.Z-Thick; z++)
					{
						FIntVector BlockIndex=FIntVector(x,y,z)+FIntVector(InputBox.Min);
						block.Add(BlockIndex);
					}
				}
				

			}
			
		}
	};

	BatchModifyVoxelData(block, EditCommand, TypeID);
	// bIsWaitingToUpdateChunk=true;
	// UE_LOG(LogTemp, Warning, TEXT("UpdateChunkNum ,%d"),ChunkIndexWaitingToUpdate.Num());
	// UE_LOG(LogTemp, Warning, TEXT("bIsWaitingToUpdateChunk: %s"), ( bIsWaitingToUpdateChunk ? TEXT("true") : TEXT("false")));

}

void UVoxelBlockWorldSubsystem::BatchModifySphere(FBox InputBox, const FVoxelEditCommand& EditCommand, int32 TypeID,
	int Thick)
{
	
	// 计算半径
	const float BaseRadius = FVector::DistSquared(InputBox.Min, InputBox.Max);
	const float AddRadius =  FVector::DistSquared(InputBox.Min+Thick, InputBox.Max+Thick);
	const float MaxRadius = FMath::Max(BaseRadius, AddRadius);  // 取较大值
	const float MinRadius = FMath::Min(BaseRadius, AddRadius);  // 取较小值
	
	const int IntRadius = FMath::CeilToInt(FMath::Sqrt(MaxRadius));
    TArray<FIntVector>block;
	for (int y = -IntRadius; y <= +IntRadius; y++)
	{
		for (int x = -IntRadius; x <= +IntRadius; x++)
		{
			int CurDistXY = x*x+y*y;
			if (CurDistXY <= MaxRadius)
			{
				for (int z =-IntRadius; z <= IntRadius; z++)
				{
					int CurDist =CurDistXY+(z*z);
					const bool bIsInner = (Thick!=0&&CurDist>=MinRadius);
					if (!bIsInner&&CurDist<=MaxRadius)
					{
						FIntVector BlockIndex=FIntVector(x,y,z)+FIntVector(InputBox.Min);
						block.Add(BlockIndex);
					}
				}
			}
		}
	};
	BatchModifyVoxelData(block, EditCommand, TypeID);

}

// --- 辅助与编辑 ---

void UVoxelBlockWorldSubsystem::RequestStreamingUpdate()
{
	if (!bStreamingRequested)
	{
		bStreamingRequested = true;
		StreamingDelay = 0.0f;
	}
}

void UVoxelBlockWorldSubsystem::ModifyVoxelData(FIntVector BlockIndex, const FVoxelEditCommand& EditCommand,const int32 TypeID)
{
	if (bIsWaitingToUpdateBlock)
	{
		return;
	}
	if (!IsBlockIndexInScene(BlockIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid Index ,%s"),*BlockIndex.ToString());
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("Modify Index ,%s"),*BlockIndex.ToString());
    
	FIntVector ChunkIndex = UChunkUtilsFunction::Get3DChunkIndexByBlockIndex(BlockIndex, ChunkSize);

	FWriteScopeLock Lock(PendingEditsLock);

	FVoxelEditModification Modification;
	Modification.SetEditCommand(EditCommand);
	Modification.Add(BlockIndex, TypeID);

	PendingModifications.Enqueue(Modification);
}

void UVoxelBlockWorldSubsystem::ProcessBlockEdits()
{
	FVoxelEditModification Modification;
	int count = 0;
	while (!PendingModifications.IsEmpty() && count < 5)
	{
		PendingModifications.Dequeue(Modification);

		if (Modification.Num() > 0)
		{
			TMap<int32, TArray<FIntVector>> BlocksByType;
			TSet<FIntVector> AllChunkIndices;

			for (const auto& Pair : Modification.GetBlocks())
			{
				const FIntVector& BlockIndex = Pair.Key;
				const int32& BlockType = Pair.Value;

				BlocksByType.FindOrAdd(BlockType).Add(BlockIndex);

				const FIntVector ChunkIndex = UChunkUtilsFunction::Get3DChunkIndexByBlockIndex(
					BlockIndex,
					ChunkSize
				);
				AllChunkIndices.Add(ChunkIndex);
				IsBlockOnBorder(BlockIndex, ChunkIndex, AllChunkIndices);
			}

			UE_LOG(LogTemp, Log, TEXT("[EditChain] ProcessBlockEdits: %d blocks, %d chunks"),
				Modification.Num(), AllChunkIndices.Num());

			for (const auto& TypePair : BlocksByType)
			{
				const int32& BlockType = TypePair.Key;
				const TArray<FIntVector>& BlockIndices = TypePair.Value;

				TArray<int32> BlockIds;
				BlockIds.Reserve(BlockIndices.Num());
				for (const auto& BlockIndex : BlockIndices) {
					BlockIds.Add(UChunkUtilsFunction::GetBlockIdByIndex(BlockIndex, WorldDimension));
				}

				CheckBatchEditType(BlockIds, Modification.GetEditCommand(), BlockType);
			}

			for (const auto& ChunkIndex : AllChunkIndices) {
				modifyChunk(ChunkIndex);
			}
		}

		count++;
	}
	if (PendingModifications.IsEmpty()) {
		bIsWaitingToUpdateBlock = false;
	}
}

bool UVoxelBlockWorldSubsystem::modifyChunk(FIntVector ChunkIndex)
{
	if (LoadedChunks.Contains(ChunkIndex) && ChunkMetaDataMap.Contains(ChunkIndex))
	{
		UE_LOG(LogTemp, Log, TEXT("[EditChain] modifyChunk: Chunk=%s → WToCalculate"),
			*ChunkIndex.ToString());
		UpdateChunkState(ChunkIndex, EChunkState::Any, EChunkState::WToCalculate);
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("[EditChain] modifyChunk: Chunk=%s NOT LOADED (Loaded=%d, Meta=%d), modification lost!"),
		*ChunkIndex.ToString(), LoadedChunks.Contains(ChunkIndex), ChunkMetaDataMap.Contains(ChunkIndex));
	return false;
}

void UVoxelBlockWorldSubsystem::ProcessChunkUpdates()
{
    // 优先级顺序处理
    ProcessUnloading(); // 先卸载释放内存
    
    if (!RenderingQueue.IsEmpty()) ProcessRenderUpdates();
    if (!CalculateQueue.IsEmpty()) ProcessCalculateUpdates();
    if (!GeneratingQueue.IsEmpty()) ProcessGenerating();
    if (!LoadingQueue.IsEmpty()) ProcessLoading();
}

void UVoxelBlockWorldSubsystem::ProcessLoading()
{
    // 限制每帧处理数量
    int32 Count = 0;
    FChunkLoadRequest Request;
    const int32 MaxLoads = GameInstance ? GameInstance->WorldConfig.MaxLoadsPerFrame : 8;
    while (Count < MaxLoads && LoadingQueue.Dequeue(Request))
    {
        UE_LOG(LogTemp, Error, TEXT("[BlockSys] ProcessLoading: Chunk=%s"), *Request.Coord.ToString());
        LoadChunkData(Request.Coord);
        Count++;
    }
}

void UVoxelBlockWorldSubsystem::ProcessGenerating()
{
	int32 Count = 0;
	FChunkLoadRequest Request;
    
	const int32 MaxGenerates = GameInstance ? GameInstance->WorldConfig.MaxGeneratesPerFrame : 5;

	// 限制每帧处理数量
	while (Count < MaxGenerates && GeneratingQueue.Dequeue(Request))
	{
		UE_LOG(LogTemp, Error, TEXT("[BlockSys] ProcessGenerating: Chunk=%s"), *Request.Coord.ToString());
		if (FChunkData* DataPtr = ChunkMetaDataMap.Find(Request.Coord))
		{
			if (DataPtr->State != EChunkState::Active)
			{
				// 注意：GenerateChunk 内部通常会将状态改为 WToCalculate
				UpdateChunkState(Request.Coord, EChunkState::Any, EChunkState::WToGenerate);
				// 生成 Actor
				GenerateChunk(Request.Coord); 
			}
		}
		Count++;
	}
}

void UVoxelBlockWorldSubsystem::ProcessCalculateUpdates()
{
    // 限制并发任务数
    int32 ActiveTasks = 0;
    {
        FScopeLock Lock(&TaskMutex);
        ActiveTasks = PendingGenerationTasks.Num();
    }
    
    FChunkLoadRequest Request;
    while (ActiveTasks < MaxConcurrentTasks && CalculateQueue.Dequeue(Request))
    {
    	//UE_LOG(LogTemp, Error, TEXT("[BlockSys] ProcessCalculateUpdates: Chunk=%s"), *Request.Coord.ToString());
    	if (FChunkData* DataPtr = ChunkMetaDataMap.Find(Request.Coord))
    	{
    		if (DataPtr->State != EChunkState::WToCalculate)
    		{
    			continue; // 状态不对，跳过
    		}
    	}
    	else
    	{
    		continue; // 数据不存在，跳过
    	}

        MarkTaskPending(Request.Coord);
        ActiveTasks++;

        //UE_LOG(LogTemp, Error, TEXT("[BlockSys] ProcessCalculateUpdates: Building context for Chunk=%s"), *Request.Coord.ToString());

        // 关键优化：构建快照 Context
        FChunkGenContext Context;
        BuildChunkContext(Request.Coord, Context);

        //UE_LOG(LogTemp, Error, TEXT("[BlockSys] ProcessCalculateUpdates: Context built. RawBlockData=%d, BlockDefs=%d, dispatching async mesh gen"),
        //    Context.RawBlockData.Num(), Context.BlockDefs.Num());

        // 启动异步任务
        FFunctionGraphTask::CreateAndDispatchWhenReady([this, Context]() {
            if (bShouldStopAllTasks) return;

            // 在后台线程使用 Context 计算，无锁
            FChunkMeshDataPair LocalMeshDataPair = GenerateMeshData(Context);

            // 回到主线程应用结果
            AsyncTask(ENamedThreads::GameThread, [this, ChunkIndex = Context.ChunkIndex, LocalMeshDataPair]() {
                if (!bShouldStopAllTasks) {
                    CalculateChunk(ChunkIndex, LocalMeshDataPair);
                    HandleGenerationComplete(ChunkIndex);
                }
            });
        }, TStatId(), nullptr, ENamedThreads::AnyThread);
    }
}

void UVoxelBlockWorldSubsystem::ProcessRenderUpdates()
{
    // 优化：时间片限制 (Time Budget)，防止一帧提交太多 Mesh
    double StartTime = FPlatformTime::Seconds();
    const double MaxTimeBudget = 0.003; // 3ms

    FChunkLoadRequest Request;
	while (!RenderingQueue.IsEmpty() && (FPlatformTime::Seconds() - StartTime < MaxTimeBudget))
	{
		if (RenderingQueue.Dequeue(Request))
		{
			if (FChunkData* DataPtr = ChunkMetaDataMap.Find(Request.Coord))
			{
				// 直接通过指针检查状态，零额外开销
				if (DataPtr->State == EChunkState::WToRender)
				{
					ApplyChunk(Request.Coord);
				}
			}
		}
	}
}

void UVoxelBlockWorldSubsystem::ProcessUnloading()
{
    FChunkLoadRequest Request;
    int32 Count = 0;
    const int32 MaxUnloads = GameInstance ? GameInstance->WorldConfig.MaxUnloadsPerFrame : 8;
    while (Count < MaxUnloads && UnloadingQueue.Dequeue(Request))
    {
        ReleaseChunk(Request.Coord);
        Count++;
    }
}

void UVoxelBlockWorldSubsystem::UpdateChunkState(FIntVector Coord, EChunkState Expect, EChunkState New)
{
    if (!ChunkMetaDataMap.Contains(Coord)) return;
    
    if (Expect == EChunkState::Any || ChunkMetaDataMap[Coord].State == Expect)
    {
        ChunkMetaDataMap[Coord].State = New;
        
        // 事件驱动：根据新状态推入对应队列
        FChunkLoadRequest NextReq(Coord, EChunkLoadPriority::High);
        
        switch (New)
        {
            case EChunkState::WToLoad: LoadingQueue.Enqueue(NextReq); break;
            case EChunkState::WToGenerate: GeneratingQueue.Enqueue(NextReq); break;
            case EChunkState::WToCalculate: CalculateQueue.Enqueue(NextReq); break;
            case EChunkState::WToRender: RenderingQueue.Enqueue(NextReq); break;
            case EChunkState::WToUnload: UnloadingQueue.Enqueue(NextReq); break;
            default: break;
        }
    }
}


void UVoxelBlockWorldSubsystem::ProcessChunkStreaming()
{
    PlayerPositions = LastPlayerChunkPositions;
    if (!GameInstance) return;

    int LoadDistance = GameInstance->WorldConfig.LoadDistance;
    int32 QueuedCount = 0;
    UE_LOG(LogTemp, Error, TEXT("[BlockSys] ProcessChunkStreaming: Players=%d, LoadDistance=%d"), PlayerPositions.Num(), LoadDistance);
    float nearDistance = LoadDistance * LoadDistance * LoadDistance;

    for (const FIntVector& PlayerPos : PlayerPositions)
    {
        for (int32 x = -LoadDistance; x <= LoadDistance; x++)
        {
            for (int32 y = -LoadDistance; y <= LoadDistance; y++)
            {
                for(int32 z = -LoadDistance; z <= LoadDistance; z++)
                {
                    FIntVector Index = PlayerPos + FIntVector(x, y, z);

                    if (!GameInstance->GetCurScene().IsChunkIndexInScene(Index))
                    {
                        continue;
                    }

                    // 如果已经在处理中或已激活，跳过
                    if (ActiveRequestsSet.Contains(Index)) continue;
                	if (FChunkData* DataPtr = ChunkMetaDataMap.Find(Index))
                	{
                		if (DataPtr->State == EChunkState::Active) 
                		{
                			continue;
                		}
                	}

                    ActiveRequestsSet.Add(Index);
                    QueuedCount++;
                    
                    // 初始化状态
                    if (!ChunkMetaDataMap.Contains(Index))
                    {
                        GenerateChunkData(Index); // 初始状态 WToLoad
                    }
                    
                    // 直接推入加载队列
                    ChunkMetaDataMap[Index].State = EChunkState::WToLoad;
                    LoadingQueue.Enqueue(FChunkLoadRequest(Index, EChunkLoadPriority::Medium));
                }
            }
        }
    }
    UE_LOG(LogTemp, Error, TEXT("[BlockSys] ProcessChunkStreaming: Queued %d chunks for loading"), QueuedCount);

    // 处理卸载
    TArray<FIntVector> KeysToUnload;
    for (const auto& Pair : LoadedChunks)
    {
        bool bIsInRange = false;
        for (const FIntVector& PlayerPos : PlayerPositions)
        {
            if (IsChunkIndexInLoadDistance(PlayerPos, Pair.Key, LoadDistance)) {
                bIsInRange = true; break;
            }
        }
        if (!bIsInRange) KeysToUnload.Add(Pair.Key);
    }

    for (const auto& Index : KeysToUnload)
    {
        if (ChunkMetaDataMap.Contains(Index)) {
            UpdateChunkState(Index, EChunkState::Any, EChunkState::WToUnload);
        }
    }
}


void UVoxelBlockWorldSubsystem::MarkTaskPending(FIntVector Coord) {
	FScopeLock Lock(&TaskMutex);
	PendingGenerationTasks.Add(Coord);
}

void UVoxelBlockWorldSubsystem::MarkTaskCompleted(FIntVector Coord) {
	FScopeLock Lock(&TaskMutex);
	PendingGenerationTasks.Remove(Coord);
}

void UVoxelBlockWorldSubsystem::HandleGenerationComplete(FIntVector Coord) {
	MarkTaskCompleted(Coord);
}

void UVoxelBlockWorldSubsystem::CheckPlayersMovement()
{
	TArray<FIntVector> CurrentPositions;
	bool bAnyPlayerMoved = false;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It) {
		if (APlayerController* PC = It->Get()) {
			if (APawn* Pawn = PC->GetPawn()) {
				const FIntVector CurBlockIndex = UChunkUtilsFunction::GetBlockIndexByPosition(Pawn->GetActorLocation(), BlockLength);
				FIntVector CurChunkIndex = UChunkUtilsFunction::Get3DChunkIndexByBlockIndex(CurBlockIndex, ChunkSize);
				CurChunkIndex = FIntVector(CurChunkIndex.X, CurChunkIndex.Y, 0);
				CurrentPositions.Add(CurChunkIndex);
			}
		}
	}
	if (LastPlayerChunkPositions.Num() != CurrentPositions.Num()) bAnyPlayerMoved = true;
	else {
		for (int32 i = 0; i < CurrentPositions.Num(); i++) {
			if (LastPlayerChunkPositions[i] != CurrentPositions[i]) { bAnyPlayerMoved = true; break; }
		}
	}
	LastPlayerChunkPositions = CurrentPositions;

	bPlayersMoved = bAnyPlayerMoved;

	if (bAnyPlayerMoved && CurrentPositions.Num() > 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlockSys] CheckPlayersMovement: Player moved to chunk %s, bPlayersMoved=true"), *CurrentPositions[0].ToString());
	}
	else if (CurrentPositions.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlockSys] CheckPlayersMovement: No player pawns found!"));
	}
}
void UVoxelBlockWorldSubsystem::BatchModifyVoxelData_Modification(const FVoxelEditModification& Modification)
{
	if (Modification.Num() == 0)
	{
		return;
	}

	const FVoxelEditCommand& EditCommand = Modification.GetEditCommand();

	{
		FWriteScopeLock Lock(PendingEditsLock);
		PendingModifications.Enqueue(Modification);
	}

	bIsWaitingToUpdateBlock = true;
}
