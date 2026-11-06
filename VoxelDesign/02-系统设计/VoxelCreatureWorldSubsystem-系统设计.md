# VoxelCreatureWorldSubsystem-系统设计

**版本**: 1.1
**创建日期**: 2026-01-24
**最后更新**: 2026-10-31（v1.1：需求运行时索引迁移到六条 Creature.Demand.* GameplayTag）

---

## 1. 系统上下文

### 1.1 系统边界

**VoxelCreatureWorldSubsystem负责**：
- NPC生物的生成、生命周期管理和移除
- 定时器驱动的持续生成系统
- 对象池的注册和管理（RegisterCreaturePool）
- 生物数据的初始化和配置
- 与区域的集成（SpawnAreaId）
- 生物数量的限制控制

**VoxelCreatureWorldSubsystem不负责**：
- 生物的AI行为逻辑（由AIController负责）
- 生物的战斗系统（由CombatSystem负责）
- 生物的社交交互（由SocialSystem负责）
- 生物的对话系统（由DialogueSystem负责）
- 生物的装备管理（由InventorySystem负责）

### 1.2 外部依赖

**依赖的系统**：
- **UObjectPoolSubsystem**：生物对象池管理
- **UVoxelAreaWorldSubsystem**：生成区域数据访问
- **UVoxelGameInstance**：世界配置（BlockLength）
- **UCreatureGeneratorUtils**：生物数据生成
- **UConfigDataSubsystem**：生物类型定义（通过CreatureClass）

**被依赖的系统**：
- **生物生成系统**：通过定时器持续生成
- **编辑系统**：移除特定生物
- **场景管理**：清理所有生物

### 1.3 输入输出

**输入**：
- 生物生成定时器间隔
- 生物数量上限（CreatureNumLimit；V1 仅作并发与对象池上限，不作为人口生成来源）
- 名册点名生成 / 回收请求（V1 人口来源 = 固定名册）
- 生物类型（CreatureClass）
- 生成区域ID（SpawnAreaId）
- 移除请求（CreatureID）

**输出**：
- 活跃生物列表（InScenceCreatureList）
- 生成事件日志
- 移除操作结果
- 对象池状态

---

## 2. 核心概念

### 2.1 基础概念

**生物（Creature）**
- 定义：游戏中的NPC智能体
- 类型：`AVoxelNpcBaseInstance`（基类）或 `AVoxelIntelligentCreatureInstance`（智能类型）
- 数据：`FCreatureGenerationResult` 包含种族、职业、名称等属性
- 状态：休眠、激活、死亡等生命周期状态

**对象池（Object Pool）**
- 定义：生物实例的复用系统
- 名称："NpcPool"
- 缓冲大小：`CreatureNumLimit + 2`
- 创建/重置函数：`CreateFunc` / `ResetFunc`

**定时器生成器（Timer Spawner）**
- 定义：基于时间的持续生成系统
- 间隔：0.2秒（防止帧尖峰）
- 触发条件：数量未达上限且类有效
- 处理：OnSpawnTimerTick → SpawnCreature

> **V1 人口来源**：顾客与员工都来自开局生成的固定名册（30–50 人），生物子系统按名册点名生成与回收，`CreatureNumLimit` 只作并发与对象池上限，不用于随机补充人口；离场只回收实体并保留名册业务数据，`CreatureID` 保持稳定。

**生成区域（Spawn Area）**
- 定义：生物生成的空间范围
- 数据：`AVoxelAreaBaseInstance` 的 AreaCoverage
- 位置：FIntVector Min/Max 定义立方体范围
- 方向：SpawnAreaComponent 提供生成朝向

**需求标签（Demand Tags）**
- 定义：六条 `Creature.Demand.Hunger / Thirst / Energy / Leisure / Social / Shopping` 的 `FGameplayTag`
- 用途：静态缓存 Tag，供需求推进、跨阈检测与物品回填查询；不再以旧 7 值 `EDemandEnum` 作为运行时索引
- 管理：`InitializeDemandTags()` 初始化并校验六条标签

### 2.2 组合概念

**生物生成流程**
```
定时器触发
  ↓
OnSpawnTimerTick()
  ↓
清理无效引用
  ↓
检查数量限制
  ↓
SpawnCreature()
  ↓
从池中获取对象
  ↓
生成生物数据
  ↓
设置位置和激活
  ↓
加入管理列表
```

**对象池管理**
```
RegisterCreaturePool()
  ↓
定义创建函数
  ↓
定义重置函数
  ↓
向ObjectPool子系统注册
  ↓
获取时：PoolSys->Acquire()
  ↓
释放时：PoolSys->Release()
```

