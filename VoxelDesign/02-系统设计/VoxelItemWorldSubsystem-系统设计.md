# VoxelItemWorldSubsystem-系统设计

**版本**: 1.1
**创建日期**: 2026-01-24
**最后更新**: 2026-10-31

---

## 1. 系统上下文

### 1.1 系统边界

**VoxelItemWorldSubsystem负责**：
- 家具物品的生成和销毁管理
- 物理空间占用索引（Grid → Actor）
- 物品放置碰撞检测
- 与BlockWorld和Area子系统的协调
- 多格物品的空间管理
- 物品朝向和旋转支持

**VoxelItemWorldSubsystem不负责**：
- 物品的持久化存储（由Save/Load系统负责）
- 物品的物品栏管理（由Inventory系统负责）
- 物品的交互逻辑（由ItemInteraction系统负责）
- 物品的视觉效果渲染（由Actor自身负责）
- 物品的碰撞物理计算（由Unreal引擎物理系统负责）

### 1.2 外部依赖

**依赖的系统**：
- **UVoxelBlockWorldSubsystem**：基础方块的碰撞检测
- **UVoxelAreaWorldSubsystem**：区域管理和归属
- **UConfigDataSubsystem**：物品类型定义和配置
- **UObjectPoolSubsystem**：物品Actor对象池管理（可选）

**被依赖的系统**：
- **编辑系统**：放置操作和空间验证
- **物品交互系统**：查询物品位置和状态
- **保存系统**：物品状态的持久化
- **AI系统**：物品位置查询和导航

### 1.3 输入输出

**输入**：
- 放置请求（包含物品类型、位置、朝向）
- 移除请求（指定位置）
- 查询请求（获取特定位置的物品）
- 保存加载请求（批量创建/删除）

**输出**：
- 物品实例指针（成功生成时）
- 空间占用状态（布尔值）
- 物品位置信息（FIntVector）
- 占用格子列表（TArray<FIntVector>）

---

## 2. 核心概念

### 2.1 基础概念

**物品实例（Item Instance）**
- 定义：放置在场景中的具体物品Actor
- 类型：`AUVoxelItemBaseInstance`
- 属性：包含物品静态数据和运行时状态
- 生命周期：Spawn → Place → Use → Remove → Destroy

**空间占用（Occupied Space）**
- 定义：物品在网格中占用的所有坐标集合
- 存储：`TMap<FIntVector, AUVoxelItemBaseInstance*> GridToItemMap`
- 特点：支持多格物品，根据朝向动态计算
- 更新：物品放置/移除时自动更新索引

**朝向系统（Direction System）**
- 类型：`EActorDirection`（Forward、Backward、Left、Right等）
- 影响：多格物品的占用格子布局
- 计算：`CalculateOccupiedCells()` 根据朝向重新计算占用
- 应用：物品放置和移动时考虑旋转

**碰撞检测（Collision Check）**
- 类型：基础方块碰撞 + 物品碰撞
- 时机：放置前检测，卸载后验证
- 内容：位置有效性、空间可用性、重叠检测
- 结果：成功/失败状态反馈

### 2.2 组合概念

**物品放置流程**
```
放置请求 → CheckIsSpaceAvailable() → SpawnActor() → 更新索引 → 通知系统
```

**空间索引管理**
```
GridToItemMap（核心数据结构）
├─ Key: FIntVector（世界网格坐标）
└─ Value: AUVoxelItemBaseInstance*（指向物品Actor）
```

**子系统协调机制**
```
VoxelItemWorldSubsystem
├─ 与BlockWorld协调：基础方块碰撞检测
├─ 与AreaWorld协调：区域归属管理
└─ 与ConfigData协调：物品类型查询
```

**物品生命周期管理**
```
请求放置 → 空间检查 → 生成Actor → 注册索引 → 状态维护
  ↓
请求移除 → 索引注销 → 销毁Actor → 通知清理 → 更新状态
```

### 2.3 行为概念

**物品放置状态机**
```
Idle（空闲）
  ↓ 空间检查
CheckSpace（检查中）
  ↓ 检查结果
Available（可用） → 生成物品 → Placed（已放置）
  ↓
Unavailable（不可用） → 返回失败 → Idle（空闲）
```

**空间检测流程**
```
CheckIsSpaceAvailable()
  ↓
获取物品静态数据（FVoxelNormalItemBase）
  ↓
计算占用格子（CalculateOccupiedCells）
  ↓
逐格检查BlockWorld碰撞和物品重叠
  ↓
返回检测结果
```

**索引维护操作**
```
物品生成
  ↓
CalculateOccupiedCells() 计算占用
  ↓
遍历占用格子，更新 GridToItemMap
  ↓
通知相关子系统更新状态

物品移除
  ↓
从 GridToItemMap 查找物品
  ↓
收集所有占用格子
  ↓
从索引中移除对应项
  ↓
通知相关子系统清理
```

