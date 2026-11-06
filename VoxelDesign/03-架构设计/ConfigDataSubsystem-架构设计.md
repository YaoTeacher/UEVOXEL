# ConfigDataSubsystem-架构设计

**版本**: 2.0
**创建日期**: 2026-01-24
**最后更新**: 2026-10-17
**说明**: v2.0 按实际代码（`ConfigDataSubsystem.h/.cpp`）重写，删除旧版虚构的 `GetConfigData<T>(Tag)` 统一路由、CacheManager、TemplateMap/PlayerTemplateMap 等内容。

---

## 1. 组件定位

`UConfigDataSubsystem` 是 **GameInstance 子系统**，负责把四类配置在启动时装载为内存缓存，并提供按句柄 / 按 Tag 的只读查询。

### 1.1 经营配置

除现有物品/区域/生物/UI 配置外，经营链新增以下只读配置：

| 配置 | 用途 |
|---|---|
| 营业时段模板 | 生成 `FBusinessScheduleRuntime` 的初始 N×24 格 |
| 队列容量 | 工作点默认容量（V1 = 8） |
| 扣负白名单 | 贷款月供 / 租金 / 工资 |
| 收租日 | 商圈 `RentCollectionDayOfMonth` |
| `FFlowConfig` 表 | 各业务类型流程节点 |
| 期望带默认值 | 顾客期望带与消费意愿 |
| 工种表 | 工种 `naturalPriority`、是否可暂停、工具/技能要求 |

经营配置只读、不入档；运行时状态由各组件与子系统持有。

```
UConfigDataSubsystem : UGameInstanceSubsystem
├─ 数据来源
│  ├─ DataRegistry（物品、区域、生物种族/职业/特质）
│  ├─ DataTable（Tag UI 配置）
│  ├─ AssetRegistry 扫描（生物事件、属性公式资产）
│  └─ 磁盘模板文件（Saved/Templates/*.tmpl）
├─ 内存缓存
│  ├─ ItemMap        TMap<int32, FVoxelItemBase*>
│  ├─ FullIdToHandle TMap<FString, int32>
│  ├─ AreaMap        TMap<FGameplayTag, const FAreaTypeBase*>
│  ├─ TagUIMap       TMap<FGameplayTag, FTagUIDefinition>
│  ├─ CachedRaceTags / CachedProfessionTags / CachedTraitTags
│  ├─ CachedLifeEvents   TMap<FPrimaryAssetId, UCreatureEvent*>
│  ├─ CachedFormulaMap   TMap<FGameplayTag, FDerivedAttributeDef>
│  └─ RuntimeTemplates   TMap<int32, FVoxelTemplate>
└─ 只读查询接口（物品/句柄/区域/UI/生物/事件/模板）
```

**不负责**：存档格式与读写（SaveAllData/LoadAllData 目前是存根）、配置编辑界面、资产版本管理、业务校验。

---

## 2. 数据容器

| 成员 | 类型 | 键 / 值 | 来源 | 生命周期 |
|------|------|---------|------|----------|
| `ItemTypeRegistry` | `UDataRegistry*` | — | 编辑器配置（DR_Item） | 资产引用 |
| `AreaTypeRegistry` | `UDataRegistry*` | — | 编辑器配置（DR_Area） | 资产引用 |
| `TagConfigTable` | `UDataTable*` | — | 编辑器配置（FTagUIDefinition 行） | 资产引用 |
| `ActionFlowConfig` | `UDataTable*` | — | 编辑器配置 | 资产引用（暂无消费方） |
| `ItemMap` | `TMap<int32, FVoxelItemBase*>` | HandleId → 物品行 | LoadAllItemType | 启动装载，运行期只读 |
| `FullIdToHandle` | `TMap<FString, int32>` | FullId → HandleId | LoadAllItemType / RegisterRuntimeTemplate | 运行期可自修复 |
| `AreaMap` | `TMap<FGameplayTag, const FAreaTypeBase*>` | AreaTypeTag → 区域类型行 | LoadAllAreaType | 启动装载 |
| `TagUIMap` | `TMap<FGameplayTag, FTagUIDefinition>` | Tag → UI 定义 | LoadTagUIConfig | 启动装载 |
| `CachedRaceTags` 等 | `TArray<FGameplayTag>` | — | CacheRegistryTags | 启动装载 |
| `CachedLifeEvents` | `TMap<FPrimaryAssetId, UCreatureEvent*>` | PrimaryAssetId → 事件资产 | LoadAllLifeEvents | 启动装载 |
| `CachedFormulaMap` | `TMap<FGameplayTag, FDerivedAttributeDef>` | 属性 Tag → 公式定义 | ScanAndLoadAllFormulas | 启动装载，后者覆盖前者 |
| `RuntimeTemplates` | `TMap<int32, FVoxelTemplate>` | HandleId → 模板 | ScanAndLoadAllTemplates / RegisterRuntimeTemplate | 运行期增删 |

