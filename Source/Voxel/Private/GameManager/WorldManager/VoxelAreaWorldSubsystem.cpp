#include "VoxelAreaWorldSubsystem.h"
#include "VoxelBlockWorldSubsystem.h"
#include "Core/VoxelGameInstance.h"
#include "Core/VoxelSceneSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/ChunkUtilsFunction.h"
#include "NavigationSystem.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Voxel/VoxelCharacter.h"
#include "Voxel/VoxelPlayerController.h"

// =========================================================
// [Lifecycle] 初始化与清理
// =========================================================

void UVoxelAreaWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    GameInstance = Cast<UVoxelGameInstance>(GetWorld()->GetGameInstance());
    if (!GameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get VoxelGameInstance in AreaWorldSubsystem"));
    }
}

void UVoxelAreaWorldSubsystem::Deinitialize()
{
    // 清理所有 Actor
    for (const auto& Pair : AreaIndex)
    {
        if (Pair.Value && Pair.Value->IsValidLowLevel())
        {
            Pair.Value->Destroy();
        }
    }
    
    // 清空所有容器
    AreaIndex.Empty();
    SpawnArea.Empty();
    ConnectAreaIndex.Empty();
    ChunkToAreaMap.Empty(); // [清理] 空间索引
    
    Super::Deinitialize();
}

// =========================================================
// [Spatial Hashing] 空间索引核心实现
// =========================================================

FIntVector UVoxelAreaWorldSubsystem::GetSpatialChunkCoord(FIntVector BlockPos) const
{
    // 向下取整除法，确保负坐标正确归类
    return FIntVector(
        FMath::FloorToInt((float)BlockPos.X / SPATIAL_CHUNK_SIZE),
        FMath::FloorToInt((float)BlockPos.Y / SPATIAL_CHUNK_SIZE),
        FMath::FloorToInt((float)BlockPos.Z / SPATIAL_CHUNK_SIZE)
    );
}

void UVoxelAreaWorldSubsystem::RegisterAreaToSpatialMap(const AVoxelAreaBaseInstance* Area)
{
    if (!Area) return;

    // 计算区域覆盖的 Spatial Chunk 范围（MaxIndex 为闭区间，需 +1 以包含完整体素）
    const FBox CoverageBox = Area->AreaBase.AreaCoverage.BoundingBox();
    FIntVector MinC = GetSpatialChunkCoord(FIntVector(CoverageBox.Min));
    FIntVector MaxC = GetSpatialChunkCoord(FIntVector(CoverageBox.Max));
    int32 Id = Area->AreaBase.AreaId;

    // 遍历覆盖的所有 Chunk，将 ID 写入列表
    for (int z = MinC.Z; z <= MaxC.Z; z++) {
        for (int y = MinC.Y; y <= MaxC.Y; y++) {
            for (int x = MinC.X; x <= MaxC.X; x++) {
                ChunkToAreaMap.FindOrAdd(FIntVector(x, y, z)).AddUnique(Id);
            }
        }
    }
}

void UVoxelAreaWorldSubsystem::UnregisterAreaFromSpatialMap(const AVoxelAreaBaseInstance* Area)
{
    if (!Area) return;

    const FBox CoverageBox = Area->AreaBase.AreaCoverage.BoundingBox();
    FIntVector MinC = GetSpatialChunkCoord(FIntVector(CoverageBox.Min));
    FIntVector MaxC = GetSpatialChunkCoord(FIntVector(CoverageBox.Max));
    int32 Id = Area->AreaBase.AreaId;

    for (int z = MinC.Z; z <= MaxC.Z; z++) {
        for (int y = MinC.Y; y <= MaxC.Y; y++) {
            for (int x = MinC.X; x <= MaxC.X; x++) {
                if (TArray<int32>* List = ChunkToAreaMap.Find(FIntVector(x, y, z))) {
                    List->Remove(Id);
                    // 如果该 Chunk 没有区域了，移除 Key 以节省内存
                    if (List->Num() == 0) ChunkToAreaMap.Remove(FIntVector(x, y, z));
                }
            }
        }
    }
}

