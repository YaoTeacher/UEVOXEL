# NPC AI 决策系统-系统设计（总纲）

**版本**: 5.26
**创建日期**: 2026-06-21
**最后更新**: 2026-11-05

> 修改日志：2026-11-05 意图与流程结构唯一化：`FIntent`／`FPlanStep` 各只保留一份定义，节点集收敛为 9 节点 + 5 内部原语；区域侧只保留区域类型一个分类维度，业态归 `Business.Kind.*`。
>
> 修改日志：2026-11-04 经营实体与区域分离：经营实体为资产与业务唯一所有者（Global 纯数据按类型组合模块），区域只存空间；双账户与唯一流水按经营实体归属。
>
> 修改日志：2026-11-01 时间、结算、看板、劳动与打烊口径收口：唯一游戏时钟、结算调度器逐条叫醒、看板为派生索引、雇佣与劳动安排分离、打烊店内履约店外清退。

> 章节号全局唯一、跨分册连续，分册内章节号不连续是刻意的——章节号是稳定引用锚。

## 分册导航

| 分册 | 链接 | 职责 |
|------|------|------|
| NPC 卷（NPC 个体与决策） | [AI决策系统-NPC个体与决策-系统设计.md](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md) | NPC 个体画像、日程系统、决策架构、评价体系、行为外化、决策 LOD、战斗模式 |
| 空间卷（店铺空间与消费流程） | [AI决策系统-店铺空间与消费流程-系统设计.md](/VoxelDesign/02-系统设计/AI决策系统-店铺空间与消费流程-系统设计.md) | 四层分离基础概念、消费流程、工作点队列系统 |
| 经营卷（店铺经营） | [AI决策系统-店铺经营-系统设计.md](/VoxelDesign/02-系统设计/AI决策系统-店铺经营-系统设计.md) | 口碑与店铺统计、经济循环（员工与招聘、店主决策、店铺活动、供应链已转引用或标记后续版本） |
| 商圈卷（商圈生态） | [AI决策系统-商圈生态-系统设计.md](/VoxelDesign/02-系统设计/AI决策系统-商圈生态-系统设计.md) | 商圈生态演化回路、SpawnArea 流量分布 |
| 区域店铺与商圈管理 | [AI决策系统-区域店铺与商圈管理-系统设计.md](/VoxelDesign/02-系统设计/AI决策系统-区域店铺与商圈管理-系统设计.md) | 区域/店铺/商圈层级、权属与经营关系、生命周期、招商与租约、体素编辑权限 |
| 基础层 | [AI决策系统-基础层模型-系统设计.md](/VoxelDesign/02-系统设计/AI决策系统-基础层模型-系统设计.md) | 七围能力、9 维倾向、特质、身份轴、名声 |
| 需求与状态层 | [AI决策系统-需求与状态层-系统设计.md](/VoxelDesign/02-系统设计/AI决策系统-需求与状态层-系统设计.md) | 需求 6 条（时间驱动）/ 状态 5 条（事件环境驱动）/ Remedy 通道 |
| 关系系统 | [AI决策系统-关系系统-系统设计.md](/VoxelDesign/02-系统设计/AI决策系统-关系系统-系统设计.md) | 账本与余额分离、制度型/情感型关系、派系、脸盲档案 |
| 统一效应 | [AI决策系统-统一效应模型-系统设计.md](/VoxelDesign/02-系统设计/AI决策系统-统一效应模型-系统设计.md) | 修改/Buff 两形态、修改器结构、ApplyMods 单入口、聚合规则 |
| 事件系统 | [AI决策系统-事件系统-系统设计.md](/VoxelDesign/02-系统设计/AI决策系统-事件系统-系统设计.md) | 客观事实库、事件实例、感知与传播、三公式、消亡 |
| 记忆模型 | [AI决策系统-记忆模型-系统设计.md](/VoxelDesign/02-系统设计/AI决策系统-记忆模型-系统设计.md) | 主观解读层：个人条目、解读规则集、快照、唤起 |
| 目标层 | [AI决策系统-目标层-系统设计.md](/VoxelDesign/02-系统设计/AI决策系统-目标层-系统设计.md) | 向往/目标的种子、强度、缺口地形、兑现 |

---

## 1. 系统上下文

### 1.1 系统边界

> 现行口径（5.25）：经营实体（BusinessEntity）是资产与经营业务的唯一所有者；Global `BusinessEntities` 为纯数据，按类型组合营业／销售／声誉客流／商圈租务／后续住宿等业务模块，商圈／店铺／图外生意共用基础模型。经营 ID 为 `BusinessEntityId`（逻辑名，具体引擎类型未定，不等同区域 ID），空间引用统一为 `AreaRef{SceneId, AreaId}`；`ShopId` 若出现，只作经营 ID 别名。区域只承载空间范围／关系／校验，可关联经营实体但不复制任何经营业务；子区域只有区域 ID 而非经营实体，抽象生意无区域。产权／经营人关系、营业 `bOpen`／时段、目录定价订单预留、声誉客流、活动、店主决策循环等逻辑一律归经营实体／业务模块，不挂区域 Actor；经营实体、双账户、唯一流水、雇佣与租约合同均为 Global 存档数据。
>
> 实施边界待定：连续建造创建的字段级组合、图外招聘换工等，本轮不定死；V1 范围不扩大。（`BusinessEntityId` 引擎类型见 §2.7；换租户沿用同一实体、删区域不注销实体已定）

**AI 决策系统负责**：
- NPC 个体画像（品类消费意愿 / 期望带 / 店铺认知）的定义与生成规则
- NPC 日程系统（周节律模板 / 日历覆盖层 / 需求微观填补）与出场判定
- NPC 动机（Motivation）的生成与决策：消费决策走 Utility 加法核评分，工作决策走员工优先级表 × Priority 分带两层扫描
- 硬过滤漏斗（营业状态 → 认知 → 价格上限）
- 需求衰减驱动决策触发（纯事件驱动，无轮询）
- 家具能力配置（FunctionFragment）与实例状态（FunctionModule）管理
- 区域工单索引与看板：作为 AI 发现“哪里有需求需要处理”的派生视图，由工单索引器唯一写入；支持工单认领、执行与完成，玩家只读列表
- 执行组件持有流程实例（FFlowState）并按 FFlowConfig 逐节点执行消费/工作流程，NPC 侧只存执行引用
- 角色行动与执行层（UE5 移动/导航/槽位/动画映射，见分卷）
- 评价体系（期望带模型）与经营实体的声誉客流统计（V1 为服务组总体声誉 + 四类客群构成，不启用个体记忆层）
- 经济循环对接（经营实体的商品目录 / 唯一经营账户 / 唯一流水 / 工资 / 小费 / 节点效果比例结算与原子支付边界）
- 经营实体的营业循环（开店/打烊/日结）、业态模板、库存与保质期、客房预订、店铺装修（见各分卷）
- 员工与工作执行、店主与商圈管理（见各分卷）
- 玩家侧经营操作清单与权限（人员/货/资产/活动/许可/统计/个人资产划转，见[经营操作卷](/VoxelDesign/02-系统设计/AI决策系统-经营操作-系统设计.md)）
- 工作点队列系统（工作点持有 FIFO 逻辑队列，队列线／等待区站位与顺序解耦、队列引导工单）
- 员工体系（雇佣记录与劳动安排分离、工作优先级表、熟练度、员工状态需求、待机与偷懒、权重式时段与交接、工单认领）
- 店主决策系统（经营记录、性格驱动的低频经营决策循环，逻辑归经营实体／业务模块）
- 招聘系统（人力市场 / 店主引入 / 熟人邀请）
- 经营实体的店铺活动与知名度（FShopCampaign 标准活动模型、广告触达）
- 供应链（三通道货源、采购单、缺货后果链）
- 行为外化（订阅制演出指令）与玩家侧 NPC 档案
- SpawnArea 流量分布与出场映射
- 决策 LOD（图内两档分级）
- NPC 记忆（主观解读层：个人条目、解读规则集、快照唤起，见记忆模型文档）
- 战斗模式的配置切换（同框架不同配置）

**AI 决策系统不负责**：
- NPC 身体组装与视觉重建（由 VoxelNpcBaseInstance 负责）
- 对象池生命周期管理（由 VoxelCreatureWorldSubsystem 负责，本系统只提供出场/回收判据）
- 生物数据生成（由 CreatureGeneratorUtils 负责）
- 导航寻路底层（由 NavigationSystem 负责）
- 区域数据管理（由 VoxelAreaWorldSubsystem 负责）
- 配置数据加载（由 ConfigDataSubsystem 负责）
- 时间/历法底层与周期结算调度（分别由时间子系统和经济域统一结算调度器负责，本系统只读游戏时间并消费时间边界事件）
- 经营操作的界面呈现（本系统只提供 `FMoneyAccount` / `FTransactionRecord` / `FShopReputation` / `FEmploymentRecord` 数据源，操作清单归[经营操作](/VoxelDesign/02-系统设计/AI决策系统-经营操作-系统设计.md)）
- 任务系统的可接取任务实现（突发状况只留"可转任务"的事件结构口子）
- 天气系统本体（只预留 WeatherMod 修饰器接口）
- 情报玩法机制（万事屋/侦探所等店铺核心玩法，只留玩家侧 NPC 档案记录）
- 行为外化的动画演出（外化层只发演出指令 FExternalizeEvent，演出归动画系统）

### 1.2 外部依赖

**依赖的系统**：
- **UVoxelCreatureWorldSubsystem**：NPC 实例管理、生成与回收、对象池衔接 LiveTime
- **UVoxelAreaWorldSubsystem**：区域注册、增删改查与存档编排；区域实例只存空间范围／关系／校验数据，可关联 `BusinessEntityId` 但不复制经营业务；营业状态（`bOpen` / `bOperating`）等经营业务数据归 Global 经营实体，子系统不保存业务副本
- **UConfigDataSubsystem**：事件资产、区域类型、评分配置、职业基准配置表、特质修正表、AspectTag 维度表、TimeMod 时间窗、熟练度定义表、店铺活动配置表、供应商目录等全部新配置表（派系关系归关系层群体轨，见《关系系统》§3）
- **UCreatureGeneratorUtils**：需求规则合并、生成期 backstory 事件解析（CheckAllConditions、ResolveEventOutcome、ApplyEventOutcome——只保留生成期用途）、需求衰减 Tick（UpdateDynamicAttributes，含跨阈值事件发射）
- **UNavigationSystemV1**：寻路（ProximityAtten、IsPathReachable 预估、排队格动态高成本区）
- **UVoxelFunctionAreaComponent**：功能挂载点、派生看板挂载与工作点队列站位配置（空间侧）；账本／声誉／订单队列／店铺活动／经营记录／店主决策等经营业务数据归经营实体／业务模块，不挂区域 Actor；看板只能由工单索引器维护
- **时间子系统**：唯一游戏时钟，提供当前游戏秒、倍速／暂停状态、日历和日／月／整点边界事件；业务不读真实秒计时
- **经济域统一结算调度器**：持计划任务表并按各自下一计划游戏秒叫醒贷款、租约、雇佣等域管理器；时间管理器不直接调用业务域

