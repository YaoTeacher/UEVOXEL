# VoxelBlockWorldSubsystem-架构设计

**版本**: 1.0
**创建日期**: 2026-01-24
**最后更新**: 2026-01-24

---

## 1. 组件架构

### 1.1 整体架构层次

```
┌─────────────────────────────────────────────────────────────┐
│ 用户交互层（User Interaction）                               │
│ - VoxelPlayerController                                    │
│   - VoxelControllerInteractComponent（编辑协调）           │
│   - VoxelControllerStateComponent（状态管理）           │
│   - VoxelControllerVisualUIComponent（UI更新）            │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 接口层（Interface Layer）                                   │
│ - FSingleEditRequest（单体编辑请求）                        │
│ - FVoxelEditModification（批量编辑模型）                   │
│ - FChunkLoadRequest（区块加载请求）                         │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 核心子系统层（Core Subsystem）                              │
│ ┌──────────────────────────────────────────────┐          │
│ │ 数据管理层（Data Management）                 │          │
│ │ ├── ChunkMetaDataMap（区块元数据）           │          │
│ │ ├── CurSceneBlockTypeMask（运行时修改）      │          │
│ │ └── SceneBaseBlockTypeMask（基础地形）      │          │
│ │                                              │          │
│ │ 对象池集成（Pool Integration）               │          │
│ │ └── LoadedChunks（区块Actor映射）            │          │
│ └──────────────────────────────────────────────┘          │
│ ┌──────────────────────────────────────────────┐          │
│ │ 任务队列层（Task Queues）                    │          │
│ │ ├── LoadingQueue（数据加载）                 │          │
│ │ ├── GeneratingQueue（内容生成）             │          │
│ │ ├── CalculateQueue（网格计算）              │          │
│ │ ├── RenderingQueue（渲染更新）              │          │
│ │ └── UnloadingQueue（内存卸载）              │          │
│ └──────────────────────────────────────────────┘          │
│ ┌──────────────────────────────────────────────┐          │
│ │ 线程安全层（Thread Safety）                 │          │
│ │ ├── FRWLock（读写锁）                        │          │
│ │ ├── FCriticalSection（任务锁）              │          │
│ │ └── FChunkGenContext（快照机制）            │          │
│ └──────────────────────────────────────────────┘          │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 执行层（Execution Layer）                                   │
│ - Tick处理函数                                              │
│ - 队列处理函数                                              │
│ - 异步任务回调                                              │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 子系统组件职责

#### **数据管理层**

**ChunkMetaDataMap**
- 存储所有区块的元数据（`TMap<FIntVector, FChunkData>`）
- 包含区块状态、生成进度、资源占用信息
- 只在游戏线程修改，后台线程只读

**双层存储系统**
- **SceneBaseBlockTypeMask**：基础地形（只读，程序化生成）
- **CurSceneBlockTypeMask**：玩家修改（读写，支持编辑）
- 查询时优先返回修改层，不存在则返回基础层

**LoadedChunks**
- 管理已加载的区块Actor实例
- 使用 `TWeakObjectPtr<AChunkBase>` 避免循环引用
- 与对象池子系统协同工作

#### **任务队列层**

**LoadingQueue**
- 待加载区块数据
- 优先级：EChunkLoadPriority
- 处理函数：ProcessLoading()

**GeneratingQueue**
- 待生成内容的区块
- 从数据到Actor的转换
- 处理函数：ProcessGenerating()

**CalculateQueue**
- 待计算网格的区块
- 使用 FChunkGenContext 快照
- 处理函数：ProcessCalculateUpdates()

**RenderingQueue**
- 待渲染更新的区块
- 网格数据最终应用
- 处理函数：ProcessRenderUpdates()

**UnloadingQueue**
- 待卸载的区块
- 释放内存和资源
- 处理函数：ProcessUnloading()

#### **线程安全层**

**FRWLock（读写锁）**
- 保护 `CurSceneBlockTypeMask` 的读写
- 读操作使用 `FReadScopeLock`
- 写操作使用 `FWriteScopeLock`

**FCriticalSection（任务锁）**
- 保护队列和待处理任务
- 防止多线程同时修改队列结构

**FChunkGenContext**
- 网格生成时的数据快照
- 包含邻居区块信息（PaddedSize）
- 后台线程无锁访问

---

## 2. 接口定义

### 2.1 核心编辑接口

#### **单体编辑接口**

```cpp
/**
 * 修改单个体素数据
 * @param BlockIndex 世界坐标方块索引
 * @param CurBatchMode 编辑模式（新增/删除/替换）
 * @param TypeID 目标方块类型
 */
