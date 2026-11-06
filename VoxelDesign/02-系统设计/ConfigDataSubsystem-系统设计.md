# ConfigDataSubsystem-系统设计

**版本**: 2.0
**创建日期**: 2026-01-24
**最后更新**: 2026-10-17
**说明**: v2.0 按实际代码重写，删除旧版的 LRU 缓存、PlayerTemplateMap、`GetConfigData<T>(Tag)` 统一路由等未实现内容。

---

## 1. 系统上下文

### 1.1 系统边界

**ConfigDataSubsystem 负责**：
- 启动期把配置从 DataRegistry / DataTable / AssetRegistry / 磁盘模板文件装载为内存缓存
- 提供只读查询：物品（按句柄/品类）、区域（按 Tag）、UI（按 Tag）、生物模板（按 Tag）、事件（按资产 ID）、运行时模板（按句柄/Guid）
- 维护物品身份体系：`FullId = Source:IdentityName` → `HandleId = hash(FullId)`
- 运行时模板的注册、注销与句柄分配

**不负责**：
- 配置数据的持久化读写（`SaveAllData/LoadAllData` 是存根）
- 配置编辑界面（UI 系统）
- 资产版本管理（AssetManager）
- 业务规则校验（各业务系统）
- 按类型的数据校验与安全取用（现状缺口，见 §6）

### 1.2 数据来源

| 来源 | 内容 | 装载时机 |
|------|------|----------|
| DataRegistry `DR_Item` | 物品行（方块/家具/消费品/装备） | Initialize |
| DataRegistry `DR_Area` | 区域类型行 | Initialize |
| DataTable `TagConfigTable` | Tag UI 定义（名称/图标/颜色） | Initialize |
| DataRegistry `Creature.Identity.Race/.Profession/.Trait` | 生物种族/职业/特质 | Initialize |
| AssetRegistry 扫描（EventConfig 目录） | `UCreatureEvent` 事件资产 | Initialize |
| AssetRegistry 扫描（FormulaSearchPath） | `UGlobalAttributeFormulas` 属性公式 | Initialize |
| 磁盘 `Saved/Templates/*.tmpl` | 运行时建筑模板 | Initialize + 运行期注册 |

### 1.3 输入输出

**输入**：装载请求（启动自动）、查询请求（HandleId / FullId / Tag / PrimaryAssetId）、模板注册请求。

**输出**：配置行只读指针、模板指针、Tag 展示信息、句柄（找不到返回 -1）。

---

## 2. 核心概念

### 2.1 身份与句柄

- **FullId**：`Source:IdentityName`，行级唯一身份；存档与模板只写这个字符串。
- **HandleId**：`GetTypeHash(FullId)`，运行时句柄；实例、方块、手持、工单都只存 int32。
- **ItemTag**：层级品类身份，用于 `MatchesTag` 分类查询（物品库、建造面板、配方过滤），不承担能力判定。
- **FPrimaryAssetId**：事件资产身份（模板/事件查询用）。

### 2.2 装载规则

- `IdentityName` 为空时回退 DataRegistry 行名，`Source` 为空回退 `core`；
- 句柄碰撞视为数据错误：物品装载直接 Fatal；模板同 FullId 重注册按覆盖处理，不同 FullId 撞 hash 才 Fatal；
- 单行/单资产装载失败只打 Warning，继续装载其余条目；
- 公式资产按“后加载覆盖先加载”合并（支持 MOD 覆盖）；
- 模板句柄不信任磁盘旧值，按 Guid 身份重新计算。

### 2.3 查询语义

| 查询 | 入口 | 未命中的行为 |
|------|------|--------------|
| 物品（按句柄） | `GetItemTypeById<T>` | Warning + nullptr |
| 物品（按品类） | `GetItemTypeArrayByCategory<T>` | 空数组 |
| 物品/模板统一 | `FindByHandle` | nullptr |
| 句柄（按身份串） | `GetHandleByFullId` | 扫描两表自修复，仍失败 -1 |
| 身份串（按句柄） | `GetFullIdByHandle` | 空字符串 |
| 区域类型 | `GetAreaTypeByTag` / `GetAreaTypeArrayByTag` | nullptr / 空数组 |
| UI | `GetTagDisplayName/Icon/Color` | 回退 Tag 名 / 空 / 白色 |
| 生物模板 | `GetRaceTemplate` 等 | nullptr |
| 事件 | `GetLifeEventById` | nullptr |
| 模板 | `GetTemplateById` / `FindTemplateByGuid` | nullptr |

### 2.4 生命周期

```
GameInstance 初始化
  ItemMap / AreaMap / RuntimeTemplates 清空
  → LoadAllItemType → 纹理数组重建
  → LoadAllAreaType → LoadTagUIConfig → CacheRegistryTags
  → LoadAllLifeEvents → ScanAndLoadAllFormulas → ScanAndLoadAllTemplates
  → 就绪
GameInstance 生命周期内：只读查询 + 模板注册/注销
Deinitialize：无显式清理（依赖子系统销毁）
```