**被依赖的系统**：
- **AVoxelIntelligentCreatureInstance**：持有决策组件
- **ACreatureAIController**：执行移动指令
- **NPCUI 系统**：显示 NPC 当前动机与执行状态
- **经营操作与统计 UI**：读取经营实体的唯一经营账户与唯一流水、V1 五组统计、Global 雇佣合同（[经营操作](/VoxelDesign/02-系统设计/AI决策系统-经营操作-系统设计.md)）
- **招聘界面 UI**：读取人力市场候选与雇佣记录
- **动画系统**：消费外化层演出指令（FExternalizeEvent）做演出

### 1.3 输入输出

**输入**：
- NPC 需求当前值（FCreatureRuntimeState::Dynamics）
- NPC 个性数值（FCreatureSaveData::PersistentBasicValues）与特质（FTrueCreatureData::TraitTags）
- NPC 个体画像（ActivityWillingness / Expectations / KnownShopIds）
- NPC 日程（周节律模板 + 日历覆盖层）与当前 RoleContext
- NPC 记忆（个人记忆条目，含快照，见记忆模型文档）与关系程度（关系系统文档）
- 场景内所有家具实例状态（功能模块：占用/库存/维护槽）与经营实体的经营数据（声誉客流 / 商品目录 / 订单队列 / 店铺活动）
- 所属经营实体的派生看板；劳动者只读自己所属实体的一块板（`WorkAreaRef` 只表示去哪干活），跨店支援 V1 不做
- 外部事件（日程时段切换、代办日程触发、被攻击、强制指令）
- 评分器配置（UUtilityEvaluatorConfig DataAsset）
- SpawnArea 流量数据（FSpawnArea）
- 全局天气状态（接口预留）

**输出**：
- 当前 FIntent 与执行组件持有的 FFlowState 引用及流程断点
- 需求值回填（Consume / Pay 节点结算）
- 工单认领状态变更（认领 / 释放）；看板只由工单索引器根据业务事实增量重建与维护
- 工作点队列状态变更（队列顺序 / 站位）
- FExperienceReport → V1 直接更新店铺五组统计（服务组总体声誉 + 四类客群构成）；完整版本再经事件解读管线写个体记忆
- FTransactionRecord 流水写入人物个人钱包 / 唯一经营账户与 Global 唯一流水注册表（记录 SourceBusinessEntityId / TargetBusinessEntityId / OccurredAreaRef 可选），一笔结算只存一份
- 性格塑变（解读输出指向九维的永久修改器，见记忆模型文档）
- 行为外化演出指令（FExternalizeEvent，外化层订阅制输出）
- 采购单（FPurchaseOrder）与店铺活动投放（FShopCampaign）
- 店铺经营决策结果（BusinessSchedule 改写 / 逐商品调价 / 招聘需求）

---

## 2. 核心概念


> 本分册仅保留 2.6 概念关系；2.1 / 2.4 见 [空间卷](/VoxelDesign/02-系统设计/AI决策系统-店铺空间与消费流程-系统设计.md)，2.2 / 2.3 / 2.5 见 [NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)。

### 2.6 概念关系

```
NPC AI 决策系统
├─ 数据层
│  ├─ 复用现有: Dynamics / CachedDemandRules / PersistentBasicValues / TraitTags
│  │            个人记忆条目（FLifeHistoryEntry 扩展，引用事实库）
│  ├─ FTrueCreatureData 扩展: ActivityWillingness / Expectations
│  │            / KnownShopIds / 收支统计(占位)
│  ├─ FCreatureSaveData 扩展: WorkExperience（工作经验）/ WorkAssignment（劳动安排）
│  ├─ 日程: 周节律模板 / FAppointment 代办日程日历覆盖层
│  ├─ 评价: FExpectationBand / FExperienceReport
│  ├─ 口碑: FShopReputation
│  └─ 经济: FShopProduct / FMoneyAccount / FTransactionRecord / FEconomyRuntimeData / FPurchaseOrder
│
├─ 环境层（道具/区域；区域只空间范围/关系/校验，可关联经营实体但不复制经营业务）
│  ├─ FunctionFragment（家具行上的能力片段配置，TInstancedStruct）
│  ├─ FunctionModule（家具实例上的功能模块状态；工作点队列状态归对应工作点）
│  ├─ FBusinessTaskBoard（经营实体派生看板，唯一写入口为工单索引器）/ FWorkClaim（工单认领）
│  │            / FIntent（唯一定义见《意图与流程统一模型》）
│  ├─ 工作点队列（FIFO 逻辑队列；QueueLine / WaitingAreaRef 站位配置）
│  ├─ FFlowConfig / FPlanStep（业务流程模板）
│  ├─ FBillOrder（店铺订单队列）
│  ├─ FShopCampaign（店铺活动）/ FShopOwnership（经营记录）
│  └─ FSpawnArea（地图切换点与流量）
│
├─ 决策层（NPC 内部）
│  ├─ 触发判定（事件驱动 + 日程时段切换 + 代办日程）
│  ├─ 硬过滤漏斗（营业状态 → 认知 → 价格上限）
│  ├─ 消费决策: Utility 加法核评分（含组团消费）
│  ├─ 工作决策: 优先级表 × Priority 分带两层扫描
│  ├─ V1 工作区待机（后续版本才接待机/偷懒分流）
│  ├─ 决策 LOD（图内两档分级）
│  └─ FIntent（决策结果）；行为外化 = 订阅制演出（§16.2 NPC卷）
│
├─ 经营层（经营实体）
│  ├─ Global BusinessEntities 纯数据（BusinessEntityId；按类型组合营业/销售/声誉客流/商圈租务/后续住宿模块；商圈/店铺/图外生意共用基础模型，抽象生意无区域，空间引用 AreaRef{SceneId, AreaId}）
│  ├─ FEmploymentRecord（雇佣记录：合同与工资关系）
│  ├─ FWorkAssignment（劳动安排：挂在 Creature，所有实际劳动者均有）
│  ├─ UShopkeeperDecisionComponent（店主经营决策循环）
│  └─ 供应链（采购单与库存）
│
├─ 执行层
│  ├─ FFlowState（由执行组件持有，可休眠持久化；NPC 侧只存引用）
│  └─ FPlanStep 逐节点推进（Goto/排队/结算等为节点内部原语）
│
└─ 事件与记忆层（客观账 + 主观解读）
   ├─ 事件系统：全局事实库 / 感知与传播 / 三公式（事件系统文档）
   ├─ 记忆系统：个人条目 / 解读规则集 / 快照唤起（记忆模型文档）
   ├─ 突发状况事件（"可转任务"结构口子，事件系统 §10）
   └─ 性格塑变（解读输出指向九维的永久修改器）
```

### 2.7 身份与引用

四类实体身份，分两种唯一性范围：

| 实体 | 唯一性范围 | 标识 | 说明 |
|---|---|---|---|
| 经营实体 | 全存档 | `FBusinessEntityId`（GUID） | 商圈跨图、图外生意无图，需跨场景稳定 |
| 生物 | 全存档（名册） | `FCreatureId`（GUID） | 雇佣、关系、记忆、认知均指向它；名册全局单例 |
| 物品实例 | 全存档 | `FItemInstanceId`（GUID） | 唯一道具只有一把，可能跨场景携带、交易、存放 |
| 区域 | 场景内 | `FAreaRef{SceneId, AreaId}` | 区域是某张图的几何，跨场景必须带场景引用 |

**ID 的分配域 ≠ 数据的存储域。** 物品实例的数据随所属场景存档，但 ID 由全局分配器产生，因此 Global 的流水、租约、认知可以安全指向它。

**两条规则（所有 GUID 身份通用）**：

1. **生成后终身不变**：只在各自的创建入口调 `FGuid::NewGuid()` 生成一次，之后任何业务都不得改写；删除实体不回收、不复用其 ID。
2. **复制实体必须重新生成**：复制 / 模板实例化 / 存档导入时，新实体一律取新 GUID，并重映射所有指向旧 ID 的引用（租约、雇佣、流水、NPC 认知集合、Flow 断点）。**复制存档文件不属于"复制实体"**——整份存档自洽，ID 原样保留。

补充约束：`ShopId` 等旧别名取消，全库只用 `BusinessEntityId`；场景内 `int32` 编号（`AreaId`、场景内物品槽索引等）不得冒充任一 GUID 身份。

---

## 3. 设计原则

### 3.1 架构原则

**四层概念严格分离**
- 功能（静态配置）/ 状态（运行时实例）/ 任务（区域级分配）/ 动机（NPC 决策结果）
- 每层有明确数据归属，不混淆

**硬过滤先于评分**
- 营业状态、认知、价格上限是绝对约束，不进评分公式
- 过滤不通过的候选直接出局，不参与打分

**消费/工作决策分流**
- RoleContext 提供 Duty / Free 基础分流：Duty 工作走优先级表 × Priority 分带，Free 走 Utility 加法核（Sims needs-based 结构，随机效用理论背书）
- 劳动安排的时段块只调整工作子树权重：Work 最高，Anything 也允许低权重工作，不是工作门禁；打烊不接客由工单来源关闭保证
- 不按身份建系统——同一 NPC 时段切换即角色切换

**环境驱动**
- 家具实例声明自己能做什么（FunctionFragment）
- 工单索引器订阅设施、库存、队列、建造等业务状态变化，按 `Area.EntityId` 增量维护经营实体的派生看板；各业务域不直写看板，玩家指派走强制意图、不直接写看板
- 消费流程的状态变化同样由索引器派生工单（点餐 → AI.Work.Serve，退房 → AI.Work.Clean，首位顾客进入收银队列 → 按收银台生成 AI.Work.Cashier，队列清空后移除）
- NPC 只感知不判断——"这个对象能做什么"由对象声明
- 看板主要服务 AI 需求发现；玩家只能查看列表，不能在看板创建、编辑或指派工单，只能手动执行统一劳动动作或通过经营操作改变世界状态