两个映射的职责必须分清：
- **ItemMap** 只放配置物品（DataRegistry 行），不含运行时模板；
- **RuntimeTemplates** 只放模板（磁盘文件与玩家编辑产物）；
- `FindByHandle` 先查 ItemMap 再查 RuntimeTemplates，是两者唯一的合并入口。

---

## 3. 启动加载流程

`Initialize()` 固定顺序：

```
ItemMap/AreaMap/RuntimeTemplates 清空
  → LoadAllItemType        (DR_Item → ItemMap/FullIdToHandle → 纹理数组)
  → LoadAllAreaType        (DR_Area → AreaMap)
  → LoadTagUIConfig        (DataTable → TagUIMap)
  → CacheRegistryTags      (3 个生物 DataRegistry → Tag 数组)
  → LoadAllLifeEvents      (AssetRegistry 扫描 → CachedLifeEvents)
  → ScanAndLoadAllFormulas (AssetRegistry 扫描 → CachedFormulaMap)
  → ScanAndLoadAllTemplates(Saved/Templates/*.tmpl → RuntimeTemplates)
```

### 3.1 LoadAllItemType

1. `ItemTypeRegistry->GetItemNames()` 取全部行名；
2. 逐行 `GetCachedItem<FVoxelItemBase>(FDataRegistryId("DR_Item", RowName))`；
3. 身份补全：`IdentityName` 为空回退 RowName，`Source` 为空回退 `core`；
4. `HandleId = GetTypeHash(FullId)`，写回行数据；
5. 句柄碰撞 → `Fatal`（改 IdentityName 解决）；
6. 写 `ItemMap[Handle]` 与 `FullIdToHandle[FullId]`；
7. 装载完成后调 `UVoxelTextureRegistry::BuildTextureArrays(ItemMap)` 重建方块贴图数组。

### 3.2 LoadAllAreaType

`DR_Area` 逐行 `GetCachedItem<FAreaTypeBase>`，以 `AreaTypeTag` 为键；重复键 Warning 跳过。

### 3.3 LoadTagUIConfig

`TagConfigTable->GetAllRows<FTagUIDefinition>`，合法 Tag 行写入 `TagUIMap`。

### 3.4 CacheRegistryTags

扫描三个 DataRegistry 类型：`Creature.Identity.Race` / `Creature.Identity.Profession` / `Creature.Identity.Trait`，用成员指针取 Tag，写入三个 Tag 数组。

### 3.5 LoadAllLifeEvents

AssetRegistry 递归扫描 `/Game/Resource/DataRresource/Config/EventConfig` 下的 `UCreatureEvent` 资产，同步 `GetAsset()` 加载并按 `PrimaryAssetId` 入表（无效 ID 用 `"CreatureLifeEvent"` + 资产名兜底）。

### 3.6 ScanAndLoadAllFormulas

扫描 `FormulaSearchPath`（默认 `/Game/Resource/DataRresource/Config/CreatureConfig/AttributeConfig`）下的 `UGlobalAttributeFormulas`，逐个 `MergeFormulasToCache`；同名属性后者覆盖前者（MOD 覆盖原生）。

### 3.7 ScanAndLoadAllTemplates

扫描 `Saved/Templates/**/*.tmpl`，`UVoxelTemplateUtils::LoadFromTmplFile` 读取后 `RegisterRuntimeTemplate`；句柄不信任磁盘旧值，由 Guid 身份重新计算。

---

## 4. 接口定义

### 4.1 物品查询