UFUNCTION(BlueprintCallable, Category = "WBS|Function")
void ModifyVoxelData(FIntVector BlockIndex, EVoxelControllerEditState CurBatchMode, FGameplayTag TypeID);
```

#### **批量编辑接口**

```cpp
/**
 * 批量修改体素数据（使用新的修改模型）
 * @param Modification 修改模型（包含 BlockIndex→BlockType 映射、ChunkIndices、EditMode）
 */
void BatchModifyVoxelData_Modification(const FVoxelEditModification& Modification);

/**
 * 批量修改体素数据（接收完整的编辑请求）
 * @param SingleEditRequest 单次编辑请求（包含 BlockIndex、ChunkIndex、EditMode、NewType）
 */
void BatchModifyVoxelData_Single(const FSingleEditRequest& SingleEditRequest);
```

#### **特殊形状编辑接口**

```cpp
/**
 * 批量修改（基于包围盒）
 * @param InputBox 编辑区域
 * @param CurType 形状类型（方块、球体、圆柱等）
 * @param CurBatchMode 编辑模式
 * @param TypeID 目标类型
 * @param Thick 厚度参数
 */
void BatchModifyVoxelDataByIndex(FBox InputBox, EBatchShapeType CurType, EVoxelControllerEditState CurBatchMode, FGameplayTag TypeID, int Thick = 0);

/**
 * 批量修改（球体）
 */
void BatchModifySphere(FBox InputBox, EVoxelControllerEditState CurBatchMode, FGameplayTag TypeID, int Thick);

/**
 * 批量修改（圆柱）
 */
void BatchModifyCylinder(FBox InputBox, EVoxelControllerEditState CurBatchMode, FGameplayTag TypeID, int Thick, bool IsThrough);

/**
 * 批量修改（墙壁）
 */
void BatchModifyWallByIndex(FBox InputBox, EVoxelControllerEditState CurBatchMode, FGameplayTag TypeID, int PillarSize, TArray<EActorDirection> GenDir, int Thick = 0);
```

### 2.2 区块管理接口

#### **流式加载接口**

```cpp
/**
 * 请求流式更新（基于玩家位置）
 */
void RequestStreamingUpdate();

/**
 * 检查玩家移动（触发加载）
 */
void CheckPlayersMovement();

/**
 * 加载区块数据
 * @param InputIndex 区块索引
 */
void LoadChunkData(FIntVector InputIndex);

/**
 * 生成区块数据
 * @param InputIndex 区块索引
 */
void GenerateChunkData(FIntVector InputIndex);

/**
 * 生成区块Actor
 * @param InputIndex 区块索引
 */
void GenerateChunk(FIntVector InputIndex);

/**
 * 释放区块
 * @param InputChunkIndex 区块索引
 */
void ReleaseChunk(FIntVector InputChunkIndex);

/**
 * 应用网格数据
 * @param InputIndex 区块索引
 * @param MeshData 网格数据
 */
void ApplyChunk(FIntVector InputIndex);

/**
 * 计算区块网格
 * @param InputIndex 区块索引
 * @param MeshData 网格数据
 */
void CalculateChunk(FIntVector InputIndex, FChunkMeshData MeshData);
```

#### **状态管理接口**

```cpp
/**
 * 更新区块状态
 * @param Coord 区块坐标
 * @param Expect 期望状态
 * @param New 新状态
 */
void UpdateChunkState(FIntVector Coord, EChunkState Expect, EChunkState New);

/**
 * 标记任务待处理
 * @param Coord 区块坐标
 */
void MarkTaskPending(FIntVector Coord);

/**
 * 标记任务完成
 * @param Coord 区块坐标
 */
void MarkTaskCompleted(FIntVector Coord);
```

### 2.3 队列处理接口

```cpp
/**
 * 处理渲染更新队列
 */
void ProcessRenderUpdates();

/**
 * 处理网格计算队列
 */
void ProcessCalculateUpdates();

/**
 * 处理数据加载队列
 */
void ProcessLoading();

/**
 * 处理内容生成队列
 */
void ProcessGenerating();

/**
 * 处理内存卸载队列
 */
void ProcessUnloading();

/**
 * 处理流式加载
 */