### 2.4 概念关系

```
UVoxelItemWorldSubsystem
├─ 数据层
│  ├─ GridToItemMap（空间索引表）
│  ├─ ItemCache（物品实例缓存）
│  └─ CachedSubsystems（子系统引用）
│
├─ 业务逻辑层
│  ├─ 物品管理（Spawn/Remove）
│  ├─ 空间管理（Occupied Cells）
│  └─ 碰撞检测（Collision Check）
│
├─ 协调层
│  ├─ BlockWorld协调
│  ├─ AreaWorld协调
│  └─ ConfigData协调
│
└─ 接口层
   ├─ 核心API（SpawnItemAtGrid等）
   └─ 查询API（GetItemAtGrid等）
```

---

## 3. 设计原则

### 3.1 架构原则

**空间管理优先**
- 高效的GridToItemMap空间索引
- O(1)时间复杂度的位置查询
- 支持多格物品的复杂占用模式

**子系统解耦**
- 通过接口与其它子系统交互
- 最小化直接依赖
- 保持系统独立性

**安全第一**
- 严格的碰撞检测
- 完整的空间验证
- 异常情况处理

### 3.2 接口设计原则

**统一放置接口**
- `SpawnItemAtGrid()`：标准放置流程
- 支持加载模式跳过检查
- 返回明确的成功/失败结果

**高效查询接口**
- `GetItemAtGrid()`：快速位置查询
- `CheckIsSpaceAvailable()`：预检查接口
- 支持批量操作优化

**批量操作支持**
- 保存加载时的批量创建
- 高效的索引更新
- 最小化性能开销

### 3.3 数据管理原则

**索引维护**
- 物品操作时自动更新索引
- 保证数据一致性
- 避免脏数据

**缓存优化**
- 缓存常用子系统引用
- 减少重复查找开销
- 支持动态更新

**内存管理**
- 及时清理无效引用
- 支持对象池复用
- 避免内存泄漏

### 3.4 性能原则

**快速位置查询**
- 使用TMap实现O(1)查询
- 避免遍历所有物品
- 支持范围查询

**批量操作优化**
- 合并多个放置请求
- 批量更新索引
- 减少函数调用开销

**空间计算优化**
- 预计算占用格子
- 缓存计算结果
- 避免重复计算

---

## 4. 系统约束

### 4.1 技术约束

**坐标系统**
- 必须使用 FIntVector 表示网格位置
- 与BlockWorld使用相同的坐标系
- 支持负坐标

**数据结构**
- GridToItemMap：`TMap<FIntVector, AUVoxelItemBaseInstance*>`
- 占用计算：`TArray<FIntVector>`
- 朝向支持：`EActorDirection` 枚举

**Actor管理**
- 继承自AActor
- 支持Blueprint可调用
- 需要清理引用

### 4.2 性能约束

**查询性能**
- 单个位置查询 < 1ms
- 空间检测 < 10ms（复杂物品）
- 批量操作 < 100ms（100个物品）

**内存使用**
- GridToItemMap内存占用 < 100MB（10000个物品）
- 支持动态增长
- 及时清理无效项

**并发限制**
- 游戏线程为主
- 支持多查询并发
- 避免写入竞争

### 4.3 业务约束

**放置规则**
- 不能放置到已占用位置
- 必须通过碰撞检测
- 支持特殊物品类型

**朝向规则**
- 只支持预设的4个朝向
- 多格物品根据朝向重新布局
- 避免非法旋转

**加载规则**
- 加载模式跳过碰撞检查
- 必须保持数据一致性
- 支持恢复场景状态

---

## 5. 设计目标

### 5.1 功能目标

**核心功能**
1. ✅ 物品生成和销毁管理
2. ✅ 空间占用索引维护
3. ✅ 碰撞检测和空间验证
4. ✅ 多格物品支持
5. ✅ 朝向系统支持
6. ✅ 子系统协调

**扩展功能**
1. ✅ 批量操作优化
2. ✅ 对象池集成
3. ✅ 事件通知系统
4. ✅ 调试和监控工具
5. ✅ 性能分析支持

### 5.2 质量目标

**可维护性**
- 清晰的职责划分
- 简单的接口设计
- 易于扩展新功能

**可测试性**
- 独立的测试接口
- 模拟外部依赖
- 性能基准测试

**可靠性**
- 完整的错误处理
- 数据一致性保证
- 异常情况恢复

### 5.3 性能目标

**查询性能**
- 单位置查询 < 1ms
- 空间检测 < 10ms
- 批量查询 < 50ms

**放置性能**
- 单物品放置 < 5ms
- 批量放置（100个）< 200ms
- 内存占用 < 100MB

