# VoxelItemWorldSubsystem-架构设计

**版本**: 1.2
**创建日期**: 2026-01-24
**最后更新**: 2026-10-31

> v1.2：2026-10-31 R11-1 确认 RimWorld 式堆实例身份：整堆转移不换 GUID，仅部分拆分分配新 GUID；合堆保留目标身份，来源耗尽后删除。

---

## 1. 组件架构

### 1.0 经营组件

家具实例是容器，经营数据在组件：

```text
AUVoxelItemBaseInstance
 ├─ UVoxelFacilityComponent        HP/清洁度/功能模块状态、跨阈事件
 ├─ UVoxelWorkPointComponent       队列配置、FIFO 队列、队列线/等待区、服务位
 └─ UVoxelStorageComponent         货架/后备储物容量与库存引用
```

规则：

1. 家具实例创建时按家具类型装配组件；组件不随使用状态销毁。
2. `UVoxelWorkPointComponent` 持有 `FWorkPointQueue`：容量 8、`QueueMode{Line, WaitingArea}`、队列线可编辑或自动生成；玩家不需要单独摆"排队区"。
3. 设施状态跨阈由 `UVoxelFacilityComponent` 发事件，工单索引器据此派生 Clean/Repair 工单。
4. 储物组件只保存容量与实例引用，不复制物品数量/品质/HP。
5. 物品子系统负责实例登记、网格索引与工作点注册，不承担队列与工单业务。
6. 物品、家具与工作点队列属于场景数据，随 `FSceneSaveData` 存档；经济账户与合同在 `FSaveData.Global`。

### 1.1 整体架构层次

```
┌─────────────────────────────────────────────────────────────┐
│ 用户交互层（Player Interaction）                             │
│ - VoxelPlayerController                                      │
│   - 通过编辑系统发起放置/移除请求                              │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 接口层（API Layer）                                          │
│ - FInteractableData（放置数据）                               │
│ - 核心接口（SpawnItemAtGrid, RemoveItemAtGrid）               │
│ - 查询接口（GetItemAtGrid, CheckIsSpaceAvailable）          │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 业务逻辑层（Business Logic）                                  │
│ ├── 空间管理（Space Management）                             │
│ │  ├── GridToItemMap（空间索引）                             │
│ │  ├── CalculateOccupiedCells（占用计算）                    │
│ │  └── 占用格子维护                                          │
│ ├── 物品管理（Item Management）                             │
│ │  ├── Spawn/Remove（物品生命周期）                        │
│ │  ├── 朝向支持（Direction Support）                        │
│ │  └── 索引更新                                              │
│ └── 碰撞检测（Collision Detection）                         │
│    ├── 基础方块检测（BlockWorld）                          │
│    ├── 物品重叠检测（GridToItemMap）                        │
│    └── 边界检查                                             │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 协调层（Coordination Layer）                                 │
│ ├── BlockWorld协调（基础方块状态）                            │
│ ├── AreaWorld协调（区域归属管理）                            │
│ └── ConfigData协调（物品类型查询）                          │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 数据层（Data Layer）                                         │
│ ├── GridToItemMap（空间索引表）                             │
│ ├── 物品实例缓存                                            │
│ └── 子系统引用缓存                                          │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 核心组件职责

#### **空间索引管理组件**

**GridToItemMap**
- 数据结构：`TMap<FIntVector, AUVoxelItemBaseInstance*>`
- 职责：维护网格坐标到物品Actor的映射
- 特点：支持多格物品，一个物品占用多个格子
- 更新：物品操作时自动维护

**CalculateOccupiedCells**
- 功能：根据朝向计算物品占用的所有格子
- 输入：原点位置、朝向、物品静态数据
- 输出：占用的世界坐标数组
- 算法：旋转OccupiedBlocks并转换为世界坐标

#### **物品生命周期管理组件**

**SpawnItemAtGrid**
- 职责：创建物品实例并注册到空间索引
- 流程：空间检查 → 创建Actor → 更新索引 → 通知系统
- 特性：支持加载模式跳过检查

**RemoveItemAtGrid**
- 职责：移除物品实例并清理索引
- 流程：查找物品 → 清理索引 → 销毁Actor → 通知系统

#### **碰撞检测组件**

**CheckIsSpaceAvailable**
- 检查项：世界边界、基础方块、物品重叠
- 算法：逐格验证占用格子
- 结果：返回是否可放置布尔值

#### **子系统协调组件**

**BlockWorld协调**
- 目的：获取基础方块状态
- 方法：调用BlockSys->CanPlaceItemOnBlock()
- 用途：确保物品放置在合适的基础方块上

**AreaWorld协调**
- 目的：管理物品区域归属
- 方法：物品放置时注册到区域
- 用途：支持区域相关的物品管理

**ConfigData协调**
- 目的：获取物品类型定义
- 方法：调用ConfigSys->GetItemTypeById&lt;FVoxelNormalItemBase&gt;(HandleId)
- 用途：获取物品的占用模式和属性

### 1.3 多格物品支持架构

```
多格物品空间管理
├─ 占用定义（FVoxelNormalItemBase.OccupiedBlocks）
│  └─ 相对坐标数组（如：[(0,0,0), (1,0,0), (0,1,0)]）
├─ 朝向系统（EActorDirection）
│  ├── Forward（原朝向）
│  ├── Backward（旋转180°）
│  ├── Left（旋转90°）
│  └── Right（旋转-90°）
├─ 坐标计算（CalculateOccupiedCells）
│  └─ 根据朝向旋转并转换为世界坐标
└─ 索引维护（GridToItemMap）
   └─ 所有占用格子映射到同一物品Actor