void UVoxelAreaWorldSubsystem::RebuildSpatialIndex()
{
    ChunkToAreaMap.Empty();
    UE_LOG(LogTemp, Log, TEXT("Rebuilding Area Spatial Index..."));
    
    for (const auto& Pair : AreaIndex)
    {
        if (Pair.Value && Pair.Value->IsValidLowLevel())
        {
            RegisterAreaToSpatialMap(Pair.Value);
        }
    }
}

// =========================================================
// [Query API] 空间查询
// =========================================================

int32 UVoxelAreaWorldSubsystem::GetAreaIndexByPosition(FIntVector BlockPos) const
{
    // 1. 快速定位到 Spatial Chunk (Broad Phase)
    FIntVector ChunkCoord = GetSpatialChunkCoord(BlockPos);
    
    // 2. 获取候选区域列表
    const TArray<int32>* Candidates = ChunkToAreaMap.Find(ChunkCoord);
    if (!Candidates) return INVALID_AREA_ID;

    AVoxelAreaBaseInstance* BestMatch = nullptr;
    int64 MinVolume = MAX_int64;

    // 3. 遍历候选，精确检测 + 择优 (Narrow Phase)
    for (int32 AreaId : *Candidates)
    {
        AVoxelAreaBaseInstance* const* FoundPtr = AreaIndex.Find(AreaId);
        if (!FoundPtr) continue;
        AVoxelAreaBaseInstance* Area = *FoundPtr;

        // 调用 Area 自身的几何判断 (支持 Box/Sphere 混合逻辑)
        // 注意：IsPointInArea 需要在 AVoxelAreaBaseInstance 中实现
        if (Area && Area->IsPointInArea(BlockPos))
        {
            // 计算体积 (长x宽x高)
            FIntVector Size = Area->AreaBase.AreaCoverage.MaxIndex - Area->AreaBase.AreaCoverage.MinIndex;
            int64 Volume = (int64)FMath::Abs(Size.X) * FMath::Abs(Size.Y) * FMath::Abs(Size.Z);

            // 优先选择体积更小的 (层级逻辑：子区域优先于父区域)
            if (Volume < MinVolume)
            {
                MinVolume = Volume;
                BestMatch = Area;
            }
        }
    }
    
    return BestMatch ? BestMatch->AreaBase.AreaId : INVALID_AREA_ID;
}

AVoxelAreaBaseInstance* UVoxelAreaWorldSubsystem::GetAreaActor(int32 AreaId) const
{
    if (AVoxelAreaBaseInstance* const* Ptr = AreaIndex.Find(AreaId)) return *Ptr;
    return nullptr;
}

TArray<AVoxelAreaBaseInstance*> UVoxelAreaWorldSubsystem::GetPlayerTopLevelAreas(int32 PlayerCreatureId) const
{
    TArray<AVoxelAreaBaseInstance*> Result;
    for (const auto& Pair : AreaIndex)
    {
        AVoxelAreaBaseInstance* Area = Pair.Value;
        if (!Area) continue;
        // 无父级（顶级区域）：ParentAreaId == INVALID_AREA_ID（-1）
        if (Area->AreaBase.ParentAreaId != INVALID_AREA_ID) continue;
        // Owner 或 Manager 匹配
        const bool bOwnerMatch = Area->AreaBase.OwnerCreatureId == PlayerCreatureId;
        const bool bManagerMatch = Area->AreaBase.ManagerCreatureId == PlayerCreatureId;
        if (bOwnerMatch || bManagerMatch)
        {
            Result.Add(Area);
        }
    }
    return Result;
}

TArray<AVoxelAreaBaseInstance*> UVoxelAreaWorldSubsystem::GetOtherPlayersAreas(int32 PlayerCreatureId) const
{
    TArray<AVoxelAreaBaseInstance*> Result;
    for (const auto& Pair : AreaIndex)
    {
        AVoxelAreaBaseInstance* Area = Pair.Value;
        if (!Area) continue;
        // 排除无主区域（Owner == -1 且 Manager == -1）
        const int32 Owner = Area->AreaBase.OwnerCreatureId;
        const int32 Manager = Area->AreaBase.ManagerCreatureId;
        if (Owner == -1 && Manager == -1) continue;
        // 排除自己的区域
        const bool bOwnerMine = Owner == PlayerCreatureId;
        const bool bManagerMine = Manager == PlayerCreatureId;
        if (bOwnerMine || bManagerMine) continue;
        // 剩余即为"别人的"区域
        Result.Add(Area);
    }
    return Result;
}