**决策与执行分离**
- 决策层只产出 FIntent（"做什么、去哪做"）
- 执行层按 FFlowConfig 解释执行（"怎么一步步完成"）
- 两层解耦，决策不关心流程细节，执行不参与选择

**事件驱动决策**
- 无周期轮询，无定时刷新
- 需求衰减跨阈值才触发决策
- 日程时段切换 / 代办日程 / Job 状态变化才触发决策
- 本店索引出现新工单或更高优先级工单时，工单索引器通过内部通知叫醒本店在岗且空闲的劳动者；该通知不登记为业务事件
- 无活可干的劳动者可长期睡眠，只在索引变化或冷却到点时醒来
- 无激活时段的 NPC 不在图，零开销

**认知与到访分工**
- V1 的 `KnownShopIds` 是 NPC 所有的 `TSet<BusinessEntityId>`（认知指生意／经营实体，不指区域；`ShopId` 只作经营 ID 别名），只由开局老店预置、简化路过发现和 `Shop.CheckoutCompleted` 成交写入；路过与导航另用空间引用 `AreaRef{SceneId, AreaId}`，不写认知集；不接广告触达、口碑记忆、NPC 传播或完整感知系统。
- 路过发现只做店铺发现范围与招牌配置的业务距离判定，不是首客保护；开业不全量写入，也不保证新店被发现或成交。
- 来不来仍由现有评分公式决定（好奇 = PersonalityMod 探索性维度、声誉 = QualityScore），距离衰减自然筛选；任何认知来源都没有“强制到访”特权。
- 完整版本的广告触达、图外流量池与传播写入后续再接入。

### 3.2 接口设计原则

**按需缩小候选范围，全动机评分**
- 触发需求只决定候选品类过滤（性能优化），不独占评分
- 评分是全动机的：每个候选按对全部需求的综合贡献打分（主需求高权重 + FunctionFragment 声明的次要回填低权重，见 §5.3（[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)），"顺便吃饭、顺便社交"型决策自然涌现
- 硬过滤漏斗先砍掉出局候选，再进评分
- 空间优先级逐层扩展（当前区域 → 附近 → 全局），找到 N 个即停

**流程配置化**
- 消费流程不写死在代码里：`FFlowConfig` 按 `FlowTypeTag` 索引，消费行为与工作类型共用同一流程表
- 节点类型集由代码各实现一次，编排完全交给配置
- 新店铺类型复用已有 `FlowTypeTag` 与流程；只有新增服务行为或工作类型时才新增流程配置

**同一把尺**
- 排队：选店时的 QueueFactor 预估与入队后的忍耐实判，共用 Expectations[Service] 带
- 价格：选店硬过滤的店铺平均溢价率与结账价格好感共用 `PremiumRate = (Price - Value) / Value`；`Expectations[Price]` 决定个人可接受边界，价格好感按 `-PremiumRate` 线性增减
- 估算等待统一入口：评分与入队后重估调同一函数

**异步双机制解耦主流程**
- 店铺侧订单队列（FBillOrder，RimWorld 式 Bill）：生产与交付异步
- NPC 侧代办日程（FAppointment）：到点/事件触发后注入候选池参与评分，非强制

### 3.3 数据管理原则

**全复用现有数据模型**
- 需求系统：直接用 Dynamics + CachedDemandRules + DemandRule 系列
- 性格系统：直接用 PersistentBasicValues + TraitTags
- 事件系统：全局事实库 + 三上报点，业务落定处调 ReportEvent（事件系统文档）
- 履历系统：FLifeHistoryEntry 扩展为个人记忆条目（加来源/角色位/确信度/封口/锚点/快照/传播计数）
- 属性管线：熟练度与员工 BUFF 直接接入现有四层属性管线（FCreatureRuntimeState），不新建数值系统

**记忆即"引用 + 快照"**
- 记忆条目不复制事件内容，存事件ID + 解读快照（记忆模型文档 §2）
- 机制配置（什么事件对我有什么效果）在解读规则集资产，不在事件资产配置
- 唤起按快照直接重放，不回头查规则

**需求与期望彻底分离**
- 需求 = 会衰减的驱动值（Dynamics / CachedDemandRules，不动）
- 期望 = 不衰减的评价基准线（Expectations，评价尺子）
- 需求驱动行为，期望评判体验——两套字段互不引用

**数据驱动配置**
- 评分曲线和权重在 UUtilityEvaluatorConfig DataAsset 配置
- 功能定义在道具/区域蓝图配置
- 事件和记忆结果在事件资产配置
- 职业基准 / 特质修正 / AspectTag 维度 / TimeMod 窗口 / 熟练度公式 / 店铺活动 / 偷懒概率等全部由 ConfigDataSubsystem 承载

**全 Tag 化**
- 不引入新枚举来分类业务（旧 EDemandEnum 迁移到 Creature.Demand.* Tag）
- 需求类型用 Creature.Demand.* Tag
- 动作类型用 Creature.Action.* Tag
- 工作类型用 AI.Work.* Tag
- 评价维度用独立的 AspectTag 体系（与需求 Tag 解耦，各自演化）
- 店铺活动/雇佣角色/外化演出用 Campaign.* / Employment.Role.* / Externalize.* Tag
- 只有纯运行时状态机（如 EJobState、ERoleContext、EDecisionLOD）和代码固定实现的节点类型集（EFlowNodeType）保留 enum

### 3.4 性能原则

**零无效开销**
- 无激活时段的 NPC 不在图（回收对象池），零计算
- 没需求的 NPC 不决策
- Execute 阶段不重评估（除 Critical）
- 缺货信息不缓存，店内执行阶段直接查设施可用状态（占用/故障/堵塞/库存综合判定；候选过滤不查库存，见 §5.1 认知边界（[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)））

**事件驱动非轮询**
- 需求突破阈值才触发，不是每帧检查
- 对象状态变化才由工单索引器更新派生看板，不是定时扫描，也不由各业务域直写
- 出场判定由日程时段切换事件驱动，不是轮询日程表

**固定槽位防爆炸**
- 看板任务支持去重 + 过期清理
- 候选数控制在 3-10 个

**长流程休眠**
- 跨时段/跨天的流程实例（如定制订单）休眠持久化，不占 Tick
- 事件唤醒后从断点恢复

**决策 LOD 分级**
- 事件驱动的基础上，图内按重要度对演出与执行分两档（决策与数据层不降档，见第 18 章（[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)））
- 玩家关注点（玩家店铺 / 熟人 / 被注视目标）始终全量计算

---

## 4. 系统约束

### 4.1 技术约束

**数据结构**
- 需求值：`TMap<FGameplayTag, float>`（Dynamics）
- 履历：`TArray<FLifeHistoryEntry>`（LifeHistory）
- 家具能力配置：`FunctionSet: TArray<TInstancedStruct<FunctionFragment>>`（挂在家具行上）
- 家具实例状态：`FunctionModules`（按 FunctionSet 初始化，挂实例，不挂区域组件）
- 意图：`FIntent`，唯一定义见[意图与流程统一模型](/VoxelDesign/02-系统设计/AI决策系统-意图与流程统一模型-系统设计.md) §2.3；本文不复制字段定义
- 流程状态：`FFlowState` 由执行组件持有，NPC 侧只存引用，不内嵌于 `FIntent`
- 流程：`FFlowConfig`（FlowTypeTag 索引，消费行为与工作类型同表）
- 任务看板：`FBusinessTaskBoard` 是**经营实体**的派生物化视图，每个经营实体一块（Shop / District 各一块）；区域只通过 `EntityId` 指向所属实体，子区域与仓库里的设施因此直接落到本店看板，不做祖先解析。看板不作为业务事实所有者。
- 工单认领：`FWorkClaim{WorkOrderId, CreatureID, StartGameSec}`，只用于去重与防抢单，不涉及钱、不是合同、不入档
- 消费意愿：`TMap<FGameplayTag, float>`（ActivityWillingness，[0,2]）
- 期望带：`TMap<FGameplayTag, FExpectationBand>`（Expectations，AspectTag 索引）
- 店铺认知：`TSet<BusinessEntityId>`（KnownShopIds 为 NPC 所有；V1 只记经营实体 ID／生意，重复写入幂等，不含强度 / 锚点 / 最后到访时间，不做遗忘或容量淘汰；路过／导航另用 `AreaRef` 空间引用；完整认知条目留后续扩展）
- 流程模板：`TArray<FPlanStep>`（由 `FlowTypeTag` 索引的 `FFlowConfig` 持有，消费与工作共用）
- 雇佣记录：`FEmploymentRecord`（Global 唯一合同登记）只承载合同与工资关系，雇主字段为 `EmployerBusinessEntityId`；同一 NPC 同一时间最多一条有效记录
- 劳动安排：`FWorkAssignment` 挂在 Creature 上，承载工作区域、时段模板、工作优先级与启用状态；图外工作（真实 Global 雇佣合同）可无实际 `WorkAreaRef`；员工、承租经营人、自营产权人和薪酬制店主 NPC 都有劳动安排
- 店铺活动：`TArray<FShopCampaign>`（完整版本，归经营实体／业务模块，不挂区域组件；V1 不生成活动实例）
- 采购单：`FPurchaseOrder`
- 地图切换点：`FSpawnArea`（区域配置）

**线程模型**
- 需求变化：游戏线程固定逻辑 tick，按游戏时间增量更新并做跨阈值检测；倍速只放大游戏时间增量，暂停时增量为 0
- 决策触发：游戏线程事件回调
- 寻路：NavigationSystem 异步

**事件系统集成**
- 业务系统在落定处调 ReportEvent 上报事件（三个上报点收口，事件系统 §4.1）
- 感知与传播由事件系统统一管线处理，不在业务系统各自实现
- 记忆解读由记忆系统统一管线处理（解读规则集 + 快照，记忆模型文档）
- 行为外化（FExternalizeEvent 演出指令）走订阅制演出层，不进事件/记忆系统

### 4.2 性能约束

**决策开销**
- 触发需求决定候选品类过滤，候选集不膨胀；全动机评分只是对同一候选集多算一个次要回填分量，开销不变
- 候选数控制在 3-10 个
- 评分计算为纯数值运算，无 I/O
- LOD 1 档位降低演出与执行精度（决策与数据层不降档，见第 18 章（[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)））

