# AI决策系统-角色行动与执行层-系统设计

**版本**: 1.0
**创建日期**: 2026-10-17
**所属**: [AI 决策系统总纲](/VoxelDesign/02-系统设计/AI决策系统-系统设计.md)
**本分册职责**: 消费者与员工如何行动——UE5 能力映射、槽位占用、动画驱动、移动与导航

> 执行编排使用自研 `FFlowState`，不使用 StateTree / Behavior Tree。UE5 只提供移动、导航、槽位与动画原语。

---

## 1. 系统边界

**负责**：消费者/员工的动作链、槽位占用、动画驱动接口、移动与导航的使用方式、中断与失败处理。

**不负责**：决策与仲裁（意图与流程统一模型）、设施业务规则（家具系统）、店铺经营规则（经营卷）。

---

## 2. UE5 能力映射

| 引擎能力 | 用途 | 采用 |
|----------|------|------|
| Navigation System（Recast） | NavMesh、动态重建、NavArea、NavModifier、NavLink | 是 |
| AIController + PathFollowing | `MoveToLocation/MoveToActor`、`OnMoveCompleted`、`StopMovement` | 是 |
| CharacterMovement | 行走、旋转、根运动 | 是 |
| StaticMesh Socket | 家具槽位声明（route/consumer/staff/surface/entry/exit/queue） | 是 |
| AnimBP + Montage + AnimNotify | 动作播放与事件回写 | 是 |
| MotionWarping | 进槽位的位置/朝向对齐 | 是 |
| Skeletal Socket / Attach | 手持货物、餐盘 | 是 |
| SmartObjects | 槽位定义 + 引擎 claim | 否（自建 claim） |
| GameplayTasks | 异步任务编排 | 否（自研 FlowState） |
| EQS | 空间候选查询 | 否（区域设施索引 + 导航查询） |
| AI Perception | 视觉/听觉 | 只服务事件/战斗 |
| Mass Entity | 大规模人群 | 否（固定名册个体） |
| GAS | 技能/消耗 | 否 |
| ControlRig/IK | 精细手部贴合 | 可选 |

---

## 3. 消费者行动链

```
FIntent（Consume）绑定 → FFlowState 第 1 步
→ AIController::MoveToLocation(入口/队列点)
→ 到点：TryReserveSlot(Queue/Seat) 预约
→ 等待（服务驱，Timer/事件，无移动）
→ 付款 / 下单（数据操作）
→ MoveToLocation(slot_route)
→ snap 到 slot_consumer_<sit|stand>：CommitSlot + MotionWarping
→ AnimBP 播吃饭/等待/饮酒动作
→ Settle：需求回填、支付、体验上报
→ ReleaseSlot → MoveToLocation(出口) → 流程结束
```

- 寻路只到 route 点，功能位靠 snap；
- 预约与占用分开：预约后可被延迟，超时自动回收；
- 打断（Critical/打烊）：停止移动、释放槽位、`FFlowState` 保存断点或失败。

---

## 4. 员工行动链

```
扫描看板 → 认领 FIntent（工单认领）→ FFlowState 第 1 步
→ MoveToLocation(仓库 / 灶台 / 桌位 / 货架)
→ Acquire：占功能槽 + Attach 货物到手持 Socket
→ AnimBP 播工种动作
     短动作：等 Montage 完成 / AnimNotify
     长循环动作：逻辑计时，Montage 循环
→ Settle：设施三槽 / 库存 / 流程节点推进
→ Release：释放槽位与工单认领 → 回到扫描
```

- 工单认领（工单归属）与槽位占用（物理资源）是两把锁，分开释放；
- 可暂停工单中断时进度存 `FFlowState`；不可暂停工单当前原子步骤清零回池；
- 携带上限 = 个体数值 + 搬运工具加成，超出分多趟。

---

## 5. 槽位与占用（自建 claim）

### 5.1 数据

```
类型级（按 HandleId 缓存）：
FSlotTable
└─ TArray<FSlotDef { Index, Role, LocalTransform, Tags, AllowedOccupantType }>

实例级（随家具实例存档）：
FSlotModule
└─ TMap<int32, FVoxelSlotUse>
     ├─ OccupantType   None / Creature / Item
     ├─ OccupantId
     ├─ State          Free / Reserved / Occupied
     └─ Expiry         预约到期时刻
```

槽位角色与允许占用者：

| 角色 | 允许占用者 |
|------|-----------|
| slot_consumer_sit/stand | Creature |
| slot_staff | Creature |
| slot_queue | Creature |
| slot_surface | Item |
| slot_route / slot_entry / slot_exit | None（只做寻路/通行） |

### 5.2 方法

```
TryReserveSlot(Inst, SlotIndex, OccupantType, OccupantId, TTL) → bool
CommitSlot(Inst, SlotIndex, OccupantId)        // 预约 → 占用
ReleaseSlot(Inst, SlotIndex)                   // 清条目
ReleaseAllFor(CreatureId)                      // 死亡/传送/流程失败
ExpireReservations(Now)                        // 低频清理过期预约
```

