# WorldObjectPoolSubsystem-架构设计

**版本**: 1.0
**创建日期**: 2026-01-24
**最后更新**: 2026-01-24

---

## 1. 组件架构

### 1.1 整体架构层次

```
┌─────────────────────────────────────────────────────────────┐
│ 使用者接口层（User Interface）                                │
│ - VoxelBlockWorldSubsystem                                  │
│ - VoxelCreatureWorldSubsystem                                │
│ - VoxelItemWorldSubsystem                                   │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 对象池管理层（Pool Management Layer）                       │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ UObjectPoolSubsystem（主系统）                        │ │
│ │ ├── 泳道一：类型管理层                                 │ │
│ │ │  ├─ TMap<FName, TSharedPtr<FObjectPoolBase>>      │ │
│ │ │  └─ 类型映射与统一接口                              │ │
│ │                                                       │ │
│ │ ├── 泳道二：实例管理层                                 │ │
│ │ │  ├─ TObjectPool<T>（具体池实现）                     │ │
│ │ │  ├─ FreeObjects（空闲对象列表）                    │ │
│ │ │  └─ ActiveObjects（活跃对象列表）                    │ │
│ │                                                       │ │
│ │ └── 泳道三：操作管理层                                 │ │
│ │    ├─ RegisterPool<T>()（池注册）                     │ │
│ │    ├─ Acquire<T>()/Release<T>()（对象操作）            │ │
│ │    └─ PreWarm()/AdjustPoolSize（池管理）             │ │
│ └─────────────────────────────────────────────────────────┘ │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 对象实现层（Object Implementation）                          │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ 模板类 TObjectPool<T>                                  │ │
│ │ ├── 泛型参数 T（继承自 UObject）                       │ │
│ │ ├── 创建函数：TFunction<T*()> CreateFunc              │ │
│ │ ├── 重置函数：TFunction<void(T*)> ResetFunc           │ │
│ │ └─ 数组管理：FreeObjects + ActiveObjects            │ │
│ └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 对象池层次结构

```
FObjectPoolBase（抽象基类）
├─ 纯虚函数接口：
│   ├── virtual void* Acquire() = 0
│   ├── virtual void Release(void* Object) = 0
│   ├── virtual int32 NumTotal() const = 0
│   ├── virtual int32 NumActive() const = 0
│   ├── virtual int32 NumFree() const = 0
│   ├── virtual void PreWarm(int32 Count) = 0
│   └── virtual void Shrink(int32 Count) = 0
│
└─ TObjectPool<T>（模板实现类）
    └─ 模板参数 T 必须是 UObject 或其派生类

    成员变量：
    ├── TArray<T*> FreeObjects（空闲对象列表）
    ├── TArray<T*> ActiveObjects（活跃对象列表）
    ├── TFunction<T*()> CreateFunc（创建函数）
    └─ TFunction<void(T*)> ResetFunc（重置函数）
```

### 1.3 模板设计模式

```cpp
// 模板接口统一性
template<typename T>
class TObjectPool : public FObjectPoolBase
{
    // 统一的接口实现，支持任意 UObject 类型
    virtual void* Acquire() override;
    virtual void Release(void* Object) override;
    // ... 其他接口实现
};

// 使用示例：
// RegisterPool<AChunkBase>("ChunkPool", CreateFunc, ResetFunc, 10);
// AChunkBase* chunk = Acquire<AChunkBase>("ChunkPool");
// Release<AChunkBase>("ChunkPool", chunk);
```

---

## 2. 接口定义

### 2.1 核心模板接口

#### **池注册接口**
```cpp
template<typename T>
void RegisterPool(
    FName PoolName,
    TFunction<T*()> CreateFunc,
    TFunction<void(T*)> ResetFunc,
    int32 InitialSize = 0
);

// 参数说明：
// - PoolName：池的唯一标识符
// - CreateFunc：创建对象的函数（工厂函数）
// - ResetFunc：重置对象状态的函数
// - InitialSize：初始预热数量
```

#### **对象获取接口**
```cpp
template<typename T>
T* Acquire(FName PoolName);

// 返回值：
// - 成功：返回有效的对象指针
// - 失败：返回 nullptr（池不存在或创建失败）

// 执行流程：
// 1. 根据池名称查找对应池
// 2. 从 FreeObjects 中取出对象
// 3. 将对象移到 ActiveObjects
// 4. 返回对象指针
```

#### **对象释放接口**
```cpp
template<typename T>
void Release(FName PoolName, T* Object);