```cpp
template<typename T> T* GetItemTypeById(int32 HandleId);          // ItemMap 直查，未命中 Warning + nullptr
template<typename T> TArray<T*> GetItemTypeArrayByCategory(FGameplayTag CategoryTag); // MatchesTag 遍历 ItemMap
FVoxelItemBase* FindByHandle(int32 Handle);                        // ItemMap → RuntimeTemplates
```

- `GetItemTypeById` 内部 `static_cast<T*>`，**无运行时类型校验**；调用方若不能确定真实子类，应先按 ItemTag 判别（`VoxelPlacementWorldSubsystem.cpp:500` 即此做法）。
- `GetItemTypeArrayByCategory` 是 O(n) 全表遍历，供物品库/建造面板等低频 UI 查询；高频场景需要上层缓存结果。

### 4.2 句柄

```cpp
static int32 ComputeHandle(const FString& FullId);   // GetTypeHash(FullId)
int32  GetHandleByFullId(const FString& FullId) const; // 查表 → 回退扫描 ItemMap → RuntimeTemplates → 自修复；找不到 -1
FString GetFullIdByHandle(int32 Handle) const;         // ItemMap → RuntimeTemplates
```

`GetHandleByFullId` 带自修复：`FullIdToHandle` 缺条目时扫描两个容器补写并打 Warning。这暴露了一个既有事实——运行时注册模板/编辑模板会产生映射不同步，调用方仍应把 `-1` 当失败处理。

### 4.3 区域查询

```cpp
const FAreaTypeBase* GetAreaTypeByTag(FGameplayTag AreaId);
TArray<const FAreaTypeBase*> GetAreaTypeArrayByTag(FGameplayTag ParentTag); // MatchesTag 子类查询
TArray<const FAreaTypeBase*> GetAllAreaTypes();
FText GetAreaTypeDisplayName(FGameplayTag AreaTypeTag) const;  // 空名回退 Tag 名
const FAreaTypeTemplate* GetAreaTypeTemplateById(FGameplayTag); // ⚠ 空实现，恒返回 nullptr
```

### 4.4 UI 查询

`GetTagDisplayName` / `GetTagIcon` / `GetTagColor`：查 `TagUIMap`，未命中分别回退 Tag 名 / 空指针 / 白色。

### 4.5 生物模板与事件

```cpp
UCreatureRaceTemplate*       GetRaceTemplate(FGameplayTag RaceTag);
UCreatureProfessionTemplate* GetProfessionTemplate(FGameplayTag ProfessionTag);
UCreatureTraitDefinition*    GetTraitDefinition(FGameplayTag TraitTag);
TMap<FGameplayTag, UCreatureTraitDefinition*> GetAllTraitDefinitions();
FGameplayTag GetRandomRaceTag();
UCreatureEvent* GetLifeEventById(FPrimaryAssetId Id) const;
TArray<UCreatureEvent*> GetAllLifeEvents() const;
```

模板类接口走 `GetAssetFromRegistry<TRegistryItem,TAssetType>`：`DataRegistryId(RegistryType, Tag.GetTagName())` → `GetCachedItem` → 软引用 `LoadSynchronous`。

### 4.6 运行时模板

```cpp
void  ScanAndLoadAllTemplates();
int32 RegisterRuntimeTemplate(FVoxelTemplate&& T);   // 身份 = core:tmpl.<Guid>；同 Guid 重注册=覆盖
bool  UnregisterTemplate(int32 HandleId);
FVoxelTemplate* GetTemplateById(int32 Handle);
TArray<FVoxelTemplate*> GetAllTemplates();
const FVoxelTemplate* FindTemplateByGuid(const FGuid& Guid) const;
TArray<int32> GetAllTemplateIds() const;
int32 AllocateNewTemplateId();                       // NextTemplateId 自增
```

### 4.7 存根

`SaveAllData` / `LoadAllData` 目前仅返回 `FPaths::FileExists(FilePath)`，不是真实存取实现。

---

## 5. 一致性与约束