**执行开销**
- 节点内移动调用 AIController 异步寻路
- 执行类节点按时间子系统提供的游戏时间增量推进 Progress，禁止使用真实秒业务计时，无密集计算
- 结算节点一次性回填，无重复计算
- 休眠流程不占 Tick

**任务推送开销**
- 仅状态变化时推送，非每帧
- 看板去重避免重复任务，派生索引与认领释放避免残留运行时引用

### 4.3 设计约束

**Execute 阶段约束**
- 仅 Critical 新需求可打断
- 同一需求的其他候选绝不换
- 打断时仅对节点允许的效果按 PartialSettle 比例结算，商品支付与所有权仍按业务原子合同；流程类 Job 记录断点，可休眠恢复

**角色约束**
- 角色是时段属性：不为员工和顾客分别建决策系统
- RoleContext 提供 Duty / Free 基础分流；劳动安排的 Work / Anything 等块只调权重，Anything 不禁止工作
- 劳动者只读自己劳动安排 `WorkAreaRef` 对应的看板（图外工作可无实际 `WorkAreaRef`）；工作归属先读劳动安排，雇佣记录只提供合同与工资关系，不用 HomeAreaID
- 工作优先级维持 4 最高、0 禁止；简易模式启用项同优先级并按工种固有顺序，高级模式手动设置 0–4

**战斗模式约束**
- 不新建独立战斗 AI 系统
- 同一评分框架，加载 Combat 配置
- 通过 BehaviorContext 切换

**经济约束**
- 选店评分不查个人钱包；加入购物篮／锁价预留时统一执行双门禁：营业与货页可售、聚合现货足够、个人钱包覆盖整单锁价金额、16 槽背包整单可放置模拟。预留阶段可缩篮子，结账仍整单原子，任一步失败均不部分付款、扣货或入包
- 定价粒度 = 单个商品（FShopProduct.Price 逐商品定价），禁止"调价系数"概念
- 店铺／商圈等经营实体不持有按区域分的独立余额；每笔付款实时解析经营实体的资金责任人（经营人／产权人），从其唯一经营账户结算，不依赖区域／Actor；权限按当前经营关系与业务类型计算，不使用玩家主角色特判
- 周期结算由经济域统一结算调度器按计划任务表逐条触发；同刻先足额组后扣负组，扣负白名单仅贷款月供、租金、工资

**Tag 化约束**
- 决策层数据完全 Tag 化
- 不引入新枚举来分类业务概念
- 仅纯运行时状态机与代码固定实现的节点类型集用 enum

---

## 21. 完整逻辑链

### 21.1 主循环

```
时间子系统推进唯一游戏时钟，业务系统只读游戏秒
  │
  ├─ 固定逻辑 tick: 需求按游戏时间增量变化
  │         Dynamics 按配置变化率 × 游戏时间增量更新
  │         变化前后跨阈值检测 → 突破则发布统一目录事件（同步应用阈值规则 EffectModifiers）
  │
  ├─ 日／月／整点边界（事实事件）
  │         高倍速一帧跨多个边界时按游戏时间先后逐个补发
  │
  ├─ 周期结算（时间子系统 → 经济域结算调度器 → 域管理器）
  │         调度器持计划任务表，严格按贷款／租约／雇佣等各自 Next...Sec 逐条触发
  │         到点叫醒对应域管理器算钱、写流水并推进下一时点
  │         同一时刻先足额组后扣负组；扣负白名单仅贷款月供／租金／工资
  │         时间管理器不直接调用业务域，不做月界统一批处理
  │
  ├─ 日程时段切换（事件）
  │         无激活时段 → 回收对象池
  │         有激活 + 不在图 → 从住所绑定 SpawnArea 刷出通勤
  │         有激活 + 在图 → 切 RoleContext 重决策
  │
  ├─ 代办日程触发（到点 / 订单完成事件）
  │         → 注入候选池 / 唤醒休眠流程
  │
  └─ 环境事件（对象状态变化）
           家具清洁度超阈 → 工单索引器更新派生看板(AI.Work.Clean) + 更新维护模块
           货架库存低于阈值 → 工单索引器更新派生看板(AI.Work.Restock) + 设施置为不可用
           索引出现新工单或更高优先级工单 → 索引器内部叫醒本店在岗且空闲劳动者，不进业务事件目录
           bOpen（手动入档）／营业时段变化 → 纯派生 bOperating（= bOpen && 当前处于营业时段，不入档、无自动复检）并发布状态事实；打烊不再进新客:
               店内已锁价预留／已排进收银队列／正在结账 → 服务到完成拿货与离店
               店外门口未进店队列 → 清退并各记一次「白跑」负服务样本，不计入来访／成交客群
               清洁／补货／维修等内部工单继续，员工离开时机由自己的劳动安排决定
           店主经营决策到点/关键事件 → 调用定价、进货、招聘、排班、开业等抽象业务方法，不占排班、不生成工单
```

### 21.2 决策周期

```
决策触发时:

  ① 确定触发源（哪个需求 / 哪个事件 / 哪个时段切换）

  ② RoleContext 基础分流 + 劳动安排权重
       Duty  → 工作决策（转 ④）
       Free  → 消费者决策（转 ③）
       Sleep → 居家低耗态，不决策
       Work 块提高工作子树权重；Anything 块仍允许低权重工作，不作为门禁

  ③ 消费者决策
       硬过滤漏斗: 营业状态 → 认知(KnownShopIds) → 价格上限(Critical 时放宽)
       触发需求决定候选品类过滤（性能优化，不独占评分）
       收集候选（空间优先级 Layer 1/2/3）
       加法核评分: 需求/品质/意愿/次要回填四分量加权合成基础分（全动机）
         → 距离与排队压力全局衰减
         → 性格/角色/时段/天气/记忆/客群修饰器逐项修正
       过滤零分 → 最高分低于行动阈值 → 闲逛
       Top-3 加权随机

  ④ 工作决策
       Critical 个人需求 → V1 离店 / 回抽象住所处理，不转普通消费者路径，不生成别店消费
       读取 Creature 上的劳动安排；只扫自己 WorkAreaRef 对应看板
        简易模式：启用工种同优先级，按工种固有顺序；高级模式：0–4 手动，4 最高、0 禁止
       工种有工单 → 工种内任务侧 Priority 分带（紧急 → 常规 → 低优）
       同带 tie-break: 就近 + 先到先得
       无工单 → 在本工作区睡眠待机；索引变化或冷却到点时重新找活，不启用待机/偷懒行为池

  ⑤ 生成 FIntent → 派发时由执行组件创建并持有 FFlowState，NPC 侧只存引用
       关键节点结果 → 外化层订阅演出（§16.2 NPC卷）
```

### 21.3 执行周期

```
意图经槽位承载（主槽/并发槽/子动作，见 §2.5 执行槽位结构（[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)））。
执行组件持有的 FFlowState 按 FlowConfig 逐节点推进，主槽与 NPC 侧只保存执行引用:

  Browse   → 读商品目录 → 选品（按回填量与品类意愿）
  QueueAt  → 入队（工作点 FIFO 逻辑队列；队列线／等待区只决定站位）
           → 等待体验值跌破 Expectations[Service].Min → 离队+差评+写冷却
  ClaimSeat→ 锁定座位
  Order    → 下单 → 状态变化由工单索引器派生工单（Serve/Craft）/ 定制单入 Bill Queue
  Pay      → 逐项计价 → 原子划转买家个人钱包至经营实体资金责任人的唯一经营账户并写唯一流水（SourceBusinessEntityId／TargetBusinessEntityId／OccurredAreaRef 可选）
  Consume  → 回填需求（中断按 PartialSettle 比例结算）
  AcquireResource / Release → 拿货 / 释放

  节点失败 → 按 OnFail 策略（放弃/重试/跳节点）
  定制流程 → 休眠持久化（断点）→ 订单完成事件唤醒

  节点产出 FExperienceReport → 期望带评价（第 6 章，[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)）
    → V1 经 Shop.ExperienceReported 直接更新店铺五组统计：服务组总体声誉 + 四类客群构成
    → 完整版本再经解读管线写个体记忆（《记忆模型》§4）；V1 不启用该分支

  流程结束 → 触发下一轮决策
```

### 21.4 事件与记忆周期

**V1 只保留业务事实直接订阅**：体验事件写店铺五组统计，不进入全局长期事实库，不创建个人记忆，也不触发完整感知或传播。完成上报的原始体验样本不逐条入档；只保存五组统计聚合、当日集中评价保护所需的最小状态，以及仍随消费流程断点保存的在途未上报报告。

完整版本的事件产生、感知、传播、消亡与记忆解读、快照、唤起全流程，见 [事件系统](/VoxelDesign/02-系统设计/AI决策系统-事件系统-系统设计.md) §4-§8 与 [记忆模型](/VoxelDesign/02-系统设计/AI决策系统-记忆模型-系统设计.md) §4-§6。要点：

- 业务落定处上报事件 → 写入全局事实库 → 在场者建个人条目 → 解读管线发放效果并快照
- 传播（闲聊/公告板/广告）→ 接收者建听闻条目 → 同一解读管线
- 评分时：活跃修改器（含记忆快照重放）作为修饰量进入候选评分

---

## 22. 现有资产复用总表