void ProcessChunkStreaming();

/**
 * 处理所有区块更新（总入口）
 */
void ProcessChunkUpdates();
```

### 2.4 网格生成接口

#### **核心网格生成**

```cpp
/**
 * 生成网格数据（使用快照上下文）
 * @param Context 快照上下文（包含区块和邻居数据）
 * @return 网格数据
 */
FChunkMeshData GenerateMeshData(const FChunkGenContext& Context);

/**
 * 创建四边形面
 * @param Mask 方块信息
 * @param AxisMask 轴向掩码
 * @param UVAxis1 UV轴1
 * @param UVAxis2 UV轴2
 * @param TileSize 纹理平铺大小
 * @param V1-V4 顶点坐标
 * @param MeshData 输出网格数据
 * @param VertexCount 顶点计数
 * @param Context 快照上下文
 */
void CreateQuad(const FMask& Mask, const FIntVector& AxisMask,
               int UVAxis1, int UVAxis2, int TileSize,
               const FIntVector& V1, const FIntVector& V2, const FIntVector& V3, const FIntVector& V4,
               FChunkMeshData& MeshData, int& VertexCount,
               const FChunkGenContext& Context);
```

#### **辅助查询接口**

```cpp
/**
 * 获取纹理索引
 * @param TypeID 方块类型
 * @param Normal 法线方向
 * @return 纹理索引
 */
int GetTextureIndex(FGameplayTag TypeID, FVector Normal) const;

/**
 * 检查方块是否不透明
 * @param InputId 方块类型
 * @return 是否不透明
 */
bool CheckBlockOpaque(FGameplayTag InputId);

/**
 * 获取方块信息
 * @param TypeTag 方块类型标签
 * @return 方块数据
 */
FVoxelBlockItemBase* GetBlockInform(FGameplayTag TypeTag) const;
```

### 2.5 生命周期接口

#### **子系统生命周期**

```cpp
/**
 * 是否应该创建子系统
 */
virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

/**
 * 初始化子系统
 */
virtual void Initialize(FSubsystemCollectionBase& Collection) override;

/**
 * 反初始化子系统
 */
virtual void Deinitialize() override;

/**
 * Tick处理
 */
virtual void Tick(float DeltaTime) override;

/**
 * 世界初始化完成后回调
 */
void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues IVS);
```

---

## 3. 数据流

### 3.1 区块生命周期流程

```
未加载（Unloaded）
  ↓ 玩家接近 / 请求加载
请求加载（Loading Requested）
  ↓ 添加到 LoadingQueue
加载数据（Loading）
  ↓ 数据加载完成
生成内容（Generating）
  ↓ 添加到 GeneratingQueue
生成完成（Generated）
  ↓ 添加到 CalculateQueue
计算网格（Calculating）
  ↓ 计算完成
渲染（Rendering）
  ↓ 添加到 RenderingQueue
已加载（Loaded）
  ↓ 玩家远离 / 请求卸载
请求卸载（Unloading Requested）
  ↓ 添加到 UnloadingQueue
卸载（Unloading）
  ↓
未加载（Unloaded）
```

### 3.2 体素编辑流程

```
玩家编辑请求（单体/批量）
  ↓
ModifyVoxelData() / BatchModifyVoxelData_Modification()
  ↓
构建 FVoxelEditModification
  ↓
添加到 PendingModifications 队列
  ↓
Tick::ProcessBlockEdits()
  ↓
遍历修改，更新 CurSceneBlockTypeMask（读写锁保护）
  ↓
标记受影响的区块
  ↓
调用 modifyChunk(ChunkIndex)
  ↓
添加到 CalculateQueue / RenderingQueue
  ↓
后台线程生成网格（FChunkGenContext快照）
  ↓
CalculateChunk() 接收计算结果
  ↓
ApplyChunk() 应用到 Actor
  ↓
通知其他系统（导航等）
```

### 3.3 流式加载触发流程

```
Tick()
  ↓
CheckPlayersMovement()
  ↓
获取玩家位置
  ↓
计算当前区块索引
  ↓
与 LastPlayerChunkPositions 比较
  ↓
如果区块变化：
  ↓
设置 bPlayersMoved = true
  ↓
在下一个 Tick 调用 RequestStreamingUpdate()
  ↓
RequestStreamingUpdate()
  ↓
计算需要加载/卸载的区块
  ↓
根据优先级添加到对应队列
  ↓
