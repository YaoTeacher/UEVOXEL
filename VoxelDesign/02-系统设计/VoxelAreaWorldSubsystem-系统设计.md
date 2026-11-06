# VoxelAreaWorldSubsystem-系统设计

**版本**: 2.0
**创建日期**: 2026-01-24
**最近更新**: 2026-11-05
**所属**: [AI 决策系统总纲](/VoxelDesign/02-系统设计/AI决策系统-系统设计.md)
**本分册职责**: 区域子系统的职责边界与对外接口索引。区域概念、空间模型、权限与存档语义见[《区域系统-完整设计》](/VoxelDesign/02-系统设计/区域系统-完整设计.md)；实现结构、数据结构与算法见[《VoxelAreaWorldSubsystem-架构设计》](/VoxelDesign/03-架构设计/VoxelAreaWorldSubsystem-架构设计.md)。

> 修改日志：2026-11-05 收敛为职责与接口索引（669 行 → 86 行）；概念、原则、约束、目标、算法与示例移出，改为引用。

---

## 1. 职责与边界

| 做 | 不做 |
|---|---|
| 区域增删改查：单区域/批量生成、按 ID 或指针修改、删除 | 经营事实：产权、经营关系、租约、bOpen、目录、订单、声誉 |
| 范围/层级/父子与重叠校验；空间索引维护 | 区域概念、覆盖与面的定义、编辑权限布尔（→《区域系统-完整设计》§2/§4/§6） |
| 空间查询：按格/世界坐标、按类型、按 ID、顶层区域列表 | 经营实体与子功能区的建销、经营登记（→《区域系统-完整设计》§3） |
| 创建/修改权限校验：消费经营授权结果，不存授权真值 | 体素/面覆盖层与挂件的存储（→《区域系统-完整设计》§5） |
| 可视化显隐开关；场景存档中区域数据的编排（目标，接口待补） | 导航网格生成：只提供边界，更新由 NavigationSystemV1 执行 |

边界与空间规则的唯一信源是《区域系统-完整设计》§1 目标与非目标、§4 空间模型、§16.1 业务边界；本文不重复。

---

## 2. 对外接口

只列签名；实现结构、参数校验与内部流程见[《VoxelAreaWorldSubsystem-架构设计》](/VoxelDesign/03-架构设计/VoxelAreaWorldSubsystem-架构设计.md) §2。

| 类别 | 签名 | 用途 |
|---|---|---|
| 查询 | `int32 GetAreaIndexByPosition(FIntVector BlockPos) const` | 按格坐标返回最小包含区域 ID，未命中返回 -1 |
| 查询 | `AVoxelAreaBaseInstance* GetAreaActor(int32 AreaId) const` | 按 ID 安全取区域 Actor |
| 查询 | `AVoxelAreaBaseInstance* GetAreaAtWorldPosition(const FVector& WorldPos) const` | 按世界坐标取所属区域 |
| 查询 | `TArray<AVoxelAreaBaseInstance*> FindAreasByType(FGameplayTag InputType) const` | 按区域类型取区域列表 |
| 查询 | `TArray<AVoxelAreaBaseInstance*> GetTopLevelAreas() const` | 全部顶层区域，UI 树/列表统一取数入口 |
| 查询 | `TArray<AVoxelAreaBaseInstance*> GetPlayerTopLevelAreas(int32 PlayerCreatureId) const` | 玩家管理/拥有的顶层区域 |
| 查询 | `TArray<AVoxelAreaBaseInstance*> GetOtherPlayersAreas(int32 PlayerCreatureId) const` | 他人区域，用于黑名单排除 |
| 生成 | `void GenerateAreas(FSceneData& SceneData)` | 按场景存档批量生成区域并重建索引 |
| 生成 | `AVoxelAreaBaseInstance* GenerateAreaInstance(const FAreaModificationData&, AVoxelAreaBaseInstance* CurSelectedArea, AVoxelPlayerController*)` | 运行时生成单个区域 |
| 修改 | `void UpdateAreaByPtr(AVoxelPlayerController*, AVoxelAreaBaseInstance*, const FAreaModificationData&)` | 修改覆盖范围/名称等空间字段并重注册索引 |
| 修改 | `void UpdateAreaById(AVoxelPlayerController*, int32 AreaId, const FAreaModificationData&)` | 同上，按 AreaId 入参 |
| 修改 | `void UpdateAreaCoverageById(int32 Id, const FBox& InputCoverage)` | 只改覆盖范围并重注册索引 |
| 修改 | `void UpdateAreaType(int32 AreaId, FGameplayTag)` / `CheckAreaType` / `UpdateAreaTypeAfterCheck` | 区域类型校验与写入 |
| 初始化 | `void InitializeAreaInstance(FAreaInstance&, int32 CreatorId) const` | 由区域数据初始化实例 |
| 删除 | `void CleanAreaFromIndexById(int32 InputId)` | 删除区域并清理全部索引 |
| 校验 | `bool IsAreaConflicting(FAreaInstance& Input) const` | 与现有区域做重叠与父子约束检测 |
| 可视化 | `void UpdateAreaVisual(AVoxelAreaBaseInstance*, bool)` / `UpdateSubAreasVisual(AVoxelAreaBaseInstance*, bool, int32 CharId = -1)` / `UpdateAllAreasVisual(bool)` | 单区域 / 子树 / 全局显隐 |

