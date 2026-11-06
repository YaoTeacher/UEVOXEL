# WorldObjectPoolSubsystem-系统设计

**版本**: 1.0
**创建日期**: 2026-01-24
**最后更新**: 2026-01-24

---

## 1. 系统上下文

### 1.1 系统边界

**WorldObjectPoolSubsystem负责**：
- 模板化对象池的创建和管理
- 对象的获取（Acquire）和释放（Release）
- 动态池大小调整
- 统计信息和监控
- 对象的生命周期管理

**WorldObjectPoolSubsystem不负责**：
- 对象的创建逻辑（由调用者提供）
- 对象的具体业务逻辑
- 持久化存储
- 网络同步（由网络系统负责）
- 垃圾回收（GC）的强制执行

### 1.2 外部依赖

**依赖的系统**：
- **UObjectSubsystem**：基础对象管理
- **UConfigDataSubsystem**：配置数据访问
- **UVoxelGameInstance**：游戏实例生命周期管理

**被依赖的系统**：
- **VoxelBlockWorldSubsystem**：区块Actor池管理
- **VoxelAreaWorldSubsystem**：区域对象池管理
- **VoxelCreatureWorldSubsystem**：生物对象池管理
- **VoxelItemWorldSubsystem**：物品对象池管理

### 1.3 输入输出

**输入**：
- 泛型类型 `T*`（对象池注册）
- 池名称 `FName`（标识符）
- 预热数量 `int`（可选）
- 池大小配置（容量限制）

**输出**：
- 对象指针 `T*`
- 统计信息（TotalCount, ActiveCount, FreeCount）
- 错误状态（成功/失败）
- 池使用率

---

## 2. 核心概念

### 2.1 基础概念

**对象池（Object Pool）**
- 定义：预先创建一组对象实例，复用而非重复创建
- 类型：模板化设计 `FObjectPool<T>`
- 组成：空闲列表 + 活跃列表 + 配置信息
- 优势：减少GC压力，提高性能

**对象生命周期**
- **创建阶段**：调用 `Acquire()` 从池中获取对象
- **使用阶段**：对象被外部系统使用
- **释放阶段**：调用 `Release()` 归还对象到池中
- **销毁阶段**：池销毁时清理所有对象

**池统计信息**
- `TotalCount`：池中总对象数（Active + Free）
- `ActiveCount`：正在使用的对象数
- `FreeCount`：可用的空闲对象数
- `UsageRate`：使用率 = ActiveCount / TotalCount

### 2.2 组合概念

**模板化池管理**
- `RegisterPool<T>()`：注册新对象池
- `Acquire<T>()`：获取类型为T的对象
- `Release<T>()`：释放类型为T的对象
- 泛型实现，支持多种类型

**动态调整策略**
- **自动扩展**：空闲数低于阈值时自动扩展
- **自动收缩**：空闲数过多时收缩
- **容量限制**：可配置最大容量
- **收缩延迟**：避免频繁调整

**预热机制**
- `PreWarm()`：启动时预创建对象
- 预热数量可配置
- 减少首次获取延迟
- 平衡内存和性能

### 2.3 行为概念

**对象获取流程**
```
请求 Acquire<T>()
  ↓
查找对应类型的池
  ↓
检查是否有空闲对象
  ↓
有：从 FreeObjects 取出
  ↓
无：检查是否可创建新对象
  ↓
可创建：新建对象并返回
  ↓
不可创建：等待或返回失败
  ↓
将对象移到 ActiveObjects
  ↓
返回对象指针
```

**对象释放流程**
```
请求 Release<T*>(ObjectPtr)
  ↓
验证对象有效性
  ↓
从 ActiveObjects 移除
  ↓
检查池容量
  ↓
未满：加入 FreeObjects
  ↓
已满：销毁对象
  ↓
更新统计信息
```