TArray<AVoxelAreaBaseInstance*> UVoxelAreaWorldSubsystem::GetTopLevelAreas() const
{
    TArray<AVoxelAreaBaseInstance*> Result;
    for (const auto& Pair : AreaIndex)
    {
        AVoxelAreaBaseInstance* Area = Pair.Value;
        if (!Area) continue;
        // 顶级区域判定与 GetPlayerTopLevelAreas 同源：ParentAreaId == INVALID_AREA_ID
        if (Area->AreaBase.ParentAreaId != INVALID_AREA_ID) continue;
        Result.Add(Area);
    }
    Result.Sort([](const AVoxelAreaBaseInstance& A, const AVoxelAreaBaseInstance& B)
    {
        return A.AreaBase.AreaId < B.AreaBase.AreaId;
    });
    return Result;
}

AVoxelAreaBaseInstance* UVoxelAreaWorldSubsystem::GetAreaAtWorldPosition(const FVector& WorldPos) const
{
    // 世界坐标 → 体素索引
    // BlockLength 从 GameInstance 获取
    float BlockLength = 20.0f;
    if (GameInstance)
    {
        BlockLength = GameInstance->GetBlockLength();
    }
    const FIntVector BlockPos(
        FMath::FloorToInt(WorldPos.X / BlockLength),
        FMath::FloorToInt(WorldPos.Y / BlockLength),
        FMath::FloorToInt(WorldPos.Z / BlockLength)
    );
    const int32 AreaId = GetAreaIndexByPosition(BlockPos);
    return AreaId != INVALID_AREA_ID ? GetAreaActor(AreaId) : nullptr;
}

// =========================================================
// [Generation] 生成逻辑 (已集成 Spatial Hash)
// =========================================================

void UVoxelAreaWorldSubsystem::GenerateAreas(FSceneData& SceneData)
{
    if (!GameInstance) return;
    
    // 1. 清理旧数据
    if(!AreaIndex.IsEmpty())
    {
        for (auto& Pair : AreaIndex)
        {
            if (Pair.Value && Pair.Value->IsValidLowLevel()) Pair.Value->Destroy();
        }
        AreaIndex.Empty();
        SpawnArea.Empty();
        ConnectAreaIndex.Empty();
        ChunkToAreaMap.Empty(); // [清理] 索引
    }

    if (SceneData.Areas.IsEmpty()) return;

    const int32 BlockLength = GameInstance->WorldConfig.BlockLength;
    AreaIndex.Reserve(SceneData.Areas.Num());
    
    // 2. 循环生成区域 Actor
    for (const auto& AreaPair : SceneData.Areas)
    {
        const FAreaInstance& AreaData = AreaPair.Value;
        const FTransform Transform(FRotator::ZeroRotator, FVector(AreaData.AreaCoverage.MinIndex * BlockLength), FVector::OneVector);
        
        AVoxelAreaBaseInstance* AreaInstance = CreateAreaInstance(Transform);
        if (!AreaInstance) continue;
        
        SetupAreaInstance(AreaInstance, AreaData, nullptr);
        
        AreaInstance->UpdateAreaBorderByPoints(AreaData.AreaCoverage.MinIndex, AreaData.AreaCoverage.MaxIndex);
        // AreaInstance->UpdateAreaTypeFunc(); // 如有需要启用
        
        AreaIndex.Add(AreaInstance->AreaBase.AreaId, AreaInstance);
        FinalizeAreaInstance(AreaInstance);
        CheckAreaType(AreaInstance->AreaBase.AreaId, AreaInstance->AreaBase.Function.AreaType);
    }

    // 3. [新增] 批量建立空间索引
    RebuildSpatialIndex();
    
    UE_LOG(LogTemp, Log, TEXT("Areas Generated. Spatial Index Built. Total Areas: %d"), AreaIndex.Num());
}