**待补（目标设计，代码中尚无，引用时不得当作已有）：**

| 签名 | 用途 | 出处 |
|---|---|---|
| `void CollectSaveData(TArray<FAreaSaveRecord>& Out) const` / `void ApplySaveData(const TArray<FAreaSaveRecord>& In)` | 场景存档中区域数据的导出与导入 | 《AI决策系统-架构设计》§3.5 |
| `TArray<int32> GetSubAreas(int32 AreaId) const` | 直接子区域 | 同上 §4.1.8 |
| `TArray<int32> GetAreasByEntity(FBusinessEntityId EntityId) const` | 按经营实体查区域（含子功能区） | 同上 §4.1.8 |
| `FBusinessEntityId GetEntityOfArea(int32 AreaId) const` | 读区域的 EntityId 引用 | 同上 §4.1.8 |

---

## 3. 依赖关系

| 方向 | 对象 | 内容 |
|---|---|---|
| 依赖 → | UConfigDataSubsystem | 区域类型定义与配置 |
| 依赖 → | UVoxelGameInstance | 场景数据与存档数据 |
| 依赖 → | UVoxelPlayerController | 操作者上下文；创建/修改权限入参 |
| 依赖 → | UNavigationSystemV1 / UWorldObjectPoolSubsystem | 边界变化通知导航更新；区域 Actor 复用（可选） |
| ← 被依赖 | AI 系统 / 物品系统 | 按格或坐标定位区域、按类型取区域，用于决策与准入 |
| ← 被依赖 | 编辑建造系统 / UI 系统 | 区域增删改与冲突校验；区域树/列表取数与可视化开关 |

---

## 4. 相关文档

| 文档 | 唯一负责的内容 |
|---|---|
| [区域系统-完整设计](/VoxelDesign/02-系统设计/区域系统-完整设计.md) | 术语、空间模型与覆盖、编辑权限、创建/编辑/删除与存档语义 |
| [VoxelAreaWorldSubsystem-架构设计](/VoxelDesign/03-架构设计/VoxelAreaWorldSubsystem-架构设计.md) | 接口实现、数据结构、算法、生命周期、扩展点 |
| [AI决策系统-架构设计](/VoxelDesign/03-架构设计/AI决策系统-架构设计.md) | §3.5/§4.1 区域子系统类声明、待补查询与存档接口 |
| [AI 决策系统总纲](/VoxelDesign/02-系统设计/AI决策系统-系统设计.md) | 本分册所属与跨系统边界 |

---

**状态**: 目标设计；经营/空间分离与组合创建待迁移，本文不表示代码已实现。
