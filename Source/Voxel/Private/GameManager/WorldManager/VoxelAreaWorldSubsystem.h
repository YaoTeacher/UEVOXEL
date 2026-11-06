
#pragma once

#include "CoreMinimal.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Subsystems/WorldSubsystem.h"
#include "Voxel/VoxelCharacter.h"
#include "Voxel/VoxelPlayerController.h"
#include "VoxelAreaWorldSubsystem.generated.h"

class UVoxelGameInstance;
class UVoxelSceneSaveGame;

/**
 * 区域管理子系统
 * 职责：管理所有逻辑区域（房间、店铺），提供基于位置的快速查询。
 * 优化：使用空间哈希 (Spatial Hashing) 处理大地图查询。
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class UVoxelAreaWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
    
public:
    // =========================================================
    // [Core Data] 核心数据
    // =========================================================
    
    // 所有区域的索引 (Key: AreaId, Value: Actor)
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AreaData")
    TMap<int, AVoxelAreaBaseInstance*> AreaIndex;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AreaData")
    TMap<int, AVoxelAreaBaseInstance*> SpawnArea;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AreaData")
    TMap<int, AVoxelAreaBaseInstance*> ConnectAreaIndex;
    
    // 默认区域 Actor 类
    UPROPERTY(EditAnywhere, Category = "Area/Type")
    TSubclassOf<AVoxelAreaBaseInstance> DefaultAreaType;

    // =========================================================
    // [New API] 空间查询接口 (供 ItemSubsystem/AI 使用)
    // =========================================================

    /** * 根据世界网格坐标获取区域 ID
     * 逻辑：
     * 1. 计算坐标所在的 Spatial Chunk。
     * 2. 获取该 Chunk 内的候选区域列表。
     * 3. 精确检测并返回体积最小的包含区域（实现子区域优先）。
     * @return AreaId，如果没有找到返回 -1
     */
    UFUNCTION(BlueprintCallable, Category = "Area|Query")
    int32 GetAreaIndexByPosition(FIntVector BlockPos) const;

    /** 安全获取区域 Actor 指针 */
    UFUNCTION(BlueprintCallable, Category = "Area|Query")
    AVoxelAreaBaseInstance* GetAreaActor(int32 AreaId) const;

    /**
     * 查询玩家无父级（顶级）的管理/拥有区域
     * 条件：ParentAreaId == INVALID_AREA_ID 且 (OwnerCreatureId == PlayerCreatureId 或 ManagerCreatureId == PlayerCreatureId)
     */
    UFUNCTION(BlueprintCallable, Category = "Area|Query")
    TArray<AVoxelAreaBaseInstance*> GetPlayerTopLevelAreas(int32 PlayerCreatureId) const;

    /**
     * 查询所有"别人的"区域（Owner/Manager != PlayerCreatureId 且 != -1）
     * 用于无自己区域时的黑名单排除
     */
    UFUNCTION(BlueprintCallable, Category = "Area|Query")
    TArray<AVoxelAreaBaseInstance*> GetOtherPlayersAreas(int32 PlayerCreatureId) const;

    /** 获取全部顶层区域（ParentAreaId == INVALID_AREA_ID），按 AreaId 升序。
     *  UI 树/列表的「世界根层级」取数统一入口，禁止 UI 直接遍历 AreaIndex */
    UFUNCTION(BlueprintCallable, Category = "Area|Query")
    TArray<AVoxelAreaBaseInstance*> GetTopLevelAreas() const;

    /** 根据世界坐标获取所属区域 Actor（无匹配返回 nullptr） */
    UFUNCTION(BlueprintCallable, Category = "Area|Query")
    AVoxelAreaBaseInstance* GetAreaAtWorldPosition(const FVector& WorldPos) const;

    // =========================================================
    // [Lifecycle API] 生成与管理
    // =========================================================

    // 从存档数据生成所有区域
    UFUNCTION(BlueprintCallable, Category = "Area/Function")
    void GenerateAreas(FSceneData& SceneData);
    
    // 运行时动态生成新区域
    UFUNCTION(BlueprintCallable, Category = "Area/Function")
    AVoxelAreaBaseInstance* GenerateAreaInstance(const FAreaModificationData& ModificationData, AVoxelAreaBaseInstance* CurSelectedArea, AVoxelPlayerController* Controller);
    
    // 修改区域属性 (位置/大小/名称)
    UFUNCTION(BlueprintCallable, Category = "Area/Function")
    void UpdateAreaByPtr(AVoxelPlayerController* Controller, AVoxelAreaBaseInstance* Area, const FAreaModificationData& ModificationData);

    // 删除指定区域
    UFUNCTION(BlueprintCallable, Category = "Chunk/Function")
    void CleanAreaFromIndexById(int32 InputId);

    // =========================================================
    // [Helper API] 辅助与可视化
    // =========================================================

    UFUNCTION(BlueprintCallable, Category = "Area/Function")
    void InitializeAreaInstance(FAreaInstance& Input, int CreatorId) const;

    UFUNCTION(BlueprintCallable, Category = "Area/Function")
    bool IsAreaConflicting(FAreaInstance& Input) const;

    UFUNCTION(BlueprintCallable, Category = "Area/Function")
    void UpdateAreaCoverageById(int32 Id, const FBox& InputCoverage);

    UFUNCTION(BlueprintCallable, Category = "Area/Function")
    void UpdateAreaById(AVoxelPlayerController* Controller, int AreaId, const FAreaModificationData& ModificationData);
    // 类型管理
    UFUNCTION(BlueprintCallable, Category = "Area/Function")
    void UpdateAreaTypeAfterCheck(int32 AreaId, FGameplayTag InputAreaType);
    UFUNCTION(BlueprintCallable, Category = "Area/Function")
    void CheckAreaType(int32 AreaId, FGameplayTag InputAreaType);
    UFUNCTION(BlueprintCallable, Category = "Area/Function")
    void UpdateAreaType(int32 AreaId, FGameplayTag InputAreaType);
    UFUNCTION(BlueprintCallable, Category = "Chunk")
    TArray<AVoxelAreaBaseInstance*> FindAreasByType(FGameplayTag InputType) const;

    // 可视化
    UFUNCTION(BlueprintCallable, Category = "Chunk/Function")
    void UpdateAllAreasVisual(bool IsVisible);
    UFUNCTION(BlueprintCallable, Category = "Chunk/Function")
    void UpdateSubAreasVisual(AVoxelAreaBaseInstance* ParentArea, bool IsVisible, int32 CharId = -1);
    UFUNCTION(BlueprintCallable, Category = "Chunk/Function")
    void UpdateAreaVisual(AVoxelAreaBaseInstance* Area, bool IsVisible);