// 执行流程：
// 1. 验证对象有效性
// 2. 从 ActiveObjects 中移除对象
// 3. 调用 ResetFunc 重置对象状态
// 4. 将对象添加到 FreeObjects
```

#### **池管理接口**
```cpp
// 预热接口：预先创建指定数量的对象
void PreWarm(FName PoolName, int32 Count);

// 调整池大小接口
void AdjustPoolSize(FName PoolName, int32 DesiredSize);

// 参数：
// - Count：要预热/调整的对象数量
// - DesiredSize：目标池总大小（Active + Free）
```

### 2.2 统计接口

#### **单池统计**
```cpp
UFUNCTION(BlueprintCallable, Category = "Object Pool")
FObjectPoolStats GetPoolStats(FName PoolName) const;

// FObjectPoolStats 结构：
struct FObjectPoolStats
{
    UPROPERTY(BlueprintReadOnly)
    FName PoolName;     // 池名称

    UPROPERTY(BlueprintReadOnly)
    int32 TotalCount = 0;    // 总对象数

    UPROPERTY(BlueprintReadOnly)
    int32 ActiveCount = 0;   // 活跃对象数

    UPROPERTY(BlueprintReadOnly)
    int32 FreeCount = 0;    // 空闲对象数
};
```

#### **所有池统计**
```cpp
UFUNCTION(BlueprintCallable, Category = "Object Pool")
TArray<FObjectPoolStats> GetAllPoolStats() const;
```

### 2.3 调试接口

```cpp
UFUNCTION(Exec, Category = "Object Pool")
void DumpPoolStats();

// 功能：
// - 输出所有池的详细统计信息
// - 检查对象泄漏
// - 输出到控制台
```

---

## 3. 数据流

### 3.1 对象获取流程

```
调用 Acquire<T>(PoolName)
    ↓
1. 类型查找（编译时）
   └─ 模板参数 T 确定类型安全
    ↓
2. 池查找（运行时）
   └─ Pools.Find(PoolName) 查找对应池
    ↓
3. 空闲对象检查
   ├─ FreeObjects.Num() > 0：
   │   └─ FreeObjects.Pop() 取出对象
   │       ↓
   │   ├─ 将对象添加到 ActiveObjects
   │   └─ 返回对象指针
   │
   └─ FreeObjects.Num() == 0：
       └─ 动态创建新对象
           ├─ CreateFunc() 调用创建函数
           ├─ 验证对象有效性
           ├─ 将对象添加到 ActiveObjects
           └─ 返回新对象指针
```

### 3.2 对象归还流程

```
调用 Release<T>(PoolName, Object)
    ↓
1. 对象验证
   ├─ 检查 Object 是否为 nullptr
   ├─ 检查对象类型是否匹配
   └─ 检查对象是否已在 ActiveObjects 中
    ↓
2. 状态转移
   ├─ ActiveObjects.Remove(Object) 移除
   ├─ 调用 ResetFunc(Object) 重置状态
   └─ FreeObjects.Add(Object) 添加到空闲列表
```

### 3.3 池扩展流程

```
池需要扩展（获取对象时无空闲对象）
    ↓
1. 检查是否达到最大容量
   ├─ 容量未满：创建新对象
   └─ 容量已满：返回 nullptr 或等待策略
    ↓
2. 创建新对象
   ├─ 调用 CreateFunc()
   ├─ 初始化对象状态
   └─ 直接添加到 ActiveObjects
    ↓
3. 更新统计信息
    └─ ActiveCount++, TotalCount++
```

### 3.4 池收缩流程

```
调用 AdjustPoolSize(DesiredSize)
    ↓
1. 计算需要收缩的数量
   ├─ CurrentSize = NumTotal()
   ├─ TargetShrink = CurrentSize - DesiredSize
   └─ ShrinkCount = FMath::Min(TargetShrink, FreeObjects.Num())
    ↓
2. 执行收缩操作
   ├─ 循环 ShrinkCount 次
   │   ├─ FreeObjects.Pop() 取出对象
   │   └─ 条件销毁对象
   │       ├─ 是 UObject：ConditionalBeginDestroy()
   │       └─ 普通对象：delete
    ↓
3. 更新统计信息
    └─ FreeCount, TotalCount 更新
```

---

## 4. 生命周期管理

### 4.1 对象生命周期

```
对象创建（Acquire）
    ↓
对象激活（Active）
    │
    ├─ 对象被系统使用
    ├─ 在 ActiveObjects 中维护
    └─ 执行业务逻辑
    │
    ↓