**内存管理**
- 及时清理无效引用
- 支持对象池复用
- 内存使用可控

---

## 6. 与其他子系统的关系

### 6.1 与VoxelBlockWorldSubsystem的关系

```
VoxelItemWorldSubsystem
  ↓ 查询
基础方块碰撞信息
  ↓
VoxelBlockWorldSubsystem
  ↓ 返回
基础方块状态
```

**协作方式**：
- 放置前检查基础方块是否可放置
- 避免放置到不可穿越的方块上
- 支持特殊方块类型（如空地）

### 6.2 与VoxelAreaWorldSubsystem的关系

```
VoxelItemWorldSubsystem
  ↓ 管理
物品区域归属
  ↓
VoxelAreaWorldSubsystem
  ↓ 注册
区域物品列表
```

**协作方式**：
- 物品放置时关联到区域
- 支持跨区域物品
- 区域更新时同步物品状态

### 6.3 与ConfigDataSubsystem的关系

```
VoxelItemWorldSubsystem
  ↓ 查询
物品类型定义
  ↓
ConfigDataSubsystem
  ↓ 返回
FVoxelNormalItemBase 数据
```

**协作方式**：
- 获取物品的占用格子定义
- 查询物品的放置限制
- 支持物品类型的动态加载

### 6.4 与编辑系统的关系

```
编辑系统
  ↓ 发起
放置/移除请求
  ↓
VoxelItemWorldSubsystem
  ↓ 执行
空间检查 → 生成/销毁 → 更新索引
```

**交互接口**：
- `SpawnItemAtGrid()`：标准放置接口
- `RemoveItemAtGrid()`：移除接口
- `CheckIsSpaceAvailable()`：预检查

### 6.5 与保存系统的关系

```
保存系统
  ↓ 请求
批量创建物品
  ↓
VoxelItemWorldSubsystem
  ↓ 执行
加载模式放置 → 跳过检查
```

**特殊处理**：
- 支持bLoadFromSave参数
- 加载时不进行碰撞检测
- 保持数据一致性

V1 稳定身份与顺序：

- `ItemInstances` 是世界、容器与背包全部 ITEM 的唯一实例登记表，以 `FGuid ItemInstanceId` 为键；槽位只保存该 GUID，不复制实例数量或状态。
- 读档先恢复全部实例记录，再以加载模式创建放置 Actor 并重建 `ActivePlacedItems / GridToItemMap`，之后由区域和人物记录恢复容器／背包槽位引用；派生索引不入档。
- 非空引用缺失或重复时整档不激活，不用默认物品补齐。结构见[物品子系统架构 §9](/VoxelDesign/03-架构设计/VoxelItemWorldSubsystem-架构设计.md)，全局顺序见 [AI 决策系统架构 §7 存档](/VoxelDesign/03-架构设计/AI决策系统-架构设计.md)。

---

## 7. 关键算法

### 7.1 空间检测算法

```cpp
bool CheckIsSpaceAvailable(FIntVector OriginPos, EActorDirection Direction, const FVoxelNormalItemBase* StaticData)
{
    // 1. 计算物品占用的所有格子
    TArray<FIntVector> OccupiedCells = CalculateOccupiedCells(OriginPos, Direction, StaticData);

    // 2. 逐格检查
    for (const FIntVector& Cell : OccupiedCells)
    {
        // 3. 检查是否超出世界边界
        if (!IsBlockIndexInScene(Cell))
            return false;

        // 4. 检查基础方块是否可放置
        if (!BlockSys->CanPlaceItemOnBlock(Cell))
            return false;

        // 5. 检查是否与其他物品重叠
        if (GridToItemMap.Contains(Cell))
            return false;
    }

    return true;
}
```

### 7.2 占用格子计算算法

```cpp
TArray<FIntVector> CalculateOccupiedCells(FIntVector Origin, EActorDirection Dir, const FVoxelNormalItemBase* StaticData)
{
    TArray<FIntVector> Result;

    // 根据朝向对OccupiedBlocks进行旋转
    TArray<FIntVector> RotatedBlocks = RotateBlocks(StaticData->OccupiedBlocks, Dir);

    // 计算世界坐标
    for (const FIntVector& LocalBlock : RotatedBlocks)
    {
        Result.Add(Origin + LocalBlock);
    }

    return Result;
}
```

### 7.3 索引维护算法