**池注册流程**
```
调用 RegisterPool<T>(PoolName, MaxSize)
  ↓
创建 FObjectPool<T> 实例
  ↓
初始化配置（MaxSize, GrowStrategy）
  ↓
添加到 Pools 映射
  ↓
返回注册成功
```

### 2.4 概念关系

```
WorldObjectPoolSubsystem
├─ 类型管理层
│  ├─ TMap<FName, TSharedPtr<FObjectPoolBase>>
│  │  └─ 池基类接口
│  └─ TMap<FString, TSharedPtr<FObjectPool<T>>>
│     └─ 模板化池实现
│
├─ 对象管理层
│  ├─ FreeObjects（空闲列表）
│  └─ ActiveObjects（活跃列表）
│
└─ 管理控制层
   ├─ RegisterPool（池注册）
   ├─ Acquire/Release（对象操作）
   ├─ PreWarm（预热）
   └─ 统计监控
```

---

## 3. 设计原则

### 3.1 架构原则

**泛型设计**
- 模板化支持任意 UObject 类型
- 统一的接口设计
- 类型安全的对象管理

**资源优化**
- 减少动态内存分配
- 复用对象实例
- 按需扩展池大小

**统计驱动**
- 实时监控池使用情况
- 基于统计数据优化配置
- 支持调试和性能分析

### 3.2 接口设计原则

**简洁统一**
- 所有类型使用相同接口
- 封装复杂性，提供简单API
- 异常情况明确反馈

**线程安全**
- 多线程环境下的安全访问
- 使用适当的同步机制
- 避免死锁和竞争

**性能优先**
- 快速的对象获取和释放
- 最小化锁的持有时间
- 无锁设计当可能时

### 3.3 数据管理原则

**高效存储**
- 使用 TMap 快速查找池
- 数组存储空闲对象
- 指针管理对象所有权

**内存管理**
- 对象所有权清晰
- 自动清理未使用对象
- 避免内存泄漏

**生命周期管理**
- 对象创建/销毁可追踪
- 支持对象重置
- 支持自定义清理逻辑

### 3.4 性能原则

**延迟创建**
- 首次获取时才创建对象
- 避免启动时内存占用过高
- 按需分配资源

**批量操作**
- 支持批量获取和释放
- 减少函数调用开销
- 提高吞吐量

**智能调整**
- 根据使用模式自动调整
- 学习最佳池大小
- 平衡内存和性能

---

## 4. 系统约束

### 4.1 技术约束

**类型系统**
- 只支持继承自 UObject 的类型
- 模板类型必须明确指定
- 不能混合不同类型

**数据结构**
- 池映射：`TMap<FName, TSharedPtr<FObjectPoolBase>>`
- 空闲列表：`TArray<T*> FreeObjects`
- 活跃列表：`TArray<T*> ActiveObjects`

**同步机制**
- 使用 FCriticalSection 保护共享数据
- 池操作必须是线程安全的
- 避免在持有锁时执行耗时操作

**内存管理**
- UObject 使用标准 UE 内存管理
- 池销毁时自动清理对象
- 避免循环引用

### 4.2 性能约束

**获取延迟**
- 单对象获取 < 1ms
- 批量获取（100个）< 10ms
- 不影响主线程性能

**内存使用**
- 单个池占用不超过 100MB
- 总系统内存占用 < 500MB
- 可配置的内存限制

**并发限制**
- 默认最大并发获取数 = 16
- 可根据硬件调整
- 避免线程过多导致性能下降

**池大小**
- 默认初始大小 = 10
- 默认最大大小 = 100
- 可动态调整但需考虑性能

### 4.3 业务约束

**对象重置**
- 释放时对象必须重置到初始状态
- 调用 `Reset()` 方法（需实现）
- 清除所有引用和状态

**池注册**
- 必须在游戏开始时注册所有池
- 运行时不能修改池配置
- 支持池的热重载

**错误处理**
- 无效对象必须检测和拒绝
- 池满时的策略可配置
- 提供详细的错误信息

---

## 5. 设计目标