AVoxelAreaBaseInstance* UVoxelAreaWorldSubsystem::GenerateAreaInstance(const FAreaModificationData& ModificationData, AVoxelAreaBaseInstance* CurSelectedArea, AVoxelPlayerController* Controller)
{
    UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 3] GenerateAreaInstance 进入"));

    if(!CheckGenerateAreaPermission(Controller, ModificationData))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CreateArea] [Step 3.1] 早退：CheckGenerateAreaPermission 失败 (Type=%s)"),
            *ModificationData.AreaType.ToString());
        return nullptr;
    }
    UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 3.1] 权限校验通过"));
    
    FAreaInstance AreaInfo = GenerateAreaInfo(ModificationData);
    UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 3.2] 已生成 AreaInfo, AreaId=%d, ParentAreaId=%d"),
        AreaInfo.AreaId, AreaInfo.ParentAreaId);

    if (!ValidateAreaInput(AreaInfo))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CreateArea] [Step 3.3] 早退：ValidateAreaInput 失败 (AreaName=%s)"),
            *AreaInfo.AreaName.ToString());
        return nullptr;
    }
    UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 3.3] ValidateAreaInput 通过"));

    if (IsAreaConflicting(AreaInfo))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CreateArea] [Step 3.4] 早退：IsAreaConflicting 检测到冲突"));
        return nullptr;
    }
    UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 3.4] IsAreaConflicting 通过"));

    const UVoxelBlockWorldSubsystem* BlockSubsystem = GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>();
    if (!BlockSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[CreateArea] [Step 3.5] 早退：BlockWorldSubsystem 为空"));
        return nullptr;
    }
    
    // 边界计算
    const int32 BlockLength = GameInstance->WorldConfig.BlockLength;
    const FBox WorldBorder = BlockSubsystem->GetWorldBorder();
    const FBox Coverage = UChunkUtilsFunction::ChangeArea(
        static_cast<FIntVector>(ModificationData.AreaCoverage.MinIndex),
        static_cast<FIntVector>(ModificationData.AreaCoverage.MaxIndex),
        WorldBorder
    );
    UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 3.5] ChangeArea 夹紧后 Coverage Min=%s, Max=%s"),
        *Coverage.Min.ToString(), *Coverage.Max.ToString());
    
    if (!ValidateAreaBounds(Coverage))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CreateArea] [Step 3.6] 早退：ValidateAreaBounds 失败 (Min>Max)"));
        return nullptr;
    }
    UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 3.6] ValidateAreaBounds 通过"));

    // 将夹紧/正序化后的边界回写到 AreaInfo，保证实例 Coverage 与实际生成一致
    AreaInfo.AreaCoverage.SetBoundingBox(FIntVector(Coverage.Min), FIntVector(Coverage.Max));
    
    // 生成实例
    const FTransform Transform(FRotator::ZeroRotator, FVector(Coverage.Min * BlockLength), FVector::OneVector);
    AVoxelAreaBaseInstance* AreaInstance = CreateAreaInstance(Transform);
    if (!AreaInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("[CreateArea] [Step 3.7] 早退：CreateAreaInstance 返回 nullptr (DefaultAreaType 未配置?)"));
        return nullptr;
    }
    UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 3.7] SpawnActor 完成"));

    // 检查父子关系
    if (CurSelectedArea && !IsAreaWithinParent(Coverage, CurSelectedArea))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CreateArea] [Step 3.8] 早退：IsAreaWithinParent 失败，已 Destroy 新 Actor (ParentId=%d)"),
            CurSelectedArea->AreaBase.AreaId);
        AreaInstance->Destroy();
        return nullptr;
    }
    UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 3.8] 父子关系校验通过"));
    
    if (!SetupAreaInstance(AreaInstance, AreaInfo, CurSelectedArea))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CreateArea] [Step 3.9] 早退：SetupAreaInstance 失败（AreaInfo 写入被拒），已 Destroy 新 Actor"));
        AreaInstance->Destroy();
        return nullptr;
    }
    FinalizeAreaInstance(AreaInstance);
    UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 3.9] Setup + Finalize 完成"));
    
    AreaIndex.Add(AreaInstance->AreaBase.AreaId, AreaInstance);
    CheckAreaType(AreaInstance->AreaBase.AreaId, AreaInstance->AreaBase.Function.AreaType);
    // [新增] 注册新区域到空间索引
    RegisterAreaToSpatialMap(AreaInstance);
    UE_LOG(LogTemp, Log, TEXT("[CreateArea] [Step 3.10] 注册到 AreaIndex / SpatialMap, 创建成功 AreaId=%d"),
        AreaInstance->AreaBase.AreaId);
    return AreaInstance;
}