**生物生命周期管理**
```
生成（Spawn）
  ↓
激活（ForceWakeUp）
  ↓
运行（Tick）
  ↓
移除（RemoveCreature）
  ↓
回收（Pool Release）
```

**区域位置选择**
```
SetSpawnId()
  ↓
检查SpawnArea有效性
  ↓
随机选择区域或保持当前
  ↓
SetCreatureSpawnTransform()
  ↓
GetRandomPositionInArea()
  ↓
返回最终Transform
```

### 2.3 行为概念

**生物生成状态机**
```
开始
  ↓
定时器启动
  ↓
等待触发
  ↓
生成中
  ↓
激活完成
  ↓
运行中
  ↓
移除/回收
  ↓
结束
```

**对象池状态转换**
```
对象池
  ↓
创建（CreateFunc）
  ↓
就绪
  ↓
获取（Acquire）
  ↓
使用中
  ↓
释放（Release）
  ↓
重置（ResetFunc）
  ↓
就绪
```

**生物数量控制**
```
每帧检查
  ↓
InScenceCreatureList.Num()
  ↓
>= CreatureNumLimit?
  ↓
是 → 跳过
  ↓
否 → 继续生成
```

**无效引用清理**
```
定时器触发
  ↓
遍历 InScenceCreatureList
  ↓
检查 IsValid()
  ↓
否 → RemoveCurrent()
  ↓
是 → 跳过
```

### 2.4 概念关系

```
UVoxelCreatureWorldSubsystem
├─ 数据层
│  ├─ InScenceCreatureList（生物管理）
│  ├─ SpawnTimerHandle（定时器控制）
│  └─ CreatureNumLimit（数量限制）
│
├─ 生成层
│  ├─ SpawnCreature（核心生成）
│  ├─ OnSpawnTimerTick（定时触发）
│  └─ SetSpawnId（区域选择）
│
├─ 对象池层
│  ├─ RegisterCreaturePool（池注册）
│  ├─ CreateFunc（创建函数）
│  └─ ResetFunc（重置函数）
│
├─ 位置层
│  ├─ SetCreatureSpawnTransform（位置计算）
│  └─ GetRandomPositionInArea（随机位置）
│
└─ 生命周期层
   ├─ ForceWakeUp（激活）
   ├─ ResetCreature（重置）
   └─ RemoveCreature（移除）
```

---

## 3. 设计原则

### 3.1 架构原则

**对象池优先**
- 所有生物从对象池获取，避免频繁创建销毁
- 完整的重置机制确保对象状态一致性
- 缓冲区设计应对突发峰值

**定时器节流**
- 固定0.2秒间隔防止帧率波动
- 懒惰清理无效引用，减少遍历开销
- 数量限制防止性能问题

**区域集成**
- 优先使用指定的SpawnAreaId
- 失败时随机选择可用区域
- 防止生成在无效区域

### 3.2 接口设计原则

**统一管理接口**
- 所有生物操作通过系统接口
- 统一的移除和回收流程
- 蓝图可调用的公共函数

**异步生成模式**
- 生成操作不阻塞主线程
- 定时器驱动非阻塞流程
- 错误处理和回退机制

**数据驱动初始化**
- 使用CreatureGeneratorUtils生成RPG数据
- 支持随机配置和预设配置
- 数据与表现分离

### 3.3 数据管理原则

**弱引用管理**
- 使用TWeakObjectPtr避免循环引用
- 定期清理无效引用
- 安全的指针检查

**生命周期控制**
- 明确的激活/休眠状态
- 组件级别的启用/禁用
- 物理引擎的彻底关闭

**ID映射管理**
- 使用CreatureID作为键值
- 快速查找和移除
- 支持批量清理

### 3.4 性能原则

**生成节流**
- 定时器间隔控制生成速率
- 数量限制控制内存占用
- 懒惰清理减少开销

**对象复用**
- 完整的对象池机制
- 快速的获取/释放操作
- 状态完全重置

**位置优化**
- 基于区域的批量位置计算
- 随机位置生成优化
- 空间索引支持（未来扩展）

---

## 4. 系统约束

### 4.1 技术约束

**数据结构**
- 生物列表：`TMap<int, TWeakObjectPtr<AVoxelNpcBaseInstance>>`
- 定时器：`FTimerHandle`
- 对象池：必须注册到UObjectPoolSubsystem

**线程模型**
- 游戏线程：所有管理操作
- 不涉及后台线程（与BlockWorld不同）
- 定时器在游戏线程执行

**组件管理**
- 生成时必须完全冻结物理和移动
- 重置时必须彻底关闭所有组件
- 使用ForceWakeUp激活而不是直接启用

### 4.2 性能约束