```

---

## 2. 接口定义

### 2.1 核心操作接口

#### **物品放置接口**

```cpp
/**
 * 在指定位置生成物品
 * @param Data 包含物品类型、位置、朝向
 * @param bLoadFromSave 是否为加载模式（跳过碰撞检查）
 * @return 成功返回物品Actor，失败返回nullptr
 */
UFUNCTION(BlueprintCallable, Category = "ItemSystem")
AUVoxelItemBaseInstance* SpawnItemAtGrid(const FInteractableData& Data, bool bLoadFromSave = false);

/**
 * 移除指定位置的物品
 * @param GridPos 网格坐标（任意占用格子）
 * @return 是否移除成功
 */
UFUNCTION(BlueprintCallable, Category = "ItemSystem")
bool RemoveItemAtGrid(FIntVector GridPos);
```

#### **查询接口**

```cpp
/**
 * 获取指定位置的物品
 * @param GridPos 网格坐标
 * @return 占用该位置的物品Actor（多格物品返回同一Actor）
 */
UFUNCTION(BlueprintCallable, Category = "ItemSystem")
AUVoxelItemBaseInstance* GetItemAtGrid(FIntVector GridPos);

/**
 * 检查空间是否可用
 * @param OriginPos 基准位置
 * @param Direction 物品朝向
 * @param StaticData 物品静态数据
 * @return 是否可放置
 */
bool CheckIsSpaceAvailable(FIntVector OriginPos, EActorDirection Direction, const FVoxelNormalItemBase* StaticData);
```

#### **辅助接口**

```cpp
/**
 * 计算物品占用的所有格子（静态方法）
 * @param Origin 基准位置
 * @param Dir 朝向
 * @param StaticData 物品静态数据
 * @return 占用的世界坐标数组
 */
static TArray<FIntVector> CalculateOccupiedCells(FIntVector Origin, EActorDirection Dir, const FVoxelNormalItemBase* StaticData);

/**
 * 获取指定物品占用的所有格子
 * @param Item 物品Actor
 * @return 占用的坐标数组
 */
TArray<FIntVector> GetItemOccupiedCells(AUVoxelItemBaseInstance* Item) const;
```

### 2.2 数据结构定义

#### **FInteractableData**

```cpp
/**
 * 物品交互数据结构
 */
USTRUCT(BlueprintType)
struct FInteractableData
{
    GENERATED_BODY()

    /** 物品类型标签 */
    UPROPERTY(BlueprintReadOnly, Category = "ItemSystem")
    FGameplayTag ItemTag;

