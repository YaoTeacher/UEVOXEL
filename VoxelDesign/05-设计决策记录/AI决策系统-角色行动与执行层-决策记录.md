# AI决策系统-角色行动与执行层-决策记录

**日期**: 2026-10-17
**决策状态**: 已采纳
**相关文档**: [角色行动与执行层](/VoxelDesign/02-系统设计/AI决策系统-角色行动与执行层-系统设计.md)

---

## 背景

决策层（Utility + 优先级表）与流程层（FIntent/FFlowState）已经统一，但"角色具体怎么动"没有定：

- `ACreatureAIController` 为空，StateTree 组件是早期验证残留，未接入；
- NPC 是骨骼网格拼装（主躯干 + MasterPose 子部件），AnimBP 设置被注释，没有任何行为动画；
- 家具槽位只有 Socket 命名约定，没有占用/预约的实现；
- 导航是 Dynamic Recast，但家具、门与体素编辑没有接导航更新。

需要决定执行编排、槽位占用、动画驱动、移动导航四条路线。

## 决策

### 1. 执行编排：自研 FFlowState

| 选项 | 结论 |
|------|------|
| StateTree | 不用；早期验证残留，行为算法是自研 Utility，不引入第二套编排 |
| Behavior Tree | 不用；同上，且与优先级表/流程节点重复 |
| GOAP | 不用；`UGOAPAction` 是遗留实验资产，未接入 |
| 自研 FFlowState | **采纳**；流程节点、等待、租约、断点都在自有数据结构里 |

### 2. 槽位占用：StaticMesh Socket + 自建 claim

| 选项 | 结论 |
|------|------|
| Socket + 自建 claim | **采纳**；槽位已是类型级 Socket 数据，占用随家具实例存档 |
| SmartObjects | 不采纳；槽位要另做定义资产生成，占用状态在引擎子系统，预约/超时/存档仍要自建；StateTree 集成用不上 |
| 无 claim | 不采纳；并发与预约不可控 |

占用结构：`FVoxelSlotUse { OccupantType(None/Creature/Item), OccupantId, State(Free/Reserved/Occupied), Expiry }`，队列/台面/工位/路线同一套。

### 3. 动画：AnimBP 全权

| 选项 | 结论 |
|------|------|
| AnimBP 全权 | **采纳**；C++ 只设 ActionTag/ActionPhase/Posture/持物，AnimBP 播 Montage，Notify 回写 |
| C++ 播 Montage | 不采纳；动画资源与状态机分散在逻辑层 |
| ControlRig 程序化 | 不采纳；资产与调试成本高 |

时长规则：短动作以 Montage 时长为准，长循环动作以逻辑计时为准。

### 4. 移动与导航

- `AIController::MoveToLocation` + PathFollowing；
- Dynamic Recast 沿用；体素编辑已按区块触发导航更新（`ChunkBase::ApplyMeshRender → FNavigationSystem::UpdateComponentData`），批量编辑的合并/节流待评估；
- 家具 `NavModifierComponent`，门 `NavLinkProxy`/动态 Modifier；
- 可达性查询用导航系统，不引入 EQS；
- SmartObjects/GameplayTasks/Mass/GAS 均不引入。

## 后果与风险

- AnimBP 全权意味着动作时序、Slot 抢占、打断都在 AnimBP 内，动画资产要按体型/种族成套维护；
- 自建 claim 要自己实现预约超时、死亡清理、存档恢复，但没有第二份占用状态；
- Dynamic Recast 在 20cm 格体素世界上重建成本高；当前区块重建逐次触发 `UpdateComponentData`，没有合并/节流，批量编辑需要评估脏区控制；
- 没有引擎 SmartObject 调试工具，槽位占用要自己做可视化。

## 影响文档

- [角色行动与执行层](/VoxelDesign/02-系统设计/AI决策系统-角色行动与执行层-系统设计.md)
- [家具系统](/VoxelDesign/02-系统设计/家具系统-系统设计.md)
- [员工与工作执行](/VoxelDesign/02-系统设计/AI决策系统-员工与工作执行-系统设计.md)
- [店铺空间与消费流程](/VoxelDesign/02-系统设计/AI决策系统-店铺空间与消费流程-系统设计.md)
