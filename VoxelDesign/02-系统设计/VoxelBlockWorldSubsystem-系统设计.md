# VoxelBlockWorldSubsystem-系统设计

**版本**: 1.0
**创建日期**: 2026-01-24
**最后更新**: 2026-01-24

---

## 1. 系统上下文

### 1.1 系统边界

**VoxelBlockWorldSubsystem负责**：
- 体素世界的数据存储和管理（运行时）
- 区块（Chunk）的流式加载和卸载
- 体素编辑操作的单体和批量处理
- 区块网格的生成和更新
- 多线程任务调度和队列管理
- 与导航系统的集成

**VoxelBlockWorldSubsystem不负责**：
- 世界数据的持久化存储（由Save/Load系统负责）
- 玩家输入处理（由PlayerController负责）
- 网格渲染（由Chunk Actor负责）
- UI显示（由UI系统负责）
- 历史记录（由HistoryComponent负责）

### 1.2 外部依赖

**依赖的系统**：
- **UObjectPoolSubsystem**：Chunk Actor对象池管理
- **UConfigDataSubsystem**：方块类型定义和配置
- **UVoxelGameInstance**：场景数据（SceneData）访问
- **UNavigationSystemV1**：导航网格更新

**被依赖的系统**：
- **编辑系统**：体素编辑操作
- **预览系统**：网格数据查询
- **AI系统**：导航数据访问
- **物理系统**：碰撞数据查询

### 1.3 输入输出

**输入**：
- 玩家位置（流式加载触发）
- 体素编辑请求（单体或批量）
- 区块加载/卸载请求
- 网格更新请求

**输出**：
- 区块数据（FChunkData）
- 网格数据（FChunkMeshData）
- 方块类型查询（FGameplayTag）
- 世界边界信息（FBox）
- 区块状态（EChunkState）

---

## 2. 核心概念

### 2.1 基础概念

**区块（Chunk）**
- 定义：世界的逻辑划分单元，固定大小的立方体区域
- 类型：`FChunkData`（元数据）+ `AChunkBase`（Actor）
- 尺寸：`ChunkSize`（可配置，如32×32×32）
- 状态机：Unloaded → Loading → Generating → Calculating → Rendering → Loaded → Unloading

**体素（Voxel）**
- 定义：世界的最小数据单位，表示一个方块的类型
- 存储：使用整数ID（BlockId）索引
- 类型：使用`FGameplayTag`标识（如`Item.Block.Stone`）
- 坐标：世界坐标索引（FIntVector）

**快照上下文（ChunkGenContext）**
- 定义：区块数据的线程安全快照
- 组成：
  - `RawBlockData`：扁平化的方块类型数组
  - `BlockDefs`：方块渲染信息映射
  - `PaddedSize`：包含邻居边界的尺寸
- 用途：后台线程网格生成，无锁访问

### 2.2 组合概念

**流式加载（Streaming）**
- 基于玩家位置的动态加载/卸载
- 多级队列：Loading → Generating → Calculating → Rendering
- 优先级系统：EChunkLoadPriority（Low/Medium/High）
- 加载距离：可配置的Chunk半径

**体素编辑模型**
- **单体编辑**：`ModifyVoxelData(BlockIndex, EditMode, TypeID)`
- **批量编辑**：`BatchModifyVoxelData_Modification(Modification)`
- **修改数据结构**：`FVoxelEditModification`
  - `TMap<FIntVector, FGameplayTag>` Edits
  - `TSet<FIntVector>` ChunkIndices
  - `EVoxelControllerEditState` EditMode

**多级队列系统**
- **LoadingQueue**：待加载数据
- **GeneratingQueue**：待生成内容
- **CalculateQueue**：待计算网格
- **RenderingQueue**：待渲染更新
- **UnloadingQueue**：待卸载区块

**线程安全机制**
- **读写锁**：`FRWLock` 保护 `CurSceneBlockTypeMask`
- **临界区**：`FCriticalSection` 保护任务队列
- **快照模式**：`FChunkGenContext` 用于后台线程

### 2.3 行为概念

**区块生命周期**
```
Unloaded（未加载）
  ↓ 玩家接近 / 请求加载
Loading（加载数据）
  ↓ 数据加载完成
Generating（生成内容）
  ↓ 生成完成
Calculating（计算网格）
  ↓ 计算完成
Rendering（渲染）
  ↓ 渲染完成
Loaded（已加载）
  ↓ 玩家远离 / 请求卸载
Unloading（卸载中）
  ↓
Unloaded（未加载）
```

