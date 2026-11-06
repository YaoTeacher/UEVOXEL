# AI决策系统-需求与状态层-系统设计

**版本**: 1.5
**创建日期**: 2026-09-14
**最后更新**: 2026-11-01（v1.5：工作区抑制改读劳动安排，统一需求事件载荷与 UseItem 驱动口径）
**状态**: 定案（继承 v4.7 三层模型讨论，BuffTag 层改述归并统一效应模型）

> 本文是 NPC 动态数值层（需求/状态）的独立设计文档。与旧分册的关系：取代《AI决策系统-NPC个体与决策-系统设计.md》§2.7 中"BuffTag 层"的表述（效应不再是独立层，统一走《AI决策系统-统一效应模型-系统设计.md》）；需求层/状态层定义沿用并收编于此。旧文档未改动。

## 1. 定位与分层依据

NPC 的内在数值按**变动方式**分两层，分层只看变动规则，不看业务主题。统一服务四类 Actor（消费者/员工/公共服务 NPC/敌人野兽）与四类场景（经营/消费/社交对话/探索战斗）。

- **需求层（时间驱动 → 决定"去做什么"）**：随时间自动变动（消耗或积累），行为回填
- **状态层（事件/环境驱动 → 决定"以什么状态做"）**：不随时间自动变动，只被事件/环境改变
- **效应不在本文**：一切过阈/事件命中的产物统一为修改/Buff 两形态，形态与管线见《AI决策系统-统一效应模型-系统设计.md》

**全局只有一条触发规则**：需求或状态的值过阈值（或特定事件命中）→ 发放 Buff（读 Buff 配置挂实例）或即时修改。行为系统主要消费 Buff 列表与 Tag 容器，不直接消费数值条。

## 2. 需求层（6 条）

| 需求 Tag | 时间变化 | 过阈产物（发放 Buff） |
|------|------|------------------------|
| `Creature.Demand.Hunger` 饥饿 | 负速率，小时级 | 向下跨 Critical → `Buff.Urgency.Famished`；向下触底 → 饿晕（断片）+ 持续扣 Health |
| `Creature.Demand.Thirst` 口渴 | 负速率，小时级 | 向下跨 Critical / 触底，后果同 Hunger |
| `Creature.Demand.Energy` 精力 | 负速率；活动 / 工作另施加负向增量 | 向下跨 Critical → `Buff.Urgency.Exhausted`（可被玩家指派压制，面露难色）；向下触底 → 昏睡（断片） |
| `Creature.Demand.Leisure` 娱乐 | 负速率，慢 | 向下跨低值阈值 → `Buff.Urgency.Bored`（压低 Mood 基线） |
| `Creature.Demand.Social` 社交 | 负速率，慢 | 向下跨低值阈值 → `Buff.Urgency.Lonely`（激活 SocialDrive 修饰器通道） |
| `Creature.Demand.Shopping` 消费欲 | 正速率，天 / 周级慢积累；橱窗 / 广告 / 话题 / 耐久损耗 / 社交比较另施加事件脉冲 | 向上跨配置阈值 → `Buff.Urgency.Wanting`（购物候选加分）；品类偏好由 ActivityWillingness 承载，需求条只有一个 |

六条需求共用 `FinalMin / FinalMax / ChangeRatePerGameSecond / Thresholds` 规则：变化率负值表示衰减，正值表示积累。行为回填与事件脉冲作为一次性增量进入同一数值和跨阈管线，不另设只用于回升的触发器；具体速率、阈值与 Buff 均由配置给出。

**V1 工作区需求例外**：实际劳动者位于其 `FWorkAssignment.WorkAreaRef` 对应的 Shop 区域内时，会导致需求下降的自然时间变化与工作负向增量均不应用，六条需求不因上班耗时或工作动作下降；离开该工作区域后恢复。该规则不清空已有需求值、Buff 或 Critical 状态，也不阻止正向回填通过统一入口生效。判断只读劳动安排对应的 Shop 与实际区域在场，不读雇佣关系推导工作区域，不新增冻结标志或存档字段。员工、经营人与薪酬制店主均按同一劳动安排判定。

**吃饭与使用由需求驱动**：`UseItem` Flow 的优先级高于工作，时段块只调整权重；`Dine` 不是唯一允许吃饭的时段，吃饭也不占独立排班类型。流程为：选物品（背包优先）→ 就位（店内餐桌 / 座位优先，有限搜索半径；没有则原地并记“无餐桌”负面体验）→ 等待使用时长 → 触发物品使用效果 → 扣减数量 / 剩余物。需求回填、状态效果与消耗均由物品配置定义，Flow 只执行通用流程。

**触底后果通用化（定案）**：所有硬性生理需求共用双阈值模式——Critical 线（可压制：压下冲动继续当前行为，外化面露难色）/ 触底线（不可压制 = 断片等生理失控：不可寻路，原地昏厥/失禁等，定时苏醒 + 负面记忆 + 外化）。后果按需求配置，不做"精力特殊化"。

## 3. 状态层（5 条）

**V1 五条状态只计算数值与展示，不驱动行为；表中效率、小费、社交、等待、离店、战斗和其他行为作用均为后续版本。**