    /** 放置位置（网格坐标） */
    UPROPERTY(BlueprintReadOnly, Category = "ItemSystem")
    FIntVector Position;

    /** 朝向 */
    UPROPERTY(BlueprintReadOnly, Category = "ItemSystem")
    EActorDirection Direction;

    /** 可选的物品数据 */
    UPROPERTY(BlueprintReadOnly, Category = "ItemSystem")
    FItemInstance ItemInstance;
};
```

#### **物品类型定义**

```cpp
/**
 * 物品静态数据（来自ConfigDataSubsystem）
 */
USTRUCT(BlueprintType)
struct FVoxelNormalItemBase
{
    GENERATED_BODY()

    /** 占用的相对坐标（相对于原点） */
    UPROPERTY(EditAnywhere, Category = "ItemSystem")
    TArray<FIntVector> OccupiedBlocks;

    /** 是否允许旋转 */
    UPROPERTY(EditAnywhere, Category = "ItemSystem")
    bool bCanRotate;

    /** 最大堆叠数量 */
    UPROPERTY(EditAnywhere, Category = "ItemSystem")
    int32 MaxStackSize;

    /** 放置要求 */
    UPROPERTY(EditAnywhere, Category = "ItemSystem")
    EBlockType RequiredBlockType;

    /** 碰撞类型 */
    UPROPERTY(EditAnywhere, Category = "ItemSystem")
    ECollisionType CollisionType;
};
```

---

## 3. 数据流

### 3.1 物品放置流程

```
用户放置请求
    ↓
编辑系统 → SpawnItemAtGrid()
    ↓
1. 数据准备
   ├─ 获取 FInteractableData
   └─ 从 ConfigData 获取 StaticData
    ↓
2. 空间检查（除非 bLoadFromSave）
   ├─ CalculateOccupiedCells() 计算占用格子
   ├─ 检查世界边界
   ├─ 检查基础方块（BlockWorld）
   └─ 检查物品重叠（GridToItemMap）
    ↓
3. 创建物品Actor
   ├─ SpawnActor<AUVoxelItemBaseInstance>()
   ├─ 设置位置和朝向
   ├─ 初始化物品数据
   └─ 设置网格引用
    ↓
4. 更新空间索引
   ├─ 调用 CalculateOccupiedCells()
   ├─ 遍历占用格子
   └─ 更新 GridToItemMap
    ↓
5. 通知子系统
   ├─ AreaWorld：注册区域归属
   └─ 其他系统：更新状态
    ↓
返回物品Actor指针
```

### 3.2 物品移除流程

```
用户移除请求
    ↓
编辑系统 → RemoveItemAtGrid()
    ↓
1. 查找物品
   ├─ GetItemAtGrid() 查找任意占用格子
   └─ 获取物品Actor
    ↓
2. 清理索引
   ├─ 获取物品占用的所有格子
   ├─ 从 GridToItemMap 移除所有项
   └─ 更新相关缓存
    ↓
3. 销毁Actor
   ├→ 物品Actor清理自身
   ├→ 从场景中移除
   └→ 通知子系统
    ↓
4. 通知清理
   ├→ AreaWorld：移除区域注册
   └→ 其他系统：更新状态
    ↓
返回移除成功状态
```

### 3.3 空间查询流程

```
查询请求
    ↓
GetItemAtGrid() 或 CheckIsSpaceAvailable()
    ↓
1. 坐标计算
   ├─ 对于多格物品，计算所有占用格子
   └─ 处理朝向旋转
    ↓
2. 索引查询
   ├─ GridToItemMap.Find()
   └─ O(1)时间复杂度查询
    ↓
3. 结果返回
   ├─ GetItem：返回物品Actor
   └─ CheckSpace：返回布尔值
    ↓
完成查询
```

---

## 4. 生命周期管理

### 4.1 子系统生命周期

```
Subsystem Init
  ↓
Initialize()
  ├─ 缓存子系统引用（BlockWorld, AreaWorld, ConfigData）
  ├─ 初始化空的空间索引
  └─ 注册事件监听（可选）
  ↓