// =========================================================
// [Update/Delete] 修改与删除 (已集成 Spatial Hash)
// =========================================================

void UVoxelAreaWorldSubsystem::CleanAreaFromIndexById(int32 InputId)
{
    if (AVoxelAreaBaseInstance** AreaPtr = AreaIndex.Find(InputId))
    {
        // [新增] 销毁前移除索引
        UnregisterAreaFromSpatialMap(*AreaPtr);

        if (*AreaPtr && (*AreaPtr)->IsValidLowLevel())
        {
            (*AreaPtr)->Destroy();
        }
        AreaIndex.Remove(InputId);
        SpawnArea.Remove(InputId);
    }
}

void UVoxelAreaWorldSubsystem::UpdateAreaByPtr(AVoxelPlayerController* Controller, AVoxelAreaBaseInstance* Area, const FAreaModificationData& ModificationData)
{
    if(!CheckModificationPermission(Controller, Area)) return;
    if (!Area || !Area->IsValidLowLevel()) return;
    
    // 因为边界可能变化，先移除旧索引
    UnregisterAreaFromSpatialMap(Area);

    // 更新属性（包含名称、类型、状态、权属、覆盖范围）
    int32 AreaId = Area->AreaBase.AreaId;
    AreaIndex.FindOrAdd(AreaId, Area);
    
    if (GameInstance)
    {
        Area->UpdateBaseAreaInfo(ModificationData, Controller ? Controller->PlayerId : -1);
        UpdateAreaTypeAfterCheck(AreaId, ModificationData.AreaType);
    }

    // 重新注册新边界到索引
    RegisterAreaToSpatialMap(Area);
}

// =========================================================
// [Helpers] 其他辅助函数
// =========================================================

bool UVoxelAreaWorldSubsystem::CheckModificationPermission(AVoxelPlayerController* Controller, AVoxelAreaBaseInstance* CheckArea) const
{
    switch(Controller->StateCacheComp->GetGameAuthority())
    {
    case EVoxelGameAuthority::Player:
        if(!CheckArea->AreaBase.IsShop() && !CheckArea->AreaBase.IsSubFunction()) return false;
        if(!CheckArea->AreaBase.IsOwner(Controller->PlayerId) && !CheckArea->IsAreaEditable(Controller->PlayerId)) return false;
    default: break;
    }
    return true;
}

bool UVoxelAreaWorldSubsystem::CheckGenerateAreaPermission(AVoxelPlayerController* Controller, const FAreaModificationData& Input) const
{
    if (Controller->StateCacheComp->GetGameAuthority() == EVoxelGameAuthority::Player)
    {
        // 新流程：区域工具只提交几何数据，类型为空（未分类）时允许创建，
        // 类型限制在实体详情分配类型时再校验。
        if (!Input.AreaType.IsValid())
        {
            return true;
        }
        bool bIsShopOrSub = Input.AreaType.MatchesTag(FCoreTags::Area_Type_Shop) ||
                            Input.AreaType.MatchesTag(FCoreTags::Area_Type_SubFunction);
        if (!bIsShopOrSub) return false;
    }
    return true;
}

void UVoxelAreaWorldSubsystem::UpdateAllAreasVisual(const bool IsVisible)
{
    for (const auto& Pair : AreaIndex)
    {
        if (Pair.Value && Pair.Value->IsValidLowLevel())
        {
            Pair.Value->UpdateAreaVisual(IsVisible, false);
        }
    }
}

void UVoxelAreaWorldSubsystem::UpdateSubAreasVisual(AVoxelAreaBaseInstance* ParentArea, const bool IsVisible, int32 CharId)
{
    UpdateAllAreasVisual(false); // 先隐藏所有
    
    if (!ParentArea || !ParentArea->IsValidLowLevel())
    {
        // ROOT 状态：显示所有一级区域（ParentArea == nullptr）
        for (const auto& Pair : AreaIndex)
        {
            if (Pair.Value && Pair.Value->IsValidLowLevel() && Pair.Value->ParentArea == nullptr)
            {
                bool bReadOnly = !Pair.Value->IsAreaEditable(CharId);
                Pair.Value->UpdateAreaVisual(IsVisible, false, bReadOnly);
            }
        }
        return;
    }
    
    // 显示父区域自身
    ParentArea->UpdateAreaVisual(IsVisible, false);
    // 显示其子区域
    for (const auto& Pair : ParentArea->SubAreasIndex)
    {
        if (Pair.Value && Pair.Value->IsValidLowLevel())
        {
            bool bReadOnly = !Pair.Value->IsAreaEditable(CharId);
            Pair.Value->UpdateAreaVisual(IsVisible, false, bReadOnly);
        }
    }
}