| 状态 | 变动源 | 作用 |
|------|--------|------|
| Mood 情绪 | 事件脉冲（消费体验/社交/冲突/记忆回闪/需求持续 Critical）+ 基线回归 | V1 只数值 / 展示，`MoodMod=1`；后续才接工作效率、小费率与行为修饰 |
| Health 健康 | 伤害/疾病事件；治疗/休养恢复 | V1 只数值 / 展示，不驱动效率、战斗或倒地；后续才接这些行为效果 |
| Hygiene 卫生 | 变脏事件（进食/工作/战斗/脏环境）；清洗恢复 | V1 只数值 / 展示，不驱动社交或环境行为；后续才接 `Buff.Condition.Filthy` 等行为效果 |
| Comfort 舒适 | 环境派生（座椅/拥挤/空间/美观，读所处区域设施 Stats 与槽位占用） | V1 只数值 / 展示，不改变等待破底、提前离店或 Mood；后续才接这些行为效果 |
| Loyalty 忠诚（员工域） | 工资兑现+ / 训诫− / 诉求满足+ 等明确事件 | V1 只做数值 / 展示，不以 Loyalty=0 自动解除雇佣；后续：辞职梯度、偷懒权重 |

事件脉冲 + 基线回归是状态层的标准变动模式：机制 = 对状态值的即时修改（应用即弃）+ 状态容器自身的基线回归（变动模式字段，见 §5）——RimWorld thoughts 式"情绪会过期"的语义由这条组合承载，事件本身不带时效字段；禁止"只进不出的永久抬升"。

Loyalty 在 V1 只由工资兑现、训诫、诉求满足等明确事件修正，不随时间无条件下降。工资总是足额支付并允许雇主经营账户扣负，因此不存在 `bWageUnpaid`、未领薪持续条件修正或 Loyalty=0 自动离职。

## 4. Remedy 通道（诉求 → 服务匹配）

Buff 与记忆锚点可声明"解决途径"（RemedyTag = 能清除它/解决它的服务类型）——"NPC 有悬而未决的问题 → 主动找能解决的服务"的统一通道：

```
Buff.Condition.Pain     → Remedy: 药店 / 医馆
Buff.Condition.Filthy   → Remedy: 浴场 / 洗浴设施
Buff.Social.Humiliated  → 轻微: Buff 到期自消; 严重 + 高竞争性 → Remedy: 打手/杀手服务
记忆锚点"悬案/被坑"      → Remedy: 侦探所 / 万事屋
```

带着有 Remedy 的条目 → 评分候选收集把能提供该 Remedy 的店铺纳入候选；**认知过滤照常**（不知道侦探所存在就找不了，"门路"是信息资产，与认知/情报玩法咬合）。新店铺类型 = 声明自己能解决哪些 RemedyTag，零新机制。买凶/复仇类低频高戏剧行为由"仇恨记忆 + 高竞争性 + 认知门路 + 消费力"四者交集涌现——稀有但会发生，是叙事主产地。

## 5. 代码承载

- 需求与状态数值同存 `FCreatureRuntimeState::Dynamics`（`TMap<Tag, float>`），分层语义由规则配置声明；Tag 命名空间区分为 `Creature.Demand.*` / `Creature.State.*`。`CachedDemandRules` 只缓存本文六条需求，状态值不随需求 tick 自动衰减。
- `FRuntimeDemandRule{DemandTag, ChangeMode, ChangeRatePerGameSecond, FinalMin, FinalMax, Thresholds}`；`FDemandThresholdRule{ThresholdValue, Direction, TriggerTag}`。方向为 Downward / Upward / Both，字段与伪代码见 [AI 决策系统架构 §4.2](/VoxelDesign/03-架构设计/AI决策系统-架构设计.md)。
- 固定逻辑 tick 以游戏时间增量调用 `UpdateDynamicAttributes`；行为回填与 Shopping 事件脉冲调用同一 `ApplyDemandDelta`。只有实际跨阈才发射 `OnTriggerEvent`，持续处于阈值另一侧不重复发射；一次推进跨多个阈值按跨越顺序处理。倍速只放大游戏时间增量，暂停时增量为 0，见 [时间系统-系统设计](/VoxelDesign/02-系统设计/时间系统-系统设计.md)。
- 需求跨阈事件载荷统一为 `{CreatureID, NeedTag, 方向, 阈值, GameTimeSec}`：方向记录本次实际跨越方向，阈值记录被跨越的配置值，时刻取统一游戏时钟。跨阈游标只存名册 `Dynamics`，不随事件复制，也不另建持久化状态。
- Buff 实例的结构、配置与解析管线：见《AI决策系统-统一效应模型-系统设计.md》。
- **需求索引迁移（定案）**：旧 `EDemandEnum`（7 值，被 UI/物品引用）退役；ini 旧三需求 `Creature.Demand.Mentality/Satiety/Thirsty` 映射到新六条——Mentality 是旧"精神需求"残留，拆给 Leisure/Social/Mood 后退役；Satiety→Hunger；Thirsty→Thirst（命名统一为 Hunger/Thirst，ini 与物品 LinkedDemand 同步改造）