运行中（Active）
  ├─ 处理物品放置请求
  ├─ 处理物品移除请求
  ├─ 维护空间索引
  └─ 响应查询请求
  ↓
Deinitialize()
  ├─ 清理空间索引
  ├→ 销毁所有物品Actor
  ├─ 清理子系统引用
  └→ 清理事件监听
  ↓
Subsystem Shutdown
```

### 4.2 物品生命周期

```
物品创建（Created）
  ↓
放置前检查（CheckSpace）
  ├─ 空间验证通过
  └─ 空间验证失败 → 创建失败
  ↓
放置中（Spawning）
  ├→ 创建Actor实例
  ├→ 设置位置和朝向
  ├→ 初始化数据
  └→ 更新空间索引
  ↓
已放置（Placed）
  ├→ 激活状态
  ├→ 响应交互
  └→ 维护索引
  ↓
移除中（Removing）
  ├→ 清理索引
  ├→ 销毁Actor
  └→ 通知系统
  ↓
已销毁（Destroyed）
```

### 4.3 空间索引生命周期

```
初始化（Empty）
  ↓
添加物品（Add Item）
  ├─ 计算占用格子
  ├─ 遍历格子添加到索引
  └─ 更新计数器
  ↓
维护状态（Maintained）
  ├─ 支持查询
  ├─ 支持更新
  └─ 保持一致性
  ↓
移除物品（Remove Item）
  ├─ 查找物品
  ├─ 收集占用格子
  └─ 从索引移除
  ↓
清理完成（Clean）
```

---

## 5. 类关系图

### 5.1 子系统核心关系

```
VoxelItemWorldSubsystem（核心）
├─ 持有 → GridToItemMap（空间索引）
├─ 依赖 → UVoxelBlockWorldSubsystem
├─ 依赖 → UVoxelAreaWorldSubsystem
├─ 依赖 → UConfigDataSubsystem
└─ 生成 → AUVoxelItemBaseInstance（物品Actor）
```

### 5.2 数据流向关系

```
外部系统
  ↓
编辑系统 → 发起放置/移除请求
  ↓
VoxelItemWorldSubsystem
  ├─ 查询 → ConfigDataSubsystem（物品类型）
  ├─ 检查 → BlockWorldSubsystem（基础方块）
  ├─ 注册 → AreaWorldSubsystem（区域归属）
  └─ 管理 → GridToItemMap（空间索引）
  ↓
物品Actor（AUVoxelItemBaseInstance）
```

### 5.3 接口关系

```
VoxelPlayerController（用户交互）
  ↓
编辑系统（批处理工具）
  ↓
VoxelItemWorldSubsystem接口
  ├─ SpawnItemAtGrid（放置）
  ├─ RemoveItemAtGrid（移除）
  ├─ GetItemAtGrid（查询）
  └─ CheckIsSpaceAvailable（检查）
  ↓
执行结果
```

---

## 6. 扩展点

### 6.1 新物品类型支持

**添加新的物品类型**：
1. 在ConfigDataSubsystem中定义新的FVoxelNormalItemBase
2. 设置OccupiedBlocks定义占用模式
3. 配置朝向和碰撞属性
4. 系统自动支持新类型

**示例**：
```cpp
// 新的物品类型定义
USTRUCT()
struct FVoxelSpecialItem : public FVoxelNormalItemBase
{
    UPROPERTY()
    bool bIsInteractive;

    UPROPERTY()
    float InteractionRange;

    // 自定义占用模式（L形）
    TArray<FIntVector> GetLShapeOccupied() const
    {
        return {
            FIntVector(0, 0, 0),
            FIntVector(1, 0, 0),
            FIntVector(0, 1, 0),
            FIntVector(0, 1, 1)
        };
    }
};
```

### 6.2 新朝向支持

**扩展朝向系统**：
```cpp
// 扩展朝向枚举
enum class EActorDirection : uint8
{
    Forward = 0,
    Backward = 1,
    Left = 2,
    Right = 3,
    // 新增
    Forward45 = 4,    // 45度旋转
    Backward45 = 5    // -45度旋转
};