**生成频率**
- 最大生成频率：5个/秒（0.2秒间隔）
- 单次生成时间：< 50ms
- 内存占用：生物数量 ≤ CreatureNumLimit

**对象池大小**
- 默认缓冲：CreatureNumLimit + 2
- 创建位置：FVector(0, 0, -100000)
- 重置位置：返回池中隐藏位置

**清理开销**
- 每次生成前清理无效引用
- 不使用立即清理，采用懒惰策略
- 遍历复杂度：O(n)，n为生物数量

### 4.3 业务约束

**生物类型**
- 必须继承自AVoxelNpcBaseInstance
- 必须支持AVoxelIntelligentCreatureInstance接口
- 必须实现InitializeFromGenerationResult方法

**生成区域**
- 必须在UVoxelAreaWorldSubsystem注册
- AreaCoverage必须有效
- 可选SpawnAreaComponent指定朝向

**数据依赖**
- 依赖CreatureGeneratorUtils生成数据
- 必须支持FCreatureGenerationResult数据结构
- 依赖六条 `Creature.Demand.*` GameplayTag 与对应 `FRuntimeDemandRule` 配置

---

## 5. 设计目标

### 5.1 功能目标

**核心功能**
1. ✅ 定时器驱动的生物生成
2. ✅ 对象池复用机制
3. ✅ 生物数量限制控制
4. ✅ 基于区域的生成位置
5. ✅ 生物移除和回收
6. ✅ 批量生物清理

**扩展功能**
1. ✅ 需求枚举静态缓存
2. ✅ 懒惰无效引用清理
3. ✅ 生成回退机制
4. ✅ 区域随机选择
5. ✅ 完整的重置流程

### 5.2 质量目标

**可维护性**
- 清晰的生成流程
- 完整的异常处理
- 模块化的组件设计

**可测试性**
- 单独的生成逻辑可测试
- 对象池功能可模拟
- 位置生成可验证

### 5.3 性能目标

**生成性能**
- 生成延迟 < 50ms
- 定时器开销 < 1ms/帧
- 内存占用稳定

**管理性能**
- 移除操作 < 10ms
- 清理操作 < 5ms
- 查找操作 < 1ms

**渲染性能**
- 生成的生物数量可控
- 不影响主线程帧率
- 物理开销最小化

---

## 6. 与其他子系统的关系

### 6.1 与ObjectPoolSubsystem的关系

```
VoxelCreatureWorldSubsystem
  ↓ 请求创建/重置
对象池管理
  ↓ 提供
AVoxelNpcBaseInstance 实例
```

**职责划分**：
- VoxelCreatureWorldSubsystem：定义生物生命周期逻辑
- ObjectPoolSubsystem：提供对象复用机制

### 6.2 与VoxelAreaWorldSubsystem的关系

```
VoxelCreatureWorldSubsystem
  ↓ 查询区域
SpawnArea 数据
  ↓ 返回
生成位置和方向
```

**职责划分**：
- VoxelCreatureWorldSubsystem：使用区域进行生成
- VoxelAreaWorldSubsystem：提供区域数据

### 6.3 与CreatureGeneratorUtils的关系

```
VoxelCreatureWorldSubsystem
  ↓ 请求数据生成
CreatureGeneratorUtils
  ↓ 返回
FCreatureGenerationResult
```

**职责划分**：
- VoxelCreatureWorldSubsystem：管理生物生成流程
- CreatureGeneratorUtils：生成生物属性数据

### 6.4 与GameInstance的关系

```
VoxelCreatureWorldSubsystem
  ↓ 查询配置
UVoxelGameInstance
  ↓ 返回
世界配置（BlockLength）
```

**职责划分**：
- VoxelCreatureWorldSubsystem：使用配置数据
- UVoxelGameInstance：提供全局配置

### 6.5 与AI系统的关系

```
VoxelCreatureWorldSubsystem
  ↓ 创建生物
AIController
  ↓ 控制行为
生物AI逻辑
```

**职责划分**：
- VoxelCreatureWorldSubsystem：创建和管理生物
- AIController：控制生物的行为逻辑

---

## 7. 相关文档

- [VoxelBlockWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelBlockWorldSubsystem-系统设计.md)
- [VoxelAreaWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelAreaWorldSubsystem-系统设计.md)（待创建）
- [CreatureGeneratorUtils-工具设计](/VoxelDesign/02-系统设计/CreatureGeneratorUtils-工具设计.md)（待创建）
- [VoxelNpcBaseInstance-类设计](/VoxelDesign/03-架构设计/VoxelNpcBaseInstance-架构设计.md)（待创建）

---

**文档版本**: 1.0
**最后更新**: 2026-01-24
**状态**: ✅ 系统设计已完成