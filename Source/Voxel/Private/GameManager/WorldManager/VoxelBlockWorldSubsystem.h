// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/VoxelConfigStruct.h"
#include "Core/Enum/EditCommandEnums.h"
#include "Data/VoxelEditCommand.h"
#include "Data/Scene/ChunkData.h"
#include "Data/Scene/SceneData.h"
#include "Subsystems/WorldSubsystem.h"
#include "Containers/Queue.h"
#include "Core/CoreTags.h"
#include "Core/VoxelGameInstance.h"
#include "Utils/ChunkUtilsFunction.h"
#include "VoxelBlockWorldSubsystem.generated.h"

struct FSingleEditRequest;
struct FVoxelEditModification;
class UVoxelGameInstance;
class UObjectPoolSubsystem;
class UVoxelTextureRegistry;
class UMaterialInstanceDynamic;
struct FBlockRenderInfo
{
	bool bIsOpaque = true;     // 是否不透明（用于 CheckBlockOpaque）
	int32 TextureIndices[6];   // 上下左右前后的纹理索引

	FBlockRenderInfo() { FMemory::Memzero(TextureIndices); }
};

// 切面三域：面分流判定（替代二值 Opaque）
enum class EVoxelFaceDom : uint8
{
	Air,      // 非实体 / 空气
	Ghost,    // 实体 且 InSliceScope（半透明 Section1，无碰撞）
	Opaque,   // 实体 且 !InSliceScope（不透明 Section0，有碰撞）
};

// FChunkMeshDataPair 定义于 ChunkMeshData.h（双 Section 输出）

// 优化：快照上下文，用于后台线程无锁访问数据
struct FChunkGenContext
{
	FIntVector ChunkIndex;
	FIntVector ChunkSize;
	FIntVector PaddedSize=ChunkSize+FIntVector(2); // Size + 2 (包含邻居)
	TArray<int32> RawBlockData; // 扁平化的一维数组
	TMap<int32, FBlockRenderInfo> BlockDefs;

	// === 切面快照（由 BuildChunkContext 注入，后台线程只读） ===
	int32 SliceLayer = INT32_MAX;          // INT32_MAX = 无切面（退化为二域）
	int32 SliceAreaMode = 0;               // 0=全局 1=白名单 2=黑名单
	TArray<FBox> SliceAreaBoxes;           // 体素索引空间区域盒（≤8个）

	// === 透明区域框快照（由 BuildChunkContext 注入，仅含与本 chunk 相交的框，控逐体素成本） ===
	TArray<FBox> CutoutBoxes;              // 体素索引空间闭盒（Min..Max 含端点）

	// 辅助函数：从快照中获取 BlockID
	FORCEINLINE int32 GetBlockId(int32 X, int32 Y, int32 Z) const
	{
		// 坐标偏移 +1 (因为包含了 -1 的边界)
		const int32 LocalX = X + 1;
		const int32 LocalY = Y + 1;
		const int32 LocalZ = Z + 1;

		const int32 Index = LocalZ * (PaddedSize.X * PaddedSize.Y) + LocalY * PaddedSize.X + LocalX;

		if (RawBlockData.IsValidIndex(Index))
		{
			return RawBlockData[Index];
		}
		return EBlockId::Air;
	}
	// 【新增】线程安全的查询函数
	FORCEINLINE bool IsOpaque(int32 BlockId) const
	{
		if (const FBlockRenderInfo* Info = BlockDefs.Find(BlockId))
		{
			return Info->bIsOpaque;
		}
		return false; // 默认空气或未知方块为透明
	}

	// 【新增】线程安全的获取纹理函数
	FORCEINLINE int32 GetTexture(int32 BlockId, const FVector& Normal) const
	{
		if (const FBlockRenderInfo* Info = BlockDefs.Find(BlockId))
		{
			// 将法线转换为 0-5 的索引
			int32 FaceIndex = 0;
			if (Normal == FVector::UpVector) FaceIndex = 2;      // Up
			else if (Normal == FVector::DownVector) FaceIndex = 3; // Down
			else if (Normal == FVector::LeftVector) FaceIndex = 4; // Left
			else if (Normal == FVector::RightVector) FaceIndex = 5;  // Right
			else if (Normal == FVector::ForwardVector) FaceIndex = 1;  // Forward
			else if (Normal == FVector::BackwardVector) FaceIndex = 0; // Backward

			return Info->TextureIndices[FaceIndex];
		}
		// BlockDefs 中找不到该 TypeId（不应该发生，说明 BlockDefs 构建有问题）
		UE_LOG(LogTemp, Error, TEXT("[ChunkGen] GetTexture: BlockDef NOT FOUND for TypeId=%d"), BlockId);
		return 0;
	}