// 修改CalculateOccupiedCells支持新角度
static TArray<FIntVector> CalculateOccupiedCells(
    FIntVector Origin,
    EActorDirection Dir,
    const FVoxelNormalItemBase* StaticData
);
```

### 6.3 高级碰撞检测

**扩展碰撞检测类型**：
```cpp
enum class ECollisionType : uint8
{
    None = 0,          // 无碰撞
    Static = 1,       // 静态碰撞
    Dynamic = 2,      // 动态碰撞
    Interactive = 3,  // 可交互碰撞
    // 新增
    Transparent = 4,   // 透明碰撞（可透视）
    Temporary = 5      // 临时碰撞
};

// 在CheckIsSpaceAvailable中添加新检测逻辑
bool CheckAdvancedCollision(
    const FVoxelNormalItemBase* StaticData,
    const TArray<FIntVector>& OccupiedCells
);
```

### 6.4 批量操作支持

**扩展批量接口**：
```cpp
/**
 * 批量放置物品
 * @param Items 放置数据数组
 * @param bSkipValidation 是否跳过验证
 * @return 成功放置的数量
 */
int32 BatchSpawnItems(const TArray<FInteractableData>& Items, bool bSkipValidation = false);

/**
 * 批量移除物品
 * @param Positions 移除位置数组
 * @return 成功移除的数量
 */
int32 BatchRemoveItems(const TArray<FIntVector>& Positions);

/**
 * 范围查询
 * @param Center 中心点
 * @param Radius 查询半径
 * @return 范围内的所有物品
 */
TArray<AUVoxelItemBaseInstance*> GetItemsInRange(
    FIntVector Center,
    int32 Radius
) const;
```

### 6.5 事件系统扩展

**添加事件通知**：
```cpp
// 声明事件委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnItemSpawned,
    AUVoxelItemBaseInstance*, Item,
    FIntVector, Position
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnItemRemoved,
    AUVoxelItemBaseInstance*, Item,
    FIntVector, Position
);

// 在类中添加事件
UPROPERTY(BlueprintAssignable, Category = "ItemSystem")
FOnItemSpawned OnItemSpawned;

UPROPERTY(BlueprintAssignable, Category = "ItemSystem")
FOnItemRemoved OnItemRemoved;
```

---

## 7. 性能优化

### 7.1 空间索引优化

**分区索引**：
```cpp
// 可选：世界分区索引
TMap<FIntVector, TMap<FIntVector, AUVoxelItemBaseInstance*>> ZonedGridToItemMap;

// 按区查询优化
AUVoxelItemBaseInstance* GetItemInZone(FIntVector ZonePos, FIntVector LocalPos);
```

**热点缓存**：
```cpp
// 热点物品缓存
TMap<FIntVector, AUVoxelItemBaseInstance*> HotCache;
TArray<FIntVector> RecentQueries;

// LRU缓存管理
void UpdateHotCache(FIntVector Pos, AUVoxelItemBaseInstance* Item);
```

### 7.2 计算结果缓存

**占用格子缓存**：
```cpp
// 预计算占用格子
TMap<TPair<FGameplayTag, EActorDirection>, TArray<FIntVector>> OccupiedCache;

// 获取缓存结果
const TArray<FIntVector>& GetCachedOccupiedCells(
    const FGameplayTag& ItemTag,
    EActorDirection Direction
);
```

### 7.3 批量操作优化

**批量索引更新**：
```cpp
// 批量添加索引
void BatchAddToIndex(const TArray<FItemIndexEntry>& Entries);

// 批量移除索引
void BatchRemoveFromIndex(const TArray<FIntVector>& Positions);
```

---

## 8. 错误处理与调试

### 8.1 错误处理策略

**数据一致性检查**：
```cpp
// 验证索引一致性
bool ValidateIndexConsistency() const;

// 检查孤立物品
void CheckOrphanedItems();
```

**异常处理**：
```cpp
// 安全放置包装
AUVoxelItemBaseInstance* SafeSpawnItem(const FInteractableData& Data);