### 5.1 功能目标

**核心功能**
1. ✅ 模板化对象池注册和管理
2. ✅ 对象获取（Acquire）和释放（Release）
3. ✅ 动态池大小调整
4. ✅ 预热（PreWarm）机制
5. ✅ 统计信息和监控
6. ✅ 多线程安全访问

**扩展功能**
1. ✅ 池配置热重载
2. ✅ 自定义对象重置逻辑
3. ✅ 池使用报告和日志
4. ✅ 性能优化建议
5. ✅ 泄漏检测机制

### 5.2 质量目标

**可维护性**
- 清晰的类型参数化设计
- 统一的错误处理
- 易于添加新的池类型

**可测试性**
- 单元测试覆盖核心逻辑
- 模拟池使用场景
- 性能基准测试

### 5.3 性能目标

**访问性能**
- 单对象获取 < 1ms
- 单对象释放 < 1ms
- 批量操作线性时间复杂度

**内存效率**
- 对象重用率 > 90%
- 内存碎片 < 5%
- GC 压力降低 80%

**扩展性**
- 支持池类型数量 > 100
- 单池容量 > 1000 对象
- 并发访问支持 > 32 线程

---

## 6. 与其他子系统的关系

### 6.1 与VoxelBlockWorldSubsystem的关系

```
VoxelBlockWorldSubsystem
  ↓ 请求
AChunkBase 对象
  ↓
WorldObjectPoolSubsystem
  ↓ 提供
AChunkBase 实例（复用）
```

**职责划分**：
- VoxelBlockWorldSubsystem：管理区块生命周期和逻辑
- WorldObjectPoolSubsystem：提供区块Actor的池化管理

### 6.2 与VoxelCreatureWorldSubsystem的关系

```
VoxelCreatureWorldSubsystem
  ↓ 请求
ACreatureBase 对象
  ↓
WorldObjectPoolSubsystem
  ↓ 提供
ACreatureBase 实例（复用）
```

**职责划分**：
- VoxelCreatureWorldSubsystem：管理生物逻辑和AI
- WorldObjectPoolSubsystem：提供生物对象的池化

### 6.3 与VoxelItemWorldSubsystem的关系

```
VoxelItemWorldSubsystem
  ↓ 请求
AItemBase 对象
  ↓
WorldObjectPoolSubsystem
  ↓ 提供
AItemBase 实例（复用）
```

**职责划分**：
- VoxelItemWorldSubsystem：管理物品逻辑和交互
- WorldObjectPoolSubsystem：提供物品对象的池化

### 6.4 与ConfigDataSubsystem的关系

```
WorldObjectPoolSubsystem
  ↓ 查询
池配置信息
  ↓
ConfigDataSubsystem
  ↓ 返回
FObjectPoolConfig 数据
```

**配置信息**：
- 默认池大小
- 扩展策略
- 收缩策略
- 最大容量限制

### 6.5 与游戏实例的关系

```
VoxelGameInstance
  ↓ 初始化
WorldObjectPoolSubsystem
  ↓ 生命周期管理
游戏运行期间保持活跃
  ↓ 游戏结束
清理所有池和对象
```

**生命周期**：
- 随游戏实例创建
- 随游戏实例销毁
- 单例模式运行

---

## 7. 相关文档

- [VoxelBlockWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelBlockWorldSubsystem-系统设计.md)
- [VoxelGameInstance-架构设计](/VoxelDesign/03-架构设计/VoxelGameInstance-架构设计.md)
- [ConfigDataSubsystem-数据管理-分析报告](/VoxelDesign/08-分析报告/ConfigDataSubsystem-数据管理-分析报告.md)
- [WorldObjectPoolSubsystem-架构设计](/VoxelDesign/03-架构设计/WorldObjectPoolSubsystem-架构设计.md)（待创建）

---

**文档版本**: 1.0
**最后更新**: 2026-01-24
**状态**: ✅ 系统设计已完成