| 现有资产 | 在系统中的角色 | 是否需修改 |
|---------|-------------|----------|
| Tag 体系（Creature.Demand.* 等） | 需求/动作分类的唯一标识 | 需补全子节点（AspectTag、Creature.Attribute.Skill.*、AI.Work.Tidy、Campaign.*、Employment.Role.*、Externalize.* 等） |
| `FCreatureRuntimeState::Dynamics` | 当前需求值存储（含新增精力/情绪/忠诚度） | 不改 |
| `FRuntimeDemandRule` / `FDemandThresholdRule` | 六条需求的带符号变化率、上下限、阈值方向与触发 Tag；行为回填共用跨阈入口 | 需扩展 `ChangeMode / ChangeRatePerGameSecond / FinalMin / FinalMax / Thresholds{ThresholdValue, Direction, TriggerTag}` |
| 属性四层管线（基础池/衍生面板/动态值）+ `FDerivedAttributeDef` / `FAttributeModifier` | 熟练度公式与特质修正的挂载管线 | 不改（新增 Skill 衍生命名空间配置） |
| `FTriggerCondition` | 评分条件 | 不改 |
| `FPersonalityRangeRule` / `FEventTriggerCondition` | 性格条件 | 不改 |
| `UCreatureEvent` / `UEventOutcomeBase` 体系 | 生成期 backstory 事件资产 | 只保留生成期用途；运行时事件走事件系统（运行时实例 + 文本模板，不再走事件资产） |
| `FLifeHistoryEntry` | 个人记忆条目载体 | 扩展字段：来源/角色位/确信度/封口/锚点/快照/传播计数（记忆模型 §2/§7） |
| `UCreatureGeneratorUtils` | 需求规则合并、事件解析 | 需接 Tick + 补跨阈值检测发射 + 扩展运行时事件触发 |
| `UVoxelFunctionAreaComponent` | 功能挂载／派生看板／工作点队列站位配置（空间侧） | 看板只由工单索引器维护；账本／声誉／订单队列／活动／经营记录／店主决策等经营业务状态归 Global 经营实体，不归区域实例、不挂区域 Actor（见 23.2） |
| `BusinessSchedule` | 提供营业时段；`bOpen` 是经营人手动意图并入档，`bOperating` = `bOpen` && 当前处于营业时段（纯派生、不入档、无自动复检） | 人工开业**不校验设施**（05 决策29）；业态必需设施改为运行期提醒项；临时停业只改 `bOpen` |
| 时间子系统 | 唯一游戏时钟：当前游戏秒、倍速／暂停、日历与日／月／整点边界事件 | 业务统一读游戏秒；不使用引擎全局时间缩放或引擎暂停 |
| `FTrueCreatureData` | NPC 个体数据 | 扩展 ActivityWillingness / Expectations / KnownShopIds（TSet<BusinessEntityId>）/ 个人钱包 / 唯一经营账户 / 收支统计占位 |
| `FCreatureSaveData` | NPC 持久化数据 | 扩展 WorkExperience（工作经验）与 WorkAssignment（劳动安排） |
| `HomeAreaID` | 住所；V1 普通居民直接保存抽象住所对应的 `SpawnArea.AreaId`，进图与回住所共用 | 语义固化，不改结构；不新增住所配置表 |
| 阵营关系表 DT_FactionRelations | 派系↔派系关系矩阵（派系实体属性，《关系系统》§3；ClienteleMod 不读它） | 新增配置表 |

### 已删除的死代码

- `EDietaryHabitEnum`（仅定义无引用）
- `EActionState`（仅定义无引用）
- `FlowBase.h` 整个文件（ECommerceState / ECompositeBehavior / ETransitionCondition / FCommerceFlowConfig / FCommerceStateContext / FConditionList / FActionCategoryConfig）
- ConsumComponent 全部注释代码段
- VoxelGameMode 中 GetCommerceState 注释代码

### 枚举迁移

- `EDemandEnum` / `ECreatureActionType` / `ECreatureRace/Profession/Class` — 被 UI 和物品系统引用，AI 系统全面改用 Tag；旧 EDemandEnum 随 UI/物品系统改造迁移到 Creature.Demand.* 后删除

---

## 23. 数据模型清单

### 23.1 完全不改的现有模型（7 个）
1. FPreferenceRuleConfig
2. FAttributeModifier
3. FTriggerCondition
4. FPersonalityRangeRule
5. FEventTriggerCondition
6. FEventChoice
7. FCreatureRuntimeState

> UCreatureEvent / UEventOutcomeBase 只保留生成期 backstory 用途；FLifeHistoryEntry 扩展为个人记忆条目（见 §22）。

### 23.2 扩展字段的现有模型（4 个）

**FRuntimeDemandRule / FDemandThresholdRule**
```
+ DemandTag / ChangeMode / ChangeRatePerGameSecond / FinalMin / FinalMax
+ Thresholds{ThresholdValue, Direction, TriggerTag}
// 六条 Creature.Demand.* 共用；时间推进与行为回填都从 ApplyDemandDelta 进入跨阈检测
```

**UVoxelFunctionAreaComponent**
```
新增字段（区域侧只保留空间范围／关系／校验与空间引用）:
+ FacilityIndex: TArray<TWeakObjectPtr<AUVoxelItemBaseInstance>> // 区域设施索引（只登记实例引用，不复制家具数据）
+ TaskBoard: FBusinessTaskBoard               // 经营实体派生看板；唯一写入口为工单索引器，不入档
+ WorkingNPCs: TArray<TWeakObjectPtr<AActor>> // 在岗 NPC 列表
+ 工作点队列与 QueueLine / WaitingAreaRef 配置 // 逻辑顺序与站位分离（空间侧）
+ BusinessEntityRef: <经营 ID 引用>           // 只关联经营实体，不复制经营业务；子区域只有区域 ID、只作空间关联

// 以下经营业务数据归 Global 经营实体／业务模块，不挂区域 Actor:
//   FlowConfig（业务流程模板）/ BillQueue（订单队列）/ Reputation（声誉客流）
//   / Campaigns（店铺活动）/ Ownership（店主经营记录）/ UShopkeeperDecisionComponent（店主决策循环）
```

**FTrueCreatureData**
```
新增字段:
+ ActivityWillingness: TMap<FGameplayTag, float>   // 品类消费意愿 [0,2]
+ Expectations: TMap<FGameplayTag, FExpectationBand> // 期望带（AspectTag 索引）
+ KnownShopIds: TSet<BusinessEntityId>            // V1 认知只记经营实体 ID（指生意；ShopId 仅作经营 ID 别名）；路过／导航另用 AreaRef；完整认知条目后续扩展
+ PersonalWalletAccountId: AccountRef              // 个人钱包引用；每人建立；引擎类型待定（不在本轮定 GUID/int32）；余额存 FEconomyRuntimeData
+ BusinessWalletAccountId: AccountRef              // 唯一经营账户引用；每人建立，无经营资产也有；引擎类型待定；余额存 FEconomyRuntimeData
+ 收支统计字段（占位，供职业流动/储蓄统计）
```

**FCreatureSaveData**
```
新增字段:
+ WorkExperience: TMap<FGameplayTag, float>        // 工作经验（AI.Work.* Tag 索引，对数收敛上限）
+ WorkAssignment: FWorkAssignment                 // 劳动安排；挂在 Creature，入档
```

### 23.3 新建数据模型

| 模型 | 类型 | 职责 |
|------|------|------|
| FunctionFragment | USTRUCT | 家具能力片段配置（TInstancedStruct，结构+效果一体） |
| FunctionModule | USTRUCT | 家具实例的功能模块状态（槽位占用/库存/清洁/完善/锁） |
| FIntent | USTRUCT | 统一目的意图；唯一定义见《意图与流程统一模型》§2.3，本文不复制字段定义 |
| FTargetSpec / ELogicOp | USTRUCT / UENUM | 目标条件：GoalTag + TargetRef + And/Or/Not |
| FBusinessTaskBoard | USTRUCT | 经营实体派生看板：服务 AI 需求发现，唯一写入口为工单索引器；玩家只读，不作为业务事实所有者且不入档 |
| FWorkClaim | USTRUCT | 工单认领 `{WorkOrderId, CreatureID, StartGameSec}`：去重与防抢单，不涉及钱、不是合同、不入档 |
| FFlowState | USTRUCT | 流程实例状态，由执行组件持有；NPC 侧只存引用，消费与工作共用 |
| UUtilityEvaluatorConfig | UDataAsset | 评分器配置 |
| FExpectationBand | USTRUCT | 期望带 {Min, Max, Direction}（含漂移方向/速率/边界预留字段，二阶段启用，§6.8（[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)）） |
| FExperienceReport | USTRUCT | 体验报告（按 AspectTag + 归因字段，§6.2（[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)）） |
| FShopReputation | USTRUCT | V1 服务组声誉：服务速度 / 服务质量 / 商品丰富度 / 价格 / 环境氛围五项滚动聚合与有效样本数，合成总体有效声誉；种族 / 职业 / 阵营 / 街区客群构成由店铺统计保存。完整 6 维 × 5 群体矩阵与独立全局组留后续（§7.2，[经营卷](/VoxelDesign/02-系统设计/AI决策系统-店铺经营-系统设计.md)） |
| FFlowConfig / FPlanStep | UDataAsset / USTRUCT | 业务流程模板与节点 |
| FBillOrder | USTRUCT | 店铺订单（生产队列） |
| FAppointment | USTRUCT | 代办日程（义务/待办，日历覆盖层） |
| 周节律模板 | USTRUCT/DataAsset | 24h × N 天时段表 |
| 工作点队列配置 | USTRUCT | 需要排队的工作点持有 FIFO 逻辑队列；QueueLine 可编辑或自动生成，WaitingAreaRef 关联既有等待区域；站位与顺序解耦 |
| FShopProduct | USTRUCT | 商品目录条目 |
| FMoneyAccount / FEconomyRuntimeData | USTRUCT | 每人一个个人钱包 + 一个唯一经营账户（无经营资产也有），余额入 Global 存档；业务不按区域分账户 |
| FLoanOffer / FLoanDebt | USTRUCT | 随机经营贷款报价与等额本金借据快照 |
| FTransactionRecord | USTRUCT | Global 唯一流水：一笔结算只一条，记录 SourceBusinessEntityId／TargetBusinessEntityId／OccurredAreaRef（可选，空间引用）；个人账户视图与经营实体业务归属共享同一条流水，不按区域分账 |
| FCandidateCooldown | USTRUCT | 当日候选冷却（仅限排队主动放弃防徘徊，挂决策组件；失败候选不写冷却，§2.5（[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)）） |
| FEmploymentRecord | USTRUCT | Global 唯一雇佣合同登记（图外工资来自真实合同）：EmploymentId／CreatureID／EmployerBusinessEntityId／RoleTag／WageConfig／HireTime／HireSource／AvailabilityTemplate；工资只走此记录，付款时实时解析经营实体资金责任人 |
| FWorkAssignment | USTRUCT | 劳动安排＝干活的关系：CreatureID／WorkAreaRef（可空，图外工作可无实际工作区域）／ShiftTemplate／WorkPriorities／bEnabled；挂在 Creature，所有实际劳动者均有并入档 |
| 结算计划任务表 | 运行时数据 | 经济域统一结算调度器持有 `{DueGameTimeSec, 类型, 目标Id}`，严格按各自 Next...Sec 逐条叫醒域管理器 |
| FShopOwnership | USTRUCT | 店主经营记录（任命来源/就任时间） |
| UShopkeeperDecisionComponent | 业务模块组件 | 店主经营决策循环（归经营实体／业务模块，不挂区域 Actor，独立于店主个体） |
| FShopCampaign / FActivityEffect | USTRUCT | 标准店铺活动与效果条目 |
| FPurchaseOrder | USTRUCT | 采购单（三通道货源） |
| FExternalizeEvent | USTRUCT | 外化演出指令（订阅制，§16.2 NPC卷） |
| FShopProposal | USTRUCT | 店主向玩家的提案（关系度解锁的商量通道，§12.4（[经营卷](/VoxelDesign/02-系统设计/AI决策系统-店铺经营-系统设计.md)）） |
| ControlBinding | （预留） | 玩家角色与实体的控制绑定层，附身玩法口子（§11.9，[经营卷](/VoxelDesign/02-系统设计/AI决策系统-店铺经营-系统设计.md)） |
| FSpawnArea / FAreaLink | USTRUCT | 地图切换点与区域对接 |
| Buff 实例 | USTRUCT | {配置引用, 强度快照, 激活时间, 来源}（统一效应模型文档 §6，带时效入存档） |
| BusinessEntity（Global BusinessEntities） | USTRUCT / 纯数据 | 资产与经营业务唯一所有者；按类型组合营业／销售／声誉客流／商圈租务／后续住宿模块；商圈／店铺／图外生意共用基础模型，抽象生意无区域 |
| AreaRef{SceneId, AreaId} | USTRUCT | 空间引用；区域只存空间范围／关系／校验，可关联经营实体但不复制经营业务，子区域只有区域 ID |
| Lease 合同（Global 唯一登记） | USTRUCT / 运行时数据 | 租约唯一合同登记，经营实体只引用；有效 Lease 先于真实租户建造 |

