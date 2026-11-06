# AI决策系统-意图与流程统一-决策记录

**日期**: 2026-10-17
**决策状态**: 已采纳
**相关文档**: [意图与流程统一模型](/VoxelDesign/02-系统设计/AI决策系统-意图与流程统一模型-系统设计.md)

---

## 背景

消费行为链与工作行为链原本是两套入口：

- 消费：需求跨阈 → 硬过滤 → Utility 评分 → FMotivation → 行为 FlowConfig → 执行
- 工作：设施状态/流程节点 → FTaskItem 看板 → 优先级表扫描 → 步骤模板 → 执行

两链共用五原语段、原子步骤与中断优先级，但数据结构（FMotivation / FTaskItem / FActionJob）、模板（行为 Flow / 工单步骤模板）与入口逻辑各不相同，导致：

- 同一件事（给顾客送餐）既是"消费流程的工单"又是"工作链的任务"，语义重复；
- 加新业态/新工单类型要在两套模板里各写一遍；
- 文档与代码里三套容器互相引用，状态机难对齐。

## 选项

| 选项 | 内容 | 结论 |
|------|------|------|
| A | 保持两条链，只抽公共执行器 | 不满足"从事件触发开始统一" |
| B | 只统一事件→入口，后面仍两套 Flow/步骤模板 | 执行层仍分裂 |
| C | 全链路统一：事件→Intent→仲裁→Flow→执行→结算 | **采纳** |

## 决策

采纳 C，具体结构：

1. **事件→意图**：按事件类别写工厂（需求跨阈/设施状态/流程节点/指令/突发），目标、Flow、优先级进模板表；
2. **FIntent**：`Targets: TArray<FTargetSpec {GoalTag, TargetRef, ELogicOp}>` + Payload + FlowType + 内含 FlowState；Work/Self/Command 同结构；
3. **ELogicOp**：公共 `And / Or / Not` 枚举，数组求值（And 全满足、Not 全排除、Or 至少一个）；不使用 TagQuery、不做嵌套；
4. **统一 Flow 表**：`FFlowConfig` 按 FlowTypeTag 索引，消费行为（Consume.*）与工作类型（Work.*）同表；节点 = 一级 PlanStep（原 FFlowNode）；
5. **一条意图一个对象**：流程状态内含，租约、回池、休眠都是它的状态策略；
6. **仲裁策略可插拔**：Free=Utility、Duty=优先级表、强制=指令/安全/硬生理极限。

## 迁移映射（旧→新）

| 旧 | 新 |
|----|----|
| `FMotivation` | `FIntent`（Source=Self，State=Bound） |
| `FTaskItem` | `FIntent`（Source=Work，State=Pending，挂区域看板） |
| `FActionJob` | `FFlowState`（FIntent 内的流程实例状态） |
| `FBusinessFlowConfig` | `FFlowConfig` |
| 工单"步骤模板" | 工作类型的短 Flow（Work.*） |

## 废弃内容

- 消费/工作两套入口与两套模板；
- `FTaskItem`、`FMotivation`、`FActionJob`、`FBusinessFlowConfig` 作为独立结构/名称；
- "每店铺类型一份 Flow"的口径（改为按 FlowTypeTag 索引）；
- 步骤模板与 Flow 节点两套结构（统一为一级 PlanStep）。

## 权衡与风险

- 重构成本：三套容器合并、既有引用全量改名；
- 语义风险：服务驱等待（排队/等上菜）与工作租约/回池不能压成一种状态，需在 FlowState 中并存；
- 文档风险：旧文档引用多，已做全量改名，历史版本记录保留旧名；
- 后续扩展：嵌套条件、全配置映射 DSL 均未做，出现真实需求再升级。

## 影响文档

- [AI决策系统-系统设计](/VoxelDesign/02-系统设计/AI决策系统-系统设计.md)
- [AI决策系统-NPC个体与决策-系统设计](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)
- [AI决策系统-店铺空间与消费流程-系统设计](/VoxelDesign/02-系统设计/AI决策系统-店铺空间与消费流程-系统设计.md)
- [AI决策系统-店铺经营-系统设计](/VoxelDesign/02-系统设计/AI决策系统-店铺经营-系统设计.md)
- [AI决策系统-员工与工作执行-系统设计](/VoxelDesign/02-系统设计/AI决策系统-员工与工作执行-系统设计.md)
- [AI决策系统-业态模板-系统设计](/VoxelDesign/02-系统设计/AI决策系统-业态模板-系统设计.md)
- [AI决策系统-架构设计](/VoxelDesign/03-架构设计/AI决策系统-架构设计.md)