对象释放（Release）
    │
    ├─ 重置状态（ResetFunc）
    ├─ 转入空闲状态（Free）
    └─ 在 FreeObjects 中等待复用
    │
    ↓
对象销毁（Shrink 或池销毁）
    │
    ├─ UObject ConditionalBeginDestroy()
    ├─ 普通对象 delete
    └─ 内存释放
```

### 4.2 池生命周期

```
子系统初始化（Initialize）
    ↓
池注册（RegisterPool）
    ├─ 创建 TObjectPool<T> 实例
    ├─ 初始化创建函数和重置函数
    ├─ 执行预热（PreWarm）
    └─ 添加到 Pools 映射
    │
    ↓
运行时管理
    ├─ 对象获取/释放
    ├─ 动态调整大小
    └─ 统计监控
    │
    ↓
子系统销毁（Deinitialize）
    ├─ 清理所有池
    ├─ 释放所有对象
    └─ 清理映射
```

### 4.3 内存管理

```cpp
// UObject 处理
if constexpr (std::is_base_of_v<UObject, T>) {
    if (IsValid(Object)) {
        Object->ConditionalBeginDestroy();
    }
} else {
    delete Object;
}
```

---

## 5. 类关系图

### 5.1 模板类继承关系

```
FObjectPoolBase（抽象基类）
│
├─ 虚函数接口：
│   ├── virtual void* Acquire() = 0
│   ├── virtual void Release(void* Object) = 0
│   ├── virtual int32 NumTotal() const = 0
│   ├── virtual int32 NumActive() const = 0
│   ├── virtual int32 NumFree() const = 0
│   ├── virtual void PreWarm(int32 Count) = 0
│   └── virtual void Shrink(int32 Count) = 0
│
└─ TObjectPool<T>（模板实现）
    │
    ├── 模板参数约束：T 必须是 UObject 或其派生类
    ├── 成员变量：
    │   ├── TArray<T*> FreeObjects
    │   ├── TArray<T*> ActiveObjects
    │   ├── TFunction<T*()> CreateFunc
    │   └─ TFunction<void(T*)> ResetFunc
    │
    └─ 接口实现（覆盖所有虚函数）
```

### 5.2 与使用者关系

```
UObjectPoolSubsystem
│
├─ 管理 → 多个 TObjectPool<T>
│   ├── TObjectPool<AChunkBase>
│   ├── TObjectPool<ACreatureBase>
│   ├── TObjectPool<AItemBase>
│   └─ 其他具体类型的池
│
├─ 提供 API → 各个子系统
│   ├── VoxelBlockWorldSubsystem
│   ├── VoxelCreatureWorldSubsystem
│   ├── VoxelItemWorldSubsystem
│   └─ 其他需要池化的子系统
│
└─ 统一接口 → 调用者
    ├── RegisterPool<T>()
    ├── Acquire<T>()
    ├── Release<T>()
    └─ 统计接口
```

### 5.3 数据结构关系

```
UObjectPoolSubsystem
│
├─ Pools（池映射）
│   └─ TMap<FName, TSharedPtr<FObjectPoolBase>>
│       ├── "ChunkPool" → TObjectPool<AChunkBase>*
│       ├── "CreaturePool" → TObjectPool<ACreatureBase>*
│       └─ "ItemPool" → TObjectPool<AItemBase>*
│
└─ TObjectPool<T> 内部结构
    ├── FreeObjects：TArray<T*>（空闲对象）
    └─ ActiveObjects：TArray<T*>（活跃对象）
```

---

## 6. 扩展点

### 6.1 注册新的对象池类型

**步骤**：
1. 确定对象类型必须继承自 UObject
2. 提供创建函数和重置函数
3. 调用 RegisterPool<T> 注册

```cpp
// 示例：注册新的建筑池
TFunction<ABuildingBase*()> CreateBuilding = []() {
    return GetWorld()->SpawnActor<ABuildingBase>();
};

TFunction<void(ABuildingBase*)> ResetBuilding = [](ABuildingBase* Building) {
    Building->Reset();
    Building->SetActorLocation(FVector::ZeroVector);
    Building->SetActorHiddenInGame(true);
};

RegisterPool<ABuildingBase>("BuildingPool", CreateBuilding, ResetBuilding, 5);
```

### 6.2 自定义对象创建逻辑

```cpp
// 方式一：Lambda 函数
TFunction<AItemBase*()> CreateItem = []() {
    AItemBase* Item = GetWorld()->SpawnActor<AItemBase>();
    Item->InitializeRandomItem();
    return Item;
};