---

## 3. 设计原则

1. **启动期一次性装载，运行期只读**：查询不做懒加载、不加锁；未配置的数据源跳过并告警。
2. **身份决定句柄，句柄不落盘**：任何持久化字段只存 FullId 或 HandleId 之一，HandleId 永不手填。
3. **两个容器隔离**：配置物品进 ItemMap，运行时模板进 RuntimeTemplates；合并查询只允许 `FindByHandle` 一个入口。
4. **失败不阻断**：单条坏数据只跳过并记录，不破坏整体装载。
5. **宁可 Fatal 不可错配**：身份哈希碰撞属于数据损坏，直接终止，避免运行期错行。

---

## 4. 系统约束

- **线程**：装载在 GameInstance 初始化期完成；运行期按游戏线程只读访问，无并发写。
- **内存**：全部缓存常驻；物品行指针指向 DataRegistry 内存（`const_cast`），代码约定只读。
- **性能**：句柄查询 O(1)；品类查询 O(n) 全表遍历（低频 UI 使用）。
- **一致性**：`FullIdToHandle` 与 ItemMap/RuntimeTemplates 可能短暂不同步，`GetHandleByFullId` 自带扫描自修复。

---

## 5. 与其他系统的关系

- **物品数据模型**：本文只负责装载与查询；行结构、FunctionFragment、实例模块见[物品数据模型-系统设计](/VoxelDesign/02-系统设计/物品数据模型-系统设计.md)。
- **家具系统**：家具行经 DR_Item 装载；设施运行时数据不在此子系统，见[家具系统-系统设计](/VoxelDesign/02-系统设计/家具系统-系统设计.md)。
- **VoxelBlockWorldSubsystem**：启动时用 `GetHandleByFullId("core:block.stone")` 缓存地面方块句柄。
- **VoxelItemWorldSubsystem**：放置/占用计算用 `GetItemTypeById<FVoxelNormalItemBase>` 取静态数据。
- **UI**：物品库与建造面板用品类查询，Tag 展示走 TagUIMap。
- **AI 决策系统**：生物模板、事件资产、属性公式的查询入口。

---

## 6. 现状缺口

| 缺口 | 说明 |
|------|------|
| `SaveAllData/LoadAllData` 存根 | 只判断文件存在，没有真实存取 |
| `GetAreaTypeTemplateById` 空实现 | 恒返回 nullptr |
| `GetItemTypeById<T>` 无类型校验 | `static_cast` 直转，行结构与 T 不匹配时 UB；需 `GetItemAs<T>` 安全入口 |
| DR_Item 行结构无加载期校验 | 方块行与普通物行混存，业务侧靠 ItemTag 判别 |
| 品类查询无索引 | O(n) 遍历，物品数量大时面板打开开销增长 |
| `ActionFlowConfig` 无消费方 | 死配置引用 |
| 事件/公式扫描路径部分硬编码 | 目录调整需改代码 |

## 7. 设计目标（未实现）

下列方向逻辑成立但当前未实现，不构成现有能力；实施前单独评估优先级。

| 目标 | 说明 | 备注 |
|------|------|------|
| 模板注册变更通知 | `RegisterRuntimeTemplate` / `UnregisterTemplate` 发出广播，UI 与索引层订阅刷新 | 现在只能轮询 |
| 查询性能监控 | 统计查询次数、耗时、未命中热点 Tag | 运维可选 |
| 批量查询接口 | 按 Tag 集合或条件批量取配置 | 当前无需求 |
| 大型资产异步预加载 | 生物模板 / 事件从同步 `LoadSynchronous` 改为异步加载 | 资产量增长后需要 |
| MOD 支持与热更新 | `Source` 命名空间 + 公式“后者覆盖前者”已具雏形；缺热重载与多版本迁移 | 见物品数据模型 §2 |
| 性能验收目标 | 启动装载 < 2s、单次查询 < 1ms、配置常驻内存设上限 | 数值待实测校准 |

明确不采纳：**LRU 缓存淘汰**。配置数据全量常驻、体量可控，淘汰策略只会引入抖动和命中率问题。

---

## 8. 相关文档

- [ConfigDataSubsystem-架构设计](/VoxelDesign/03-架构设计/ConfigDataSubsystem-架构设计.md)
- [物品数据模型-系统设计](/VoxelDesign/02-系统设计/物品数据模型-系统设计.md)
- [家具系统-系统设计](/VoxelDesign/02-系统设计/家具系统-系统设计.md)
- [VoxelItemWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelItemWorldSubsystem-系统设计.md)
- [VoxelBlockWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelBlockWorldSubsystem-系统设计.md)