ProcessChunkStreaming() 总控处理
```

### 3.4 多线程网格生成流程

```
游戏线程：
  ↓
GenerateMeshData(const FChunkGenContext& Context)
  ↓
BuildChunkContext() 创建快照
  ↓
后台线程执行网格计算
  ↓
计算完成后调用 CalculateChunk()
  ↓
游戏线程接收结果
  ↓
ApplyChunk() 更新 Actor
```

---

## 4. 生命周期管理

### 4.1 Subsystem生命周期

```
ShouldCreateSubsystem() → Initialize() → OnPostWorldInit() → Tick()
    ↓
持续运行（处理队列和编辑）
    ↓
Deinitialize() → CleanupAllResources() → StopAllBackgroundTasks()
```

### 4.2 区块生命周期

```cpp
// 区块状态枚举
enum class EChunkState
{
    Unloaded,      // 未加载
    Loading,       // 加载数据中
    Generating,    // 生成内容中
    Calculating,   // 计算网格中
    Rendering,     // 渲染中
    Loaded,        // 已加载完成
    Unloading      // 卸载中
};

// 状态转换
void UpdateChunkState(FIntVector Coord, EChunkState Expect, EChunkState New);
```

### 4.3 任务生命周期

```
任务创建 → 加入队列 → 任务处理 → 任务完成
    ↓
特殊状态：
- PendingGenerationTasks：等待生成的任务集合
- ActiveRequestsSet：活跃请求集合（防止重复）
- MarkTaskPending/MarkTaskCompleted：状态标记
```

### 4.4 资源生命周期

```
启动：Initialize() 初始化数据结构和队列
运行：Tick() 处理队列和编辑请求
关闭：Deinitialize() 清理资源和停止任务
```

---

## 5. 类关系图

### 5.1 核心类关系

```
UVoxelBlockWorldSubsystem
├── 持有 → TMap<FIntVector, FChunkData> ChunkMetaDataMap
├── 持有 → TMap<FIntVector, FGameplayTag> CurSceneBlockTypeMask
├── 持有 → TMap<FIntVector, FGameplayTag> SceneBaseBlockTypeMask
├── 持有 → TMap<FIntVector, TWeakObjectPtr<AChunkBase>> LoadedChunks
│
├── 管理队列系统
│   ├── TQueue<FChunkLoadRequest> LoadingQueue
│   ├── TQueue<FChunkLoadRequest> GeneratingQueue
│   ├── TQueue<FChunkLoadRequest> CalculateQueue
│   ├── TQueue<FChunkLoadRequest> RenderingQueue
│   └── TQueue<FChunkLoadRequest> UnloadingQueue
│
├── 使用子系统
│   ├── UObjectPoolSubsystem* PoolSubsystem（对象池）
│   ├── UVoxelGameInstance* GameInstance（游戏实例）
│   └── UConfigDataSubsystem* ConfigSubsystemCache（配置数据）
│
└── 线程安全保护
    ├── FRWLock PendingEditsLock
    ├── FRWLock BlockMapLock
    └── FCriticalSection TaskMutex
```

### 5.2 与外部系统关系

```
编辑系统（VoxelControllerInteractComponent）
  ↓ 调用
UVoxelBlockWorldSubsystem
  ↓ 通知
渲染系统（AChunkBase）
  ↓ 集成
导航系统（UNavigationSystemV1）
  ↓ 使用
配置系统（UConfigDataSubsystem）
  ↓ 依赖
对象池系统（UObjectPoolSubsystem）
```

### 5.3 数据流向关系

```
玩家输入 → VoxelPlayerController
  ↓ → 编辑系统 → 编辑请求
  ↓ → UVoxelBlockWorldSubsystem
  ↓ → 队列处理 → 后台线程
  ↓ → 网格生成 → 渲染更新
  ↓ → 导航更新 → 其他系统