// 安全移除包装
bool SafeRemoveItem(FIntVector GridPos);
```

### 8.2 调试工具

**可视化调试**：
```cpp
// 显示空间索引
void ShowGridIndexVisualization();

// 高亮显示物品占用
void HighlightItemOccupancy(AUVoxelItemBaseInstance* Item);
```

**性能分析**：
```cpp
// 性能统计
struct FItemSystemStats
{
    int32 TotalItems;
    int32 QueryCount;
    float AverageQueryTime;
    int32 CollisionCheckCount;
};

FItemSystemStats GetStatistics() const;
```

---

## 9. V1 实例稳定身份与存读档

```cpp
USTRUCT(BlueprintType)
struct FItemInstanceRef
{
    GENERATED_BODY()
    FGuid ItemInstanceId; // 无效 GUID = 空槽；不是配置 ItemId / HandleId
};

USTRUCT(BlueprintType)
struct FItemInstanceSaveRecord
{
    GENERATED_BODY()
    FGuid ItemInstanceId;
    FVoxelItemInstanceBase Instance; // 核心 + 实际存在的 FunctionModules，状态只此一份
    bool bPlacedInWorld = false;
    FIntVector GridOrigin;           // 仅放置形态使用；容器内位置由槽位引用表达
};

// UVoxelItemWorldSubsystem：世界、家具容器与私人背包内全部 ITEM 的唯一实例登记表
TMap<FGuid, FItemInstanceSaveRecord> ItemInstances; // 入档

// 派生索引，不入档：放置 Actor 与 GridToItemMap 均由上述记录重建
TMap<FGuid, TWeakObjectPtr<AUVoxelItemBaseInstance>> ActivePlacedItems;
```

- 容器槽位和私人背包槽位只保存 `FItemInstanceRef.ItemInstanceId`；`Quantity / HitPoints / Cleanliness / QualityTag` 及实际存在的模块状态读取 `ItemInstances` 中唯一实例，不在槽位或存档其他位置复制。
- 整堆转移属于纯移动，保留原 GUID；只有部分拆分创建的新堆才生成新 GUID，原堆保留原 GUID。合并后目标堆保留目标 GUID，源堆数量耗尽时删除源 GUID；数量归零时同步清空所有槽位引用。
- 存档只序列化一次 `ItemInstances` 和各槽位 GUID，不保存进程指针、`ActivePlacedItems` 或 `GridToItemMap`。读档先恢复实例记录，再以加载模式创建放置 Actor 并重建派生索引，最后恢复区域容器与人物背包槽位引用。
- `ResolveItemInstance(FItemInstanceRef)` 只通过稳定 GUID 查询；空 GUID 只表示空槽。非空 GUID 在实例表缺失、重复，或被多个槽位同时引用时，该档不激活，不以默认物品补洞；完整失败边界与全局顺序见 [AI 决策系统架构 §7 存档](/VoxelDesign/03-架构设计/AI决策系统-架构设计.md)。

---

## 10. 相关文档

### 10.1 系统设计文档

- [VoxelItemWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelItemWorldSubsystem-系统设计.md)
- [VoxelItemBase-数据结构设计](/VoxelDesign/04-数据结构设计/VoxelItemBase-数据结构设计.md)

### 10.2 相关子系统文档

- [VoxelBlockWorldSubsystem-架构设计](/VoxelDesign/03-架构设计/VoxelBlockWorldSubsystem-架构设计.md)
- [VoxelAreaWorldSubsystem-架构设计](/VoxelDesign/03-架构设计/VoxelAreaWorldSubsystem-架构设计.md)
- [ConfigDataSubsystem-数据管理-分析报告](/VoxelDesign/08-分析报告/ConfigDataSubsystem-数据管理-分析报告.md)

### 10.3 实现文档

- [VoxelItemWorldSubsystem.h](/Source/Voxel/Private/GameManager/WorldManager/VoxelItemWorldSubsystem.h)

---

**文档版本**: 1.1（V1 实例稳定身份、唯一实例登记与读档重建）
**最后更新**: 2026-10-31
**状态**: ✅ 架构设计完成