> 实施边界待定：连续建造创建时各数据结构的字段级组合、图外招聘换工等，均不在本轮定死；V1 范围不扩大（仍只预置商圈玩家杂货标准承租，NPC 招商／模板／旅店留后续）。（`BusinessEntityId` 引擎类型见 §2.7；换租户沿用同一实体、删区域不注销实体已定）

### 23.4 新建枚举（纯运行时状态机 / 代码固定实现集）

```
ERoleContext:       Duty / Free / Sleep / Forced
EJobState:          Running / Completed / Failed / Interrupted / Dormant
EMotivationKind:    （已随 FMotivation 废弃：意图类别由 FIntent.Source + Targets[].GoalTag 表达，
                     见《意图与流程统一模型》§2.3 与《意图与流程统一-决策记录》迁移映射）
ETaskStatus:        （不另立：看板工单即 FIntent(Source=Work)，状态取值见《意图与流程统一模型》§2.3
                     的 FIntent.State；原 Candidate/Claimed/Executing/Completed/Invalid 已并入其中）
EFlowNodeType:      Browse / QueueAt / ClaimSeat / Order / Pay / Consume / UseItem / AcquireResource / Release
                    （节点类型集唯一定义见《店铺空间与消费流程》§8.1，由代码各实现一次，固定集合，不用 Tag；
                     节点内部另有五原语段 Goto / Acquire / Advance / Settle / Release，见同文档 §8.2；
                     Reserve 不是节点，是 Acquire 原语的用途之一）
EPayTiming:         预付 / 定金 / 尾款 / 日结 / 月结 / 离店结
EQueuePolicy:       站位排 / 叫号制（留扩展位）
EJobStage:          Goto / Reserve / Queue / Execute / Release / Settle / Done
                    （降级为节点内部原语状态机，不再是全局流水线）
EDecisionLOD:       Full / Simplified（图内演出与执行 LOD 两档，第 18 章，[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)）
EPurchaseState:     Pending / InTransit / Stocked（采购单状态）
```

### 23.5 新增 Tag 命名空间

```
Creature.Demand.*           (需求层，时间驱动：Hunger/Thirst/Energy/Leisure/Social/Shopping — 补全；
                             Spirit/Cloth/Luxury 已删除，Mood/Loyalty/Health 移入 State 层，见需求与状态层文档）
Creature.State.*            (状态层，事件/环境驱动：Mood/Health/Hygiene/Comfort/Loyalty)
Buff.*                      (BuffTag 效应单元：Buff.Urgency.*（Famished/Exhausted/Bored/Lonely/Wanting/NatureCall）
                             / Buff.Condition.*（Drunk/Filthy/Uncomfortable/Pain）
                             / Buff.Combat.*（恐惧/狂暴/燃烧/鼓舞）/ Buff.Social.*（面露难色/被羞辱）)
Creature.Action.*           (Consume/Rest/Work.*/Social/Wander)
Creature.Attribute.Skill.*  (工作熟练度，每个 AI.Work.* 工种一条衍生公式)
AspectTag                   (Service/Quality/Luxury/Environment/Price — 与需求 Tag 解耦)
AI.Work.*                   (Clean/Restock/Serve/Cashier/Craft/Guide/Tidy)
Campaign.*                  (Campaign.Ad.Flyer/Bard… / Campaign.Promo.Tasting/Opening… /
                             Campaign.Effect.Reach/ReputationBoost/ExperienceBuff/FootTraffic)
Employment.Role.*           (Staff/Shopkeeper)
Externalize.*               (Externalize.Emotion.* / Externalize.Behavior.* / Externalize.WorkState.*)
Event.*                     (完整后续树：Death/Crime/Transaction/Social/World 等；V1 仅启用事件系统 §11
                             统一登记的事件，不启用记忆解读、传播与长期社交事实库)
```

---

## 24. 关键场景走查


场景正文按主题分发至各分册，此处保留索引：

| 场景 | 所在分册 |
|------|---------|
| 24.1 V1 工作区需求与入店前 Critical | [经营卷](/VoxelDesign/02-系统设计/AI决策系统-店铺经营-系统设计.md) |
| 24.2 吃饭中途上厕所 | [NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md) |
| 24.3 缺货自动处理（含供应链） | [经营卷](/VoxelDesign/02-系统设计/AI决策系统-店铺经营-系统设计.md) |
| 24.4 排队太长离队换店 | [空间卷](/VoxelDesign/02-系统设计/AI决策系统-店铺空间与消费流程-系统设计.md) |
| 24.5 记忆影响评分与个体偏差 | [NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md) |
| 24.6 装备定制全流程（异步双机制） | [空间卷](/VoxelDesign/02-系统设计/AI决策系统-店铺空间与消费流程-系统设计.md) |
| 24.7 深巷店铺用活动引流 | [经营卷](/VoxelDesign/02-系统设计/AI决策系统-店铺经营-系统设计.md) |
| 24.8 店主经营决策循环 | [经营卷](/VoxelDesign/02-系统设计/AI决策系统-店铺经营-系统设计.md) |
| 24.9 组团消费 | [NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md) |

---

## 25. 设计决策总结