	// === 切面三域辅助 ===

	// 体素世界索引是否在切面作用域内（z >= SliceLayer 且 区域过滤通过）
	FORCEINLINE bool IsInSliceScope(const FIntVector& WorldBlockIndex) const
	{
		if (SliceLayer == INT32_MAX) return false; // 无切面
		if (WorldBlockIndex.Z < SliceLayer) return false; // 层不够高

		// 区域过滤
		if (SliceAreaMode == 0) return true; // 全局
		// SliceAreaMode == 1：白名单（在任一区域盒内才受控；空盒列表=无区域受影响）
		return IsInAnyAreaBox(WorldBlockIndex);
	}

	// 体素世界索引是否在任一区域盒内（体素索引空间 IsInsideOrOn）
	FORCEINLINE bool IsInAnyAreaBox(const FIntVector& WorldBlockIndex) const
	{
		for (const FBox& Box : SliceAreaBoxes)
		{
			if (Box.IsInsideOrOn(FVector(WorldBlockIndex)))
			{
				return true;
			}
		}
		return false;
	}

	// 体素世界索引是否在任一透明区域框内（闭盒 IsInsideOrOn）
	FORCEINLINE bool IsInAnyCutoutBox(const FIntVector& WorldBlockIndex) const
	{
		for (const FBox& Box : CutoutBoxes)
		{
			if (Box.IsInsideOrOn(FVector(WorldBlockIndex)))
			{
				return true;
			}
		}
		return false;
	}

	// 体素是否为 Ghost（切面作用域 或 透明区域框内）
	FORCEINLINE bool IsGhostVoxel(const FIntVector& WorldBlockIndex) const
	{
		return IsInSliceScope(WorldBlockIndex) || IsInAnyCutoutBox(WorldBlockIndex);
	}

	// 计算体素的面域（Air/Ghost/Opaque）
	FORCEINLINE EVoxelFaceDom GetFaceDom(int32 BlockId, const FIntVector& WorldBlockIndex) const
	{
		if (!IsOpaque(BlockId)) return EVoxelFaceDom::Air;
		return IsGhostVoxel(WorldBlockIndex) ? EVoxelFaceDom::Ghost : EVoxelFaceDom::Opaque;
	}
};

USTRUCT(BlueprintType)
struct FChunkLoadRequest
{
	GENERATED_BODY()
    
	UPROPERTY()
	FIntVector Coord;
    
	UPROPERTY()
	EChunkLoadPriority Priority = EChunkLoadPriority::Medium;
	
	// 优化：移除 IsStepFinish，改用队列流转控制
	// double Timestamp 已移除，TQueue 不支持排序，改用多级队列策略或简化处理
    
	FChunkLoadRequest() {}
	FChunkLoadRequest(FIntVector InCoord, EChunkLoadPriority InPriority = EChunkLoadPriority::Medium)
		: Coord(InCoord), Priority(InPriority) {}

	bool operator==(const FChunkLoadRequest& Other) const
	{
		return Coord == Other.Coord;
	}
};

