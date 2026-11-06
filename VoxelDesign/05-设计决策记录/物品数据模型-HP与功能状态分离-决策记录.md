# 物品数据模型-HP与功能状态分离-决策记录

**日期**: 2026-10-26（2026-10-27 补充 HP 合并规则）  
**状态**: 已确认（含实例、堆叠、生命周期与 HP 合并补充）

## 背景

RimWorld 公开反编译代码中，`Thing.TryAbsorbStack` 只在 `def.useHitPoints` 时合并 HP；腐败由独立 `CompRottable` 管理，品质由独立 `CompQuality` 管理。DF 与 Minecraft 又分别采用“原版不提供通用合堆”和“组件一致／耐久物单件”的不同方案。项目需要明确自己的实例归属、堆叠和 ITEM／OBJECT 生命周期，不能把任一外部游戏的条件规则原样当成本项目规则。

## 决策

### 1. HP 与派生完整度

- **所有 ITEM 都有 HP**，HP 数值使用 `float`。静态 ITEM 配置保存 `MaxHitPoints`，运行时 ITEM 实例保存 `HitPoints`；不再使用“存在 `FDamageableFuncFragment` 才有 HP”的可选模型；
- `Integrity = HitPoints / MaxHitPoints`，只作为维修、环境展示和结构判断读取的派生比例，**不作为另一份权威状态保存**；
- 使用磨损、战斗／灾害／事件伤害与维修读写同一份 HP。维修恢复 `HitPoints`，不同时维护一份 `Integrity`；
- 普通 ITEM 的 `HitPoints = 0` 时保留实例，但不可使用、可以维修；归零的是 HP，不是直接删除实例；
- 通过 `CanStackWith` 的不同 HP 堆允许合并，对当前 `HitPoints` 按实际并入数量加权，不对 `Integrity` 或 `MaxHitPoints` 加权：

  `NewHitPoints = (TargetHitPoints × TargetQuantity + SourceHitPoints × MovedQuantity) / (TargetQuantity + MovedQuantity)`

  `TargetQuantity` 是合并前目标堆数量，`MovedQuantity` 是实际移动数量。部分合并时源堆剩余数量保留原 HP；拆分复制、纯移动不变。HP 为 float，不另设取整规则；
- 合并后只保留整堆共享 HP，`Integrity` 从新 HP 派生。HP=0 的堆与正 HP 堆合并也使用同一公式；若结果大于 0，新堆不再处于 HP=0 状态，不另设 HP0 禁止合并例外；
- 消耗品通过使用被消耗／销毁；堆叠 `Quantity = 0` 时销毁该堆实例并清空容器引用。消耗品用完不以“先把 HP 扣到零”为前提；
- 体素与家具遵循同一物理耐久口径。体素正常格默认 `HitPoints = MaxHitPoints`，稀疏状态只在偏离默认值时保存当前 HP；`Integrity` 同样只派生；
- 所有 ITEM 具有 HP 不代表 V1 必须为每类物品新增损耗来源或工具消耗链。

### 2. 实例与容器

- 槽位只保存对一个 ITEM 实例的引用，并以此表达“该实例当前位于这个槽位”；`Quantity` 仍属于 ITEM 堆实例；
- `ItemInstanceRef` 是“指向具体哪一摞实例”的逻辑引用，不是“哪一种物品”的 `ItemId / HandleId`。同种物品的不同堆具有不同实例引用；
- 槽位不得复制保存 HP、腐败、清洁度、品质或数量等实例状态；容器移动与状态修改必须作用于同一个实例；
- 纯移动保持实例及其全部状态不变；数量耗尽后销毁对应堆实例并清空引用；
- `ItemInstanceRef` 目前不是已确定的 C++ 类型；运行时对象引用／句柄、稳定身份和存读档重建方式由架构／存档专项确定。存档不能直接依赖进程内存地址，也不能退化为槽位只保存 `ItemId + Quantity`。

### 3. 品质、放置与堆叠

- 品质独立于 HP。启用实例品质的 ITEM 使用独立实例状态；**不同品质不能共槽、共堆**；
- `FPlacementFragment` 只决定是否可放置，`MaxStackCount` 决定堆叠上限；可放置不等于必须单件存放。花朵、罐子等可以同时可放置、可堆叠；
- 需要保留复杂内部状态的对象可以配置为单件，但不能对全部可放置物一刀切；
- 从可放置堆中放置一件时，从原堆拆分一件并交接当前状态，不得重置为全新默认状态；收起后再按状态兼容与合并规则入包。