```cpp
void UpdateItemIndex(AUVoxelItemBaseInstance* Item, FIntVector Position, EActorDirection Direction, const FVoxelNormalItemBase* StaticData)
{
    // 1. 移除旧索引
    if (Item->OldOccupiedBlocks.Num() > 0)
    {
        for (const FIntVector& OldPos : Item->OldOccupiedBlocks)
        {
            GridToItemMap.Remove(OldPos);
        }
    }

    // 2. 添加新索引
    TArray<FIntVector> NewOccupiedCells = CalculateOccupiedCells(Position, Direction, StaticData);
    for (const FIntVector& NewPos : NewOccupiedCells)
    {
        GridToItemMap.Add(NewPos, Item);
    }

    // 3. 更新缓存
    Item->OldOccupiedBlocks = NewOccupiedCells;
}
```

---

## 8. 使用示例

### 8.1 基本放置操作

```cpp
// 获取子系统
UVoxelItemWorldSubsystem* ItemSystem = GetWorld()->GetSubsystem<UVoxelItemWorldSubsystem>();
UConfigDataSubsystem* ConfigSystem = GetGameInstance()->GetSubsystem<UConfigDataSubsystem>();

// 准备放置数据（实例只存 HandleId，身份串→句柄在加载期已解析）
FInteractableData Data;
Data.HandleId = ConfigSystem->GetHandleByFullId(TEXT("core:item.table.wooden"));
Data.Position = FIntVector(100, 100, 0);
Data.Direction = EActorDirection::Forward;

// 获取物品静态数据（HandleId 查 ItemMap）
FVoxelNormalItemBase* ItemData = ConfigSystem->GetItemTypeById<FVoxelNormalItemBase>(Data.HandleId);
if (ItemData)
{
    // 检查空间
    if (ItemSystem->CheckIsSpaceAvailable(Data.Position, Data.Direction, ItemData))
    {
        // 生成物品
        AUVoxelItemBaseInstance* Item = ItemSystem->SpawnItemAtGrid(Data);
        if (Item)
        {
            UE_LOG(LogTemp, Log, TEXT("物品生成成功"));
        }
    }
}
```

### 8.2 批量加载场景

```cpp
// 从保存数据加载
for (const FItemSaveData& SaveData : SavedItems)
{
    FInteractableData Data;
    Data.ItemTag = SaveData.ItemTag;
    Data.Position = SaveData.Position;
    Data.Direction = SaveData.Direction;

    // 使用加载模式，跳过碰撞检查
    AUVoxelItemBaseInstance* Item = ItemSystem->SpawnItemAtGrid(Data, true);
    if (Item)
    {
        // 恢复物品状态
        Item->SetQuantity(SaveData.Quantity);
    }
}
```

### 8.3 空间查询

```cpp
// 查询指定位置的物品
AUVoxelItemBaseInstance* Item = ItemSystem->GetItemAtGrid(FIntVector(100, 100, 0));
if (Item)
{
    UE_LOG(LogTemp, Log, TEXT("该位置有物品: %s"), *Item->ItemInstance.ItemTag.ToString());
}

// 检查是否可放置
bool CanPlace = ItemSystem->CheckIsSpaceAvailable(Position, Direction, ItemData);
```

---

## 9. 调试和监控

### 9.1 调试工具

**空间索引可视化**
- 显示GridToItemMap的内容
- 高亮显示占用格子
- 显示物品边界

**性能监控**
- 查询时间统计
- 内存使用跟踪
- 操作频率监控

**错误检查**
- 索引一致性验证
- 重复占用检测
- 孤立物品清理

### 9.2 日志记录

**操作日志**
- 物品生成/移除记录
- 空间检测结果
- 性能关键操作

**错误日志**
- 碰撞失败原因
- 索引更新异常
- 系统协调问题

---

## 10. 扩展设计

### 10.1 未来扩展

**动态物品类型**
- 运行时添加新的物品类型
- 支持自定义占用模式
- 动态调整物品属性

**高级空间管理**
- 3D空间索引优化
- 动态加载卸载
- 范围查询支持

**事件系统**
- 物品放置事件
- 空间变更通知
- 状态同步机制

### 10.2 性能优化

**空间分区**
- 将世界划分为区域
- 按需加载空间索引
- 减少内存占用

**批量操作**
- 支持批量放置
- 异步处理
- 优先级队列

**缓存策略**
- 热点物品缓存
- 预计算占用格子
- 智能预加载

---

## 11. 相关文档

- [VoxelItemWorldSubsystem-架构设计](/VoxelDesign/03-架构设计/VoxelItemWorldSubsystem-架构设计.md)
- [物品数据模型-系统设计](/VoxelDesign/02-系统设计/物品数据模型-系统设计.md)
- [VoxelBlockWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelBlockWorldSubsystem-系统设计.md)
- [ConfigDataSubsystem-数据管理-分析报告](/VoxelDesign/08-分析报告/ConfigDataSubsystem-数据管理-分析报告.md)

---

**文档版本**: 1.0
**最后更新**: 2026-01-24
**状态**: ✅ 系统设计已完成