- 预约超时惰性判定：查询时比较 Expiry，过期视为 Free；
- 查询为区域内小数组线性扫描，不建空间索引；
- 占用是逻辑状态，不依赖碰撞体；
- 读档时清理过期预约与已不存在的占用者。

---

## 6. 动画（AnimBP 全权）

### 6.1 分工

```
C++ / FFlowState                      AnimBP
设置 ActionTag / ActionPhase  ──▶     状态切换 + 播 Montage
设置 Posture / Speed / 持物    ──▶     移动状态机 + 上下半身分层
动作完成 / 关键帧             ◀──     AnimNotify / 委托广播
```

### 6.2 C++ 侧接口

```
UCreatureAnimInstance（基类）
├─ SetActionTag(FGameplayTag)        // Work.Cook / Consume.Eat / Sit / Sleep
├─ SetActionPhase(Start/Loop/End)
├─ SetPosture(Sit/Stand/Lie)
├─ SetCarriedItem(Actor / Mesh)
├─ OnActionFinished 委托
└─ OnActionPoint(PointTag) 委托      // 放盘/切割完成等
```

- C++ 不调用 `PlayAnimMontage`；
- AnimBP 内部维护 `ActionTag → Montage` 映射（放数据资产，不硬编码）；
- 动作结束由 AnimBP 广播委托或 AnimNotify 触发。

### 6.3 时长规则

- **短动作**（擦桌/上菜/吃饭）：以 Montage 时长为准，播完 Notify 完成；
- **长循环动作**（做饭/搬运）：以逻辑计时为准，Montage 循环播放；
- 中断：C++ 清空/改 ActionTag，AnimBP 停止 Montage 回 Locomotion 并广播取消。

### 6.4 身体与对齐

- 子部件 MasterPose 跟随 `MainBodyMesh`，动作只播主躯干；
- 手持物 Attach 到主躯干骨骼/Socket；
- MotionWarping 目标由 C++ 在进槽位前设置，AnimBP 播带 Warp 窗口的 Montage。

---

## 7. 移动与导航

- 项目已启用 Dynamic Recast（CellSize 20cm / AgentRadius 35cm）；
- 家具放置加 `NavModifierComponent`（footprint 障碍）；
- 门用 `NavLinkProxy` 或动态 NavModifier：锁门封路、开门放行；
- 体素编辑已按区块触发导航更新：`ChunkBase::ApplyMeshRender` 重建网格后对 Mesh/MeshGhost 调 `FNavigationSystem::UpdateComponentData`，Dynamic Recast 重建脏 tile；粒度是区块不是逐格；批量编辑的合并/节流待评估；
- 可达性用 `FindPathToLocationSynchronously` / `ProjectPointToNavigation`，不跑 EQS；
- 大量 NPC 时可启用 Navigation Invokers（当前关闭）。

---

## 8. 中断与失败

| 情形 | 处理 |
|------|------|
| 需求 Critical / 硬生理极限 | 停止移动、释放槽位、FlowState 保存或失败 |
| 下班 / 交接 | 可暂停工单留存，不可暂停清零回池 |
| 目标被占 / 槽位预约失败 | 候选过滤或换下一个候选，不重复寻路 |
| 寻路失败 | 释放预约、记录失败、重试或换目标 |
| 打烊清场 | 顾客流程按空间卷规则结束，槽位释放 |

---

## 9. 现状缺口

| 缺口 | 说明 |
|------|------|
| AIController 空 | `ACreatureAIController` 未挂 StateTree/行为逻辑，移动未接 |
| AnimBP 缺失 | NPC 体型/种族 ABP 未创建，主躯干 AnimBP 设置被注释 |
| ActionTag→Montage 映射 | 数据资产未建 |
| AnimNotify 桥 | 未与 FlowState 接通 |
| 槽位 claim 服务 | 未实现，FSlotTable 解析器未落地 |
| NavModifier/门导航 | 家具与门未接导航 |
| 导航更新节流 | 区块重建逐次触发 `UpdateComponentData`，批量编辑无合并 |
| MotionWarping | 未配置 |

---

## 10. V1 范围

**做**：MoveTo 移动、自建槽位 claim、两阶段交互、AnimBP 全权动作、短/长动作时长规则、家具与门导航、中断释放。

**不做**：SmartObjects、GameplayTasks 编排、EQS、IK 精细贴合、物理搬运。

---

## 11. 相关文档

- [AI决策系统-意图与流程统一模型-系统设计](/VoxelDesign/02-系统设计/AI决策系统-意图与流程统一模型-系统设计.md)
- [家具系统-系统设计](/VoxelDesign/02-系统设计/家具系统-系统设计.md)
- [AI决策系统-员工与工作执行-系统设计](/VoxelDesign/02-系统设计/AI决策系统-员工与工作执行-系统设计.md)
- [AI决策系统-店铺空间与消费流程-系统设计](/VoxelDesign/02-系统设计/AI决策系统-店铺空间与消费流程-系统设计.md)