```

---

## 6. 扩展点

### 6.1 新增编辑形状

**扩展 BatchModifyVoxelDataByIndex**：
```cpp
// 在 BatchModifyVoxelDataByIndex 中添加新的形状类型
void BatchModifyVoxelDataByIndex(FBox InputBox, EBatchShapeType CurType, ...)
{
    switch (CurType)
    {
        case EBatchShapeType::Box:
            // 现有逻辑
            break;
        case EBatchShapeType::Sphere:
            // 球体逻辑
            break;
        case EBatchShapeType::Cylinder:
            // 圆柱逻辑
            break;
        case EBatchShapeType::Cone:  // 新增
            // 圆锥逻辑
            break;
        case EBatchShapeType::Custom: // 新增
            // 自定义形状
            break;
    }
}
```

### 6.2 新增队列处理模式

**扩展队列处理逻辑**：
```cpp
void ProcessChunkUpdates()
{
    // 根据系统负载动态调整处理策略
    if (IsSystemUnderHighLoad())
    {
        // 只处理高优先级任务
        ProcessHighPriorityOnly();
    }
    else
    {
        // 正常处理所有队列
        ProcessLoading();
        ProcessGenerating();
        ProcessCalculateUpdates();
        ProcessRenderUpdates();
        ProcessUnloading();
    }
}
```

### 6.3 新增网格生成优化

**扩展网格生成策略**：
```cpp
class FMeshGenerationStrategy
{
public:
    virtual void GenerateMesh(const FChunkGenContext& Context, FChunkMeshData& OutMesh) = 0;
};

// 添加新的生成策略
class FOptimizedMeshGenerationStrategy : public FMeshGenerationStrategy
{
    virtual void GenerateMesh(const FChunkGenContext& Context, FChunkMeshData& OutMesh) override;
};
```

### 6.4 新增异步任务类型

**扩展任务类型**：
```cpp
enum class ETaskType
{
    MeshGeneration,
    NavigationUpdate,
    LightCalculation,
    PhysicsSimulation,
    CustomTask  // 新增
};

struct FAsyncTask
{
    ETaskType Type;
    FIntVector ChunkIndex;
    TSharedPtr<FAsyncTaskData> Data;
    TDelegate<void(FIntVector)> OnComplete;
};
```

### 6.5 新增快照上下文类型

**扩展快照机制**：
```cpp
struct FAdvancedChunkGenContext : public FChunkGenContext
{
    // 添加光照信息
    TArray<FLightInfo> LightData;

    // 添加材质信息
    TMap<FGameplayTag, FMaterialInfo> MaterialData;

    // 添加碰撞信息
    TArray<FCollisionData> CollisionData;
};
```

---

## 7. 关键设计特性

### 7.1 多线程设计

**读写锁保护**：
- 使用 `FRWLock` 保护 `CurSceneBlockTypeMask`
- 读操作（GetBlockType）使用 `FReadScopeLock`
- 写操作（SetBlockType）使用 `FWriteScopeLock`

**快照机制**：
- `FChunkGenContext` 创建线程快照
- 后台线程无锁访问数据
- 避免数据竞争和锁等待

**队列解耦**：
- 多级队列系统
- 任务独立处理
- 避免直接线程间通信

### 7.2 性能优化

**动态并发控制**：
```cpp
int32 MaxConcurrentTasks = 1;
// 可根据系统负载动态调整
```

**批量处理优化**：
- 合并相同区块的编辑
- 批量提交网格更新
- 减少函数调用开销

**内存管理**：
- 对象池复用 Chunk Actor
- 及时卸载远离区块
- 限制队列大小防止内存溢出

### 7.3 可扩展性设计

**模块化架构**：
- 功能模块独立
- 接口清晰定义
- 易于替换和扩展

**策略模式**：
- 生成策略可替换
- 队列策略可配置
- 处理策略可扩展

**事件驱动**：
- 任务完成回调
- 状态变化通知
- 支持自定义事件处理

---

## 8. 相关文档

### 8.1 系统设计文档

- [VoxelBlockWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelBlockWorldSubsystem-系统设计.md) - 系统上下文和概念定义
- [编辑系统-系统设计](/VoxelDesign/02-系统设计/编辑系统-系统设计.md) - 编辑系统上下文

### 8.2 其他子系统文档

- [WorldObjectPoolSubsystem-系统设计](/VoxelDesign/02-系统设计/WorldObjectPoolSubsystem-系统设计.md) - 对象池管理
- [ConfigDataSubsystem-数据管理-分析报告](/VoxelDesign/08-分析报告/ConfigDataSubsystem-数据管理-分析报告.md) - 配置数据管理

### 8.3 架构设计文档

- [编辑系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-架构设计.md) - 编辑系统整体架构
- [编辑系统-选区系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-选区系统-架构设计.md) - 选区系统架构

---

**文档版本**: 1.0
**最后更新**: 2026-01-24
**状态**: ✅ 架构设计已完成