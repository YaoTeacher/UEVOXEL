# 03-架构设计

本目录包含 Voxel 项目的架构设计文档。

---

## 核心文档

| 文档 | 内容 |
|---|---|
| [经营与 AI-架构设计](/VoxelDesign/03-架构设计/AI决策系统-架构设计.md) | 经营链与 AI 的唯一运行时架构：子系统、组件、数据结构、决策/执行/结算/存档流程；原「V1 模块架构设计」「V1 架构模块清单」两份导航已并入本文，不再单独维护 |

---

## 世界子系统

| 文档 | 内容 |
|---|---|
| [VoxelBlockWorldSubsystem-架构设计](/VoxelDesign/03-架构设计/VoxelBlockWorldSubsystem-架构设计.md) | 体素世界生成、区块流式加载与编辑 |
| [VoxelAreaWorldSubsystem-架构设计](/VoxelDesign/03-架构设计/VoxelAreaWorldSubsystem-架构设计.md) | 区域 CRUD、空间查询、区域组件与存档编排 |
| [VoxelCreatureWorldSubsystem-架构设计](/VoxelDesign/03-架构设计/VoxelCreatureWorldSubsystem-架构设计.md) | 名册、生成/回收、生物组件与存档 |
| [VoxelItemWorldSubsystem-架构设计](/VoxelDesign/03-架构设计/VoxelItemWorldSubsystem-架构设计.md) | 物品实例、家具、工作点/设施/储物组件 |
| [WorldObjectPoolSubsystem-架构设计](/VoxelDesign/03-架构设计/WorldObjectPoolSubsystem-架构设计.md) | 对象池 |
| [ConfigDataSubsystem-架构设计](/VoxelDesign/03-架构设计/ConfigDataSubsystem-架构设计.md) | 配置装载与只读查询，含经营配置 |

---

## UI 与工具

| 文档 | 内容 |
|---|---|
| [UI系统-架构设计](/VoxelDesign/03-架构设计/UI系统-架构设计.md) | CommonUI + MVVM + 经营业务门面 |
| [Utils-架构设计](/VoxelDesign/03-架构设计/Utils-架构设计.md) | 通用工具 |

---

## 编辑系统

| 文档 | 内容 |
|---|---|
| [编辑系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-架构设计.md) | 编辑系统总览 |
| [编辑系统-Controller-架构设计](/VoxelDesign/03-架构设计/编辑系统-Controller-架构设计.md) | Controller 组件 |
| [编辑系统-Controller组件重组-架构设计](/VoxelDesign/03-架构设计/编辑系统-Controller组件重组-架构设计.md) | Controller 组件重组 |
| [编辑系统-工具系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-工具系统-架构设计.md) | 工具系统 |
| [编辑系统-选区系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-选区系统-架构设计.md) | 选区系统 |
| [编辑系统-模板系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-模板系统-架构设计.md) | 模板系统 |
| [编辑系统-阵列系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-阵列系统-架构设计.md) | 阵列系统 |
| [编辑系统-预览系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-预览系统-架构设计.md) | 预览系统 |
| [编辑系统-历史记录系统-两段式存储-架构设计](/VoxelDesign/03-架构设计/编辑系统-历史记录系统-两段式存储-架构设计.md) | 历史记录 |
| [编辑系统-切面系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-切面系统-架构设计.md) | 切面系统 |
| [编辑系统-FVoxelGeometryDesc数据结构-架构设计](/VoxelDesign/03-架构设计/编辑系统-FVoxelGeometryDesc数据结构-架构设计.md) | 几何描述 |
| [编辑系统-C++与蓝图职责划分-架构设计](/VoxelDesign/03-架构设计/编辑系统-C++与蓝图职责划分-架构设计.md) | C++ 与蓝图边界 |
| [编辑系统-架构设计-v1](/VoxelDesign/03-架构设计/编辑系统-架构设计-v1.md) | 历史版本（归档） |

---

## 架构原则

1. **实体即容器，数据与行为在组件**：区域、家具、生物都是 Actor + 组件。
2. **一个事实一个所有者**：钱归经济、合同归劳动、劳动安排归生物、Shop 状态归 Shop 组件、时间归时间。
3. **派生数据不入档**：看板、认领、`bOperating`、站位缓存、结算计划表，读档重建。
4. **唯一写入口**：看板只由工单索引器写；`bOpen` 只由 Shop 组件写；钱包只由经济业务方法与结算器写。
5. **事件只通知不调度**：事件同步分发、载荷只读；何时结算由结算调度器按 `Next...Sec` 决定。
6. **执行层只有一套**：自研流程解释器；表现层用 AnimBP / Motion Matching。
7. **UI 不直写数据**：UI → Controller 业务门面 → 组件业务方法 → 广播 → UI 订阅刷新。
8. **宿主按生命周期**：跨场景/跨关卡存活的状态放 `UGameInstanceSubsystem`；绑定当前场景的状态放 `UWorldSubsystem`；GameInstance 不持 Actor/World 指针。

---

## 设计流程

```text
需求与问题分析 → 系统设计 → 架构设计 → 设计计划 → 实施
```