struct FMask
{
	int32 TypeId;
	int Normal;
	FIntVector3 maskId;
	EVoxelFaceDom Dom = EVoxelFaceDom::Air; // 三域（Opaque/Ghost/Air）
	bool bTranslucent = false;              // 面归属：true=Section1(半透明) false=Section0(不透明)
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWorldBuildFinished);
UCLASS(Abstract, Blueprintable, BlueprintType)
class UVoxelBlockWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FIntVector, FChunkData> ChunkMetaDataMap;
    
	UPROPERTY(EditAnywhere)
	TMap<FIntVector, TWeakObjectPtr<AChunkBase>> LoadedChunks;
	
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InstanceResource")
    FIntVector WorldDimension;
	
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InstanceResource")
    FIntVector ChunkSize;

    // /** 每个建造请求的最大方块数（用于分批处理，避免单次建造卡顿） */
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Edit")
    // int32 MaxBlocksPerRequest = 500000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
    UNavigationSystemV1* NavSystem;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
    ARecastNavMesh* NavMesh;
    UPROPERTY(EditAnywhere, Category = "Chunk")
    ANavMeshBoundsVolume* BoundsVolume;
    UPROPERTY(EditAnywhere, Category = "Chunk")
    ANavigationData* NavData;
    
    UPROPERTY(BlueprintReadWrite, Category = "World")
    TMap<int, int32> CurSceneBlockTypeMask;
	UPROPERTY(BlueprintReadWrite, Category = "World")
	TMap<int, int32> SceneBaseBlockTypeMask;

    // --- 核心函数 ---
	void GenerateWorldBase();
    void GenerateWorldBlock(FSceneData& CurScene);
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
	bool IsTickable() const;
	virtual TStatId GetStatId() const override;
	void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues IVS);

    UFUNCTION(BlueprintImplementableEvent, DisplayName = "Initialize")
    void ReceiveInitialize();
    UFUNCTION(BlueprintImplementableEvent, DisplayName = "Deinitialize")
    bool ReceiveShouldCreateSubsystem() const;
    UFUNCTION(BlueprintImplementableEvent, DisplayName = "Deinitialize")
    void ReceiveDeinitialize();

	// 区块生成与更新
    void RequestStreamingUpdate();
    void CheckPlayersMovement();
    
	// 优化后的网格生成：接受 Context，三域双 Section 输出
    FChunkMeshDataPair GenerateMeshData(const FChunkGenContext& Context);
	
	// 辅助函数（按 Mask.Dom 分流到 Opaque/Translucent）
    void CreateQuad(const FMask& Mask, const FIntVector& AxisMask, 
					int UVAxis1, int UVAxis2, int TileSize, 
					const FIntVector& V1, const FIntVector& V2, const FIntVector& V3, const FIntVector& V4, 
					FChunkMeshDataPair& MeshDataPair, int& VertexCount, 
					const FChunkGenContext& Context);

    int GetTextureIndex(int32 TypeID, FVector Normal) const;
    bool CheckBlockOpaque(int32 InputId);
	FVoxelBlockItemBase* GetBlockInform(int32 TypeId) const;

	/** 默认墙方块（配置表首个方块类型）：区域围墙等几何功能空手时的方块回退；
	 *  无可用方块返回 EBlockId::Invalid */
	UFUNCTION(BlueprintPure, Category = "Voxel|Block")
	int32 GetDefaultWallBlockId() const;

	/** 统计选区内素材清单（方块 + 家具去重），输出 ItemId→数量 与家具总数。
	 *  业务计算统一入口；UI 只负责名称解析与格式化展示，禁止在 Widget 里做统计 */
	void CalculateSelectionMaterials(const TArray<FIntVector>& Selection, TMap<int32, int32>& OutMaterialCounts, int32& OutFurnitureCount);
    
	// 流程处理
    void LoadChunkData(FIntVector InputIndex);
    void GenerateChunkData(FIntVector InputIndex);
    void GenerateChunk(FIntVector InputIndex); // 生成 Actor
    void ReleaseChunk(FIntVector InputChunkIndex);
    void ApplyChunk(FIntVector InputIndex); // 应用 Mesh
    void CalculateChunk(FIntVector InputIndex, FChunkMeshDataPair MeshDataPair); // 接收计算结果（双 Section）

    // 体素编辑
    UFUNCTION(BlueprintCallable, Category = "WBS|Function")
    void ModifyVoxelData(FIntVector BlockIndex, const FVoxelEditCommand& EditCommand, int32 TypeID);
	void ProcessBlockEdits();
	bool modifyChunk(FIntVector ChunkIndex);

	// 批量编辑接口 (保留原有)
	UFUNCTION(BlueprintCallable, Category = "WBS|Function")
	void ModifyVoxelDataByPosition(FVector Position, const FVoxelEditCommand& EditCommand, int32 TypeID);
	UFUNCTION(BlueprintCallable, Category = "WBS|Function")
	void ModifyVoxelDataByIndex(FIntVector Position, const FVoxelEditCommand& EditCommand, int32 TypeID);
	UFUNCTION(BlueprintCallable, Category = "WBS|Function")
	void BatchModifyVoxelDataByIndex(FBox InputBox, EBatchShapeType CurType, const FVoxelEditCommand& EditCommand, int32 TypeID, int Thick = 0);

	/**
	 * 批量修改体素数据（接收完整的 FSingleEditRequest）
	 * @param SingleEditRequest 单次编辑请求（包含 BlockIndex、ChunkIndex、EditMode、NewType）
	 */
	void BatchModifyVoxelData_Single(const FSingleEditRequest& SingleEditRequest);

	/**
	 * 批量修改体素数据（使用新的修改模型 FVoxelEditModification）
	 * @param Modification 修改模型（包含 BlockIndex→BlockType 映射、ChunkIndices、EditMode）
	 */
	void BatchModifyVoxelData_Modification(const FVoxelEditModification& Modification);

	void BatchModifyWallByIndex(FBox InputBox, const FVoxelEditCommand& EditCommand, int32 TypeID, TArray<EActorDirection>
	                            GenDir, int Thick = 0);

	/**
	 * 计算墙壁条带覆盖的方块集合（纯计算，不写世界）
	 * 与 BatchModifyWallByIndex 共用同一份方向条带数学，供工具层采集历史记录使用
	 * @param InputBox 区域包围盒（体素索引空间）
	 * @param GenDir 墙壁方向集合
	 * @param Thick 墙厚（正=向外扩展，负=向内收缩）
	 * @param OutBlocks 输出方块索引集合（已去重）
	 */
	void CalculateWallBlocks(FBox InputBox, const TSet<EActorDirection>& GenDir, int Thick, TSet<FIntVector>& OutBlocks) const;
	void BatchModifyBox(FBox InputBox, const FVoxelEditCommand& EditCommand, int32 TypeID, int Thick = 0);
    void BatchModifyVoxelData(TArray<FIntVector> BlockIndexList, const FVoxelEditCommand& EditCommand, int32 TypeID);
	void BatchModifySphere(FBox InputBox, const FVoxelEditCommand& EditCommand, int32 TypeID, int Thick);
	void BatchModifyCylinder(FBox InputBox, const FVoxelEditCommand& EditCommand, int32 TypeID, int Thick, bool IsThrough);
	void IsBlockOnBorder(const FIntVector BlockIndex, const FIntVector ChunkIndex, TSet<FIntVector>& Result);
	void CheckBatchEditType(TArray<int> BlockIds, const FVoxelEditCommand& EditCommand, int32 TypeID);

	// 任务与队列管理
    void UpdateChunkState(FIntVector Coord, EChunkState Expect, EChunkState New);
    
	// 队列处理函数
    void ProcessRenderUpdates();
    void ProcessCalculateUpdates();
	void ProcessLoading();
	void ProcessGenerating();
    void ProcessUnloading();
    void ProcessChunkStreaming();
    void ProcessChunkUpdates(); // 总入口

    // 任务完成回调
	void HandleGenerationComplete(FIntVector Coord);
    void MarkTaskPending(FIntVector Coord);
    void MarkTaskCompleted(FIntVector Coord);

	// 内联工具
	FORCEINLINE FBox GetWorldBorder() const
	{
		FBox Border;
		for (int Index=0; Index<3; Index++)
		{
			const int32 Dim = WorldDimension[Index];
			if (Dim == 0) {
				Border.Min[Index]=0;
				Border.Max[Index]=ChunkSize[Index];
				continue;
			}
			Border.Min[Index]= -Dim;
			Border.Max[Index]= Dim;
		}
		return Border;
	}
	
    FORCEINLINE bool IsPositionInScene(const FVector& BlockPosition) const
    {
    	return IsBlockIndexInScene(UChunkUtilsFunction::GetBlockIndexByPosition(BlockPosition, BlockLength));
    }
    FORCEINLINE bool IsBlockIndexInScene(const FIntVector& BlockIndex) const
    {
    	if (!GameInstance)
    	{
    		return false;
    	}
    	const FBox Border = GetWorldBorder();
    	return BlockIndex.X >= Border.Min.X && BlockIndex.X < Border.Max.X
    		&& BlockIndex.Y >= Border.Min.Y && BlockIndex.Y < Border.Max.Y
    		&& BlockIndex.Z >= Border.Min.Z && BlockIndex.Z < Border.Max.Z;
    }

    // 线程安全的方块读写
	int32 GetBlockType(int BlockId)
    {
        FReadScopeLock _(BlockMapLock);
		if (int32* ModifiedBlock = CurSceneBlockTypeMask.Find(BlockId))
		{
			//if (BlockId == 191627) {
			//	UE_LOG(LogTemp, Error, TEXT("ModifiedBlock ID(%d) -> Found(%d)"),BlockId, *ModifiedBlock);
			//}
			return *ModifiedBlock;
		}
		// 2. 如果玩家没动过这个方块，才去查基础地形
		if (const int32* BaseBlock = SceneBaseBlockTypeMask.Find(BlockId))
		{
			//if (BlockId == 191627) {
			//	UE_LOG(LogTemp, Error, TEXT("BaseBlock ID(%d) -> Found(%d)"),BlockId, *BaseBlock);
			//}
			return *BaseBlock;
		}
		return EBlockId::Air;
    };

	// 重载版本：使用 FIntVector 获取方块类型
	int32 GetBlockType(const FIntVector& BlockIndex)
	{
		const int BlockId = UChunkUtilsFunction::GetBlockIdByIndex(BlockIndex, WorldDimension);
		return GetBlockType(BlockId);
	}
    
    void SetBlockType(int BlockId, int32 BlockType)
    {
        FWriteScopeLock _(BlockMapLock);
    	CurSceneBlockTypeMask.FindOrAdd(BlockId) = BlockType;
    };
	static bool IsChunkIndexInLoadDistance(FIntVector CurChunkIndex, FIntVector CheckChunkIndex, int LoadDistance)
	{
		return  FMath::Abs(CheckChunkIndex.X - CurChunkIndex.X) <= LoadDistance &&
				FMath::Abs(CheckChunkIndex.Y - CurChunkIndex.Y) <= LoadDistance &&
				FMath::Abs(CheckChunkIndex.Z - CurChunkIndex.Z) <= LoadDistance;
	}

	// 基础数据生成
    void GenerateScencePFNav();
    void GenerateBlockTypeMasks();
    void GenerateBlockTypeMaskInChunk(FIntVector ChunkIndex);
    void GenerateBlockTypeMask(FIntVector Input);
	UPROPERTY(BlueprintAssignable)
	FOnWorldBuildFinished OnWorldBuildFinished;
	UPROPERTY()
	UVoxelGameInstance* GameInstance;
	bool bShouldStopAllTasks = true;
	int BlockLength;

	/**
	 * 获取共享的 Chunk 材质 MID（已设置 Texture2DArray 参数）
	 * 供 ChunkBase::ApplyMeshRender 和 PreviewActor 使用
	 * 首次调用时懒创建，后续直接返回缓存
	 */
	UMaterialInstanceDynamic* GetSharedChunkMID();

	/**
	 * 应用切面：缓存 Slice 快照并触发视野内 dirty chunk 重建（三域双 Section）
	 * @param SliceHeight 切面世界 Z 高度（厘米）
	 * @param bEnabled 是否启用切面
	 * @param AreaConfig 区域过滤配置（AreaMode + AABB 列表）
	 * @note SliceLayer = round(SliceHeight / BlockLength)；bEnabled=false 或 SliceLayer=Max 时置 INT32_MAX 重建恢复
	 */
	void ApplySlice(float SliceHeight, bool bEnabled, const struct FSliceAreaConfig& AreaConfig);

	/**
	 * 应用透明区域框：缓存有效框闭盒集并触发受影响 chunk 重建（旧集∪新集 并集标脏）
	 * @param EffectiveBoxes 有效框（仅 bEnabled）的体素索引闭盒集 [Min,Max]
	 * @note 无变化跳过；框内体素判定为 Ghost（透明+穿射线+挡Pawn）
	 */
	void ApplyCutoutBoxes(const TArray<FBox>& EffectiveBoxes);

	/** 半透明 Section1 共享 MID（M_ChunkTranslucent），懒创建 */
	UMaterialInstanceDynamic* GetSharedChunkTranslucentMID();

	/** 当前切面快照（供 BuildChunkContext 读取，游戏线程访问） */
	int32 GetCurrentSliceLayer() const { return CurrentSliceLayer; }
	int32 GetCurrentSliceAreaMode() const { return CurrentSliceAreaMode; }
	const TArray<FBox>& GetCurrentSliceAreaBoxes() const { return CurrentSliceAreaBoxes; }

	/** 当前透明区域框有效闭盒集（供 BuildChunkContext 读取） */
	const TArray<FBox>& GetCurrentCutoutBoxes() const { return CurrentCutoutBoxes; }

	// 辅助：从 Map 中准备快照
	void BuildChunkContext(const FIntVector& ChunkIndex, FChunkGenContext& OutContext);

	// 辅助：从选区体素列表构建快照（复用 GenerateMeshData）
	void BuildSelectionContext(const TArray<FIntVector>& SelectedVoxels, FChunkGenContext& OutContext);

	// 辅助：从模板 Entries 构建快照（用于模板放置预览，复用 GenerateMeshData）
	// 模板的 Entries 是相对坐标（从 0 开始），写入真实的方块句柄（区分不同方块纹理）
	// Remap: 素材替换表（原 ItemId → 新 ItemId），使预览所见即所得
	void BuildTemplateContext(const struct FVoxelTemplate& Template, FChunkGenContext& OutContext, const TMap<FString, FString>& Remap = TMap<FString, FString>());