protected:
    UVoxelGameInstance* GameInstance;
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    // =========================================================
    // [Spatial Index] 空间索引私有逻辑
    // =========================================================

    /** 空间索引粒度 (16x16x16 的格子为一个索引单元) */
    static const int32 SPATIAL_CHUNK_SIZE = 16;

    /** * 空间哈希表
     * Key: 空间分块坐标 (BlockPos / 16)
     * Value: 该分块内包含的 Area ID 列表
     */
    TMap<FIntVector, TArray<int32>> ChunkToAreaMap;

    // 计算世界坐标属于哪个 Spatial Chunk
    FIntVector GetSpatialChunkCoord(FIntVector BlockPos) const;

    // 将区域注册到空间索引
    void RegisterAreaToSpatialMap(const AVoxelAreaBaseInstance* Area);

    // 从空间索引移除区域
    void UnregisterAreaFromSpatialMap(const AVoxelAreaBaseInstance* Area);

    // 重建整个空间索引
    void RebuildSpatialIndex();

    // =========================================================
    // [Internal Helpers] 内部辅助函数
    // =========================================================
    bool CheckModificationPermission(AVoxelPlayerController* Controller, AVoxelAreaBaseInstance* CheckArea) const;
    bool CheckGenerateAreaPermission(AVoxelPlayerController* Controller, const FAreaModificationData& Input) const;
    AVoxelAreaBaseInstance* CreateAreaInstance(const FTransform& Transform) const;
    bool SetupAreaInstance(AVoxelAreaBaseInstance* AreaInstance, const FAreaInstance& AreaInfo, AVoxelAreaBaseInstance* ParentArea) const;
    void FinalizeAreaInstance(AVoxelAreaBaseInstance* AreaInstance) const;
    void UpdateNavigationSystem(AVoxelAreaBaseInstance* AreaInstance) const;
    bool ValidateAreaInput(const FAreaInstance& AreaInfo) const;
    bool ValidateAreaBounds(const FBox& Coverage) const;
    bool IsAreaWithinParent(const FBox& Coverage, AVoxelAreaBaseInstance* ParentArea) const;
    int GenerateAreaId(FAreaModificationData& Input);
    FAreaInstance GenerateAreaInfo(const FAreaModificationData& Input);
    
    static const int32 INVALID_AREA_ID = -1;
};