void UVoxelAreaWorldSubsystem::UpdateAreaVisual(AVoxelAreaBaseInstance* Area, const bool IsVisible)
{
    if (Area && Area->IsValidLowLevel())
    {
        Area->UpdateAreaVisual(IsVisible, false);
    }
}

AVoxelAreaBaseInstance* UVoxelAreaWorldSubsystem::CreateAreaInstance(const FTransform& Transform) const
{
    if (!DefaultAreaType) return nullptr;
    return GetWorld()->SpawnActor<AVoxelAreaBaseInstance>(DefaultAreaType, Transform);
}

bool UVoxelAreaWorldSubsystem::SetupAreaInstance(AVoxelAreaBaseInstance* AreaInstance, const FAreaInstance& AreaInfo, AVoxelAreaBaseInstance* ParentArea) const
{
    if (!AreaInstance) return false;
    
    // AreaInfo 写入失败（如 AreaType 无效）时返回 false，由调用方销毁实例
    if (!AreaInstance->GenerateAreaInfo(AreaInfo)) return false;
    
    if (ParentArea)
    {
        AreaInstance->AreaBase.ParentAreaId = ParentArea->AreaBase.AreaId;
        AreaInstance->ParentArea = ParentArea;
        ParentArea->SubAreasIndex.Add(AreaInstance->AreaBase.AreaId, AreaInstance);
    }
    else
    {
        AreaInstance->AreaBase.ParentAreaId = INVALID_AREA_ID;
        AreaInstance->ParentArea = nullptr;
    }
    return true;
}

void UVoxelAreaWorldSubsystem::FinalizeAreaInstance(AVoxelAreaBaseInstance* AreaInstance) const
{
    if (AreaInstance)
    {
        AreaInstance->UpdateAreaColor(FColor(FMath::Rand() % 256, FMath::Rand() % 256, FMath::Rand() % 256, 200));
        UpdateNavigationSystem(AreaInstance);
        AreaInstance->UpdateAreaVisual(true, false);
    }
}

void UVoxelAreaWorldSubsystem::UpdateNavigationSystem(AVoxelAreaBaseInstance* AreaInstance) const
{
    if (AreaInstance && GetWorld())
    {
        if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetNavigationSystem(GetWorld()))
        {
            NavSystem->UpdateActorAndComponentsInNavOctree(*AreaInstance, true);
        }
    }
}

void UVoxelAreaWorldSubsystem::InitializeAreaInstance(FAreaInstance& Input, const int CreatorId) const
{
    Input.Function.Status = EAreaStatus::Planning;
    if(UConfigDataSubsystem* Cfg = GameInstance->GetSubsystem<UConfigDataSubsystem>())
    {
        if(const FAreaTypeTemplate* a = Cfg->GetAreaTypeTemplateById(Input.Function.AreaType))
        {
            Input.Authority = a->Authority;
        }
    }
    // 负值 = 未指派，补 CreatorId；0 = 测试主角（合法 ID，保留）
    if (Input.OwnerCreatureId < 0)
    {
        Input.OwnerCreatureId = CreatorId;
    }
    if (Input.ManagerCreatureId < 0)
    {
        Input.ManagerCreatureId = CreatorId;
    }
}

bool UVoxelAreaWorldSubsystem::ValidateAreaInput(const FAreaInstance& AreaInfo) const
{
    return !AreaInfo.AreaName.IsNone();
}

bool UVoxelAreaWorldSubsystem::ValidateAreaBounds(const FBox& Coverage) const
{
    return !(Coverage.Min.X > Coverage.Max.X || Coverage.Min.Y > Coverage.Max.Y || Coverage.Min.Z > Coverage.Max.Z);
}