| 决策 | 选择 | 理由 |
|------|------|------|
| 消费决策机制 | Utility 加法核（可补偿）+ 衰减器 + 修饰器 | Sims needs-based 结构，随机效用理论背书；连乘被零值一棍子打死，加法核允许补偿 |
| 工作决策机制 | Creature 劳动安排中的 0-4 优先级表 × 任务侧 Priority 分带两层扫描，不走评分公式；4 最高、0 禁止 | 简易模式启用项同优先级并按工种固有顺序，高级模式手动设置；员工意愿与任务紧急度正交，"岗位"概念由优先级表吸收 |
| 工作优先级交互 | 员工与经营人共用劳动安排矩阵，支持简易／高级切换；高级格子点击循环 0→4 | 劳动安排只存于 Creature，雇佣记录不复制排班与优先级；UI 文案明确 4 最高、0 禁止 |
| 劳动与雇佣 | 雇佣记录只管合同与工资（图外工资来自真实 Global 雇佣合同，雇主为经营实体 `EmployerBusinessEntityId`），劳动安排挂在 Creature（图外工作可无实际 WorkAreaRef）；员工、承租经营人、自营产权人、薪酬制店主 NPC 都有劳动安排 | 招聘同时建立两条记录；`AvailabilityTemplate` 只约束 `RoleTag=Staff` 的雇员，经营人不受雇员合同的可工作时段约束；经营人下场劳动不因此产生额外工资，承租经营人拿利润，薪酬制店主按雇佣记录拿月薪 |
| 工作熟练度 | 接入现有属性管线（Creature.Attribute.Skill.* 命名空间），一级能力经特质修正、经验对数收敛有上限 | 与战斗数值同源（全员皆兵）；复用 FDerivedAttributeDef/AttributeModifier 零新机制；对数收敛避免数值失控 |
| 动态状态分层 | 按变动方式分层：需求层（时间驱动：Hunger/Thirst/Energy/Leisure/Social/Shopping）/ 状态层（事件与环境驱动：Mood/Health/Hygiene/Comfort/Loyalty）；效应不分层，统一为修改/Buff 两形态；全局一条触发规则——过阈或事件命中 → 发放 Buff 或即时修改 | 分层依据是变动规则不是业务主题；如厕/醉酒降级为 Buff（吃喝下游/酒精累积，DF syndrome 式），Spirit 删除；统一服务消费者/员工/公共服务 NPC/敌人四类 Actor，战斗 Buff 与日常效应同一管线（需求与状态层 + 统一效应模型文档） |
| 员工状态 | 精力（需求层）/ 情绪·忠诚度（状态层）走 Dynamics + BuffTag 层 EffectModifiers | 数值管线不变，语义归位；Mood 为事件脉冲+时效消退（非 DecayRate=0 永不消退）；V1 五状态只数值／展示，不接 WorkEfficiency，后续完整版本才接效率等行为效果 |
| Remedy 通道 | BuffTag/记忆锚点声明"解决途径"（RemedyTag）；候选收集三来源：触发需求品类 / Remedy 诉求 / 代办日程注入（含委托准备） | "悬而未决的问题 → 主动找能解决的服务"统一通道：买药/求医/洗浴/找侦探/买凶同一条；新店铺类型=声明能解决哪些 RemedyTag，零新机制；认知过滤照常（"门路"是信息资产）；买凶类低频高戏剧行为由仇恨锚点+报复心+认知+消费力交集涌现（需求与状态层 §4 / NPC卷 §5.3） |
| 待机/偷懒（后续版本） | 完整版本可用配置表概率分流（性格+状态+监督修正）+ AI.Work.Tidy 杂务工单；V1 仅工作区待机，不启用该行为池或 Tidy | 后续偷懒只影响进度；V1 不按 Mood / Loyalty 调整待机权重 |
| 店主决策 | 双层分离（个人层实时 / 经营层低频），性格驱动决策周期与倾向；只作用于 NPC 店主 | 店铺是持续经营体，店主休息不瘫痪；UShopkeeperDecisionComponent 归经营实体／业务模块，不挂区域 Actor（玩家直接经营店不挂） |
| 玩家直接经营店 | 没有 NPC 店主、没有分店、没有委托层：玩家作为店长随时直接决策（补货/改价/维护阵营关系/广告/活动）；V1 沙盒为承租经营 | [经营操作](/VoxelDesign/02-系统设计/AI决策系统-经营操作-系统设计.md) + 第 14/15 章接口即玩家的经营决策面；本章店主决策系统只管其他商圈的 NPC 店主 |
| 经营决策输入 | 滞后指标（客流/销量/库存/声誉）+ 先行信号（等待段放弃/投诉/队列长度分布聚合为放弃率/投诉数/平均等待时长） | 店主对"顾客每天默默走掉"的慢性失血必须有感知渠道；具体指标集随功能模块设计确定 |
| 店铺空置 | 搬走清空模板内物体（所有店铺通用）；NPC 商业街 → 管理 NPC 重新招商、分配房间类型（区域不可动）；玩家商业街 → 玩家完整编辑权（房间重编；删区域不注销经营实体，只解除空间关联） | 玩家直接经营店不适用无主态；空置期间经营决策循环挂起 |
| 招聘 | V1 仅从人力市场招聘普通员工；店主引入、熟人邀请与 NPC 店主委托招聘均留后续版本 | V1 不生成 NPC 租户或 NPC 店长；完整版本再开放这些来源与关系门槛 |
| 消费/工作关系 | RoleContext 分流，不混合评分 | 角色是时段属性；两套算法各取所长 |
| 硬约束处理 | 硬过滤漏斗先于评分 | 营业状态/认知/价格上限是绝对约束，不该被分数补偿掉 |
| 决策触发 | 纯事件驱动 + 日程时段切换 | 没需求的 NPC 零开销；无激活时段不在图 |
| 饭点高峰 | 不硬编码"几点吃饭"，靠多因子错峰涌现：起床窗口按种族/职业/个体分散 + 饥饿衰减调参 + TimeMod 只作软引导 + 个体扰动（窗口取宽） | 高峰存在（饭点生意是经营内容），但到峰时刻被打散，避免同一时刻全员同一需求共振压垮服务系统 |
| 候选范围 | 硬过滤 + 按触发源 + 空间优先级缩小 | 高效，不全量评 |
| 四层分离 | 功能/状态/任务/动机分开 | 概念清晰，数据归属明确 |
| 任务归属 | 看板是经营实体派生物化视图，工单索引器唯一写入；工单认领用于去重与防抢单 | 看板主要让 AI 发现需求，玩家只读列表；看板与认领不入档，读档后重建索引并重新找活 |
| 消费流程 | FFlowConfig 配置化 + FFlowState 流程实例 | 新店铺类型零代码；流程可休眠持久化支撑定制等长周期业务 |
| 代办日程语义 | FAppointment = 义务/待办（非社交约会），逾期称"逾期未办" | 避免"约会/爽约"歧义；组团消费另行设计（§8.7，[空间卷](/VoxelDesign/02-系统设计/AI决策系统-店铺空间与消费流程-系统设计.md)） |
| 组团消费 | 发起者选店 + 同伴否决权；团是行为概念（非实体）：仅结伴出场（§2.3，[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)）与场内面对面邀请两个来源（无远程邀请）、共进退按 §11.9（[经营卷](/VoxelDesign/02-系统设计/AI决策系统-店铺经营-系统设计.md)）分层（Urgent 以下统一由发起人容忍度控制 / 可压制 Critical 只发催促外化 / 硬极限与安全事件无条件个体脱离，发起人离场团解散）、排队占座按人头、整团同桌、无合适桌整团等位不拼桌 | 发起者是"组局的人"，最自然；分层共进退消除"成员需求突变"与"不做逐成员判定"的矛盾；社交需求合法满足 + 口碑载体 |
| 评价体系 | 期望带模型（SERVQUAL 期望差） | 需求驱动行为、期望评判体验彻底分离；权重=底线免配权重表 |
| 排队忍耐 | 并入期望带（Service 维度） | 预估与实判同一把尺；废弃 QueueTimeoutSec 常量 |
| 排队去留 | 排队中自身需求跨 Critical：比较「预计剩余等待」与「该需求容忍窗口」——等得到继续排、等不到弃队走紧急决策 | 同一把尺（估算等待统一入口 × 阈值规则可压制时长）；需求在容忍窗内可压制 |
| 口碑 | V1 五组简化统计 + 服务组贝叶斯收缩；服务组为总体声誉，四类群体组只记客群构成；完整版本的个体记忆与维度 × 群体矩阵后续启用 | V1 的 QualityScore 与统计页同源，新店不被少量差评打到极端；不提前实现个人记忆、传播和完整矩阵 |
| 单日评价保护 | 同一店铺单日负面样本按序递减权重（第一条全量，后续逐条衰减） | 防单日集中差评一次性击穿声誉；保护只作用于单日内，跨日样本照常累计，慢性差评如实反映 |
| 经济 | 每个人物一个个人钱包 + 一个唯一经营账户（无经营资产也有），业务不按区域分账户；流水一条，按 SourceBusinessEntityId／TargetBusinessEntityId 记两侧经营实体业务归属，OccurredAreaRef 可选只作空间引用 | 付款时实时解析经营实体资金责任人（自营／承租用经营人、薪酬制用产权人的唯一经营账户），不依赖区域／Actor；受薪经营人只能执行授权业务，不能划转产权人的个人钱包或经营账户；本人两类账户可双向互转且源账户须足额。选店不读钱包，加入购物篮／锁价预留统一执行现货与钱包双门禁及背包模拟，预留可缩篮子、结账整单原子；Price / Value 只接受有限非负数，0 元合法，禁止调价系数 |
| 经营贷款 | 抽象银行每次展示 5 个全随机报价；本金 20 万–800 万按 1 万取整、年利率 3%–15% 整数百分点、期限 12–360 整数月；每月只补空缺，最多持有 5 笔；放款进入主角唯一经营账户 | 等额本金：固定月还本金四舍五入、末期调尾差，利息按剩余本金逐月计算；月供与提前还款直接扣主角经营账户并允许为负，不记逾期；报价与借据锁定生成时条款，不提供手动刷新或读档重摇 |
| 租约租金 | 租约是 Global 唯一合同登记，经营实体只引用，有效 Lease 先于真实租户建造；首期按签约当月剩余日历天数比例计算并在签约时立即收取，后续按所属商圈配置的收租日逐期整月收取，调租从下一个收租日起生效；租金从承租经营实体资金责任人 → 商圈产权经营实体资金责任人，一次划转在唯一流水上记两侧业务归属 | 签约时推算各租约 `NextRentGameTimeSec`，由结算调度器逐条叫醒租约管理器；租金到期付款实时解析经营实体资金责任人并足额划转，源经营账户允许为负，不记逾期、欠租或坏账；标准月租默认为 0，需非零场景由开局／剧情配置提供 |
| 设施维护 | V1 移除公共设施与公共维护费；设施维护统一为员工维修工单 | 建造装修免费，无装修欠费与没收；结算任务不登记公共维护，扣负白名单只有贷款月供／租金／工资 |
| 周期与同时结算 | 时间子系统只提供游戏时间与边界事实；经济域统一结算调度器持计划任务表，严格按各自 `Next...Sec` 逐条叫醒域管理器；同刻先足额组后扣负组 | 不做月界统一批处理，时间管理器不直接调用业务域；高倍速逐时点补执行仍遵守顺序，扣负白名单只有贷款月供／租金／工资。例：经营账户 1000，同时采购 800、工资 500，采购先成功，工资再把余额扣到 -300 |
| 广告/知名度 | 完整版本由 FShopCampaign 触达写认知；V1 不启用广告认知写入，店铺只靠老店预置、简化路过发现与成交被认知 | 认知与到访分工不变：任何来源都无强制到访特权；V1 新店可自然被发现，但不保证首客 |
| 供应链 | 三通道货源（外部市场配送 / 街区 B2B / 探索素材）+ 供应商配送 | 简单起步不占员工；探索素材与战斗冒险设定联动；缺货后果链接店主紧急决策 |
| 行为外化 | 两层信息分离（身份按关系度门槛 / 行为永远可见）+ 订阅制（外化层订阅状态/行为/事件数据源，不造平行事件通道） | 脸盲限制"这是谁"不限制"在干什么"；素体也演，行为是观察玩法通道 |
| SpawnArea | 地图切换点（MMO 式），NPC 出场与玩家探索共用一套数据 | 居住区显式绑定查表；人口总量静态，模拟流量分布 |
| 世界结构 | 主地图 = 城市里的一个核心商业街区（内含多个竞争商圈）；城市其他区域是探索模式地图 | 全部竞争商圈都在主图内照常模拟，不存在"视野外经济"；探索与经营时间轴分离，外出探索期间主地图经营暂停，返回时结算外出耗时 |
| 决策 LOD | 图内两档（LOD 0 全量演出 / LOD 1 简化演出）+ 重要度优先定档 | 事件驱动模型下不存在"休眠档"：不在图 NPC 零开销；决策与数据层（感知/记忆/传播/账本）全档位同精度、永不降档，LOD 只管演出与执行（动画/外化/寻路）；玩家店铺始终全量 |
| 出场刷新 | 日程出场规则为唯一判据 | 事件驱动不轮询；无激活时段零计算 |
| 兜底行为 | 闲逛 → 回家链 | 闲逛是 Leisure 合法满足途径；废弃"空闲超时"字段 |
| 记忆系统 | 客观账与主观解读分离：事件系统管全局事实库与传播，记忆系统管个人条目与解读快照；条目扩展 FLifeHistoryEntry 加字段，唤起按快照重放不查规则 | 同一件事对不同人解读出不同效果（Sims2 极性/DF 创伤的落地形态）；快照保真——规则改动与性格漂移不影响旧记忆 |
| 战斗模式 | 同框架换 Config | 不建独立系统 |
| 突发任务/天气/情报 | 只留口子不实现 | 突发事件按"可转任务"结构设计；天气只留 WeatherMod 槽位；情报只留玩家侧 NPC 档案 |
| Tag 化 | 业务概念全 Tag | 不引入新 Enum 来分类；AspectTag 与需求 Tag 解耦 |
| 等待模型 | 服务等待段统一原语（预警档 + 按承诺度三分支放弃），QueueAt 是其排队实例 | 所有等待一个逻辑；预估与实判同一把尺；投诉→工单提带闭环 |
| 节点骨架 | 五原语段（Goto/Acquire/Advance/Settle/Release），Advance 自驱/服务驱双驱动，子进度序列承载多品项 | 断点粒度=节点+段+子进度 |
| 记忆计数 | 个体记忆只走 MemoryMod，有效声誉保持店铺层纯统计 | 消除双重计数；双层模型各司其职 |
| 店主自治 | 分域自治：经营四域店主自主，人事归玩家；关系度解锁商量 | 关系度即影响力；商圈人事权归玩家 |
| 关系系统 | 账本与余额分离：记忆只记账，关系程度（TMap<对方ID, FRelationshipEntry>）挂 NPC 侧存储；制度型/情感型两类行为，锚点=衰减下限；派系 {Rank, Attitude} | 程度不用翻账重算（每帧读得起），账不用为程度保鲜；脸盲解锁 = 关系网络在玩家节点上的投影 |
| 叙事传播 | 统一传播模型：发布者 → 渠道 → 接收者 → 建听闻条目 → 解读；闲聊=同步通道（话题=事件ID），公告板/通缉令/店铺广告=异步信息源 | 不做全广播模拟；传播、广告触达、认知写入同一条管线；故事导演层不实现留方向 |
| 店铺日志 | 值得注意的事件以日志条目挂店铺自身数据（配置驱动、滚动上限） | 评价是统计、日志是事件；话题素材 + 群体评价依据 + 情报玩法接口 |
| 玩家与 NPC 互动 | 行为级接口（每次互动 = 事件，情绪/关系/外化/话题统一管线） | 不是数值按钮；行为集配置化可扩展，未来玩法加行为不改框架 |
| 自主发展边界 | 三层边界：自主运转（已解决，事件驱动闭环）/ 自主发展（内生变异源已接通：经济循环（[经营卷](/VoxelDesign/02-系统设计/AI决策系统-店铺经营-系统设计.md)）+ 商圈生态（[商圈卷](/VoxelDesign/02-系统设计/AI决策系统-商圈生态-系统设计.md)）+ 关系叙事（[商圈卷](/VoxelDesign/02-系统设计/AI决策系统-商圈生态-系统设计.md)））/ 节奏导演（故事导演/天气/突发事件，只留口子） | 系统能产生"之前不存在的东西"（新店、新关系、新竞争格局）；事件节奏仍是自然触发，不做主动编排 |
| 玩家指令 / 自主 | 手动劳动与 NPC 自主共用统一业务入口；玩家不是去板上看领工单，看板只读。目标有工单则建立／接管认领，无工单则直接执行合法状态修改；驱赶 + 委托 / 外派走 FAppointment；玩家 `bAutonomyEnabled && !bManualOverrideActive` 时复用同一套 NPC 工作 / 生活路径，自动消费从玩家个人钱包付款；直接操控优先，结束接管后重新决策 | 玩家只能手动去做或通过经营操作改变世界状态；手动接管只在原子边界让出，原子段不可打断并反馈工作位占用。非受薪自主工作不另领工资，薪酬制经营人的月薪按雇佣记录另算 |
| 递归护栏 | 决策独立性原则（每次决策只看当前环境，失败候选不写冷却降权，仅排队主动放弃保留当日候选冷却）+ 决策异步队列 + 连续失败计数器 + 级联上限 3 次 | 环境变了下次决策自然过滤，无需"排除上次选择"机制；事件驱动数学上保证收敛 |
| LOD 切割 | 只降决策精度与演出，数据层永不降档 | 远离时世界照常运转，回来账本声誉对得上 |
| 打断与边界 | 容忍窗 + 原子步骤边界（需求跨 Critical/下班落在工单执行中：窗内做完当前原子步骤再离岗，超窗或触硬生理极限立即中断）；RoleContext 延迟生效；V1 不做加班，员工按自己的劳动安排离岗；工单时长远超预估强制失败；打烊店内锁价／收银排队／结账继续完成，店外门口未进店队列清退并各记一次白跑负服务样本（不计来访成交客群），内部工单继续，员工按劳动安排 | "做完再走"只到当前原子步骤不是整个工单；玩家指令也不能打断原子段，需等待当前段完成或工作位释放 |
| 打断归因 | 先归因再走后果：四类（自身/店铺/环境/社交）；节点允许的需求效果与体验可按 PartialSettle 进度结算，支付与所有权由各业务原子事务定义；V1 CheckoutSale 任一步失败均不部分付款、不部分扣货或入包 | 不同的中断带来不同后果：仅店铺原因产生店铺侧负面记录；社交中断产生双方社交记录 |
| 工单交接 | 回池原则（未完成一律回池，唯一例外发起方本人取消）+ 原子步骤 + 容忍窗；工单按工种分可暂停/不可暂停（bPausable） | 员工下班/被打断/死亡都不取消工单，只释放工单认领；可暂停工单进度留存换人续做，不可暂停中断即当前原子步骤清零回池；认领不入档 |
| 优先级序列 | 外部安全事件 > 硬生理极限 > 玩家指派 > 可压制 Critical 需求 > 当前 Job 状态 > Urgent 需求 > 新需求/常规工单 | 硬生理极限是双阈值：Critical 线（低于下限）仍可行动、触发紧急睡眠 Job、可被玩家指派压制（"面露难色"，后果玩家承担）；清空线（归零）= 断片昏厥，不可寻路不可压制，原地昏厥+定时苏醒；寻路故障只报错不转玩法 |
| 员工休息区 | **V1 不开放**；后续版本可配置店铺内部员工餐台 / 休息座椅 / 打盹点子功能区 | V1 员工离店解决个人需求；后续员工设施、成本价消耗与 Mood/Loyalty 经营影响不进入 V1（§11.10，[经营卷](/VoxelDesign/02-系统设计/AI决策系统-店铺经营-系统设计.md)） |
| 时段预算 | 双档：估算往返通勤 > 时段剩余 → 硬过滤出局（纯赶路属绝对约束）；"来得及但会迟到/熬夜" → TimeBudgetAtten 柔性衰减 | 估算全复用现有件（IsPathReachable/估算等待统一入口/FlowConfig 节点参数）；保留"为吃顿好的晚睡"的涌现，杀掉"在路上过完一个时段"（§5.1④/§5.3，[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)） |
| 住所分类 | V1 普通居民与员工都使用抽象 Spawn/离场点；NPC 实际回家并离场时一次补满 6 条需求与 Mood / Health / Hygiene / Comfort，Loyalty 不恢复，不做图外时间补算；旅店客房与店主/员工实体住所留后续版本 | V1 回家统一兜底，避免抽象睡眠不恢复精力；不再宣称普通居民的一切需求都只能由商业设施满足（§2.2，[NPC卷](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)） |
| 等待社交 | 服务驱等待段触发互动池（闲聊/拼桌/打招呼/抱怨传染） | 等待有内容；外化密度对准观察热点 |
| 认知写入 | V1 只写 `TSet<BusinessEntityId>`（认知指生意／经营实体，ShopId 仅作经营 ID 别名）：老店预置 + 简化路过发现 + 交易完成；路过／导航另用 `AreaRef` 空间引用；完整版本的图内感知、图外流量池与广告传播后续接入 | V1 只验证“知道店 → 可选店”的主链，不提前做强度、衰减、锚点或容量淘汰；新店可自然被发现但无首客保证 |
| 固定月薪 | V1 只有固定 `MonthlySalary`，工资只走 Global 唯一雇佣合同登记（`FEmploymentRecord.EmployerBusinessEntityId`）；图外工资来自真实合同，不依赖区域／Actor，失业则真没钱；结算调度器按各合同自己的下一工资结算游戏秒逐条叫醒雇佣域，首末月按在职时长折算 | 到期付款实时解析经营实体资金责任人，从其唯一经营账户足额转入员工个人钱包并允许源账户扣负，不做余额校验或员工竞争排序；不做班次 / 工时 / 计件工资、辞职梯度 / 谈判 / 预告期及未领薪链。消费、采购、个人付款与普通互转必须足额，扣负白名单只有贷款月供、租金、工资 |