### 4. 清洁度生命周期与合并

- `Cleanliness` 只表示脏污，与 HP、腐败、品质分开；
- **只有放置为 OBJECT 后才运行清洁度计算**。使用、环境和清洁等行为修改放置态 OBJECT 的清洁度；
- 收起为 ITEM 时保存当前清洁度；ITEM 位于背包、货架或其他容器时只保留该值，不持续推进清洁度；
- 重新放置时恢复 ITEM 保存的清洁度，并恢复 OBJECT 的后续计算；任一时刻只有当前形态持有权威状态，不能 ITEM／OBJECT 各存一份同时变化；
- 两堆合并时按**实际并入数量**加权：

  `NewCleanliness = (TargetCleanliness × TargetQuantity + SourceCleanliness × MovedQuantity) / (TargetQuantity + MovedQuantity)`

  受 `MaxStackCount` 限制发生部分合并时，源堆剩余数量保留原清洁度；
- 拆分时复制拆分前的清洁度，纯移动不变；ITEM 期间没有持续清洁度推进；
- 该模型保存的是整堆共享清洁度。不同单件合并后不再保留逐件历史，拆出的单件继承整堆当前值。

### 5. V1 腐败边界

- HP、腐败、清洁度、品质不得互相借用或代为表示；
- **V1 不运行腐败推进**：不随时间扣新鲜度、不推进腐败进度、不过期，也不自然进入 `Spoiled`；因此不触发由自然变质引起的出售、食用、加工或丢弃后果；
- `ShelfLifeSec / PreserveFactor` 等配置可保留为后续能力，但 V1 不产生腐败计算；
- 后续再确定腐败字段、环境推进、合并和阶段规则，不提前写成 RimWorld 的 `RotProgress` 公式。

## 理由

- 避免 `HitPoints` 与 `Integrity` 双写冲突，同时满足所有 ITEM 统一拥有物理耐久的项目口径；
- 槽位只引用实例，避免容器与 ITEM 各自保存一份状态后出现数量、HP 或清洁度不一致；
- 放置能力与堆叠能力独立，才能同时支持复杂单件设施和花朵、罐子等可堆叠放置物；
- HP 与清洁度合并加权、拆分复制与“一堆一份共享状态”的模型一致；清洁度只在 OBJECT 中推进，避免容器内隐藏模拟；
- DF 的原版不合并、Minecraft 的耐久物不堆叠都不适合直接覆盖本项目已确认的自动合堆与全 ITEM HP 规则。

## 回写位置

- [物品数据模型](/VoxelDesign/02-系统设计/物品数据模型-系统设计.md)
- [家具系统](/VoxelDesign/02-系统设计/家具系统-系统设计.md)
- [店铺装修](/VoxelDesign/02-系统设计/AI决策系统-店铺装修-系统设计.md)
- [库存与保质期](/VoxelDesign/02-系统设计/AI决策系统-库存与保质期-系统设计.md)
- [店铺空间与消费流程](/VoxelDesign/02-系统设计/AI决策系统-店铺空间与消费流程-系统设计.md)
- [架构设计](/VoxelDesign/03-架构设计/AI决策系统-架构设计.md)
- [经营系统 V1 清单](/VoxelDesign/04-设计计划/经营系统-最小版本设计逻辑清单-设计计划.md)

## 2026-10-31 补充：ItemInstanceRef 采用 RimWorld 堆身份方案（已确认）

- `FItemInstanceRef` 只保存 `FGuid ItemInstanceId`，无效 GUID 表示空槽；`UVoxelItemWorldSubsystem.ItemInstances` 唯一登记实例及状态，运行对象索引读档重建。
- 整堆转移属于纯移动并保留原 GUID；只有部分拆分创建的新堆才分配新 GUID，原堆保留原 GUID。合并后保留目标 GUID，源堆耗尽时删除源 GUID。存档只序列化一次实例记录和各槽位 GUID，不保存进程指针。
- 完整字段归属、读档顺序和失败边界见[物品子系统架构 §9](/VoxelDesign/03-架构设计/VoxelItemWorldSubsystem-架构设计.md)与 [AI 决策系统架构 §12](/VoxelDesign/03-架构设计/AI决策系统-架构设计.md)。

## 尚未决策

- 后续易腐状态的最终字段、环境推进、合并公式与腐败阶段共槽规则；
- 哪些物品启用实例品质，以及实例品质的具体结构；本次不给所有物品新增品质模块；