bool UVoxelAreaWorldSubsystem::IsAreaWithinParent(const FBox& Coverage, AVoxelAreaBaseInstance* ParentArea) const
{
    if (!ParentArea) return false;
    return ParentArea->AreaBase.IsBoxInArea(Coverage);
}

void UVoxelAreaWorldSubsystem::UpdateAreaCoverageById(const int32 Id, const FBox& InputCoverage)
{
    if (AVoxelAreaBaseInstance** AreaPtr = AreaIndex.Find(Id))
    {
        if (*AreaPtr && (*AreaPtr)->IsValidLowLevel())
        {
            UnregisterAreaFromSpatialMap(*AreaPtr);
            (*AreaPtr)->UpdateAreaBorderByBox(InputCoverage);
            RegisterAreaToSpatialMap(*AreaPtr);
        }
    }
}

void UVoxelAreaWorldSubsystem::UpdateAreaById(AVoxelPlayerController* Controller, const int AreaId, const FAreaModificationData& ModificationData)
{
    if (AVoxelAreaBaseInstance** AreaPtr = AreaIndex.Find(AreaId))
    {
        if (*AreaPtr) UpdateAreaByPtr(Controller, *AreaPtr, ModificationData);
    }
}

void UVoxelAreaWorldSubsystem::UpdateAreaTypeAfterCheck(const int32 AreaId, const FGameplayTag InputAreaType)
{
    if (AVoxelAreaBaseInstance** AreaPtr = AreaIndex.Find(AreaId))
    {
        if (*AreaPtr && (*AreaPtr)->AreaBase.Function.AreaType != InputAreaType)
        {
            UpdateAreaType(AreaId, InputAreaType);
        }
    }
}

void UVoxelAreaWorldSubsystem::CheckAreaType(const int32 AreaId, FGameplayTag InputAreaType)
{
    // 1. 获取区域实例
    AVoxelAreaBaseInstance** AreaPtr = AreaIndex.Find(AreaId);
    if (!AreaPtr || !(*AreaPtr)) return;

    // 2. 判断是否是出生点类型 (假设你有 FCoreTags::Area_Type_Spawn)
    if (InputAreaType.MatchesTag(FCoreTags::Area_Type_Spawn))
    {
        // 注册到 SpawnArea 缓存
        SpawnArea.Add(AreaId, *AreaPtr);
        UE_LOG(LogTemp, Log, TEXT("Area %d registered as Spawn Area"), AreaId);
    }
    else
    {
        // 关键：如果类型变更（例如从出生点变成了商店），必须从 SpawnArea 移除
        if (SpawnArea.Contains(AreaId))
        {
            SpawnArea.Remove(AreaId);
        }
    }
}

void UVoxelAreaWorldSubsystem::UpdateAreaType(const int32 AreaId, const FGameplayTag InputAreaType)
{
    if (AVoxelAreaBaseInstance** AreaPtr = AreaIndex.Find(AreaId))
    {
        (*AreaPtr)->UpdateCurAreaType(InputAreaType);
        CheckAreaType(AreaId, InputAreaType);
    }
}