**体素编辑流程**
```
玩家编辑请求
  ↓
ModifyVoxelData() / BatchModifyVoxelData_Modification()
  ↓
添加到 PendingModifications 队列
  ↓
ProcessBlockEdits()（Tick中处理）
  ↓
遍历修改，更新 CurSceneBlockTypeMask
  ↓
标记受影响的区块
  ↓
modifyChunk(ChunkIndex)
  ↓
添加到 CalculateQueue / RenderingQueue
  ↓
重新生成网格
```

**流式加载触发**
```
Tick()
  ↓
CheckPlayersMovement()
  ↓
检测玩家是否跨越Chunk边界
  ↓
RequestStreamingUpdate()
  ↓
计算需要加载/卸载的Chunk
  ↓
添加到对应队列
  ↓
ProcessChunkStreaming()
```

### 2.4 概念关系

```
UVoxelBlockWorldSubsystem
├─ 数据层
│  ├─ ChunkMetaDataMap（区块元数据）
│  ├─ CurSceneBlockTypeMask（运行时修改）
│  └─ SceneBaseBlockTypeMask（基础地形）
│
├─ 队列层
│  ├─ LoadingQueue
│  ├─ GeneratingQueue
│  ├─ CalculateQueue
│  ├─ RenderingQueue
│  └─ UnloadingQueue
│
├─ 任务层
│  ├─ ProcessChunkStreaming（总控）
│  ├─ ProcessLoading（数据加载）
│  ├─ ProcessGenerating（内容生成）
│  ├─ ProcessCalculateUpdates（网格计算）
│  └─ ProcessRenderUpdates（网格渲染）
│
└─ 线程安全层
   ├─ FRWLock（读写锁）
   ├─ FChunkGenContext（快照）
   └─ FCriticalSection（任务锁）
```

---

## 3. 设计原则

### 3.1 架构原则

**流式优先**
- 基于玩家位置的动态加载
- 只加载视野范围内的区块
- 及时卸载远离的区块释放内存

**多线程优化**
- 后台线程计算网格
- 快照上下文避免锁竞争
- 队列化解耦任务

**分层队列**
- 多级队列管道
- 每级独立处理
- 优先级控制

### 3.2 接口设计原则

**统一编辑接口**
- 单体编辑和批量编辑使用统一底层
- 支持多种编辑模式
- 线程安全

**批量操作优化**
- 合并相同区块的编辑
- 批量提交网格更新
- 减少函数调用开销

**异步非阻塞**
- 编辑操作立即返回
- 后台队列处理
- Tick中逐步处理

### 3.3 数据管理原则

**索引计算**
- BlockId = `ChunkUtilsFunction::GetBlockIdByIndex(BlockIndex, WorldDimension)`
- ChunkIndex = `BlockIndex / ChunkSize`
- 快速整数运算

**双层存储**
- `SceneBaseBlockTypeMask`：基础地形（只读）
- `CurSceneBlockTypeMask`：玩家修改（读写）
- 查询时优先返回修改层

**快照机制**
- 网格生成前创建快照
- 后台线程无锁访问
- 避免数据竞争

### 3.4 性能原则

**按需计算**
- 只计算可见面（剔除被遮挡的面）
- 只更新编辑影响的区块
- 避免全量更新

**队列节流**
- 单帧处理有限数量任务
- 避免卡顿
- 动态调整并发数

**对象池复用**
- Chunk Actor复用
- 网格数据复用
- 减少GC压力

---

## 4. 系统约束

### 4.1 技术约束

**坐标系统**
- 必须使用 INDEX（`FIntVector`）表示方块位置
- 区块索引也是整数坐标
- 坐标转换使用整数除法

**数据结构**
- 区块元数据：`TMap<FIntVector, FChunkData>`
- 方块映射：`TMap<int, FGameplayTag>`
- 队列：`TQueue<FChunkLoadRequest>`

**线程模型**
- 游戏线程：编辑操作、队列管理
- 后台线程：网格计算
- 使用读写锁保护共享数据

**网格生成**
- 必须使用 `FChunkGenContext` 快照
- 后台线程不能修改原始数据
- 结果通过回调传回游戏线程

### 4.2 性能约束

**加载距离**
- 默认加载半径：5-10个区块
- 可根据性能动态调整
- 考虑硬件能力

**队列大小**
- 单个队列不超过100个任务
- 超过限制时降低优先级
- 防止内存溢出

**网格生成**
- 单个区块网格生成 < 50ms
- 批量编辑 < 100ms
- 帧率保持 > 60 FPS