// 方式二：类成员函数
UObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubsystem>();
PoolSubsystem->RegisterPool<AItemBase>(
    "ItemPool",
    [this]() { return CreateItemInternal(); },
    [this](AItemBase* Item) { ResetItemInternal(Item); }
);
```

### 6.3 自定义重置逻辑

```cpp
// 基础重置
TFunction<void(AItemBase*)> ResetItem = [](AItemBase* Item) {
    Item->ResetToDefault();
};

// 高级重置（包含额外处理）
TFunction<void(AItemBase*)> AdvancedReset = [](AItemBase* Item) {
    // 1. 重置基础状态
    Item->ResetToDefault();

    // 2. 清理引用
    Item->ClearReferences();

    // 3. 重置变换
    Item->SetActorTransform(FTransform::Identity);

    // 4. 隐藏对象
    Item->SetActorHiddenInGame(true);
};
```

### 6.4 扩展池功能

```cpp
// 可以通过继承 TObjectPool<T> 来扩展功能
template<typename T>
class TObjectPoolWithStats : public TObjectPool<T>
{
public:
    void LogAcquisitionTime()
    {
        // 记录获取耗时统计
    }

    void SetMaxAge(float MaxAgeSeconds)
    {
        // 设置对象最大存活时间
    }
};
```

---

## 7. 最佳实践

### 7.1 池注册时机

```cpp
// 在游戏开始时注册所有池
void AVoxelGameInstance::InitObjectPools()
{
    // 注册区块池
    RegisterPool<AChunkBase>(
        "ChunkPool",
        []() { return GetWorld()->SpawnActor<AChunkBase>(); },
        [](AChunkBase* Chunk) { Chunk->Reset(); },
        10  // 预热10个
    );

    // 注册生物池
    RegisterPool<ACreatureBase>(
        "CreaturePool",
        []() { return GetWorld()->SpawnActor<ACreatureBase>(); },
        [](ACreatureBase* Creature) { Creature->Reset(); },
        5   // 预热5个
    );
}
```

### 7.2 对象使用模式

```cpp
// 正确的使用模式
void SomeFunction()
{
    // 1. 获取对象
    AChunkBase* Chunk = Acquire<AChunkBase>("ChunkPool");
    if (!Chunk) return;

    // 2. 使用对象
    Chunk->InitializeAtPosition(Position);

    // 3. 确保最终释放
    Release<AChunkBase>("ChunkPool", Chunk);
}
```

### 7.3 错误处理

```cpp
// 错误处理示例
AChunkBase* GetChunkSafely()
{
    AChunkBase* Chunk = Acquire<AChunkBase>("ChunkPool");
    if (!Chunk)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to acquire chunk from pool"));
        return nullptr;
    }

    if (!IsValid(Chunk))
    {
        UE_LOG(LogTemp, Error, TEXT("Acquired invalid chunk object"));
        Release<AChunkBase>("ChunkPool", Chunk);
        return nullptr;
    }

    return Chunk;
}
```

---

## 8. 性能考虑

### 8.1 内存优化

- **预热策略**：根据使用频率设置不同的预热数量
- **收缩策略**：在空闲时自动收缩，减少内存占用
- **类型安全**：模板编译时类型检查，避免运行时错误

### 8.2 并发安全

- 当前实现使用 UE 的默认同步机制
- 在多线程环境下使用时需注意：
  - 获取和释放操作应尽量快
  - 避免在持有锁时执行耗时操作
  - 考虑使用无锁队列优化高频操作

### 8.3 调试支持

- 详细的统计信息输出
- 对象泄漏检测
- 性能监控接口

---

## 9. 相关文档

### 9.1 子系统文档

- [WorldObjectPoolSubsystem-系统设计](/VoxelDesign/02-系统设计/WorldObjectPoolSubsystem-系统设计.md)
- [VoxelBlockWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelBlockWorldSubsystem-系统设计.md)
- [VoxelCreatureWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelCreatureWorldSubsystem-系统设计.md)
- [VoxelItemWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelItemWorldSubsystem-系统设计.md)

### 9.2 架构文档

- [VoxelGameInstance-架构设计](/VoxelDesign/03-架构设计/VoxelGameInstance-架构设计.md)
- [ConfigDataSubsystem-数据管理-分析报告](/VoxelDesign/08-分析报告/ConfigDataSubsystem-数据管理-分析报告.md)

---

**文档版本**: 1.0
**最后更新**: 2026-01-24
**状态**: ✅ 架构设计已完成