- **身份唯一性**：`HandleId = hash(FullId)`；物品装载与模板注册都做碰撞检测，物品碰撞 Fatal，模板同 FullId 重注册按覆盖处理、不同 FullId 撞 hash 才 Fatal。
- **两表隔离**：模板不进 ItemMap；物品查询入口 `GetItemTypeById` 不返回模板，需要模板时用 `FindByHandle`/`GetTemplateById`。
- **只读约定**：ItemMap 存的是 `const_cast` 后的 DataRegistry 内存，代码约定只读；任何写行数据的代码都是破坏性操作。
- **线程**：装载在 GameInstance 初始化期完成，运行期查询按游戏线程单线程使用，无锁。
- **失败处理**：DataRegistry/DataTable 未配置时打 Warning 并跳过；单行加载失败不阻断整体装载。

---

## 6. 现状缺口（按代码事实列出，待实施）

| 缺口 | 位置 | 影响 |
|------|------|------|
| `GetAreaTypeTemplateById` 空实现 | ConfigDataSubsystem.h | 调用方恒拿 nullptr |
| `SaveAllData/LoadAllData` 存根 | ConfigDataSubsystem.cpp:36-44 | 无配置存取能力 |
| `GetItemTypeById<T>` 无运行时类型校验 | ConfigDataSubsystem.h:236 | 行结构与 T 不匹配时 UB |
| DR_Item 行结构混用（方块/普通物）无加载期校验 | LoadAllItemType | 业务侧裸 static_cast 风险 |
| `GetItemTypeArrayByCategory` O(n) 全表扫描 | ConfigDataSubsystem.h:263 | 面板打开频繁时开销随物品数增长 |
| 事件扫描路径、公式路径硬编码/半配置 | LoadAllLifeEvents / FormulaSearchPath | 目录调整需改代码/配置 |
| `ActionFlowConfig` 无消费方 | 成员声明 | 死配置引用 |

对应改进项与物品数据模型文档的「加载期形状校验 / GetItemAs 安全取用 / 品类索引」一致。

---

## 7. 与其他系统的关系

| 系统 | 关系 |
|------|------|
| 物品数据模型 | 本文只负责装载与查询；行结构、身份规则、片段/模块见[物品数据模型-系统设计](/VoxelDesign/02-系统设计/物品数据模型-系统设计.md) |
| 家具系统 | 家具行经 ItemMap 装载；Profile 缓存在此之上构建（规划中） |
| VoxelBlockWorldSubsystem | 用 `GetHandleByFullId` 取方块句柄（石头等）并缓存 |
| VoxelItemWorldSubsystem | 用 `GetItemTypeById<FVoxelNormalItemBase>` 取放置数据 |
| UI | 物品库/建造面板用 `GetItemTypeArrayByCategory`，Tag 展示用 `GetTagDisplayName` |
| AI 决策系统 | 生物模板/事件/公式查询 |

## 8. 设计目标（未实现）

以下为方向成立、当前未实现的架构演进项，不描述现有能力。

| 目标 | 架构影响 | 状态 |
|------|----------|------|
| 模板注册事件广播 | `RegisterRuntimeTemplate` / `UnregisterTemplate` 增加委托，UI 与索引层订阅 | 未实现 |
| 查询统计埋点 | 在 `GetItemTypeById` / `GetItemTypeArrayByCategory` 统计次数、耗时、未命中 | 未实现 |
| 批量查询接口 | 按 Tag 集合 / 条件查询，复用 ItemMap 遍历 | 未实现 |
| 资产异步预加载 | `GetAssetFromRegistry` 的 `LoadSynchronous` 改 StreamableManager 异步 + 句柄回调 | 未实现 |
| 配置热重载 | PIE / 编辑器内重跑 Load 系列函数，需先处理缓存指针失效 | 未实现 |
| 缓存层独立 | 当前无 CacheManager 类；引入前必须先解决 `const_cast` 只读约定 | 评估中 |

明确不采纳：**LRU 缓存淘汰**（配置全量常驻，见系统设计 §7）。

---

## 9. 相关文档

- [ConfigDataSubsystem-系统设计](/VoxelDesign/02-系统设计/ConfigDataSubsystem-系统设计.md)
- [物品数据模型-系统设计](/VoxelDesign/02-系统设计/物品数据模型-系统设计.md)
- [家具系统-系统设计](/VoxelDesign/02-系统设计/家具系统-系统设计.md)
- [VoxelItemWorldSubsystem-架构设计](/VoxelDesign/03-架构设计/VoxelItemWorldSubsystem-架构设计.md)