> 实施边界待定：连续建造创建仍走同一建造面板（拉区域／摆模板 → 填信息 → 确认），后台组合创建经营实体＋区域＋必要合同，子区域只作空间关联；有效 Lease 先于真实租户建造。图外招聘换工等不在本轮定死；V1 范围不扩大，仍只预置商圈玩家杂货标准承租，NPC 招商／模板／旅店留后续。（`BusinessEntityId` 引擎类型见 §2.7；换租户沿用同一实体、删区域不注销实体已定）

---

## 26. 修订记录

> 5.25 为现行。以下仅保留版本索引；历史版本的设计细节已由现行正文覆盖，不再复制旧口径，避免与当前设计并存。完整历史由版本控制和 2026-11-04 工作日志保留。

| 版本 | 日期 | 说明 |
|------|------|------|
| 5.25 | 2026-11-04 | 现行设计：经营实体与区域分离，Global 经营资产与业务、双账户、唯一合同 / 流水、只 Global 存全局事实、连续建造创建、打烊分流与维护规则收口；实施边界待定项见 §25 末尾 |
| 5.24 | 2026-11-01 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 5.5 | 2026-10-19 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 5.4 | 2026-10-19 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 5.3 | 2026-10-18 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 5.2 | 2026-10-18 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 5.1 | 2026-10-17 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 5.0 | 2026-10-06 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 4.9 | 2026-09-27 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 4.8 | 2026-09-13 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 4.7 | 2026-09-13 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 4.6 | 2026-09-13 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 4.5 | 2026-09-13 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 4.4 | 2026-09-13 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 4.3 | 2026-09-10 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 4.2 | 2026-09-07 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 4.1 | 2026-09-06 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 4.0 | 2026-09-06 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 3.0 | 2026-09-06 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
| 2.0 | 2026-06-21 | 历史版本，已由 5.25 覆盖，不作为现行规范 |
