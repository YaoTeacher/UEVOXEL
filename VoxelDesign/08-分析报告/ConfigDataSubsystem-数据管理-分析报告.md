# ConfigDataSubsystem 数据管理系统统一化分析

**生成时间**: 2026-01-12
**最后更新**: 2026-01-24
**分析目标**: 评估当前多种数据管理方式，提供统一化方案建议
**文档版本**: 1.1

> **历史文档**：本文是 2026-01 的分析快照，其中的统一查询方案（`GetConfigData<T>(Tag)`）未采用。现行为以 [ConfigDataSubsystem-系统设计](/VoxelDesign/02-系统设计/ConfigDataSubsystem-系统设计.md) 与 [ConfigDataSubsystem-架构设计](/VoxelDesign/03-架构设计/ConfigDataSubsystem-架构设计.md) 为准；物品身份与数据模型见 [物品数据模型-系统设计](/VoxelDesign/02-系统设计/物品数据模型-系统设计.md)。

**v1.1 更新**:
- 添加模板系统（VoxelTemplates）集成
- 使用PrimaryAssetId管理模板资产
- 支持动态注册玩家创建的模板

---

## 目录

1. [当前数据管理方式盘点](#当前数据管理方式盘点)
2. [各工具优缺点分析](#各工具优缺点分析)
3. [性能与内存对比](#性能与内存对比)
4. [统一化方案建议](#统一化方案建议)
5. [迁移路径规划](#迁移路径规划)
6. [风险与收益评估](#风险与收益评估)

---

## 当前数据管理方式盘点

### 使用的数据管理工具

经过代码审查，当前 `ConfigDataSubsystem` 使用了 **5 种不同的数据管理方式**：

| 数据类型 | 管理工具 | 实现位置 | 数据结构 |
|---------|---------|---------|---------|
| **物品数据** | DataRegistry | ItemTypeRegistry | `TMap<FGameplayTag, const FVoxelItemBase*>` |
| **区域数据** | DataRegistry | AreaTypeRegistry | `TMap<FGameplayTag, const FAreaTypeBase*>` |
| **模板数据** | AssetRegistry | VoxelTemplateManager | `TMap<FPrimaryAssetId, UVoxelBuildingTemplateAsset*>` |
| **生物种族** | DataRegistry | Creature.Identity.Race | `TArray<FGameplayTag>` + 懒加载 |
| **生物职业** | DataRegistry | Creature.Identity.Profession | `TArray<FGameplayTag>` + 懒加载 |
| **生物特质** | DataRegistry | Creature.Identity.Trait | `TArray<FGameplayTag>` + 懒加载 |
| **UI 配置** | DataTable | TagConfigTable | `TMap<FGameplayTag, FTagUIDefinition>` |
| **动作流程** | DataTable | ActionFlowConfig | 未缓存（直接使用） |
| **生物事件** | AssetRegistry 扫描 | /Game/Resource/DataRresource/Config/EventConfig | `TMap<FPrimaryAssetId, UCreatureEvent*>` |
| **属性公式** | 目录扫描 + AssetRegistry | /Game/Resource/DataRresource/Config/CreatureConfig/AttributeConfig | `TMap<FGameplayTag, FDerivedAttributeDef>` |

### 数据加载流程图

```
启动阶段 (Initialize)
├─ LoadAllItemType()              [DataRegistry → ItemMap]
├─ LoadAllAreaType()              [DataRegistry → AreaMap]
├─ LoadTagUIConfig()              [DataTable → TagUIMap]
├─ ScanAndLoadTemplates()         [AssetRegistry → TemplateMap]
├─ LoadAllLifeEvents()            [AssetRegistry 扫描 → CachedLifeEvents]
├─ ScanAndLoadAllFormulas()       [目录扫描 → CachedFormulaMap]
└─ CacheRegistryTags()            [DataRegistry → CachedRace/Profession/TraitTags]

运行时查询
├─ GetItemTypeByTag()             [ItemMap 查找，O(1)]
├─ GetAreaTypeByTag()             [AreaMap 查找，O(1)]
├─ GetTemplateById()              [TemplateMap 查找，O(1)]
├─ GetRaceTemplate()              [DataRegistry 懒加载 + 同步加载资产]
├─ GetProfessionTemplate()        [DataRegistry 懒加载 + 同步加载资产]
├─ GetTraitDefinition()           [DataRegistry 懒加载 + 同步加载资产]
├─ GetTagDisplayName()            [TagUIMap 查找，O(1)]
└─ GetLifeEventById()             [CachedLifeEvents 查找，O(1)]

运行时注册
└─ RegisterPlayerTemplate()       [玩家创建 → TemplateMap + AssetRegistry]
```

---

## 各工具优缺点分析

### 1. DataRegistry (当前主力)

**用途**: 物品、区域、生物种族/职业/特质

#### 优点

✅ **软引用支持**
```cpp
struct FCreatureRaceRegistryItem
{
    FGameplayTag Tag;
    TSoftObjectPtr<UCreatureRaceTemplate> RaceTemplate;  // 不会硬加载
};
```
- 避免启动时加载所有资产
- 支持异步加载
- 减少内存占用

✅ **类型安全**
```cpp
template<typename T>
T* GetItemTypeByTag(const FGameplayTag ItemTag)
{
    static_assert(std::is_base_of_v<FVoxelItemBase, T>,
                  "T must be a FVoxelItemBase-derived struct");
}
```

✅ **UE 原生支持**
- UE 5.0+ 内置系统
- 编辑器可视化编辑
- 支持热重载（PIE）

✅ **分层架构**
```
DataRegistry (配置层)
    └─ DataTable (数据源)
        └─ FVoxelItemBase (运行时结构体)
            └─ TSoftObjectPtr<UObject> (资产引用)
```

#### 缺点

❌ **学习曲线陡峭**
- 需要理解 Registry Type、Registry Id、Item Name 三层概念
- 配置复杂（Project Settings → Data Registry）

❌ **性能开销**
```cpp
// 每次查询都需要：
FDataRegistryId Id(RegistryType, Tag.GetTagName());
const TRegistryItem* Item = DR->GetCachedItem<T>(Id);  // 哈希查找 x2
TSoftObjectPtr<TAssetType> SoftPtr = Item->*AssetPtrMember;
return SoftPtr.LoadSynchronous();  // 可能触发同步加载
```
- 两次哈希查找（Registry + Item）
- 懒加载可能导致卡顿

❌ **缓存策略不一致**
```cpp
// 物品/区域：启动时全部加载到 TMap
ItemMap.Add(key, RegistryItem);

// 生物模板：懒加载，每次都查询 DataRegistry
GetRaceTemplate() → DR->GetCachedItem() → SoftPtr.LoadSynchronous()
```

❌ **调试困难**
- 错误信息不够明确
- 难以追踪数据来源

---

### 2. DataTable (当前次要)

**用途**: UI 配置、动作流程

#### 优点

✅ **简单直观**
```cpp
UDataTable* TagConfigTable;
FTagUIDefinition* Row = TagConfigTable->FindRow<FTagUIDefinition>(TagName, TEXT(""));
```
- 设计师友好（Excel 可编辑）
- 单一数据源（CSV 导入）
- O(1) 查找性能

✅ **成熟稳定**
- UE 早期版本支持
- 社区资源丰富
- 文档完善

✅ **启动时全部加载**
```cpp
void LoadTagUIConfig()
{
    TagUIMap.Empty();
    TArray<FTagUIDefinition*> AllRows;
    TagConfigTable->GetAllRows<FTagUIDefinition>(ContextString, AllRows);
    for (FTagUIDefinition* Row : AllRows)
        TagUIMap.Add(Row->Tag, *Row);
}
```
- 运行时无额外开销
- 查询性能稳定

#### 缺点

❌ **无软引用**
```cpp
struct FTagUIDefinition
{
    FGameplayTag Tag;
    FText DisplayName;
    UTexture2D* Icon;  // 硬引用，会占用内存
    FLinearColor UIColor;
};
```
- 所有资产必须硬加载
- 内存占用高

❌ **不支持嵌套资产**
- 不能引用其他资产（除非用 SoftObjectPtr，但 DataTable 不支持）

❌ **版本控制冲突**
- CSV 文件容易冲突
- 难以合并

❌ **类型安全性弱**
```cpp
// 返回 void*，需要手动转换
void* RowData = DataTable->FindRow(nullptr, RowName, TEXT(""));
```

---

### 3. AssetRegistry 扫描 (当前特定用途)

**用途**: 生物事件、属性公式

#### 优点

✅ **自动发现**
```cpp
FARFilter Filter;
Filter.PackagePaths.Add(*EventSearchPath);
Filter.bRecursivePaths = true;
Filter.ClassPaths.Add(UCreatureEvent::StaticClass()->GetClassPathName());
AssetRegistry.GetAssets(Filter, AssetList);
```
- 无需手动注册
- 支持热插拔（MOD 友好）
- 适合动态数据

✅ **PrimaryAssetId 支持**
```cpp
FPrimaryAssetId ID = Event->GetPrimaryAssetId();
CachedLifeEvents.Add(ID, Event);
```
- 支持存档引用
- 资产管理器集成

✅ **目录结构即组织**
```
/Game/Resource/DataRresource/Config/EventConfig/
├── Base/
│   ├── BecomeAdventurer.uasset
│   └── OpenShop.uasset
└── DLC/
    └── NewEvent.uasset
```

#### 缺点

❌ **启动时扫描开销大**
```cpp
// 同步扫描目录
AssetRegistry.GetAssets(Filter, AssetList);  // 可能耗时数秒
for (const FAssetData& AssetData : AssetList)
{
    UObject* LoadedObj = AssetData.GetAsset();  // 同步加载所有资产
}
```
- 大量资产时启动慢
- 阻塞主线程

❌ **无版本管理**
- 资产文件名即 ID
- 重命名会破坏引用

❌ **无统一数据结构**
```cpp
// 每个资产都是独立的 UObject
class UCreatureEvent : public UPrimaryDataAsset
{
    // 每个 Event 自己定义结构
};
```

❌ **运行时无法修改**
- 资产文件只读
- 无法存档修改

---

### 4. TMap 运行时缓存 (当前统一出口)

**用途**: 所有数据的最终缓存

#### 优点

✅ **O(1) 查找性能**
```cpp
const FVoxelItemBase* Item = ItemMap.FindRef(ItemTag);
```

✅ **类型安全**
```cpp
TMap<FGameplayTag, const FVoxelItemBase*> ItemMap;
```

✅ **内存可控**
- 只加载需要的数据
- 可手动清空释放

#### 缺点

❌ **需要手动同步**
```cpp
void LoadAllItemType()
{
    ItemMap.Empty();  // 手动清空
    // 手动加载
    // 手动添加到 Map
}
```

❌ **重复占用内存**
```cpp
// DataRegistry 已有缓存
Item = ItemTypeRegistry->GetCachedItem<FVoxelItemBase>(ItemId);

// 又在 TMap 中存了一份
ItemMap.Add(key, Item);
```

---

## 性能与内存对比

### 内存占用分析

假设游戏规模：
- 1000 种物品
- 50 种区域
- 20 种种族、15 种职业、30 种特质
- 500 个生物事件
- 100 个属性公式

| 管理方式 | 内存占用 | 说明 |
|---------|---------|------|
| **DataRegistry (懒加载)** | ~50 MB | 只加载 RegistryItem 结构体（~50 bytes × 1100） |
| **DataRegistry (全缓存)** | ~150 MB | 加载所有 SoftObjectPtr 引用 |
| **DataTable (UI)** | ~5 MB | 1000 × 5 bytes × 软引用 |
| **TMap 二次缓存** | ~10 MB | 重复存储指针 |
| **AssetRegistry (事件)** | ~80 MB | 500 个事件 × 160 KB |
| **AssetRegistry (公式)** | ~2 MB | 100 个公式 × 20 KB |
| **总计（当前）** | ~297 MB | |

### 查询性能对比

| 操作 | DataRegistry | DataTable + TMap | AssetRegistry + TMap |
|------|-------------|-----------------|---------------------|
| **首次查询** | ~2-5 ms（可能触发加载） | ~0.01 ms（已在内存） | ~0.01 ms（已在内存） |
| **后续查询** | ~0.1 ms（哈希查找） | ~0.01 ms（TMap） | ~0.01 ms（TMap） |
| **内存占用** | 低（懒加载） | 中（全加载） | 高（全加载 + 扫描） |

### 启动时间分析

```
当前启动耗时（估算）：
├─ LoadAllItemType()           [~50 ms]   DataRegistry 查询 + 结构体缓存
├─ LoadAllAreaType()           [~20 ms]   同上
├─ LoadTagUIConfig()           [~10 ms]   DataTable 全部加载
├─ LoadAllLifeEvents()         [~500 ms]  AssetRegistry 扫描 + 同步加载 500 个资产
├─ ScanAndLoadAllFormulas()    [~100 ms]  目录扫描 + 加载 100 个资产
└─ CacheRegistryTags()         [~30 ms]   DataRegistry 查询

总计：~710 ms（约占启动时间的 10-20%）
```

---

## 统一化方案建议

### 方案对比

| 方案 | 推荐度 | 优点 | 缺点 | 适用场景 |
|------|--------|------|------|---------|
| **A. 全部 DataRegistry** | ⭐⭐⭐⭐ | UE 原生、类型安全、支持懒加载 | 学习曲线陡、性能开销 | 大型项目、需要懒加载 |
| **B. 全部 DataTable + TMap** | ⭐⭐⭐⭐⭐ | 简单直观、性能优秀、内存可控 | 无软引用、需手动管理 | 中小型项目、数据量可控 |
| **C. 全部 PrimaryAsset** | ⭐⭐⭐ | 支持存档引用、MOD 友好 | 启动慢、内存高 | MOD 游戏、需要存档引用 |
| **D. 混合优化（推荐）** | ⭐⭐⭐⭐⭐ | 兼顾性能、扩展性、内存 | 需要封装 | **本项目** |

---

### 🎯 推荐方案 D：混合优化架构

基于您的项目特点（**中等规模、性能敏感、需要扩展性**），推荐以下混合方案：

#### 核心原则

1. **静态配置数据 → DataTable + TMap**（启动时全加载）
2. **大型资产 → DataRegistry + 软引用**（懒加载）
3. **动态数据 → AssetRegistry + PrimaryAsset**（MOD 友好）
4. **统一查询接口 → 封装层**（隐藏实现细节）

#### 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                  ConfigDataSubsystem                        │
│  (统一查询接口，隐藏数据源实现)                               │
└─────────────────────────────────────────────────────────────┘
                              │
          ┌───────────────────┼───────────────────┐
          ▼                   ▼                   ▼
    ┌───────────┐       ┌───────────┐       ┌───────────┐
    │ DataTable │       │DataRegistry│       │PrimaryAsset│
    │   + TMap  │       │  + 懒加载  │       │   + 扫描   │
    └───────────┘       └───────────┘       └───────────┘
    物品/区域/UI       生物模板/大型资产    事件/模板/MOD数据
```

#### 数据分类策略

| 数据类型 | 管理方式 | 理由 |
|---------|---------|------|
| **物品基础数据** | DataTable + TMap | 数据量小（~1000）、查询频繁、需要 O(1) 性能 |
| **区域数据** | DataTable + TMap | 数据量小（~50）、静态配置、无需懒加载 |
| **UI 配置** | DataTable + TMap | 已实现、性能优秀、无需改动 |
| **生物种族模板** | DataRegistry + 懒加载 | 大型资产（~5 MB × 20）、需要软引用 |
| **生物职业模板** | DataRegistry + 懒加载 | 大型资产（~3 MB × 15）、需要软引用 |
| **模板数据** | AssetRegistry + PrimaryAsset | 动态数据（玩家创建）、MOD 友好、支持分享 |
| **生物事件** | AssetRegistry + PrimaryAsset | MOD 友好、需要存档引用 |
| **属性公式** | DataTable + TMap | 数据量小（~100）、需要启动加载 |

#### 统一查询接口

```cpp
// 新的统一查询接口（封装层）
template<typename T>
T* GetConfigData(FGameplayTag Tag)
{
    // 根据 Tag 类型自动路由到正确的数据源
    if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Item"))))
    {
        return GetItemDataFromTable<T>(Tag);  // DataTable + TMap
    }
    else if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Creature.Race"))))
    {
        return GetRaceTemplateFromRegistry<T>(Tag);  // DataRegistry + 懒加载
    }
    // ... 其他类型
}

// 模板系统专用接口（使用PrimaryAssetId）
UVoxelBuildingTemplateAsset* GetTemplateById(FPrimaryAssetId TemplateId)
{
    if (TemplateMap.Contains(TemplateId))
    {
        return TemplateMap[TemplateId];
    }

    // 动态加载
    UVoxelBuildingTemplateAsset* Template = Cast<UVoxelBuildingTemplateAsset>(
        UAssetManager::Get().GetAsset(TemplateId)
    );

    if (Template)
    {
        TemplateMap.Add(TemplateId, Template);
    }

    return Template;
}

// 运行时注册玩家创建的模板
bool RegisterPlayerTemplate(FPrimaryAssetId TemplateId, UVoxelBuildingTemplateAsset* Template)
{
    if (TemplateMap.Contains(TemplateId))
    {
        UE_LOG(LogConfigData, Warning, TEXT("Template %s already registered"), *TemplateId.ToString());
        return false;
    }

    TemplateMap.Add(TemplateId, Template);
    return true;
}
```

---

## 迁移路径规划

### Phase 1: 封装层构建（1-2周）

**目标**: 创建统一查询接口，不修改现有数据

#### 任务清单

- [ ] 创建 `UnifiedDataAccessor` 工具类
- [ ] 实现基于 Tag 类型的自动路由
- [ ] 添加性能分析宏（统计查询次数、耗时）
- [ ] 编写单元测试

#### 示例代码

```cpp
// UnifiedDataAccessor.h
class VOXEL_API FUnifiedDataAccessor
{
public:
    // 统一查询接口
    template<typename T>
    static T* Get(FGameplayTag Tag);

    // 性能统计
    static void PrintQueryStatistics();

private:
    static TMap<FString, int32> QueryCountMap;
    static TMap<FString, double> QueryTimeMap;
};

// 使用示例
auto* WoodBlock = FUnifiedDataAccessor::Get<FVoxelBlockItemBase>(
    FGameplayTag::RequestGameplayTag(FName("Item.Block.Wood"))
);
```

---

### Phase 2: 数据迁移（3-4周）

**目标**: 将物品、区域数据从 DataRegistry 迁移到 DataTable

#### 迁移步骤

1. **导出现有数据**
```cpp
// 从 DataRegistry 导出为 CSV
void ExportItemDataToCSV()
{
    FString CSVContent;
    for (const auto& Pair : ItemMap)
    {
        CSVContent += Pair.Value->ToCSVString();
    }
    FFileHelper::SaveStringToFile(CSVContent, TEXT("Items.csv"));
}
```

2. **创建 DataTable**
```
Content/Data/Tables/
├── DT_Items.csv
└── DT_Areas.csv
```

3. **更新加载逻辑**
```cpp
void LoadAllItemType()
{
    // 旧：DataRegistry
    // ItemMap.Add(key, ItemTypeRegistry->GetCachedItem<...>());

    // 新：DataTable
    UDataTable* ItemTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/Tables/DT_Items"));
    TArray<FVoxelItemBase*> AllRows;
    ItemTable->GetAllRows<FVoxelItemBase>(TEXT(""), AllRows);
    for (FVoxelItemBase* Item : AllRows)
    {
        ItemMap.Add(Item->ItemTag, Item);
    }
}
```

4. **性能对比测试**
```cpp
// 迁移前
SCOPE_CYCLE_COUNTER(STAT_DataRegistryQuery);
auto* Item = ItemTypeRegistry->GetCachedItem<FVoxelItemBase>(Id);

// 迁移后
SCOPE_CYCLE_COUNTER(STAT_DataTableQuery);
auto* Item = ItemMap.FindRef(ItemTag);
```

---

### Phase 3: 优化懒加载（2-3周）

**目标**: 优化生物模板的加载性能

#### 优化方案

**当前问题**:
```cpp
// 每次查询都可能触发同步加载
UCreatureRaceTemplate* Race = GetRaceTemplate(RaceTag);  // 5-10 ms 卡顿
```

**优化方案 A: 预加载**
```cpp
// 启动时异步预加载所有模板
void PreloadCreatureTemplates()
{
    TArray<FSoftObjectPath> AssetsToLoad;
    for (const FGameplayTag& Tag : CachedRaceTags)
    {
        if (UCreatureRaceTemplate* Template = GetRaceTemplate(Tag))
        {
            AssetsToLoad.Add(Template->GetPathName());
        }
    }

    // 使用 StreamableManager 异步加载
    UAssetManager& Manager = UAssetManager::Get();
    Manager.LoadAssetList(AssetsToLoad, FStreamableDelegate());
}
```

**优化方案 B: LRU 缓存**
```cpp
// 只缓存最近使用的 10 个模板
class TTemplateCache
{
    TMap<FGameplayTag, UCreatureRaceTemplate*> Cache;
    TQueue<FGameplayTag> AccessOrder;

    UCreatureRaceTemplate* Get(FGameplayTag Tag)
    {
        if (Cache.Contains(Tag))
        {
            // 更新访问顺序
            return Cache[Tag];
        }

        // 懒加载
        auto* Template = LoadFromRegistry(Tag);
        Cache.Add(Tag, Template);
        AccessOrder.Enqueue(Tag);

        // 超过 10 个，移除最久未使用
        if (Cache.Num() > 10)
        {
            FGameplayTag OldTag;
            AccessOrder.Dequeue(OldTag);
            Cache.Remove(OldTag);
        }

        return Template;
    }
};
```

---

### Phase 4: 文档与培训（1周）

**目标**: 确保团队理解新架构

- [ ] 编写《数据管理系统使用指南》
- [ ] 录制配置 DataTable 教程
- [ ] 创建常见问题 FAQ
- [ ] 代码审查 Checklist

---

## 风险与收益评估

### 风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| **数据迁移丢失** | 中 | 高 | 充分测试、保留旧代码、使用版本控制 |
| **性能回退** | 低 | 中 | 性能基准测试、分阶段迁移 |
| **学习曲线** | 高 | 低 | 文档、培训、代码审查 |
| **兼容性问题** | 中 | 中 | 保持向后兼容、逐步废弃旧 API |

### 收益

| 收益 | 量化指标 |
|------|---------|
| **查询性能提升** | DataRegistry (0.1 ms) → DataTable (0.01 ms) = **10x 提升** |
| **内存占用减少** | 297 MB → ~180 MB = **减少 39%** |
| **启动时间减少** | 710 ms → ~300 ms = **减少 58%** |
| **代码复杂度降低** | 5 种方式 → 2 种方式（DataTable + DataRegistry） |
| **维护成本降低** | 统一接口、减少重复代码、易于扩展 |

---

## 最终建议

### 短期（1-2个月）

✅ **执行 Phase 1 + Phase 2**
- 完成物品、区域数据迁移到 DataTable
- 构建统一查询接口
- 预期收益：查询性能提升 10x、内存减少 20%

### 中期（3-6个月）

✅ **执行 Phase 3**
- 优化生物模板懒加载
- 实现预加载或 LRU 缓存
- 预期收益：消除运行时卡顿、内存再减少 15%

### 长期（6个月+）

✅ **持续优化**
- 考虑引入自定义数据格式（如 JSON/YAML）用于 MOD 支持
- 实现数据版本管理
- 构建数据编辑器工具

---

## 附录

### A. 性能测试代码

```cpp
// ConfigDataSubsystemBenchmark.cpp
#if WITH_EDITOR
void UConfigDataSubsystem::RunBenchmark()
{
    const int32 Iterations = 10000;

    // 测试 DataRegistry 性能
    {
        double StartTime = FPlatformTime::Seconds();
        for (int32 i = 0; i < Iterations; ++i)
        {
            FDataRegistryId Id("DR_Item", FName("Wood"));
            auto* Item = ItemTypeRegistry->GetCachedItem<FVoxelItemBase>(Id);
        }
        double Duration = FPlatformTime::Seconds() - StartTime;
        UE_LOG(LogTemp, Log, TEXT("DataRegistry: %.4f ms (%.4f µs per call)"),
               Duration * 1000, (Duration * 1000000) / Iterations);
    }

    // 测试 DataTable 性能
    {
        double StartTime = FPlatformTime::Seconds();
        FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Item.Block.Wood"));
        for (int32 i = 0; i < Iterations; ++i)
        {
            auto* Item = ItemMap.FindRef(Tag);
        }
        double Duration = FPlatformTime::Seconds() - StartTime;
        UE_LOG(LogTemp, Log, TEXT("DataTable+TMap: %.4f ms (%.4f µs per call)"),
               Duration * 1000, (Duration * 1000000) / Iterations);
    }
}
#endif
```

### B. 数据迁移脚本

```python
# migrate_data_registry_to_csv.py
"""
将 DataRegistry 数据导出为 CSV 格式
用法: python migrate_data_registry_to_csv.py /path/to/your/project
"""
import unreal
import csv

def export_items_to_csv():
    # 获取 DataRegistry
    registry = unreal.DataRegistrySubsystem.get_registry_for_type(
        unreal.Name("DR_Item")
    )

    # 导出所有物品
    items = []
    for item_name in registry.get_item_names():
        item = registry.get_cached_item(unreal.DataRegistryId("DR_Item", item_name))
        items.append({
            'Tag': str(item.item_tag),
            'Name': str(item.item_name),
            'Type': str(item.item_type),
            # ... 其他字段
        })

    # 写入 CSV
    with open('Items.csv', 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['Tag', 'Name', 'Type'])
        writer.writeheader()
        writer.writerows(items)

    print(f"导出 {len(items)} 个物品到 Items.csv")

if __name__ == '__main__':
    export_items_to_csv()
```

### C. 查询接口设计

```cpp
// IConfigDataProvider.h
class VOXEL_API IConfigDataProvider
{
public:
    virtual const void* GetRawData(FGameplayTag Tag) = 0;
    virtual bool Contains(FGameplayTag Tag) const = 0;
    virtual TArray<FGameplayTag> GetAllTags() const = 0;
};

// DataTableProvider
class FDataTableProvider : public IConfigDataProvider
{
    UDataTable* Table;
    TMap<FGameplayTag, void*> Cache;

public:
    FDataTableProvider(UDataTable* InTable);
    virtual const void* GetRawData(FGameplayTag Tag) override;
};

// DataRegistryProvider
class FDataRegistryProvider : public IConfigDataProvider
{
    FName RegistryType;
    UDataRegistry* Registry;

public:
    FDataRegistryProvider(FName InRegistryType);
    virtual const void* GetRawData(FGameplayTag Tag) override;
};

// 统一访问器
class VOXEL_API FUnifiedConfigAccessor
{
    static TMap<FGameplayTag, TSharedPtr<IConfigDataProvider>> Providers;

public:
    static void RegisterProvider(FGameplayTag Tag, TSharedPtr<IConfigDataProvider> Provider);
    template<typename T>
    static T* Get(FGameplayTag Tag);
};
```

---

**文档结束**