TArray<AVoxelAreaBaseInstance*> UVoxelAreaWorldSubsystem::FindAreasByType(FGameplayTag InputType) const
{
    TArray<AVoxelAreaBaseInstance*> Result;
    for (const auto& Pair : AreaIndex)
    {
        if (Pair.Value && Pair.Value->AreaBase.Function.AreaType == InputType)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

bool UVoxelAreaWorldSubsystem::IsAreaConflicting(FAreaInstance& Input) const
{
    UE_LOG(LogTemp, Verbose, TEXT("Checking area conflict for parent ID: %d"), Input.ParentAreaId);
    
    // 如果没有区域，则没有冲突
    if (AreaIndex.IsEmpty())
    {
        return false;
    }
    // 缓存 ConfigDataSubsystem 以避免重复获取
    UConfigDataSubsystem* ConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UConfigDataSubsystem>() : nullptr;
    if (!ConfigSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get UConfigDataSubsystem from GameInstance"));
        return false;
    }
    // 获取输入区域类型并检查有效性
    const FAreaTypeBase* InputAreaType = ConfigSubsystem->GetAreaTypeByTag(Input.Function.AreaType);
    if (!InputAreaType)
    {
        UE_LOG(LogTemp, Warning, TEXT("Input area type is invalid for AreaTypeId: %s"), *Input.Function.AreaType.ToString());
        return false;
    }
    TArray<AVoxelAreaBaseInstance*> CheckAreas;
    // 提前计算输入区域的边界框，避免在循环中重复计算
    const FBox InputBoundingBox = Input.AreaCoverage.BoundingBox();
    if (Input.ParentAreaId <= 0)
    {
        // 如果没有父区域，检查所有区域
        AreaIndex.GenerateValueArray(CheckAreas);
    }
    else
    {
        // 如果有父区域，检查是否存在于索引中
        if (auto a = AreaIndex.Find(Input.ParentAreaId))
        {
            AVoxelAreaBaseInstance* ParentArea = *a;
            if (!ParentArea)
            {
                UE_LOG(LogTemp, Warning, TEXT("Parent area with ID %d is null"), Input.ParentAreaId);
                return false;
            }
            // 检查输入区域是否完全在父区域内
            if (!ParentArea->AreaBase.AreaCoverage.BoundingBox().IsInsideOrOn(InputBoundingBox))
            {
                UE_LOG(LogTemp, Warning, TEXT("Area conflict detected: Input area is not fully inside parent area ID: %d"), Input.ParentAreaId);
                return true;
            }
            // 获取父区域的子区域进行检查
            ParentArea->SubAreasIndex.GenerateValueArray(CheckAreas);
        }
    }
    
    // 遍历所有待检查区域
    for (AVoxelAreaBaseInstance* OtherArea : CheckAreas)
    {
        if (!OtherArea)
        {
            continue; // 跳过空指针
        }
        FAreaInstance& OtherAreaBase = OtherArea->AreaBase;
        const FAreaTypeBase* OtherAreaType = ConfigSubsystem->GetAreaTypeByTag(OtherAreaBase.Function.AreaType);
        if (!OtherAreaType)
        {
            continue; // 跳过无效区域类型
        }
        // 处理抽象类型：只有相同类型ID的抽象区域才检查冲突
        if (InputAreaType->bAbstract || OtherAreaType->bAbstract)
        {
            if (!UChunkUtilsFunction::CompareTagAtLevel(InputAreaType->AreaTypeTag, OtherAreaType->AreaTypeTag,3))
            {
                continue; // 类型ID不同，跳过冲突检查
            }
        }
        UE_LOG(LogTemp, Verbose, TEXT("Checking conflict with area ID: %d"), OtherAreaBase.AreaId);
        
        // 检查边界框是否相交
        if (OtherAreaBase.AreaCoverage.BoundingBox().Intersect(InputBoundingBox))
        {
            UE_LOG(LogTemp, Warning, TEXT("Area conflict detected with area ID: %d"), OtherAreaBase.AreaId);
            return true;
        }
    }
    
    return false;
}

int UVoxelAreaWorldSubsystem::GenerateAreaId(FAreaModificationData& Input)
{
    int32 Timestamp = FDateTime::Now().ToUnixTimestamp() % 1000000;
    int32 RandomPart = FMath::RandRange(0, 999); 
    uint32 TagHash = GetTypeHash(Input.AreaType);
    int32 TypePart = TagHash % 1000;

    int32 Id = (Timestamp * 1000 + RandomPart) * 10000; 
    Id += (Input.ParentScenceId % 10) * 1000; 
    Id += TypePart;
    UE_LOG(LogTemp, Warning, TEXT("GenerateAreaId,%d"),Id );
    return FMath::Abs(Id);
}

FAreaInstance UVoxelAreaWorldSubsystem::GenerateAreaInfo(const FAreaModificationData& Input)
{
    UE_LOG(LogTemp, Warning, TEXT("GenerateAreaInfo"));
    FAreaInstance Result = {};
    Result.Function.AreaType = Input.AreaType;
    Result.Function.Status = Input.Status;
    Result.AreaName = Input.AreaName;
    Result.AreaId = GenerateAreaId(const_cast<FAreaModificationData&>(Input));
    Result.ParentAreaId = Input.ParentAreaId;
    Result.ParentScenceId = Input.ParentScenceId;
    Result.AreaCoverage = Input.AreaCoverage;
    Result.OwnerCreatureId = Input.OwnerCreatureId;
    Result.ManagerCreatureId = Input.ManagerCreatureId;
    return Result;
}