protected:
	void StopAllBackgroundTasks();
	void CleanupAllResources();
	

protected:
	//标记：是否正在等待世界重建完成
	bool bIsWorldRebuilding = false; 
    
	//防抖动：防止队列刚开始还没填进去就误判为空
	float InitialLoadStableTimer = 0.1f;
private:
	
    UPROPERTY()
    UObjectPoolSubsystem* PoolSubsystem;
    // 锁
    mutable FRWLock PendingEditsLock;
    mutable FRWLock BlockMapLock;
    mutable FCriticalSection TaskMutex;
    
    // 运行时数据
    TArray<FIntVector> PlayerPositions;
    TArray<FIntVector> LastPlayerChunkPositions;
    bool bPlayersMoved = false;
    
    TQueue<FVoxelEditModification> PendingModifications;
	TSet<FIntVector> ActiveRequestsSet; // 用于快速查找是否已在队列中

    // 优化：使用 TQueue 替代 TArray
    TQueue<FChunkLoadRequest> LoadingQueue;
	TQueue<FChunkLoadRequest> GeneratingQueue;
    TQueue<FChunkLoadRequest> CalculateQueue;
    TQueue<FChunkLoadRequest> RenderingQueue;
	TQueue<FChunkLoadRequest> UnloadingQueue;
	
    TSet<FIntVector> PendingGenerationTasks;
    
    
    float StreamingDelay = 0.0f;
    const float MaxStreamingDelay = 0.5f; // 稍微降低延迟
    bool bStreamingRequested = false;
    bool bIsWaitingToUpdateBlock = false;
	
	// 动态并发控制
	int32 MaxConcurrentTasks = 1;
	UPROPERTY()
	UConfigDataSubsystem* ConfigSubsystemCache;

	/** 贴图注册中心缓存（GameInstance 子系统，运行时动态生成 Texture2DArray） */
	UPROPERTY()
	UVoxelTextureRegistry* TextureRegistryCache = nullptr;

	/** 共享的 Chunk 材质 MID（Section0 不透明，已设置 BaseColorArray/NormalArray/ORMArray 参数） */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SharedChunkMID = nullptr;

	/** 共享的 Chunk 半透明 MID（Section1，M_ChunkTranslucent） */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SharedChunkTranslucentMID = nullptr;

	// === 当前切面快照（游戏线程写入，BuildChunkContext 读取） ===
	int32 CurrentSliceLayer = INT32_MAX;    // INT32_MAX = 无切面
	int32 CurrentSliceAreaMode = 0;
	TArray<FBox> CurrentSliceAreaBoxes;

	// === 当前透明区域框有效闭盒集（游戏线程写入，BuildChunkContext 读取） ===
	TArray<FBox> CurrentCutoutBoxes;

	// 地表默认方块的运行时句柄（由身份字符串 "core:block.stone" 决定性 hash 生成）
	// 在 GenerateWorldBase 中通过 GetHandleByFullId 查询并缓存，避免地形生成时反复查表
	int32 GroundBlockHandleId = EBlockId::Air;
};