**并发限制**
- 默认 MaxConcurrentTasks = 1
- 可根据CPU核心数调整
- 避免线程过多导致上下文切换

### 4.3 业务约束

**编辑约束**
- 不能编辑未加载的区块
- 编辑后立即可见
- 支持撤销/重做（通过HistoryComponent）

**世界边界**
- 不能编辑边界外的方块
- 检查 `IsBlockIndexInScene()`
- 返回错误或忽略

**区块状态**
- 不能卸载正在编辑的区块
- 不能重复加载已加载的区块
- 状态转换必须合法

---

## 5. 设计目标

### 5.1 功能目标

**核心功能**
1. ✅ 区块流式加载/卸载
2. ✅ 单体素编辑
3. ✅ 批量体素编辑（多种形状）
4. ✅ 区块网格生成
5. ✅ 玩家位置跟踪
6. ✅ 导航系统集成

**扩展功能**
1. ✅ 多线程网格计算
2. ✅ 优先级队列系统
3. ✅ 对象池复用
4. ✅ 快照机制优化
5. ✅ 动态并发控制

### 5.2 质量目标

**可维护性**
- 清晰的队列流转
- 独立的任务处理
- 易于扩展新的编辑类型

**可测试性**
- 单元测试覆盖核心逻辑
- 模拟流式加载场景
- 性能基准测试

### 5.3 性能目标

**加载性能**
- 启动时加载时间 < 5秒
- 玩家移动时区块加载延迟 < 100ms
- 内存占用 < 1GB（中等规模世界）

**编辑性能**
- 单体编辑响应 < 1ms
- 批量编辑（1000方块）< 50ms
- 网格更新延迟 < 100ms

**渲染性能**
- 可视区块数量 < 200个
- 网格生成吞吐量 > 20区块/秒
- 帧率稳定在 60 FPS

---

## 6. 与其他子系统的关系

### 6.1 与ObjectPoolSubsystem的关系

```
VoxelBlockWorldSubsystem
  ↓ 请求
区块Actor
  ↓
ObjectPoolSubsystem
  ↓ 提供
AChunkBase 实例（复用）
```

**职责划分**：
- VoxelBlockWorldSubsystem：管理区块生命周期
- ObjectPoolSubsystem：提供对象复用

### 6.2 与ConfigDataSubsystem的关系

```
VoxelBlockWorldSubsystem
  ↓ 查询
方块类型定义
  ↓
ConfigDataSubsystem
  ↓ 返回
FVoxelBlockItemBase 数据
```

**职责划分**：
- VoxelBlockWorldSubsystem：存储方块ID映射
- ConfigDataSubsystem：提供方块属性定义

### 6.3 与编辑系统的关系

```
编辑系统
  ↓ 发起
体素编辑请求（FVoxelEditModification）
  ↓
VoxelBlockWorldSubsystem
  ↓ 执行
更新方块映射 → 重新生成网格
  ↓ 通知
区块更新完成
```

**接口**：
- `BatchModifyVoxelData_Modification(const FVoxelEditModification&)`
- 编辑系统负责构建修改数据
- BlockWorld负责执行和网格更新

### 6.4 与导航系统的关系

```
VoxelBlockWorldSubsystem
  ↓ 通知
区块更新
  ↓
NavigationSystemV1
  ↓ 更新
NavMesh 数据
```

**职责划分**：
- VoxelBlockWorldSubsystem：检测需要更新的区块
- NavigationSystemV1：更新导航网格

### 6.5 与预览系统的关系

```
预览系统
  ↓ 查询
区块网格数据
  ↓
VoxelBlockWorldSubsystem
  ↓ 返回
FChunkMeshData
```

**接口**：
- `GenerateMeshData(const FChunkGenContext&)`
- 提供快照上下文
- 后台线程安全计算

---

## 7. 相关文档

- [VoxelBlockWorldSubsystem-架构设计](/VoxelDesign/03-架构设计/VoxelBlockWorldSubsystem-架构设计.md)（待创建）
- [编辑系统-系统设计](/VoxelDesign/02-系统设计/编辑系统-系统设计.md)
- [WorldObjectPoolSubsystem-系统设计](/VoxelDesign/02-系统设计/WorldObjectPoolSubsystem-系统设计.md)（待创建）
- [ConfigDataSubsystem-数据管理-分析报告](/VoxelDesign/08-分析报告/ConfigDataSubsystem-数据管理-分析报告.md)

---

**文档版本**: 1.0
**最后更新**: 2026-01-24
**状态**: ✅ 系统设计已完成
