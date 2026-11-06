# 经营与 AI-架构设计

**范围**: V1 经营链（开店、雇佣、营业、顾客消费、结算、存档）的运行时架构
**依据**: `02-系统设计` V1 功能要求、`05-设计决策记录/AI决策系统-经营系统-V1复核-系统设计修订-决策记录.md`、`Source/Voxel` 现有代码
**修订日期**: 2026-11-04
**版本**: 2026-11-04
**规范地位**: 本文件是经营与 AI 的唯一架构规范；V1 模块卷只作导航。以下类型、接口、迁移表与实现样例都是待实施设计目标，不表示源码已经迁移。
**类型约定**: 全文代码块是架构伪代码。三个全局业务身份统一为 `FGuid`：`FBusinessEntityId`（经营实体）、`FCreatureId`（生物）、`FItemInstanceId`（物品实例），取值与生命周期规则见 [02 总纲 §2.7](/VoxelDesign/02-系统设计/AI决策系统-系统设计.md)；它们不等同于场景内 `int32 AreaId`。`FAreaRef{SceneId:int32, AreaId:int32}` 是空间引用；`optional` 表示可空逻辑值，不指定新的引擎类型或枚举。区域局部 ID 只在已明确的当前场景空间查询中使用。

---

## 1. 架构原则

1. **经营与空间分离**：经营实体是 Global 纯数据模型，按业务需要组合逻辑模块；区域 Actor 只承载空间/设施能力。家具和在图生物由 Actor + 组件执行现场行为，图外名册与经营合同不依赖 Actor。
2. **一个事实一个所有者**：钱归 Global Economy，雇佣归 Global Employments，租约归 Global Leases，经营关系与 Shop/时段状态归 Global BusinessEntities；劳动安排归名册或 PlayerProfile，时间归 Global Time。
3. **派生数据不入档**：看板、认领、`bOperating`、空间映射索引、站位缓存、结算计划表，读档重建。
4. **唯一写入口**：看板只由工单索引器写；`bOpen` 只由经营实体的营业模块写并入档；钱包只由经济业务方法与结算器写。
5. **事件只通知不调度**：GameInstance 薄事件目录同步分发、载荷只读，不增加请求/拒绝通道；结算调度器按各记录 `Next...Sec` 调度。
6. **执行层只有一套**：自研流程解释器；表现层用 AnimBP / Motion Matching，不引入 StateTree。
7. **UI 不直写数据**：UI → Controller/创建门面 → 经营逻辑模块或现场组件业务方法 → 广播 → UI 订阅刷新。

---

## 2. 分层

```text
L0 会话基础与 Global 领域数据（GameInstance，跨场景/跨 OpenLevel 存活）
     时间子系统 / 薄事件目录 / 经济子系统 / 经营实体注册管理 / 雇佣登记 / 租约登记 / 配置子系统
     BusinessEntities：经营关系 + 按业务组合的 Shop / 营业时段等逻辑模块纯数据
L1 场景世界（World，随场景加载/卸载）
     区域子系统 / 生物子系统 / 物品子系统 / 体素子系统
L2 场景空间与设施
     区域核心组件 / 设施索引组件
     家具设施组件 / 工作点组件 / 储物组件（不挂经营业务模块）
L3 场景个体
     名册 / 劳动组件 / 需求组件 / 日程组件 / 决策组件 / 执行组件 / 记忆组件 / 顾客组件
L4 领域服务
     工单索引器 / 认领表 / 空间映射索引（World 运行时）
     结算调度器 / 租约管理器 / 贷款管理器 / 工资管理器 / 统计服务（GameInstance）
L5 门面
     经营注册与组合创建门面 / 业务 Controller 组件 / 经营 UI
```

依赖规则：

- L0 的基础服务与 Global 业务数据不依赖 Actor、World 或已加载场景；经营模块通过稳定 ID 关联合同与账户。
- L2 只提供空间/设施公开接口，不持经营模块真值；L3 经空间看板与 Global 经营接口决策。
- L4 只通过公开接口访问事实；Global 合同结算不依赖 L2/L3，现场消费服务才访问当前 World。
- L5 调 L0 经营注册与 L4/L2/L3 的业务方法；组合创建门面协调实体、区域与合同一次提交。

---

## 3. 子系统

### 3.1 时间子系统

```cpp
class UVoxelTimeSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
public:
    int64 NowGameSec() const;
    FVoxelCalendar Calendar() const;
    void SetSpeed(float Speed);      // 1 / 2 / 3 / 4 / 8
    void SetPaused(bool bPaused);
    void PushTimeSlower(FName Reason);   // 强制正常速度：关键结算、读档激活、过场
    void PopTimeSlower(FName Reason);
    FDelegateHandle OnBoundary(EBoundaryType Type, FOnBoundary Handler);
};
```

- 唯一游戏时钟，宿主为 GameInstance（不依赖 World）；日历 6 天/周、5 周/月、12 月/年、360 天。
- **逻辑频率固定 30 tick/现实秒，倍速只放大每 tick 推进的游戏秒**（1x = 2 游戏秒/tick），CPU 开销不随倍速增长。
- 每 tick 推进 `GameTimeSec`，检测日/月/整点边界，按时间先后发布 `Time.DayBoundary / MonthBoundary / HourBoundary`。
- **补发受单帧预算约束**（对齐 RimWorld `TickManager`）：① 单帧 tick 次数上限；② 墙钟预算约 `1000/22` 毫秒；③ 超限即丢弃剩余积压并把 `GameTimeSec` 推进到目标时刻，允许跳过边界事件，不允许一帧爆量。
- `PushTimeSlower/RemoveTimeSlower` 用于关键结算段强制正常速度；倍速与暂停是子系统自身状态，不使用引擎全局时间膨胀或引擎暂停。
- 存档只存 `GameTimeSec` 与日历配置引用（Global）。

### 3.2 事件子系统

```cpp
// 业务事件的登记与分发层，不是通用消息总线。语义见 02《事件系统》§1/§3/§4。
class UVoxelEventSubsystem : public UGameInstanceSubsystem
{
public:
    void ReportEvent(const FEventRecord& Record);              // 唯一写入口：业务落定处上报
    FDelegateHandle Subscribe(FGameplayTag EventType, TFunction<void(const FEventRecord&)> Handler);
    void Unsubscribe(FGameplayTag EventType, FDelegateHandle Handle);
    const TArray<FEventRecord>& GetRecentEvents() const;       // 调试：最近 N 条环形缓冲
};

USTRUCT()
struct FEventRecord
{
    FGuid        EventId;
    FGameplayTag EventType;                 // Event.*，必须已登记于事件目录
    int64        GameTimeSec = 0;
    optional FAreaRef Location;             // 可空
    TArray<FVoxelEntityRef> Instigators;    // 主动者，可空
    TArray<FVoxelEntityRef> Recipients;     // 接收者，可空
    TMap<FGameplayTag, int64> ValueParams;  // 值参数：全局注册的枚举键 → 值
    TArray<FGuid> UpstreamEventIds;         // 因果链；V1 不填，字段先留
    TArray<FEventOutcome> Outcomes;         // 结果段，只读反查；V1 不填，字段先留
};
```

- **结构恒定**：主动者/接收者/值参数是全类型共用结构，不为单个事件另立 USTRUCT；类型语义由 `EventType` 承载。
- **目录校验**：`EventType` 与其必需值参数键在加载期登记；未登记类型、未注册键 → 报错（02 事件系统 §2「写错即报」）。
- 同步分发；**载荷只读**；没有首处理器或统一业务总控，订阅者各自在自己的事务内响应。
- 不排队、不跨帧、不做事件溯源；V1 不写全局事实库、不做感知/记忆/传播，`UpstreamEventIds` 与 `Outcomes` 仅为完整版预留。
- 订阅句柄随 GameInstance 销毁；World 内订阅者必须在 EndPlay 注销。
- 本子系统只承载**业务事实**，与前端通知通道（`GameplayMessageSubsystem`，见 05 决策2）是两层，互不替代：业务事实不为给 UI 看而改走前端频道，UI 提示也不得记为业务事件进入目录。逻辑层内部通知仍走组件委托，不进本系统（02 事件系统 §1）。

### 3.3 经济子系统

```cpp
class UVoxelEconomySubsystem : public UGameInstanceSubsystem
{
public:
    bool TakeLoanOffer(FGuid OfferId, int32 BorrowerId);
    bool RepayLoan(FGuid LoanId, int64 Amount);
    bool Transfer(FGuid From, FGuid To, int64 Amount, FGameplayTag Category,
                  FBusinessEntityId SourceBusinessEntityId,
                  FBusinessEntityId TargetBusinessEntityId,
                  optional FAreaRef OccurredAreaRef);
    void SettleDue(int64 Now);
    ResolveFundsOwner(FBusinessEntityId BusinessEntityId) const;
    // 逻辑返回值 {资金责任人 CreatureId, 该责任人的经营 AccountId}，不是新 UE 类型
};
```

- 持 `FEconomyRuntimeData`：钱包、流水、贷款报价、借据；数据入 Global。
- 结算调度器 `FSettlementScheduler` 与经济子系统同宿主；计划表派生，不入档。
- 扣负白名单只有：贷款月供、租金、工资。
- 租约事实在 Global `Leases`（运行时 `LeaseRegistry`），经营实体的 Shop 逻辑模块只持 `LeaseId` 引用；`LeaseManager` 由调度器叫醒，读 Global 租约、调经济转账、推进 `NextRentGameSec`。
- `ResolveFundsOwner(BusinessEntityId)` 在付款时依据有效经营关系/租约及产权责任链解析资金责任人和其经营账户，不从区域 Actor 取身份，也不固化雇主个人 ID；按依赖产权人的关系链处理产权变化，自营/薪酬责任人可变化，承租经营收入与工资责任不因此换人。
- 转账保留两侧实际账户及各自经营归属；个人消费等无经营一侧可空，`OccurredAreaRef` 仅是可空发生地点，不用它判断资金责任或业务归属。
- 贷款报价首次打开生成 5 槽，月界只补空槽，读档不重摇；借据按 `NextPaymentGameSec` 结算。
- 租约存 Global，场景未加载也能结算；只有店铺销售/顾客模拟需要场景加载。

### 3.4 劳动子系统

```cpp
class UVoxelLaborSubsystem : public UGameInstanceSubsystem
{
public:
    FGuid Hire(int32 CreatureId, FBusinessEntityId EmployerBusinessEntityId,
               FGameplayTag Role, const FWageConfig& Wage);
    void Fire(FGuid EmploymentId);
    const FEmploymentRecord* GetEmployment(int32 CreatureId) const;
};
```

- 雇佣登记：`TMap<FGuid, FEmploymentRecord>`，`CreatureID → EmploymentId` 索引派生；入 Global。
- 劳动安排：数据本体挂在生物组件，随场景名册存档；玩家安排入 Global。
- 认领表与工单索引器：World 级运行时服务 `FClaimRegistry` / `FWorkIndexer`，随场景重建，不入档。
- 工资管理器与雇佣登记同宿主（GameInstance）；按 `EmployerBusinessEntityId` 实时解析付款责任人/经营账户，不写 `EmployerCreatureId` 快照，不依赖员工或雇主 Actor。
- 图外工作保留有效 Employment，可没有 WorkArea；工资只依赖 Global 合同、经营实体、经济账户和时间，付款时实时解析经营责任人，不依赖员工/雇主 Actor 或场景加载，不伪造工资系统单边入账；无有效雇佣（失业）则无工资。图外雇主为抽象账户（`bAbstract=true`），资金口径见 §5.9「抽象账户规则」；图外招聘与换工规则尚未定（见 §10）。

### 3.5 区域子系统

```cpp
class UVoxelAreaWorldSubsystem : public UWorldSubsystem
{
public:
    AVoxelAreaBaseInstance* GetAreaActor(int32 AreaId) const;
    int32 GetAreaIndexByPosition(FIntVector BlockPos) const;
    void CollectSaveData(TArray<FAreaSaveRecord>& Out) const;
    void ApplySaveData(const TArray<FAreaSaveRecord>& In);
};
```

- 只做区域增删改查、范围/层级/空间校验、空间查询与场景存档编排。
- 区域由 `FAreaRef` 稳定引用；关联经营 ID 仅作引用，空间所属可通过空间映射索引查询，不持 Shop、营业时段、合同或业务身份副本。
- `Area.Type.Shop.*` 配置继续用于经营实体分类；区域 `AreaType` 是实体分类在空间上的投影/用途，不作为第二份业务真值。

### 3.6 生物子系统

```cpp
class UVoxelCreatureWorldSubsystem : public UWorldSubsystem
{
public:
    const TMap<int32, FRosterRecord>& GetRoster() const;
    AVoxelIntelligentCreatureInstance* SpawnFromRoster(int32 CreatureId);
    void RecycleToRoster(int32 CreatureId, ERecycleReason Reason);
    AVoxelNpcBaseInstance* GetCreatureById(int32 CreatureId) const;
    bool IsInMap(int32 CreatureId) const;
    FText ResolveCreatureName(int32 CreatureId) const;
};
```

- 名册 30–50 人，CreatureID 稳定。
- 生成/回收由名册与日程驱动；`CreatureNumLimit` 只作池容量与同屏上限。
- 图外名册数据入档。

### 3.7 物品子系统

```cpp
class UVoxelItemWorldSubsystem : public UWorldSubsystem
{
public:
    AUVoxelItemBaseInstance* SpawnItemAtGrid(const FVoxelItemSpawnParams& Params);
    AUVoxelItemBaseInstance* GetItemAtGrid(FIntVector Grid);
    void RegisterWorkPoint(AUVoxelItemBaseInstance* Item, UVoxelWorkPointComponent* WorkPoint);
};
```

- 物品实例唯一登记；家具实例承载设施/工作点/储物组件。

### 3.8 配置子系统

- 保留 `UConfigDataSubsystem`。
- 新增经营配置：营业时段模板、队列容量、扣负白名单、收租日、`FFlowConfig` 表、期望带默认值。

### 3.9 经营实体注册管理与组合创建门面

```cpp
class UVoxelBusinessEntitySubsystem : public UGameInstanceSubsystem
{
public:
    GetBusinessEntity(FBusinessEntityId BusinessEntityId) const;
    RegisterBusinessEntity(BusinessEntityData Data);
    GetShopModule(FBusinessEntityId BusinessEntityId);
    FindBusinessEntityByArea(FAreaRef AreaRef) const; // 查询派生空间映射

    // 营业状态唯一取值入口：bOperating = bOpen && 当前处于有效营业时段（纯派生，不入档，不缓存）
    bool IsOperating(FBusinessEntityId BusinessEntityId) const;
    // 派生状态实际变化时由此发布 Shop.StateChanged，调用方不得自行比较或广播
    void NotifyOperatingReevaluated(FBusinessEntityId BusinessEntityId);
};

// 逻辑门面：名称与 UObject 实现未定，不代表现有可编译类
BusinessCreationFacade
{
    ConfirmPlayerShopCreation(圈地范围, 已填写经营信息, 标准预授权条款);
    ConfirmExistingShopLease(现有 AreaRef, 模板预览, 已同意条款, 真实租户);
};
```

- 注册管理唯一持有 Global `BusinessEntities`：`BusinessEntityId`、`ParentBusinessEntityId`、沿用 `Area.Type.Shop.*` 的业务分类、经营关系和按业务组合的模块数据。不要求每个实体都有区域，也不要求每个实体都有 Shop/时段模块。
- **创建门面一次提交，按四阶段执行**。失败点全部前移到阶段 1，因此**不需要补偿层**（口径见 02 §「组合创建是一个不可分割的业务动作」）：
  1. **纯校验（零副作用）**：操作者权限 → 区域范围合法性 / 与既有区域冲突 → 区域类型的父子规则 → 业态必需设施 → 账户与档案存在性。
  2. **Scene 提交（可逆）**：创建区域 Actor 与空间索引。失败直接返回；此时 Global 尚未写入，天然无残留。
  3. **Global 提交（不设失败分支）**：经营实体 → 经营关系 → Lease → 劳动安排 → 首期流水，全部为内存写；出现失败即断言，属逻辑错误而非运行时分支。
  4. **派生与通知**：建 `EntityId → Areas` 与 `ParentAreaId → 子区域` 索引，发布 `Area.Created` / `Business.Created`。
- **回滚语义 = 不创建，不是撤销**：阶段 2 失败时没有任何 Global 写入需要回退；阶段 3 不存在失败路径。因此不引入两阶段提交或暂存重建机制。
- 两条路径共用同一契约，只差集合成员：**新建店铺空间**参与全部六件；**租现有空铺**不参与区域（区域已存在，只做冲突复核）。
- 实体与空间通过稳定引用关联；既有区域租用复用原区域，不重建。NPC 的模板预览/同意条款/确认只属于完整版，V1 无招商。
- 买卖区域/产权不破坏有效租赁：新 Owner 仅影响依产权人的责任链；承租方经营收入与工资付款责任保持原合同关系。**换租户沿用同一经营实体 ID**（实体是资产与业务的所有者，换的只是经营关系与租约），口碑随实体延续；每次新承租关系另生成新 `LeaseId`，两者不冲突。

---

## 4. 现场组件与经营逻辑模块

### 4.1 经营实体与区域

#### 4.1.0 三条原则

1. **经营事实全部在 Global 经营实体**：产权人、经营人、经营模式、业态、合同引用与各业务模块，一律不落区域。
2. **区域只持空间事实**：范围、类型、父子、准入与归属引用。
3. **空间层与经营层是两棵正交的树**，只通过区域上的一个 `EntityId` 交叉。不由空间包含推导产权或经营关系，也不由经营层级推导空间范围。

#### 4.1.1 宿主与分层

```text
L0 Global（UGameInstanceSubsystem，跨场景/跨 OpenLevel 存活）
  UVoxelBusinessEntitySubsystem
    └─ TMap<FBusinessEntityId, FBusinessEntity>        ← 唯一事实，入档
  UVoxelEconomySubsystem / UVoxelLaborSubsystem / 租约登记 / 结算调度器 / 统计服务

L1/L2 Scene（UWorldSubsystem / Actor，随场景销毁重建）
  UVoxelAreaWorldSubsystem                             ← 区域增删改查、空间查询、场景存档
  AVoxelAreaBaseInstance（每个区域一个 Actor）
     ├─ UVoxelAreaCoreComponent          范围 / 父子 / 类型
     ├─ UVoxelFacilityIndexComponent     设施派生索引
     └─ <按区域类型挂载的功能组件>        见 §4.1.5，由 FAreaTypeBase 配置驱动

World 派生索引（不入档；读档与切图后由事实重建）
  AreaId        → AVoxelAreaBaseInstance
  粗格 16³      → AreaId                    （按 Bounds 注册，用于点查询粗筛）
  BusinessEntityId → TArray<int32 AreaId>   （实体查自己的区域与子功能区）
  ParentAreaId  → TArray<int32 AreaId>      （子区域列表）
```

**一个事实一个所有者**：经营数据只在 L0，空间数据只在 L2，派生索引只存在于 World 运行时且永不入档。两条树的节点类型不同，互不镜像。

#### 4.1.2 经营实体

```cpp
// ── Global：经营实体（唯一事实，入档）
USTRUCT()
struct FBusinessEntity
{
    FBusinessEntityId EntityId;                    // FGuid；创建时生成，终身不变
    FGameplayTag      BusinessKind;                // Business.Kind.*（见 §4.1.5）
    FName             DisplayName;

    // 经营关系：三者都直接存在实体上，运行时不做任何向上解析
    EBusinessMode     Mode = EBusinessMode::SelfOperated;
    int32             OwnerCreatureId   = INDEX_NONE;   // 产权人
    int32             ManagerCreatureId = INDEX_NONE;   // 经营人

    // 组织归属：只表达"商圈下辖店铺"这类组织层级，不参与资金责任人解析
    optional FBusinessEntityId ParentEntityId;

    // 合同引用：租约/雇佣的事实分别在 Global LeaseRegistry 与 Employments，此处只存 ID
    optional FGuid    LeaseId;

    // 空间存在性：所在地图是否存在于游戏中（设定里才有、没有实际地图的地方为 true）
    // 生成时确定，之后不变；与"当前是否已加载""有没有区域"无关
    bool bAbstract = false;

    // 按业态组合的模块（与家具 FunctionFragment 同用 TInstancedStruct）
    TArray<TInstancedStruct<FBusinessModule>> Modules;
};

UENUM(BlueprintType)
enum class EBusinessMode : uint8 { SelfOperated, Leased, Salaried };
```

V1 启用的模块（其余只定义类型、不进 V1 实例）：

```cpp
USTRUCT() struct FTradeModule            // 店铺/商超
{
    TArray<FBusinessProduct>   Catalog;
    TArray<FStockReservation>  ActiveReservations;
    TArray<FCheckoutOrder>     PendingOrders;
};

USTRUCT() struct FBusinessHoursModule    // 有营业行为者
{
    FBusinessScheduleRuntime Schedule;
    bool bOpen = false;                  // 人工开关，入档
};

USTRUCT() struct FReputationModule       // 对外营业者
{
    FBusinessReputation    ServiceAspects;   // 服务组五维滚动聚合
    FBusinessClienteleStat Clientele;        // 种族/职业/阵营/街区四类桶
    FBusinessTrafficStat   Traffic;
};

// 后续版本：FLeaseModule（商圈租务）、FEmploymentModule（雇主侧）、
//           FSecurityModule（警卫所）、FLodgingModule（旅店）
```

规则：

- **业态决定模块组合**：`FBusinessKindDef.AllowedModules` 声明该业态可装的模块集合，实体创建时按业态实例化。商圈装 `LeaseModule`，杂货店装 `TradeModule`，不需要代码分支。
- **资金责任人 = 实体自身的 `Mode` + `ManagerCreatureId` / `OwnerCreatureId`**：自营与承租取经营人，薪酬制取产权人。创建与易主时把产权人写入 `OwnerCreatureId`（店铺的产权人即所属商圈产权人），**查询期不再沿 `ParentEntityId` 上溯**。
- **`bAbstract` 与「有没有区域」无关**：图外生意、未建成的铺面都可以没有区域，只要所在地图存在就不是抽象实体。读档/切图不修改该字段。
- **`ParentEntityId` 只用于组织查询**（商圈下辖店铺列表、UI 树、统计汇总），不参与产权、资金、权限判定。

#### 4.1.3 区域

```cpp
USTRUCT()
struct FAreaInstance : public FTableRowBase
{
    int32   AreaId         = INDEX_NONE;
    int32   ParentSceneId  = INDEX_NONE;
    int32   ParentAreaId   = -1;              // -1 = 场景根
    FName   AreaName;

    FAreaFunctionBFragment Function;          // { EAreaStatus Status; FGameplayTag AreaType; }
    FAreaCoverageFragment  AreaCoverage;      // Boxes（权威）+ Bounds（派生缓存）
    FAreaAuthorityBFragment Authority;        // 准入：bIsPublic / AllowClass / AllowProf / AllowRace
    FGameplayTagContainer  GameplayTags;      // 玩法/解谜等附加标记

    FBusinessEntityId EntityId;               // 归属经营实体；可空（空置标的、公共区域）
                                              // 子功能区填其父店铺实体的 ID，不新建实体
};
```

**已删除**：`OwnerCreatureId`、`ManagerCreatureId`。产权与经营权不再存在区域上，区域只保留**准入**（谁能进入这个空间），准入是空间能力，不是经营关系。

`AVoxelAreaBaseInstance` 只保留空间职责：范围维护、父子索引、设施索引、可视化、准入判定。区域不持账本、目录、订单、声誉、租约副本。

#### 4.1.4 三条关系

| 关系 | 载体 | 方向 | 基数 | 是否入档 |
|---|---|---|---|---|
| **空间树** | `FAreaInstance.ParentAreaId` | 场景根 → 商圈范围 → 店铺空间 → 子功能区 | 一父多子，森林 | 是 |
| **经营组织** | `FBusinessEntity.ParentEntityId` | 商圈实体 ← 店铺实体 | 一父多子，森林 | 是 |
| **归属引用** | `FAreaInstance.EntityId` | 区域 → 经营实体 | 多对一，可空 | 是 |
| 模块组合 | `FBusinessEntity.Modules` | 实体 → 模块 | 一对多，模块无独立 ID | 是 |

- 前两条是**正交**的树：空间包含不推导经营归属，经营层级不推导空间范围。
- 子功能区在空间树里有父节点，在经营组织树里**没有节点**（它不是经营主体），但它有 `EntityId`（指向父店铺实体）。
- 派生索引 `BusinessEntityId → Areas` 由区域表重建，因此「实体查自己的区域和子功能区」是一次查表，不需要沿父链推。

#### 4.1.5 类型体系

两个维度，互不包含：

```text
Area.Type（空间作用）
├─ Default            未划分类        无实体、无父
├─ Spawn              出生点          无实体、无父
├─ BusinessCluster    商圈范围        必须有实体、无父、bAbstract=true
├─ Shop               店铺空间        必须有实体、父=BusinessCluster、bAbstract=true
└─ SubFunction        店铺内隔间      必须有实体(=父店铺实体)、父=Shop、bAbstract=false
    ├─ Storage        仓库
    ├─ Kitchen        后厨
    ├─ WaitingArea    等待区
    ├─ StaffRest      员工休息区
    ├─ GuestRoom      客房（V1 不启用）
    └─ Ordinary       普通空间

Business.Kind（业态）
├─ District           商圈
├─ Grocery            杂货店（V1）
├─ Inn                酒馆 / 旅店
├─ Restaurant         餐厅
├─ GuardPost          派出所
└─ Hospital           医院
```

- **子功能区的用途由创建时明文选择的区域类型确定**，不从内容物推导（推翻旧口径「房间用途由内容物推导」）。选择时机在框选之前：先选类型，再拉范围。
- **业态是经营实体的属性**，不是区域类型。`Area.Type.Shop` 只表示"这是一块店铺空间"；开杂货店还是酒馆由实体的 `BusinessKind` 决定。既有 `Area.Type.Shop.inn` 属业态混入类型，迁移为 `Area.Type.Shop` + `Business.Kind.Inn`。
- **配置驱动，不硬编码**：`FAreaTypeBase`（DataTable 行）承载全部类型规则，代码不写 `if (Type == Shop)`。

```cpp
// FAreaTypeBase 增补（其余字段沿用现状）
USTRUCT()
struct FAreaTypeBase : public FTableRowBase
{
    // …既有：AreaTypeId / AreaTypeTag / DisplayName / Description / AssetFragment /
    //          RequiredFurnitureType / ForbiddenFurnitureType / BuildLimit …

    TArray<TSubclassOf<UActorComponent>> FunctionComponentClasses; // 按类型挂载的功能组件
    bool bRequiresEntity = true;          // 该类型区域是否必须挂经营实体
    bool bRequiresOwnerManager = true;    // 创建时是否必须解析出 Owner/Manager
    TArray<FGameplayTag> RequiredParentTypes; // 既有字段，正式接管层级校验
    TArray<FGameplayTag> AllowedParentTypes;  // 既有字段，正式接管层级校验

    // bAbstract 语义修正：重叠许可标志（与类型层级无关），沿用既有实现
    // true 时只与自身同类型检查重叠，可与不同层级类型重叠
};

// 业态配置：DataAsset 或 DataTable 行
USTRUCT()
struct FBusinessKindDef
{
    FGameplayTag BusinessKind;
    TArray<TSubclassOf<FBusinessModule>> AllowedModules;    // 本业态可装哪些模块
    TArray<FGameplayTag>  RequiredAreaTypes;                // 开业必需的区域类型（如 SalesFloor）
    TArray<FGameplayTag>  AllowedSubAreaTypes;              // 允许挂哪些子功能区类型
    TArray<FGameplayTag>  RequiredFacilities;               // 运行期提醒项（不是开业门槛）
    FGameplayTag          FlowType;                         // 消费/工作流程索引
    bool bCanHaveSubEntities = false;                       // 是否可下辖子经营实体（商圈=true）
};
```

`FAreaTypeBase.IsShopType()` / `IsSubFunctionType()` 这类把具体类型写死的辅助函数**删除**：它们把 `Shop` / `SubFunction` 嵌进了代码，任何新业态都调不了；且 `IsSubFunctionType()` 查询的是 `Area.Type.Shop.SubFunction`，与实际 tag `Area.Type.SubFunction` 不匹配，恒为 false，属现存缺陷。

#### 4.1.6 权限

- **编辑权限**（唯一布尔）改为经实体归属判定，不再读区域字段：

```cpp
bool AVoxelAreaBaseInstance::IsAreaEditable(int32 CreatureId) const
{
    // 本区域无实体 → 沿 ParentAreaId 上溯至最近一个持有实体的祖先（空置标的归商圈）
    const FBusinessEntity* E = ResolveOwningEntity();
    return E && (E->OwnerCreatureId == CreatureId || E->ManagerCreatureId == CreatureId);
}
```

- 空置标的（`EntityId` 为空）是**唯一需要上溯的场景**：由父链最近的持有实体者判定可编辑。产权与资金责任人**永不**上溯。
- **准入权限**仍由 `FAreaAuthorityBFragment` 表达（`bIsPublic` / 职业 / 种族 / 阶级），与编辑权限分离：准入管"能不能进"，编辑管"能不能改"。
- 区域权属转让、经营关系变更走经营业务，不走区域权限接口。

#### 4.1.7 创建流程

经营实体与区域**同一次提交**完成，UI 只调业务门面：

```text
① 选类型（区内类型库，RootTag = Area.Type）：
     商圈 → BusinessCluster   店铺 → Shop   子功能区 → SubFunction.<用途>
② 框选范围（编辑器区域工具体，草稿态，不落世界）
③ 填写经营信息（仅需实体的类型）：名称、业态、经营意图、父实体
④ Confirm → 业务门面一次提交
```

```cpp
struct FCreateBusinessWithAreaRequest
{
    int32  OperatorId;
    FGameplayTag AreaType;                  // 第 ① 步所选
    FGameplayTag BusinessKind;              // 需要实体时必填；子功能区为空
    FName  DisplayName;
    FGameplayTag BusinessIntent;
    optional FBusinessEntityId ParentEntityId;
    FAreaDraft SpaceDraft;                  // 第 ② 步的范围草稿
};

// 需要实体（BusinessCluster / Shop）
FBusinessEntityId BusinessCreationFacade::ConfirmBusinessWithArea(const FCreateBusinessWithAreaRequest&);
// 只需空间（SubFunction）——EntityId 继承父区域
int32             BusinessCreationFacade::ConfirmAreaOnly(const FCreateBusinessWithAreaRequest&);
```

提交顺序（失败逆序撤销，SSOT 见任务 B1）：

```text
1. 校验操作者权限（父区域可编辑 / 商圈经营人）
2. 校验类型规则：RequiredParentTypes、AllowedParentTypes、bRequiresEntity、BuildLimit
3. 提交区域空间（范围/父子/粗格索引）——Scene 侧，最先，因后续全部依赖 AreaRef
4. 创建 FBusinessEntity（Owner/Manager/Mode/BusinessKind/Modules/LeaseId/ParentEntityId）
5. 回写区域 EntityId
6. 建派生索引（EntityId → Areas、ParentAreaId → 子区域）
7. 发布 Area.Created / Business.Created
```

子功能区不建实体：`EntityId = 父区域.EntityId`，业态随父店铺。

#### 4.1.8 查询接口

```cpp
class UVoxelAreaWorldSubsystem : public UWorldSubsystem
{
    // 既有
    AVoxelAreaBaseInstance* GetAreaActor(int32 AreaId) const;
    int32 GetAreaIndexByPosition(FIntVector BlockPos) const;   // 返回最小包含区域
    TArray<AVoxelAreaBaseInstance*> FindAreasByType(FGameplayTag TypeTag) const;

    // 新增
    TArray<int32> GetSubAreas(int32 AreaId) const;                       // 直接子级
    TArray<int32> GetAreasByEntity(FBusinessEntityId EntityId) const;    // 含子功能区
    FBusinessEntityId GetEntityOfArea(int32 AreaId) const;               // 读区域 EntityId
};

class UVoxelBusinessEntitySubsystem : public UGameInstanceSubsystem
{
    const FBusinessEntity* GetEntity(FBusinessEntityId) const;
    optional FBusinessEntityId FindEntityByArea(FAreaRef) const;         // 经 World 派生索引
    TArray<FBusinessEntityId> GetSubEntities(FBusinessEntityId) const;   // 经 ParentEntityId
    FTradeModule*       GetTradeModule(FBusinessEntityId);
    FBusinessHoursModule* GetHoursModule(FBusinessEntityId);
    // 业务方法一律以 FBusinessEntityId 为第一参数，不提供 Shop 专用门面
};
```

`GetAreasByEntity` 让实体直接拿到自己的区域与子功能区（一次查表，不沿父链）；`FindEntityByArea` 供 UI 选中空间后反查经营信息。

#### 4.1.9 重叠与冲突

- **同类型区域互斥**：两个 `Area.Type.SubFunction.Storage` 不能重叠；两个商圈、两个店铺同理。
- **跨类型可重叠**：任一方类型 `bAbstract = true` 时跳过冲突检查（现有实现 `VoxelAreaWorldSubsystem::…`：`CompareTagAtLevel(…, 3)` 判定同类型）。因此 `BusinessCluster` 与 `Shop` 标 `bAbstract = true`，可包住下层的 `Shop` / `SubFunction`；`SubFunction` 不标，其同类互相排斥。
- **父子包含**：子区域格集合必须落在父区域包围盒内，且不得与父区域的**同类**子区域重叠。
- 查询点所属区域时返回**最小包含区域**，使权限与归属判定始终落到最具体的那一层。

#### 4.1.10 存档、读档与切图

| 存哪 | 内容 |
|---|---|
| Global | `TMap<FBusinessEntityId, FBusinessEntity>`（含 Mode / Owner / Manager / BusinessKind / Modules / LeaseId / ParentEntityId / bAbstract） |
| Scene | 每个区域的 `AreaId` / `ParentSceneId` / `ParentAreaId` / `AreaName` / `Function`（Status + AreaType）/ `AreaCoverage`（Boxes）/ `Authority` / `GameplayTags` / `EntityId` |
| 不入档 | 全部派生索引、`bOperating`、`bOpen` 之外的营业派生值、设施索引、看板、认领 |

读档顺序（嵌回 §7.2）：

```text
1. 校验 schema 与配置
2. 恢复 Global 经营实体（此时不启用时间/AI）
3. 恢复物品实例
4. 恢复区域：写回字段、重算 Bounds
5. 建派生索引：AreaId→Actor、粗格哈希、EntityId→Areas、ParentAreaId→子区域
6. 校验三条引用完整性：
     ① 每个非空 EntityId 必须存在于 Global BusinessEntities
     ② ParentAreaId 必须指向本场景已恢复的区域
     ③ 类型规则：AllowedParentTypes / RequiredParentTypes 必须成立
   任一失败 → 激活前整体终止，不激活半份世界（口径见 04 §9）
7. 恢复名册 / 现场队列 / 执行断点 → 连接其余稳定引用 → 重建看板与认领
8. 启用时间与 AI
```

切图（`ChangeScene` / `OpenLevel`）：经营实体**全程留在 Global 不重建**；场景侧 Flush 写入区域与现场数据，返回后重跑第 4–6 步。图外实体（`bAbstract=true` 或当前场景无其区域）不受影响。

#### 4.1.11 现有代码改造清单

| 现有 | 处理 |
|---|---|
| `FAreaInstance.OwnerCreatureId` / `ManagerCreatureId` | **删除**，迁入 `FBusinessEntity` |
| `AVoxelAreaBaseInstance::IsAreaEditable` | 改为经 `EntityId` 解析实体后判定；空置区域沿父链上溯一次 |
| `AVoxelAreaBaseInstance::UpdateAreaTypeFunc` 的 if-else 三分支 | 改为遍历 `FAreaTypeBase.FunctionComponentClasses` 通用挂载 |
| `FAreaTypeBase::IsShopType()` / `IsSubFunctionType()` | **删除**；改由配置字段与 `BusinessKind` 表达（后者还有 tag 路径错误，恒为 false） |
| `AVoxelAreaBaseInstance::CheckTypeConflict` 的硬编码 Owner/Parent 规则 | 改为读 `RequiredParentTypes` / `AllowedParentTypes` / `bRequiresEntity` |
| `AVoxelAreaBaseInstance::SubAreasIndex` / `ParentArea`（裸 Actor 指针） | 保留为运行时索引；存档不写指针，读档按 `ParentAreaId` 重连 |
| `UVoxelFunctionalAreaComponent` 中的 `FBusinessSchedule` | 迁入 `FBusinessHoursModule`（Global 经营实体） |
| `FShopInformFragment`、`UVoxelShopComponent`、`FShopRuntimeData` | 合并为 `FBusinessEntity` + 各模块 struct；删除 Shop 专名 |
| `Area.Type.Shop.inn` | 拆为 `Area.Type.Shop`（类型）+ `Business.Kind.Inn`（业态） |
| `Area.Type.SubFunction.Ordin` | 塌入 `Area.Type.SubFunction` 类型树，补 `Storage / Kitchen / WaitingArea / StaffRest / GuestRoom` |

#### 4.1.12 在场聚合（PresentPersons）

```cpp
// 场景侧在场索引：PresentPersons 的唯一所有者。派生索引，不入档。
class UVoxelPresenceIndexComponent : public UWorldSubsystem
{
public:
    void OnCreatureEnteredArea(int32 CreatureId, int32 AreaId);
    void OnCreatureLeftArea(int32 CreatureId, int32 AreaId);

    // 某区域内的在场者
    void GetPresentPersons(int32 AreaId, TArray<FPresentPerson>& Out) const;
    // 某经营实体「店铺范围」（主区域 + 子区域链）内的在场者，供打烊清退枚举接收人
    void GetPresentPersonsInBusiness(FBusinessEntityId Id, TArray<FPresentPerson>& Out) const;
};

USTRUCT()
struct FPresentPerson
{
    int32        CreatureId = INDEX_NONE;
    int32        AreaId = INDEX_NONE;
    FGameplayTag Role;          // 顾客 / 员工 / 经营人；同一人按 (CreatureId, Role) 去重
};
```

- **唯一所有者**是场景侧这个索引，订阅者不各自保存副本（`02空间 §9.1` 的「由订阅者各自保存」作废）。
- **不入档**：它是生物位置的函数，读档后随生物放置与区域归属重建（与区域看板、设施索引同一原则）。
- 打烊清退的接收人 = `GetPresentPersonsInBusiness(Id) ∪ 门口队列`，按 `(CreatureId, Role)` 去重后逐个发 `Shop.ExitRequested`。
- 门口队列里的等待者**不在本索引内**（他们没进店），由工作点队列自己持有。

#### 4.1.13 待定项

- ~~`FBusinessEntityId` 底层类型与 `GetTypeHash`~~ —— **已定**：`FGuid`，`TMap`/`TSet` 用引擎原生 `GetTypeHash(const FGuid&)`。
- `Business.Kind` 与 `Area.Type` 的完整取值表、`FBusinessKindDef` 的各业态配置行（含 `RequiredFacilities` 与开业校验口径，任务 B6）。
- 商圈「不嵌套」是否需要额外约束：`BusinessCluster` 的 `RequiredParentTypes` 设空即只允许建在场景根，但 `bAbstract` 使其可与自身同类重叠——需确认是否要禁止商圈与商圈重叠。
- ~~区域删除、换租户、口碑继承策略~~ —— **已定**：换租户沿用同一 `BusinessEntityId`，口碑随实体延续；删区域不注销实体（见 §4.1.13 与 §10）。
- 子功能区类型的初始集合是否就是 §4.1.5 列出的六项。

### 4.2 家具/工作点

```text
AUVoxelItemBaseInstance
 ├─ UVoxelFacilityComponent
 ├─ UVoxelWorkPointComponent
 └─ UVoxelStorageComponent
```

```cpp
class UVoxelFacilityComponent : public UActorComponent
{
    float HitPoints;
    float MaxHitPoints;
    float Cleanliness;
    TArray<FFunctionModule> Modules;

    // 维护阈值：来自家具配置行 FFacilityConfigRow，实例只缓存，不硬编码
    float CleanThreshold = 0.f;      // 清洁度低于此值出 AI.Work.Clean
    float RepairThreshold = 0.f;     // HP 低于此值出 AI.Work.Repair
};

// 家具配置行（DataTable 行）：不同家具阈值不同，收银台比货架更怕脏
USTRUCT()
struct FFacilityConfigRow : public FTableRowBase
{
    FGameplayTag FurnitureTag;
    float CleanThreshold = 0.f;
    float RepairThreshold = 0.f;
    float DecayPerUse = 0.f;
};

class UVoxelWorkPointComponent : public UActorComponent
{
public:
    bool Enqueue(const FQueueEntry& Entry);
    bool Dequeue(int32 CustomerId);
    bool Peek(FQueueEntry& Out) const;
    void CallNext();

    FWorkPointQueue Queue;
};

class UVoxelStorageComponent : public UActorComponent
{
    int32 Capacity;
    TArray<FItemInstanceId> StoredInstances;
};
```

```cpp
// 软装（家具 / 门窗）：Actor 有实例，归属直接记在实例上，随 FItemSaveRecord 入档
USTRUCT()
struct FAssetOwnershipRef
{
    FBusinessEntityId OwnerBusinessEntityId;   // 归属经营实体
    optional FGuid    PlacedByLeaseId;         // 由哪个租约放置；产权方原有资产为空
};

// 硬装（体素墙/地板/门窗结构）：体素没有实例，只能用追加式日志记归属
USTRUCT()
struct FHardwareChangeEntry
{
    optional FGuid LeaseId;                    // 落点时的生效租约；产权方改动为空
    int32          OperatorCreatureId = INDEX_NONE;
    int64          GameSec = 0;
    FIntBox        Range;                      // 本次改动的格范围（按操作批次，非逐格）
    FGameplayTag   ChangeKind;                 // 放置 / 拆除 / 替换
    FAssetOwnershipRef Ownership;
};
```

- `AUVoxelItemBaseInstance` 增 `FAssetOwnershipRef Ownership`；`PlacedByLeaseId` 为空即产权方原有资产（对应 02 店铺卷 §9 判定表）。
- **导航影响由引擎自动维护，不建额外系统**（UE 5.8 源码口径）：`ERuntimeGenerationType::Dynamic` 的官方注释是「Fully dynamic, supports geometry changes along with navigation modifiers」（`NavigationData.h`）；项目 `DefaultEngine.ini` 已配 `RuntimeGeneration=Dynamic`。`bCanEverAffectNavigation` 定义在 `UActorComponent` 上（默认 true），`SetCanEverAffectNavigation` 会「update navigation octree if needed」——即引擎自动把放置的物体纳入导航八叉树。因此**家具放置不需要任何手动 NavMesh 调用**；只需保证家具碰撞网格阻挡导航、`bCanEverAffectNavigation` 未被关掉（Ghost 网格除外）。这是资源侧配置要求，不是缺失模块。
- **硬装按操作批次记录，不逐格**：一次大面积装修只产生一条带 `Range` 的条目；判定某格归属时遍历覆盖该格的批次（02 店铺卷 §9 判定链：有租约→租户，否则→产权方，再否则→公共）。
- **写入点唯一**：建造域的几何提交与家具放置业务方法，在写世界数据的**同一业务事务**内追加日志；UI 与被放置物自身不得写（满足 02 店铺卷「不能先落点再补租约引用」）。
- 日志追加式、不改历史；退租 / 盘下按 `LeaseId` 聚合结算。

```cpp
class UVoxelFacilityIndexComponent : public UActorComponent
{
public:
    void Register(AUVoxelItemBaseInstance* Item, int32 AreaId);   // 按「最小包含区域」注册
    void Unregister(AUVoxelItemBaseInstance* Item);
    void ReindexAreaSubtree(int32 RootAreaId);                    // 区域树变化后重建该子树

    // 按能力计数。Scope = 经营实体主区域及其子区域链，调用方传主区域 RootAreaId。
    int32 CountByCapability(int32 RootAreaId, FGameplayTag Capability, const FCapabilityFilter& Filter = {}) const;
    bool  HasCapability(int32 RootAreaId, FGameplayTag Capability, const FCapabilityFilter& Filter = {}) const;
};
```

- `FCapabilityFilter` 至少含 `bForSale` 与数量下限，用于区分可售货架与后备储物（`FStorageFuncFragment` 的 `bForSale`）。
- **唯一聚合入口**：运行期设施提醒、顾客选品、`CheckoutSale` 都走这个接口，不再各自遍历家具或另存库存副本。
- 聚合范围固定为「店铺范围」= 主区域 + 其 `ParentAreaId` 链下的全部子区域（口径见 02 库存与保质期 §2）；仓库等子功能区里的货架计入本店。

- `FWorkPointQueue`：容量 8、`QueueMode{Line, WaitingArea}`、`QueueLine` 可编辑或自动生成、`WaitingAreaRef`。
- 设施状态跨阈发布 `Facility.StateThresholdCrossed`，供索引器派生 Clean/Repair。

### 4.3 生物

```text
AVoxelNpcBaseInstance
 └─ AVoxelIntelligentCreatureInstance
      ├─ UVoxelLaborComponent
      ├─ UVoxelNeedsComponent
      ├─ UVoxelScheduleComponent
      ├─ UVoxelDecisionComponent
      ├─ UVoxelExecutorComponent
      ├─ UVoxelMemoryComponent
      ├─ UVoxelCustomerComponent
      └─ UVoxelAccountRefComponent
```

```cpp
class UVoxelLaborComponent : public UActorComponent
{
    FGuid EmploymentId;
    FWorkAssignment Assignment;
    bool bAutonomyEnabled;
};

class UVoxelNeedsComponent : public UActorComponent
{
    TMap<FGameplayTag, float> Current;
    TMap<FGameplayTag, float> ThresholdCursor;
    void Advance(int64 Now);
};

class UVoxelScheduleComponent : public UActorComponent
{
    FWeeklyScheduleTemplate WeeklyTemplate;
    TArray<FAppointment> Appointments;
    ERoleContext GetCurrentRoleContext() const;
};

class UVoxelDecisionComponent : public UActorComponent
{
public:
    void OnBoardChanged(int32 AreaId);
    void TickDecision(int64 Now);
    void EnqueueForcedIntent(const FIntent& Intent);

    TArray<FIntent> PendingIntents;
    TMap<FGuid, FCandidateCooldown> CandidateCooldowns;
};

class UVoxelExecutorComponent : public UActorComponent
{
public:
    bool StartFlow(const FIntent& Intent, FFlowState& Out);
    void TickFlow(float DeltaGameSec);
    bool Interrupt(EInterruptReason Reason);
    bool Resume();

    FFlowState MainFlow;
    TArray<FFlowState> SecondaryFlows;
};

class UVoxelMemoryComponent : public UActorComponent
{
    void OnSettle(const FExperienceReport& Report);
    TArray<FLifeHistoryEntry> ShortTerm;
    TArray<FLifeHistoryEntry> LongTerm;
};

class UVoxelCustomerComponent : public UActorComponent
{
    FExpectationBand Expectations;
    TMap<FGameplayTag, float> ActivityWillingness;
    TSet<FBusinessEntityId> KnownShopIds;      // 认知指向经营实体，不是区域 ID
};

class UVoxelAccountRefComponent : public UActorComponent
{
    FGuid PersonalWalletId;
    FGuid BusinessWalletId;
};
```

- 背包固定 16 槽：`TArray<FItemInstanceId> BackpackSlots`，容量由 `GetBackpackCapacity()` 统一返回。
- 所有实际劳动者（员工、承租经营人、自营产权人、薪酬制店主）都有劳动数据，在图时由 `UVoxelLaborComponent` 承载；经营模块不挂到生物 Actor。
- 每个名册人物与玩家均创建 Personal + Business 两账户；`UVoxelAccountRefComponent` 只持账户引用，图外人物从名册/Global 账户读取，不为领工资创建 Actor。

### 4.4 组件规则

1. 现场组件在区域 `InitArea` / 生物名册实例化时创建，不随区域用途切换销毁；Global 经营模块按业务组合，不跟随区域 Actor 生命周期。
2. 现场组件 `BeginPlay` 注册 World 服务，`EndPlay` 注销；Global 经营注册不随场景卸载注销。
3. 对象池复用重置现场组件状态，不清空 Global 生意或合同。
4. 名册/物品存各自组件载荷，区域只存空间载荷与关联引用，经营模块只存 Global；派生数据不入档。
5. 现场组件通过 Owner 或子系统公开接口协作；经营身份经 `BusinessEntityId` 解析，不从 Area 查找业务组件。
6. 经营业务方法在逻辑模块，现场行为在现场组件；门面协调调用，不保留第二份真值。

---

## 5. 数据结构

### 5.1 劳动安排

```cpp
USTRUCT()
struct FWorkAssignment
{
    int32 CreatureID = INDEX_NONE;
    optional FAreaRef WorkAreaRef;           // 场景用工空间；图外 Employment 可无工作区
    FWorkShiftTemplate Shift;                // N×24 权重块
    TMap<FGameplayTag, uint8> Priorities;    // 0 禁止，4 最高
    bool bEnabled = true;
};

USTRUCT()
struct FWorkShiftTemplate
{
    TArray<EScheduleBlockType> Slots;        // Index = Day * 24 + Hour
};
```

- 玩家劳动安排存 PlayerProfile（Global），不随场景名册；场景生物安排随各场景名册。

### 5.2 工单认领

```cpp
USTRUCT()
struct FWorkClaim
{
    FGuid WorkOrderId;
    int32 CreatureID = INDEX_NONE;
    int64 StartGameSec = 0;
};
```

认领只做去重与防抢单：完成、失败、失效、离职、被接管时释放；不入档。

### 5.2.1 工单看板

```cpp
USTRUCT()
struct FBusinessTaskBoard
{
    TArray<FIntent> WorkOrders;              // 派生索引，不入档；仅收本实体范围内工单
    int64 Version = 0;                        // 索引变化版本，供叫醒与调试
};
```

看板由工单索引器唯一写入，**宿主是经营实体**（Shop 一块、District 一块），不是区域。区域只通过 `Area.EntityId` 指向所属实体——子功能区挂店铺实体、商圈范围挂商圈实体——所以子区域、仓库里的设施直接落到本店看板，**不做祖先区域解析**。玩家只读列表，不能创建/编辑/指派工单。`WorkAreaRef` 只表示劳动者实际去哪个空间干活，不决定读哪块板。

### 5.3 意图

`FIntent` 的字段唯一定义在[意图与流程统一模型 §2.3](/VoxelDesign/02-系统设计/AI决策系统-意图与流程统一模型-系统设计.md)，本节**不复制字段**（依据 `05复核` 决策 19：`FIntent` 只保留一份定义，其余位置全部引用）。本节只登记「02 语义 → UE 承载类型」的映射；需要新增字段时先改 02，再在此登记。

```cpp
// 承载 02 §2.3 的 Source 取值；不新增语义
UENUM(BlueprintType)
enum class EIntentSource : uint8 { Self, Work, Command };

// 承载 02 §2.3 的 State 取值（含 Waiting；此前本文漏列 Waiting，属缺陷而非新状态）
UENUM(BlueprintType)
enum class EIntentState : uint8
{
    Pending, Bound, Waiting, Executing, Done, Failed, Cancelled
};

// FTargetSpec.TargetRef 的具体 UE 类型随「全局 ID 类型」决策（任务 A3）确定后登记；
// 在此之前代码不得假定为 int32、裸 FGuid 或 AreaId（不得退化为仅 int32，见下方注）。
```

- 意图类别由 02 的 `Source`（Self/Work/Command）+ `Targets[].GoalTag` 表达，**不另立 `Kind` 枚举**；原 `EIntentKind{Consume,Work,Wander,Home}` 与 `EMotivationKind` 的语义重复，随 `FMotivation` 一起废弃（见 `05/AI决策系统-意图与流程统一-决策记录` 决策 C 与迁移映射）。
- `Score` 是裁决/派发阶段的**运行时评分快照**，不是 `FIntent` 的持久字段；需要展示或调试时由决策组件缓存，不入档。
- 看板上的工单也是 `FIntent`（`Source=Work`，`State=Pending`），由索引器维护；不再有第二份看板任务结构。

### 5.4 流程

节点类型集与五原语段唯一定义在[店铺空间与消费流程 §8.1/§8.2](/VoxelDesign/02-系统设计/AI决策系统-店铺空间与消费流程-系统设计.md)，本节只登记 UE 承载类型。**节点 = 一级 PlanStep（骨架），原语 = 节点内部的填充段**；`Goto / Acquire / Advance / Settle / Release` 是原语，**不是节点**；`Reserve` 是 `Acquire` 原语的一种用途（锁座位 / 占队列位 / 预留商品），不是独立节点。

```cpp
// 9 个节点（见 02 空间 §8.1）。ClaimSeat 在 V1 不启用（V1 无座位/住宿业务），
// 但保留在代码固定集合中，供后续餐厅/旅店业态使用。
UENUM(BlueprintType)
enum class EFlowNodeType : uint8
{
    Browse, QueueAt, ClaimSeat, Order, Pay, Consume, UseItem, AcquireResource, Release
};

// 五原语段：每个节点内部共用同一条流水线，各段可跳过。
UENUM(BlueprintType)
enum class EFlowPrimitive : uint8 { Goto, Acquire, Advance, Settle, Release };

// 承载 02 空间 §8.2：Advance 只有自驱 / 服务驱两种驱动方式
UENUM(BlueprintType)
enum class EWaitMode : uint8 { SelfDriven, ServiceDriven };

// 承载 02 意图 §2.4 的 OnFail 策略（放弃 / 重试 / 跳过）
UENUM(BlueprintType)
enum class EOnFail : uint8 { Abort, Retry, Skip };

USTRUCT()
struct FFlowPrimitiveStep
{
    EFlowPrimitive Kind = EFlowPrimitive::Advance;
    FGameplayTag Params;
    bool bSkippable = true;
};

USTRUCT()
struct FPlanStep                            // 原名 FFlowNode，一级结构
{
    EFlowNodeType StepType = EFlowNodeType::Consume;
    FTargetRef TargetRef;                    // 槽位/设施/区域；具体类型待任务 A3
    EWaitMode WaitMode = EWaitMode::SelfDriven;
    bool bPausable = true;
    TArray<FFlowPrimitiveStep> Primitives;
    EOnFail OnFail = EOnFail::Fail;
};

USTRUCT()
struct FFlowConfig
{
    FGameplayTag FlowType;
    TArray<FPlanStep> Nodes;
};
```

`FFlowState` 是流程实例状态的**唯一**定义（02 侧只声明"由执行组件持有，NPC 侧只存引用"，不复制字段）：

```cpp
USTRUCT()
struct FFlowState
{
    FGuid ExecutionId;
    FGuid PrimaryIntentId;
    TArray<FGuid> CoveredIntentIds;
    FGameplayTag FlowType;
    int32 CurrentNodeIndex = 0;
    float NodeProgress = 0.f;
    int64 StageStartGameSec = 0;
    ESlotType Slot = ESlotType::Main;
    EInterruptPolicy InterruptPolicy = EInterruptPolicy::Boundary;
    bool bAtomic = false;
};
```

> 待补：02 要求 Flow 持有 `StockReservationId` / `ActiveCheckoutOrderId` 与在途体验报告（见任务 D3），本结构补齐前不得声称"离店释放 / 读档续跑"已闭环。

### 5.5 雇佣记录

```cpp
USTRUCT()
struct FEmploymentRecord
{
    FGuid EmploymentId;
    int32 CreatureID = INDEX_NONE;           // 同一 NPC 最多一条有效记录
    FBusinessEntityId EmployerBusinessEntityId;  // 雇主经营实体（Shop 或商圈）
    FGameplayTag RoleTag;                    // Staff / Shopkeeper
    FWageConfig Wage;
    int64 HireGameSec = 0;
    int64 NextWageGameSec = 0;
    FGameplayTag HireSource;
    FWeeklyHourMask AvailabilityTemplate;    // 仅 Staff
    EEmploymentStatus Status;
};
```

雇佣记录不含**排班、优先级、工作区**（这些在劳动安排），但含 **`AvailabilityTemplate` 合同约定的可工作时间**。两者是约束关系：**实际排班必须是可工作时段的子集**，排班只能在 `AvailabilityTemplate` 覆盖的小时格内落点；合同写了 8–10 点，就只能排 8–10 点。改可工作时间等于换合同条款，不是改排班。

### 5.6 经营实体运行时数据

> 经营实体的字段与模块结构唯一定义在 §4.1.2。本节保留与其配套的贸易/合同级载荷：`FLeaseRuntimeData`、`FBusinessProduct`、`FStockReservation`、`FCheckoutOrder`。

```cpp
USTRUCT()
struct FLeaseRuntimeData                        // Global LeaseRegistry 条目，唯一事实
{
    FGuid LeaseId;
    FBusinessEntityId LessorBusinessEntityId;    // 出租方（商圈）经营实体
    FBusinessEntityId TenantBusinessEntityId;    // 承租方经营实体
    optional FAreaRef LeasedAreaRef;             // 承租空间引用
    int32 TenantCreatureId = INDEX_NONE;         // 真实租户人物（承租责任人）
    int64 MonthlyRent = 0;
    int64 StartGameSec = 0;
    int64 NextRentGameSec = 0;
    int32 RentCollectionDayOfMonth = 1;         // 商圈收租日快照，离线场景也能算下一期
    int32 SettledPeriodCount = 0;               // 0 = 下一期为首期
    int64 ExitGameSec = 0;
    ELeaseStatus Status = ELeaseStatus::Active;
};

USTRUCT()
struct FBusinessProduct                     // 原 FShopProduct；商品目录条目属贸易模块
{
    FGameplayTag ItemTag;
    int64 UnitPrice = 0;
    bool bOnSale = true;
    int32 RestockThreshold = 0;              // 店铺级补货阈值；判定基数是全仓库合计（货架+后备）
    int32 RestockAmount = 0;                 // 一次补货的目标数量
};

USTRUCT()
struct FStockReservation
{
    FGuid ReservationId;
    int32 BuyerId = INDEX_NONE;
    FBusinessEntityId BusinessEntityId;      // 预留所属经营实体（Global 经营业务事实）
    TArray<FReservationLine> Lines;          // ProductTag/LinkedItemId/Quantity/LockedUnitPrice
};

USTRUCT()
struct FCheckoutOrder
{
    FGuid OrderId;
    FGuid ReservationId;
    int32 BuyerId = INDEX_NONE;
    int32 CashierItemId = INDEX_NONE;
    int64 AmountDue = 0;
    ECheckoutStatus Status = ECheckoutStatus::Pending;
};

// 原 FShopRuntimeData 已拆入 §4.1.2 的模块化定义：
//   Catalog / ActiveReservations / PendingOrders        → FTradeModule
//   bOpen / 营业时段                                     → FBusinessHoursModule
//   声誉 / 客群 / 客流                                   → FReputationModule
//   LeaseId                                              → FBusinessEntity.LeaseId
// 本节不再重复声明经营实体的模块字段。
```

### 5.7 营业时段

```cpp
USTRUCT()
struct FBusinessScheduleRuntime
{
    TArray<FBusinessDayMask> Days;           // N 天，每天 24 格
    TArray<FBusinessExtension> Extensions;
    bool IsOpenAt(int64 GameSec) const;
};

USTRUCT()
struct FBusinessDayMask
{
    TArray<bool> OpenHours;                  // 24
};
```

### 5.8 工作点队列

```cpp
USTRUCT()
struct FQueueEntry
{
    FGuid ExecutionId;
    int32 CustomerId = INDEX_NONE;
    int64 EnqueueGameSec = 0;
    int32 QueueNumber = 0;
    int64 CalledGameSec = 0;
};

USTRUCT()
struct FWorkPointQueue
{
    TArray<FQueueEntry> Entries;             // 顺序即 FIFO
    int32 NextQueueNumber = 1;
    EQueueMode Mode = EQueueMode::Line;
    TArray<FIntVector> QueueLine;
    optional FAreaRef WaitingAreaRef;        // 等待区（FAreaRef，不是裸 AreaId）
    int32 Capacity = 8;
};
```

### 5.9 经济

```cpp
USTRUCT()
struct FMoneyAccount
{
    FGuid AccountId;
    int32 OwnerCreatureId = INDEX_NONE;
    EAccountKind Kind = EAccountKind::Personal;
    int64 Balance = 0;
    bool  bAbstract = false;   // 抽象账户：不参与余额运算
};

**抽象账户规则**（图外雇主、游商等不在图内的实体）：

- `bAbstract=true` 的账户**不参与余额运算**：付款时不扣 `Balance`，收款时也不加，`Balance` 恒为 0，没有"余额"这个概念；
- 因此**不做余额校验、不设负值、不参与贷款与破产**，也不需要补资机制；
- 交易仍照常入经济命令队列、照常写 `FTransactionRecord` 与发 `Economic.SettlementResolved`，只是流水两侧的抽象账户不产生余额变化；
- 这是 V1 的显式取舍：图外世界不模拟经济，工资等支出属于净货币注入。后续要做闭合经济时，把该账户的 `bAbstract` 关掉并接入真实资金/抽象营收即可，**工资链与流水结构不用改**。

USTRUCT()
struct FTransactionRecord
{
    FGuid TransactionId;
    FGuid FromAccountId;                        // 付款账户
    FGuid ToAccountId;                          // 收款账户
    FGameplayTag Category;
    int64 Amount = 0;
    int64 GameSec = 0;
    FBusinessEntityId SourceBusinessEntityId;   // 付款侧经营归属，个人消费等可空
    FBusinessEntityId TargetBusinessEntityId;   // 收款侧经营归属
    optional FAreaRef OccurredAreaRef;          // 可空发生地点，不用于业务归属/资金责任
};

USTRUCT()
struct FLoanOffer
{
    FGuid OfferId;
    int32 BorrowerCreatureId = INDEX_NONE;
    int32 SlotIndex = 0;
    int64 Principal = 0;
    float AnnualRatePercent = 0.f;
    int32 TermMonths = 0;
    int64 FixedPrincipalPayment = 0;
    int64 CreatedGameSec = 0;
};

USTRUCT()
struct FLoanDebt
{
    FGuid LoanId;
    int32 BorrowerCreatureId = INDEX_NONE;
    FGameplayTag LenderTag;
    int64 OriginalPrincipal = 0;
    int64 RemainingPrincipal = 0;
    float AnnualRatePercent = 0.f;
    int32 TermMonths = 0;
    int64 FixedPrincipalPayment = 0;
    int32 PaidInstallments = 0;
    int64 DisbursedGameSec = 0;
    int64 NextPaymentGameSec = 0;
    ELoadStatus Status = ELoadStatus::Active;
};

USTRUCT()
struct FEconomyRuntimeData
{
    TMap<FGuid, FMoneyAccount> Wallets;
    TMap<FGuid, FTransactionRecord> Transactions;
    TMap<FGuid, FLoanOffer> LoanOffers;
    TMap<FGuid, FLoanDebt> Loans;
};

USTRUCT()
struct FSettlementTask
{
    int64 DueGameSec = 0;
    ESettlementType Type;
    FGuid TargetId;
};

// 统一经济命令：所有扣款/入账都经此入队，不允许业务直接调转账。
UENUM()
enum class EEconomicCommandPhase : uint8
{
    ScheduledPayable,   // ① 到期计划任务中必须足额的
    ImmediatePayable,   // ② 本 tick 即时足额业务（采购、提前还款等）
    NegativeAllowed     // ③ 到期计划任务中属于扣负白名单的（贷款月供 / 租金 / 工资）
};

USTRUCT()
struct FEconomicCommand
{
    int64                 DueGameSec = 0;
    EEconomicCommandPhase Phase = EEconomicCommandPhase::ImmediatePayable;
    int32                 OrderInPhase = 0;       // 入队序号，同阶段内稳定排序
    ESettlementType       Type;
    FGuid                 TargetId;               // 贷款 / 租约 / 雇佣
    FBusinessEntityId     Source;
    FBusinessEntityId     Target;
    int64                 Amount = 0;
    bool                  bRequiresSufficientFunds = true;   // ① ② 为 true，③ 为 false
};
```

`FTransactionRecord` 一条流水写两账户与两侧经营归属；租金源为店铺经营实体、目标为商圈经营实体；`OccurredAreaRef` 仅是可空发生地点，不判断资金责任或业务归属；流水历史不回改。

### 5.10 时间

```cpp
USTRUCT()
struct FTimeRuntimeData
{
    int64 GameTimeSec = 0;
    FName CalendarConfigId;
};
```

### 5.10.1 配置资产清单（V1 由 `UConfigDataSubsystem` 加载）

运行时只存配置 ID，数值全在配置资产里，改数值不改代码：

| 配置资产 | 内容 | 引用方 |
|---|---|---|
| `UCalendarConfig` | 日历周期：6 天/周、5 周/月、12 月/年、360 天；营业时段格长度 | 时间子系统 |
| `ULoanConfig` | 贷款额度档、利率、期数、报价槽数量（5 槽） | 经济子系统 |
| `FFacilityConfigRow`（DataTable） | 家具维护阈值 `CleanThreshold / RepairThreshold`、`DecayPerUse` | 家具 / 工单索引器 |
| `FBusinessKindDef` | 业态的必需设施、允许经营品类、模块组合 | 经营实体 / 开业提醒 |
| `UEventInterpretationSet` | 完整版事件解读规则（V1 不加载） | 记忆系统 |

### 5.11 名册与存档记录

```cpp
USTRUCT()
struct FRosterRecord
{
    int32 CreatureID = INDEX_NONE;
    optional FAreaRef HomeAreaRef;               // 住所：V1 指向抽象住所对应的 SpawnArea；无实体住宅
    FTrueCreatureData True;
    FPerceivedCreatureData Perception;
    TMap<FGameplayTag, float> BasicValues;
    FLaborSaveData Labor;
    FNeedsSaveData Needs;
    FScheduleSaveData Schedule;
    FDecisionSaveData Decision;
    FExecutorSaveData Executor;
    FMemorySaveData Memory;
    FCustomerSaveData Customer;
    FAccountRefSaveData Account;
    FVector LastLocation = FVector::ZeroVector;
    bool bInMap = false;
};

USTRUCT()
struct FAreaSaveRecord
{
    FAreaInstance Base;                          // 空间范围、层级、用途投影
    optional FAreaRef ParentAreaRef;             // 空间父子（子区域有 AreaId 但不是经营实体）
    optional FBusinessEntityId BusinessEntityId; // 关联经营 ID 引用，不复制经营业务
};

USTRUCT()
struct FSceneSaveData
{
    // Time/Economy/BusinessEntities/Employments/Leases/PlayerProfile 仅 Global，不在 Scene；
    // 预留/Pending 订单/声誉等经营业务事实只在 Global 经营实体，Scene 不另存镜像；
    // 设施派生索引读档重建，不入档
    FSceneBaseData Base;
    TMap<int, FString> ModifiedBlockData;
    TMap<int32, FAreaSaveRecord> Areas;
    TMap<int32, FRosterRecord> Roster;
    TArray<FItemSaveRecord> Items;              // 物品/家具/现场工作点队列（家具含 FAssetOwnershipRef）
    TMap<int32, TArray<FHardwareChangeEntry>> HardwareChangeLog; // 按区域归组的硬装归属日志，追加式
    TMap<int32, FCutoutBoxData> CutoutBoxes;
    FVector PlayerPawnLocation;
    TArray<FFlowState> ExecutionBreakpoints;    // 现场执行断点引用（Flow 状态随生物存档）
};
```

---

## 6. 关键流程

### 6.1 决策

**RoleContext**

- `Duty`：个人时段为 Duty，且劳动组件存在、`bEnabled=true`、对应雇佣记录有效。
- `Free`：其余时段。
- `Sleep`：睡眠时段或硬生理极限。
- `Forced`：安全事件、玩家指令、硬生理极限覆盖。

**Duty 工作决策**

```text
读 LaborComponent.Assignment
  → 所属经营实体看板（`WorkAreaRef` 只表示去哪干活）
  → 过滤：Priorities[WorkType] == 0、权限/技能不满足、路径不可达
  → 排序：优先级 4→1，同优先级按工单 Priority 分带，再按距离、PushGameSec
  → 选一个 → 生成 FIntent（Source=Work，Targets 指向该看板工单）→ 认领 → 派发执行
```

排班是权重：Work 时段工作子树权重最高；Anything 也允许工作，权重较低；Dine 只调吃饭权重，需求驱动 `UseItem` 始终可高于工作。

**Free 消费决策**

```text
硬过滤：bOperating、KnownShopIds、价格上限、可达
  → 收集候选
  → 加法核：Score = w1·Need(主需求) + w2·Quality + w3·Willingness + w4·SecondaryFill(次要回填)
  → 衰减：ProximityAtten、QueueFactor（永不为零）
  → 修饰：Personality / TimeMod / Memory / Clientele
  → Top-3 加权随机 → FIntent(Kind=Consume) → 执行
```

选店不读钱包；钱包与背包在加入购物篮/锁价预留时由 `CanReserveBasket` 校验。

**强制覆盖顺序**

```text
外部安全事件 > 硬生理极限 > 玩家指令 > 可压制 Critical 需求 > 当前执行 > Urgent > 常规
```

强制只改变待执行顺序，不打断原子段。

### 6.2 执行

```text
StartFlow(FIntent)
  → 按 FlowType 加载 FFlowConfig
  → FFlowState 从节点 0 开始
  → 每 tick 推进当前节点：
      Goto      寻路到目标，超时/停滞判定
      Browse    读 Shop 商品目录，形成购物篮
      Reserve   调 CanReserveBasket 锁价预留（可缩篮子）
      QueueAt   入工作点队列，等待叫号
      Order     到收银台开始服务时创建 Pending 订单
      Pay       **发起结算请求并等待结果**，不直接成交：
                  由收银台业务入口对「当前活动订单」执行唯一的 CheckoutSale；
                  有收银员 → 收银员执行；无收银员 → 顾客在队首等待，破底离店并释放预留。
                  顾客与员工不得各自结算同一订单（02 空间 §「CheckoutSale 唯一入口」）。
      Consume   使用物品/推进需求
      UseItem   选物品→就位→等待→效果→扣减
      Settle    结算流水/统计
      Release   释放预留、认领、槽位
  → 节点边界保存断点
  → 完成/失败 → Decision.OnFlowFinished
```

- **原子段**：`bAtomic=true` 的节点不可被玩家指令、下班、普通高优先级打断；只能等完成或目标工作位释放。
- **中断**：Critical 需求可在节点边界打断；`PartialSettle` 按比例结算已完成部分。
- **断点**：`CurrentNodeIndex / NodeProgress / StageStartGameSec` 随生物存档。
- 玩家手动操作与 NPC 自主共用同一业务入口：有工单则认领，无工单直接执行业务方法，不补建工单。

### 6.3 工单索引与叫醒

```text
业务状态变化（设施跨阈/库存跨阈/队列变化/建造/经营操作）
  → 发布事件
  → FWorkIndexer 按 (SourceId, WorkType) 去重
  → 写对应经营实体看板（派生索引，按 Area.EntityId 归组）
  → 若新增工单或优先级升高：内部通知本区在岗空闲劳动者
  → 决策组件重置找活冷却并重新决策
```

- 看板只由索引器写；业务域不直写。
- 劳动者只读自己 `WorkAreaRef` 的看板。
- 玩家只看列表，不能创建/编辑/指派工单。
- 找活有冷却、退避、分帧预算与前 K 候选可达性检查。

### 6.4 消费链

```text
进店 → Browse（不校验钱包）
  → 加入购物篮/锁价预留：CanReserveBasket
     ① bOperating 且货页可售
     ② 聚合现货 ΣOnHand − ActiveReserved 足够
     ③ 钱包 ≥ 整单锁价金额
     ④ 16 槽背包整单可放置（GetBackpackCapacity）
     不通过则缩篮子；一件都不行则离店
  → 收银台排队（工作点 FIFO，容量 8）
      破底（等待超过该队列的等待阈值）→ 一次性事务，顺序固定：
          出队 Dequeue → 释放整单预留 → 上报一次白跑负样本（只写服务组）
          → 写该经营实体的候选冷却 → ExitRequested → 离店
          （即使之后又回来成交，这次破底仍记一次白跑）
  → 到收银台开始服务时创建 Pending 订单
  → CheckoutSale 原子：锁价转账 + 核销预留 + 扣货 + 入包 + 所有权转移 + 流水
  → 成功 Completed；失败 Failed 并释放预留
  → 按需 UseItem（需求驱动，优先级高于工作）
  → 离店：ExitRequested（意图）→ Presence.Exited（事实）
  → 体验一次性上报；未完成 Pending 订单强制 Cancelled
```

打烊：不再进新客、不再产生接客/收银工单；店内已锁价预留、已在收银队列、正在结账的顾客继续服务到完成，店外未进店的门口队列清退；内部工单继续，员工按劳动安排离岗。**白跑 = 有投入但未完成目的**：店外等待被清退、进店未成交、任一排队等待破底（即使之后成交）都记一次负满意度样本进入服务组；来访/成交/客群按真实事实独立统计，不在白跑路径上增减。

### 6.5 结算链

```text
时间推进 → 收集本 tick 的经济命令进统一队列（不是直接转账）
  → 到期计划任务：
       扣负白名单三项（贷款月供 / 租金 / 工资） → ③ NegativeAllowed
       其余到期应收应付                          → ① ScheduledPayable
  → 玩家 / AI 即时业务（采购、提前还款等）       → ② ImmediatePayable
  → 按 (DueGameSec, Phase, OrderInPhase) 稳定排序后逐条执行
       ① ② 必须验足额，不足则该条失败并回退，不影响后续阶段
       ③ 允许源经营账户扣负，不验余额
  → 每条写唯一流水 → 发布 Economic.SettlementResolved
  → 推进各自 Next...Sec
```

**唯一入口约束**：任何经济扣款/入账都必须入队执行，业务代码不得绕过队列直接调转账——否则「先足额后扣负」失效。这条要能写成断言（`check(IsInEconomicCommandExecution())`）。

计划表派生，不入档；读档由贷款/租约/雇佣的 `Next...Sec` 重建。

- 租约唯一事实在 Global `LeaseRegistry`；`LeaseManager` 由调度器叫醒，读 Global 租约、按有效租约实时解析经营责任人与经营账户、调经济转账、推进 `NextRentGameSec`，不经 Shop 组件或场景。租金流水源为店铺经营实体、目标为商圈经营实体，历史不回改。
- 工资按 Global 有效 `Employment` 实时解析经营责任人与经营账户，不依赖雇主/员工 Actor 或场景加载；无有效雇佣（失业）则无工资。
- 贷款报价首次打开生成 5 槽，月界只补空槽，读档不重摇；借据按 `NextPaymentGameSec` 结算。
- 租约存 Global，场景未加载也能结算；只有店铺销售/顾客模拟需要场景加载。

### 6.6 开局装配与开店链

```text
新游戏（Global 初始化，不自动创建玩家承租店）：
  生成名册（30–50），每人创建 Personal + Business 双账户
  → 创建玩家档案（个人钱包 / 经营账户 / 劳动安排 / 自主开关）
  → 预置商圈：商圈产权经营实体（抽象产权人/游商，可无实际区域）及其经营账户
  → 为玩家写入商圈薪酬雇佣记录（Global Employment，图外有效）
  V1 只预置商圈范围与产权经营实体；**店铺区域由玩家在建造面板圈定、并在同一次组合提交中创建**（不预置空铺）
  不自动创建玩家承租经营实体/租约/劳动安排
  NPC 招商、模板经营、住宿等后续模块 V1 不启用

开店（玩家在同一建造面板内确认）：
  拉区域 / 摆模板 → 填经营信息 → 确认
  → 组合创建门面（BusinessCreationFacade）一次提交：
       创建承租经营实体 + 关联区域空间 + 写入有效 Lease（Global LeaseRegistry）
       有效 Lease 先于真实租户的建筑/家具写入
       子区域只建空间（有 AreaId，但不是经营实体），不另建经营实体
  → 收首期（签约当月剩余日历天数比例，0 租金跳过流水）→ 原子提交
  → 建造/装修（免费）→ 放置后备储物 / 货架 / 收银台（真实租户资产）
  → 即时采购：Economy 扣款 → 储物组件入库
  → 上架/定价：Shop 业务方法
  → 开业 OpenShop：**不做设施校验**（05 决策29），直接置 bOpen=true；不校验入口/可达性
  → bOpen 手动入档；bOperating = bOpen && 营业时段（纯派生，不入档）
```

- 签约失败整体回滚，不留下半份经营关系、租约或劳动安排。
- 采购、上架、定价都走业务方法；UI → Controller → 业务门面 → Global 经营 / Scene 空间 / 合同账户 → 事件 → 订阅刷新，不直写数据。
- 产权/经营人权限按经营实体关系与有效授权，不从空间包含自动转移租户资产；易主只改依赖产权人的链，承租收支与工资仍归承租经营人。

---

## 7. 存档

### 7.1 根结构

```text
FSaveData
├─ BaseData（存档槽 / 版本 / 当前场景 ID）
├─ Global（仅 Global，跨场景/OpenLevel 存活，场景卸载不销毁）
│   ├─ Time（GameTimeSec + 日历引用）
│   ├─ Economy（钱包/流水/贷款/交易流水）
│   ├─ BusinessEntities（经营关系 + 按业务组合的模块数据：营业/销售、预留、Pending 订单、声誉、客流）
│   ├─ Employments（雇佣登记）
│   ├─ Leases（所有场景的租约登记，按 LeaseId/LeasedAreaRef 索引）
│   └─ PlayerProfile（玩家劳动安排/自主开关/账户引用）
└─ Scenes: TMap<int32, FSceneSaveData>
    ├─ Base / ModifiedBlockData / Areas / CutoutBoxes / PlayerPawnLocation
    ├─ Roster（名册 + 生物组件载荷）
    ├─ Items（物品/家具/现场工作点队列）
    └─ ExecutionBreakpoints（现场执行断点引用）
    预留 / Pending 订单 / 声誉属 Global 经营业务事实，Scene 不另存镜像
```

### 7.2 读档顺序

1. 校验 schema 与配置
2. 恢复 Global：时间、经济、经营实体（含预留/Pending 订单/声誉/客流）、雇佣、租约、玩家档案（此时不启用时间/AI）
3. 恢复物品实例
4. 恢复区域与空间组件（范围/父子/关联引用，不恢复业务镜像）
5. 恢复名册与生物组件
6. 恢复现场队列与执行断点引用（预留/Pending 订单不在 Scene，已在第 2 步随经营实体恢复）
7. 连接稳定 ID（经营实体、合同、账户、区域引用）
8. 重建派生：看板、认领、`bOperating`、空间映射索引、结算计划表
9. 启用时间与 AI

### 7.3 不入档

看板、认领、`bOperating`、站位/可达性缓存、派生索引、结算计划表、`PresentPersons`。

### 7.3.1 存档边界（原子事务内禁止快照）

- **只允许在业务事务边界取快照**：Flow 节点之间、`CheckoutSale` 返回之后、结算命令执行完之后。
- **`CheckoutSale` 原子提交内部不设存档点**：事务内出现的 `Processing / Completed / Failed`、一次性扣货/入包计划、已提交结果都不入档；成功交易只保留唯一流水，读档后不得再次成交。
- Flow 消费 `FCheckoutResult` 并**离开收银节点后**才允许快照，因此存档里只会有 Active 预留与 Pending 当前订单。
- 存档入口统一走 `SaveGame()`，任何业务代码不得在事务中途触发存档（与 §6.5 的「经济命令唯一入口」同属可断言约束）。

### 7.4 场景切换与跨关卡

```text
ChangeScene（同 World 换场景）
  → FlushSceneRuntime()：区域组件/名册/物品/现场队列/执行断点引用写入 FSceneSaveData
  → LoadSceneData(new)：重建体素 → 区域 → 名册 → 物品
  → RestoreSceneRuntime()：重建看板/认领/索引
  → Global（时间/经济/经营实体/雇佣/租约/玩家档案）不参与切换，不随场景卸载销毁

EnterTemplateEditMode（OpenLevel）
  → PauseGlobalTime()
  → SaveCurScene()：当前场景写入 FSceneSaveData
  → OpenLevel(TemplateEditLevel)：World 销毁，World 子系统重建
  → 返回主关卡：LoadSceneData + RestoreSceneRuntime → ResumeGlobalTime()

约束：
- GameInstance 子系统只存稳定 ID 与纯数据，不持 Actor/World 指针。
- World 内事件订阅者在 EndPlay 注销。
- 切场景/OpenLevel 前必须 Flush，返回后必须 Restore；未 Flush 的场景运行时会丢失。
- 场景卸载只销毁现场 Actor/组件运行时；Global 经营实体、租约、雇佣、账户不随之销毁。
```

---

## 8. 性能

- 决策事件驱动，非逐帧；图外 NPC 零决策开销。
- 需求推进 1 游戏秒一次。
- 看板按区域分片，劳动者只读本区。
- 索引器增量更新，不重建全图。
- 找活冷却/退避/分帧预算；只对前 K 个候选做可达性。
- 结算任务表按 `Next...Sec` 排序，逐条触发。

---

## 9. 与现有代码的改造

以下为待实施架构目标，当前源码未迁移/未验收；表中"处理"列描述目标改造方向，不表示代码已改。

| 现有 | 处理 |
|---|---|
| `UCreatureBaseStateTreeAIComponent` | 删除 |
| `UGOAPAction` / `GOAPWorldState` | 删除 |
| `ACreatureAIController` | 保留，只做 MoveTo/StopMovement |
| `UConsumComponent` | 替换为 `UVoxelNeedsComponent` |
| `LiveTime / MaxLiveTime / UpdateLiveTime` | 删除，生命周期由名册与日程驱动 |
| `UVoxelFunctionAreaComponent` | 拆分为区域空间组件（范围/父子/校验，不持经营业务） |
| `UVoxelCreatureWorldSubsystem` | 名册驱动；新增 SpawnFromRoster/RecycleToRoster |
| `FBusinessSchedule` | 保留为配置模板；运行时用 `FBusinessScheduleRuntime` |
| `FSaveData` / `FSceneData` | 新增 Global 根（时间/经济/经营实体/雇佣/租约/玩家）与 Scenes 根（区域/名册/物品/现场队列/执行断点引用） |
| `UVoxelGameInstance` | 新增 GameInstance 级时间/事件/经济/经营实体/劳动/租约子系统；存档协调 Global + Scene |
| `UVoxelSceneSaveGame` | 保存 Global 根；场景根写区域/名册/物品/现场队列/执行断点引用（预留/Pending 订单归 Global） |
| `ChangeScene` / `EnterTemplateEditMode` | 切换前 FlushSceneRuntime，返回后 RestoreSceneRuntime；模板编辑暂停全局时间 |
| `AVoxelIntelligentCreatureInstance` | V1 统一智能生物；只挂现场组件，经营模块归 Global 经营实体 |
| `AVoxelNpcBaseInstance` | 保留基类，不承载经营逻辑 |

---

## 10. 待定实施边界与蓝图调试

### 10.1 待定实施边界

以下边界尚未确认，本规范只标"未定"，不预设方案；实现前需另行决策：

- ~~`FBusinessEntityId` 底层采用 GUID 还是 int32~~ —— **已定**：`FBusinessEntityId` / `FCreatureId` / `FItemInstanceId` 统一为 `FGuid`，规则见 02 总纲 §2.7。
- 换租户是否启用新的经营实体 ID、口碑/声誉与客户资产如何继承。
- ~~删除区域后其关联经营实体是否注销~~ —— **已定**：删区域只解除空间关联，**不注销经营实体**；实体可继续作为无区域实体存在。注销经营是独立业务动作。
- 图外雇主的工资资金来源见 §5.9「抽象账户规则」——`bAbstract=true` 的账户不参与余额运算，支出直接发生，不存在补资机制。
- 图外招聘与图外换工规则。
- NPC 招商、模板经营、住宿等后续模块的接入方式（V1 不启用）。
- 产权/经营人权限链的具体授权校验规则（原则已定：按经营实体关系与有效授权，不从空间包含自动转移租户资产；易主只改依赖产权人的链，承租收支/工资仍归承租经营人）。

### 10.2 蓝图与调试

- 业务方法 `BlueprintCallable`，配置用 DataAsset/DataTable。
- 调试可视化：意图队列、当前 Flow 节点、看板工单、认领、需求条。
- 事件目录可视化：最近发布事件与订阅者。

---

## 11. 实现细节

### 11.1 评分配置 `UUtilityEvaluatorConfig`

```cpp
UCLASS(BlueprintType)
class UUtilityEvaluatorConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    // 加法核权重
    float WeightNeed = 1.0f;
    float WeightQuality = 1.0f;
    float WeightWillingness = 1.0f;
    float WeightSecondaryFill = 0.15f;   // 次要回填：主需求之外、该候选能覆盖的需求，权重远小于前三项

    // 加法核分量
    TMap<FGameplayTag, FRichCurve> NeedCurves;
    FRichCurve DistanceCurve;

    // 修饰器
    TArray<FPersonalityWeightEntry> PersonalityEntries;
    float DutyConsumePenalty = 0.45f;
    TArray<FTimeWindowMod> TimeModifiers;
    float MemoryRecencyDecayRate = 0.05f;

    // 选择器
    float MinActionThreshold = 0.02f;
    int32 TopNForRandom = 3;

    // 硬过滤
    float CriticalPriceRelaxFactor = 1.5f;

    // 候选扫描
    float ScanNearbyRadius = 2000.0f;

    // 执行保护
    float MoveTimeoutSec = 60.0f;
    float StagnationEpsilon = 10.0f;
    float StagnationTimeoutSec = 15.0f;
    float CandidateCooldownSec = 300.0f;
    float CooldownScoreScale = 0.1f;
    int32 TemplateFailLimit = 3;

    // 声誉（贝叶斯收缩）
    float ReputationPriorMean = 0.5f;
    float ReputationShrinkStrength = 5.0f;
};

USTRUCT(BlueprintType)
struct FPersonalityWeightEntry
{
    FGameplayTag DimensionTag;
    FGameplayTagContainer AffectedActionTags;
    float MultiplierPerUnit = 0.5f;
};

USTRUCT(BlueprintType)
struct FTimeWindowMod
{
    float StartHour = 0.f;
    float EndHour = 24.f;
    FGameplayTagContainer AffectedActionTags;
    float Multiplier = 1.0f;
};
```

V1 中 `MemoryMod` 与 `ClienteleMod` 固定为 1，配置留字段但不参与评分。

### 11.2 候选上下文 `FCandidateContext`

```cpp
struct FCandidateContext
{
    TWeakObjectPtr<AUVoxelItemBaseInstance> TargetFacility;
    FBusinessEntityId BusinessEntityId;       // 候选经营实体（唯一候选身份，不持模块指针）
    optional FAreaRef AreaRef;                // 可选空间引用，供路过/导航
    FGameplayTag TargetFuncTag;
    FGameplayTag ActionTag;
    FGameplayTag PrimaryPurposeTag;
    FVector TargetLocation;

    float FinalScore = 0.f;
    float NeedScore = 0.f;
    float QualityScore = 0.f;
    float WillingnessScore = 0.f;
    float SecondaryFillScore = 0.f;   // 主需求之外、该候选能覆盖的需求贡献之和
    float ProximityAtten = 1.f;
    float QueueFactor = 1.f;
    float PersonalityMod = 1.f;
    float DutyMod = 1.f;
    float TimeMod = 1.f;
    float MemoryMod = 1.f;
    float ClienteleMod = 1.f;
};
```

### 11.3 体验与口碑

```cpp
UENUM(BlueprintType)
enum class EExperienceAttribution : uint8 { Shop, Self, Environment };

USTRUCT(BlueprintType)
struct FExperienceReport
{
    int32 ReporterCreatureId = INDEX_NONE;
    FGuid ExecutionId;
    EExperienceAttribution Attribution = EExperienceAttribution::Shop;
    TMap<FGameplayTag, float> Aspects;   // Aspect.Service/Quality/Price/Luxury/Environment
    FBusinessEntityId BusinessEntityId;  // 体验归属的经营实体
    float Progress = 1.f;                // 中断时 <1，体验分按进度打折
};

USTRUCT(BlueprintType)
struct FServiceAspectReputation
{
    float RollingScore = 0.5f;
    int32 SampleCount = 0;
};

USTRUCT(BlueprintType)
struct FBusinessReputation                   // 原 FShopReputation；属 FReputationModule
{
    TMap<FGameplayTag, FServiceAspectReputation> ServiceAspects;
    int64 NegativeProtectionGameDay = 0;
    int32 NegativeSampleCount = 0;
};

USTRUCT(BlueprintType)
struct FClienteleStat
{
    int64 VisitCount = 0;
    int64 CheckoutCount = 0;
    int64 SalesAmount = 0;
    double SatisfactionSum = 0.0;
    int32 SatisfactionSampleCount = 0;
};

USTRUCT(BlueprintType)
struct FBusinessClienteleStat                // 原 FShopClienteleRuntimeData；四类客群桶
{
    TMap<FGameplayTag, FClienteleStat> ByRace;
    TMap<FGameplayTag, FClienteleStat> ByProfession;
    TMap<FGameplayTag, FClienteleStat> ByFaction;
    TMap<int32, FClienteleStat> ByDistrict;
};

USTRUCT(BlueprintType)
struct FBusinessTrafficStat                  // 原未定义的 FShopTrafficStats，此处补全
{
    TMap<int32, int32> CustomerEntriesByHour; // 小时 → 入店人次（派生统计用）
    int64  LastDayClosedGameSec = 0;          // 日结游标
    int32  DayEntryCount = 0;                 // 当日累计，日结后清零
};

float ComputeEffectiveReputation(const FServiceAspectReputation& Rep,
                                 float PriorMean, float ShrinkStrength)
{
    return (Rep.RollingScore * Rep.SampleCount + PriorMean * ShrinkStrength)
         / (Rep.SampleCount + ShrinkStrength);
}
```

- 体验在离店时一次性上报；单日集中差评保护由统计订阅者执行。
- 负面记录入口：排队超时离开、满员拒绝入队、进店买不起。
- **白跑 = 有投入但未完成目的**，三种情形各记一次负满意度样本：店外等待被清退、进店未成交、任一排队等待破底（即使之后成交）。样本只进服务组；来访/成交/客群按真实事实独立统计（`VisitCount/CheckoutCount/SalesAmount` 不因白跑增减）。

### 11.4 流程节点与订单

```cpp
// EFlowNodeType / EFlowPrimitive / EWaitMode / EOnFail 唯一定义在 §5.4，此处不重复声明。

// 承载 02 空间 §8.1 的 Pay(时机) 节点参数
UENUM(BlueprintType)
enum class EPayTiming : uint8 { Prepaid, Deposit, Final, Daily, Monthly, OnLeave };

// 节点结构唯一定义在 §5.4 FPlanStep，此处不重复声明。
// 原 UBusinessFlowConfigAsset 违反《意图与流程统一-决策记录》决策 C：
// 统一 Flow 表按 FlowTypeTag 索引、消费与工作同表，"每店铺类型一份 Flow" 口径已废弃。
// bAllowTips 属 V1 不做（小费），不随 V1 流程资产下发。
UCLASS(BlueprintType)
class UFlowConfigAsset : public UDataAsset
{
    FGameplayTag FlowTypeTag;                   // Consume.* / Work.*，同表索引
    TArray<FPlanStep> Nodes;
};

USTRUCT(BlueprintType)
struct FBillOrder
{
    FGuid OrderId;
    FGameplayTag ProductTag;
    TArray<FGameplayTag> MaterialRequirements;
    float WorkAmountSec = 300.f;
    int32 ClientNPCId = INDEX_NONE;
    int64 PaidDeposit = 0;
    int64 DeliveryDeadlineGameSec = 0;
    float Priority = 1.f;
};
```

### 11.5 候选冷却

```cpp
USTRUCT(BlueprintType)
struct FCandidateCooldown
{
    // 候选经营实体 ID → 下一游戏日界的绝对 GameTimeSec；读档直接恢复
    TMap<FBusinessEntityId, int64> CooldownUntilByBusinessEntityId;
    TMap<FGameplayTag, int32> ConsecutiveFailCountByTemplate;

    void ClearOnSuccess(FBusinessEntityId BusinessEntityId, FGameplayTag TemplateTag);
};
```

- 仅"排队等待期望破底后主动放弃"写入冷却；冷却期内评分 ×0.1，不硬剔除。
- 普通失败、不可达、Goto 超时、停滞不写冷却。
- 同模板连续失败 `TemplateFailLimit` 次只走兜底闲逛，不生成店铺冷却。
- 冷却键是经营实体 ID（认知/经营口径），不使用 `AreaId`；路过/导航仍经空间引用。

### 11.6 需求推进与跨阈

```cpp
UENUM(BlueprintType)
enum class EDynamicValueChangeMode : uint8
{
    TimeDriven, EventOnly, BaselineReturn, EnvironmentDerived
};

UENUM(BlueprintType)
enum class EDemandThresholdDirection : uint8 { Downward, Upward, Both };

USTRUCT(BlueprintType)
struct FDemandThresholdRule
{
    float ThresholdValue = 0.f;
    EDemandThresholdDirection Direction = EDemandThresholdDirection::Downward;
    FGameplayTag TriggerTag;
};

USTRUCT(BlueprintType)
struct FRuntimeDemandRule
{
    FGameplayTag DemandTag;                  // Creature.Demand.*
    EDynamicValueChangeMode ChangeMode = EDynamicValueChangeMode::TimeDriven;
    float ChangeRatePerGameSecond = 0.f;
    float FinalMin = 0.f;
    float FinalMax = 100.f;
    TArray<FDemandThresholdRule> Thresholds;
};

void EmitDemandThresholdCrossings(
    AVoxelIntelligentCreatureInstance* NPC,
    const FRuntimeDemandRule& Rule,
    float OldValue, float NewValue, float AppliedDelta)
{
    TArray<const FDemandThresholdRule*> Crossed;
    for (const FDemandThresholdRule& Th : Rule.Thresholds)
    {
        const bool bDown = OldValue > Th.ThresholdValue && NewValue <= Th.ThresholdValue;
        const bool bUp = OldValue < Th.ThresholdValue && NewValue >= Th.ThresholdValue;
        const bool bMatch =
            (bDown && Th.Direction != EDemandThresholdDirection::Upward) ||
            (bUp && Th.Direction != EDemandThresholdDirection::Downward);
        if (bMatch) Crossed.Add(&Th);
    }

    Crossed.Sort([AppliedDelta](const FDemandThresholdRule& A, const FDemandThresholdRule& B)
    {
        return AppliedDelta < 0.f
            ? A.ThresholdValue > B.ThresholdValue
            : A.ThresholdValue < B.ThresholdValue;
    });

    if (auto* Decision = NPC->FindComponentByClass<UVoxelDecisionComponent>())
        for (const FDemandThresholdRule* Th : Crossed)
            Decision->OnNeedCrossed(Rule.DemandTag, Th->TriggerTag);
}

void ApplyDemandDelta(
    AVoxelIntelligentCreatureInstance* NPC,
    const FRuntimeDemandRule& Rule, float Delta)
{
    auto* Needs = NPC->FindComponentByClass<UVoxelNeedsComponent>();
    if (!Needs || Delta == 0.f) return;

    float* Value = Needs->Current.Find(Rule.DemandTag);
    if (!Value) return;

    const float OldValue = *Value;
    *Value = FMath::Clamp(OldValue + Delta, Rule.FinalMin, Rule.FinalMax);
    EmitDemandThresholdCrossings(NPC, Rule, OldValue, *Value, Delta);
}

// 固定逻辑 tick 输入游戏时间增量
void AdvanceNeeds(AVoxelIntelligentCreatureInstance* NPC, float GameDeltaSeconds)
{
    if (GameDeltaSeconds <= 0.f) return;
    auto* Needs = NPC->FindComponentByClass<UVoxelNeedsComponent>();
    for (const auto& Pair : Needs->Rules)
    {
        if (Pair.Value.ChangeMode != EDynamicValueChangeMode::TimeDriven) continue;
        ApplyDemandDelta(NPC, Pair.Value, Pair.Value.ChangeRatePerGameSecond * GameDeltaSeconds);
    }
}
```

工作区例外：劳动者位于本人 `FWorkAssignment.WorkAreaRef` 对应 Shop 内时，使需求下降的自然增量与工作负向增量按 0 应用；离开恢复；不新增冻结标志。

### 11.7 决策仲裁

```cpp
void UVoxelDecisionComponent::OnTriggerEvent(FGameplayTag Source, FGameplayTag PurposeTag)
{
    UpsertPurposeIntent(Source, PurposeTag);
    TryInterruptExecuteLikeNode(Source, PurposeTag);
    RequestArbitration();
}

void UVoxelDecisionComponent::RequestArbitration()
{
    bArbitrationDirty = true;
    // 同一最外层事件事务结束后统一整理
}

void UVoxelDecisionComponent::ArbitratePendingIntents()
{
    if (!bArbitrationDirty) return;
    bArbitrationDirty = false;

    RemoveCompletedOrInvalidIntents();
    BuildExecutionGroupsBySharedEffectsAndCompatibility();
    SortExecutionGroups(GetCurrentRoleContext());   // Duty 分带 / Free Utility / Forced 覆盖

    DispatchMainSlot();
    DispatchSecondarySlots();
}

void UVoxelDecisionComponent::DispatchMainSlot()
{
    auto* Executor = GetOwner()->FindComponentByClass<UVoxelExecutorComponent>();
    if (!Executor || Executor->HasMainFlow() || PendingExecutionGroups.IsEmpty()) return;

    const FExecutionGroup Head = PendingExecutionGroups[0];
    if (!ValidateGroupPreconditions(Head))
    {
        bArbitrationDirty = true;
        return;
    }

    PendingExecutionGroups.RemoveAt(0);
    Executor->StartFlow(Head, EExecutionSlot::Main);
}
```

- Flow 完成/失败/取消只释放对应槽位；未满足目的回待执行队列并标记仲裁。
- 强制覆盖先 `Interrupt` 并按原子边界结算，再派发；不参与普通队列竞争。

### 11.8 候选收集与评分

```cpp
void UVoxelDecisionComponent::CollectCandidates(
    FGameplayTag PrimaryPurposeTag, TArray<FCandidateContext>& Out)
{
    auto* NPC = GetOwner<AVoxelIntelligentCreatureInstance>();
    const int32 TargetCount = 5;
    const auto& Known = NPC->Customer->KnownShopIds;

    // 候选按经营实体枚举；营业状态、认知、设施索引均以 FBusinessEntityId 为参数查询
    auto ScanBusiness = [&](FBusinessEntityId EntityId)
    {
        // 硬过滤①：营业（派生值经实体查，不取模块指针）
        if (!IsOperating(EntityId)) return;
        // 硬过滤②：认知（经营实体 ID）
        if (!Known.Contains(EntityId)) return;

        for (auto* Facility : GetFacilityIndex(EntityId)->Facilities)
        {
            if (!IsFacilityUsable(Facility)) continue;

            const auto& Profile = GetProfile(Facility->ItemInstance.HandleId);
            const auto* Func = Profile.FindFuncFor(PrimaryPurposeTag);
            if (!Func) continue;

            // 硬过滤③：价格上限（用平均溢价率，不读单品精确价）
            const float ShopPremium = ComputeShopMeanPremiumRate(Shop);
            const float MaxPremium = GetPriceExpectationMax(
                NPC->Customer->Expectations, IsAnyDemandCritical(NPC));
            if (ShopPremium > MaxPremium) return;

            // 硬过滤④：可达
            if (!IsPathReachable(NPC->GetActorLocation(), Facility->GetActorLocation()))
                continue;

            FCandidateContext Ctx;
            Ctx.TargetFacility = Facility;
            Ctx.Shop = Shop;
            Ctx.BusinessEntityId = Shop->GetBusinessEntityId();
            Ctx.AreaRef = Shop->GetAreaRef();          // 可选空间引用，供路过/导航
            Ctx.TargetFuncTag = Func->FuncTag;
            Ctx.TargetLocation = Facility->GetActorLocation();
            Ctx.PrimaryPurposeTag = Profile.GetPrimaryDemandTag(*Func);
            Out.Add(Ctx);
        }
    };

    if (auto* Current = GetCurrentShop(NPC)) ScanShop(Current);
    if (Out.Num() < TargetCount) ScanNearbyShops(NPC, Config->ScanNearbyRadius, ScanShop);
    if (Out.Num() < TargetCount) ScanAllShops(NPC, ScanShop);
}

void UUtilityEvaluator::EvaluateCandidates(
    const AVoxelIntelligentCreatureInstance* NPC,
    TArray<FCandidateContext>& Candidates,
    const UUtilityEvaluatorConfig* Config) const
{
    for (auto& Ctx : Candidates)
    {
        Ctx.NeedScore        = ComputeNeedScore(Ctx, NPC, Config);
        Ctx.QualityScore     = ComputeQualityScore(Ctx, NPC, Config);
        Ctx.WillingnessScore = ComputeWillingnessScore(Ctx, NPC, Config);

        Ctx.ProximityAtten   = ComputeProximityAtten(Ctx, NPC, Config);
        Ctx.QueueFactor      = ComputeQueueFactor(Ctx, NPC, Config);

        Ctx.PersonalityMod   = ComputePersonalityMod(Ctx, NPC, Config);
        Ctx.DutyMod          = ComputeDutyMod(Ctx, NPC, Config);
        Ctx.TimeMod          = ComputeTimeMod(Ctx, NPC, Config);
        Ctx.MemoryMod        = 1.f;
        Ctx.ClienteleMod     = 1.f;

        Ctx.SecondaryFillScore = ComputeSecondaryFillScore(Ctx, NPC, Config);   // 主需求之外、该候选能覆盖的需求

        Ctx.FinalScore =
            (Config->WeightNeed * Ctx.NeedScore
           + Config->WeightQuality * Ctx.QualityScore
           + Config->WeightWillingness * Ctx.WillingnessScore
           + Config->WeightSecondaryFill * Ctx.SecondaryFillScore)
           * Ctx.ProximityAtten * Ctx.QueueFactor
           * Ctx.PersonalityMod * Ctx.DutyMod * Ctx.TimeMod
           * Ctx.MemoryMod * Ctx.ClienteleMod;
    }
}

float UUtilityEvaluator::EstimateWaitTime(
    int32 QueueLength, float UsageDuration, int32 MaxConcurrency)
{
    return QueueLength * UsageDuration / FMath::Max(1, MaxConcurrency);
}

float UUtilityEvaluator::ComputeQualityScore(
    const FCandidateContext& C, const AVoxelIntelligentCreatureInstance* NPC,
    const UUtilityEvaluatorConfig* Config) const
{
    const float Effective = ComputeCompositeEffectiveReputation(
        C.Shop->Reputation, *Config);
    const auto& Profile = GetProfile(C.TargetFacility->ItemInstance.HandleId);
    return Profile.GetStat(Stat.Quality) * Effective;
}
```

`QueueFactor`：用 `EstimateWaitTime` 估算等待，映射到 `Expectations[Service]` 带，低于 Min 趋 0；评分与入队后重估使用同一函数。

### 11.9 工作决策

```cpp
FIntent UVoxelDecisionComponent::DecideAsWorker(AVoxelIntelligentCreatureInstance* NPC)
{
    auto* Labor = NPC->FindComponentByClass<UVoxelLaborComponent>();
    if (!Labor || !Labor->Assignment.bEnabled) return MakeIdleAtPost(NPC);

    auto* WorkShop = GetAreaShop(Labor->Assignment.WorkAreaRef);
    if (!WorkShop) return MakeIdleAtPost(NPC);

    auto Tasks = WorkShop->Board->GetCandidatesFor(NPC);
    if (Tasks.IsEmpty()) return MakeIdleAtPost(NPC);

    // 优先级 0 = 禁止
    Tasks.RemoveAll([&](const FIntent* T)
    {
        return Labor->Assignment.Priorities.FindRef(T->TaskTypeTag) == 0;
    });
    if (Tasks.IsEmpty()) return MakeIdleAtPost(NPC);

    // 临近下班只认领 预估工期 <= 剩余班次时间
    const float RemainingShiftSec = GetRemainingShiftSec(Labor->Assignment);
    Tasks.RemoveAll([&](const FIntent* T)
    {
        return T->EstimatedDurationSec > RemainingShiftSec;
    });
    if (Tasks.IsEmpty()) return MakeIdleAtPost(NPC);

    // Priority 分带：紧急 → 常规 → 低优
    for (const FPriorityBand& Band : GetPriorityBands())
    {
        TArray<FIntent*> InBand = Tasks.FilterByPredicate(
            [&](const FIntent* T) { return Band.Contains(T->Priority); });
        if (InBand.IsEmpty()) continue;

        InBand.Sort([&](const FIntent& A, const FIntent& B)
        {
            const uint8 PA = Labor->Assignment.Priorities.FindRef(A.TaskTypeTag);
            const uint8 PB = Labor->Assignment.Priorities.FindRef(B.TaskTypeTag);
            if (PA != PB) return PA > PB;
            const float DA = GetTaskDistance(NPC, &A);
            const float DB = GetTaskDistance(NPC, &B);
            if (!FMath::IsNearlyEqual(DA, DB)) return DA < DB;
            return A.PushGameSec < B.PushGameSec;
        });

        return MakeWorkIntent(NPC, InBand[0], WorkShop);
    }

    return MakeIdleAtPost(NPC);
}
```

### 11.10 流程推进与保护

```cpp
void UVoxelExecutorComponent::TickFlow(float GameDeltaSeconds)
{
    if (MainFlow.State != EFlowState::Running) return;
    TickCurrentNode(GameDeltaSeconds);
}

void UVoxelExecutorComponent::TickCurrentNode(float GameDeltaSeconds)
{
    const FPlanStep& Node = GetCurrentNode();

    // 节点 = 骨架，五原语段（Goto/Acquire/Advance/Settle/Release）在节点内部通用执行；
    // 节点类型只决定 Advance 段做什么。原 Goto / Reserve / Settle 分支已按 02 空间 §8.2
    // 降为原语，不再是节点类型。
    switch (Node.StepType)
    {
    case EFlowNodeType::Browse:          Advance_Browse(Node); break;
    case EFlowNodeType::QueueAt:         Advance_QueueAt(Node); break;
    case EFlowNodeType::ClaimSeat:       Advance_ClaimSeat(Node); break;
    case EFlowNodeType::Order:           Advance_Order(Node); break;
    case EFlowNodeType::Pay:             Advance_Pay(Node); break;
    case EFlowNodeType::Consume:         Advance_Consume(Node, GameDeltaSeconds); break;
    case EFlowNodeType::UseItem:         Advance_UseItem(Node, GameDeltaSeconds); break;
    case EFlowNodeType::AcquireResource: Advance_AcquireResource(Node); break;
    case EFlowNodeType::Release:         Advance_Release(Node); break;
    }
}

void UVoxelExecutorComponent::Primitive_Goto(FVector Target)
{
    auto* NPC = GetOwner<AVoxelIntelligentCreatureInstance>();
    const int64 Now = GetGameTimeSec();

    if (Now - MainFlow.StageStartGameSec > Config->MoveTimeoutSec)
    {
        FailCurrentNode();
        return;
    }

    const float Moved = FVector::Dist(NPC->GetActorLocation(), MainFlow.LastStagnationPos);
    if (Moved < Config->StagnationEpsilon)
    {
        MainFlow.StagnationAccumSec += GetGameDeltaSeconds();
        if (MainFlow.StagnationAccumSec > Config->StagnationTimeoutSec)
        {
            FailCurrentNode();
            return;
        }
    }
    else
    {
        MainFlow.StagnationAccumSec = 0.f;
        MainFlow.LastStagnationPos = NPC->GetActorLocation();
    }
}

void UVoxelExecutorComponent::PartialSettle(float Progress)
{
    ApplyNodeEffects(NPC, GetCurrentNode().EffectSet, Progress);
    Decision->EvaluatePurposeCompletionAfterEffects(MainFlow.CoveredIntentIds);
    // 支付/取货/所有权由各业务节点的原子事务定义，通用 PartialSettle 不按进度扣款
    MainFlow.AccumulatedReport.Progress = Progress;
}
```

### 11.11 排队离队

```cpp
void UVoxelExecutorComponent::TickQueueAt(const FPlanStep& Node)
{
    auto* NPC = GetOwner<AVoxelIntelligentCreatureInstance>();
    // 待补（任务 D3）：FFlowState 尚无 TargetAreaRef / AccumulatedReport 字段；
    // 当前临时代码以主意图的 Targets 解析所属经营实体，补齐后换成独占字段。
    auto* Shop = GetShop(ResolvePrimaryTarget(Node));
    auto* WorkPoint = Shop->FindWorkPoint(Node.PointTag);
    auto& Queue = WorkPoint->Queue;

    // FIFO：只有队首可进入服务；叫号制还要求已完成叫号
    const bool bCanEnter = Queue.Mode == EQueueMode::Line
        || GetEntry(Queue, MainFlow.ExecutionId).CalledGameSec >= 0;

    if (Queue.IsFront(MainFlow.ExecutionId) && bCanEnter
        && AcquireServiceSlot(WorkPoint, MainFlow.ExecutionId))
    {
        Queue.Remove(MainFlow.ExecutionId, Tag("Shop.Queue.EnteredService"));
        AdvanceNode();
        return;
    }

    const int64 WaitSec = GetGameTimeSec() - GetEntry(Queue, MainFlow.ExecutionId).EnqueueGameSec;
    const float ComfortSlow = 1.f - WorkPoint->Comfort;
    const float WaitExp = MapWaitToExperience(WaitSec, ComfortSlow, IsGuideOnDuty(Shop));

    const FExpectationBand& ServiceBand = NPC->Customer->Expectations[Tag("Aspect.Service")];
    if (WaitExp < ServiceBand.Min)
    {
        MainFlow.State = EFlowState::Failed;
        PublishPresenceChanged(false, MainFlow.ExecutionId, NPC->GetCreatureId(),
            EShopPresenceRole::Customer, Tag("Shop.Queue.WaitLimitExceeded"));

        Shop->ApplyExperienceReport(MakeNegativeReport(Node, WaitSec), -0.5f);
        Decision->CandidateCooldowns.AddCooldown(Shop->GetBusinessEntityId());
        OnFlowFinished.Broadcast(EFlowState::Failed, MainFlow);
    }
}
```

- 等待体验值随游戏时间下降；暂停不增长，倍速加速。
- 只有“期望破底主动放弃”写候选冷却；技术失败/超时不写。

### 11.12 选品、预留、原子收银

```cpp
// 整单放置模拟：在分离的背包快照上模拟，先合并兼容槽，再放空槽
bool TryPlanWholeBasketPlacement(
    int32 BuyerId, const TArray<FStockDeduction>& Deductions,
    FBackpackPlacementPlan& OutPlan)
{
    FBackpackInventory Sim = MakeDetachedBackpackSimulation(BuyerId);
    OutPlan.Lines.Reset();

    for (int32 DeductionIndex = 0; DeductionIndex < Deductions.Num(); ++DeductionIndex)
    {
        const FStockDeduction& Deduction = Deductions[DeductionIndex];
        int32 Remaining = Deduction.Quantity;

        auto Place = [&](int32 SlotIndex)
        {
            auto& Slot = Sim.Slots[SlotIndex];
            const int32 Current = HasItem(Slot) ? GetQuantity(Slot.ItemRef) : 0;
            const int32 Space = GetMaxStackCount(Deduction.LinkedItemId) - Current;
            const int32 Accepted = FMath::Min(Remaining, Space);
            if (Accepted <= 0) return;

            if (IsEmpty(Slot))
                Slot.ItemRef = CreateDetachedStackFrom(Deduction.SourceItemRef, Accepted);
            else
                Slot.ItemRef = MergeIntoDetachedSlot(Slot.ItemRef, Deduction.SourceItemRef, Accepted);

            AddOrIncreasePlacement(OutPlan, DeductionIndex, SlotIndex, Accepted);
            Remaining -= Accepted;
        };

        for (int32 i = 0; i < Sim.Slots.Num() && Remaining > 0; ++i)
            if (CanMergeIntoSlot(Sim.Slots[i], Deduction.SourceItemRef)) Place(i);
        for (int32 i = 0; i < Sim.Slots.Num() && Remaining > 0; ++i)
            if (IsEmpty(Sim.Slots[i])) Place(i);

        if (Remaining > 0)
        {
            OutPlan.Lines.Reset();
            return false;   // 整单不可放，不提交部分计划
        }
    }
    return true;
}
```

`UVoxelBusinessEntitySubsystem::CheckoutSale`（跨域原子事务：Global 预留/订单/钱包/流水 + Scene 库存/背包/设施；若后续膨胀为独立 L4 服务再拆分）：

```cpp
FCheckoutResult UVoxelBusinessEntitySubsystem::CheckoutSale(FBusinessEntityId EntityId, FGuid OrderId)
{
    FCheckoutOrder& Order = GetOrder(EntityId, OrderId);
    const FCheckoutResult Existing = GetCommittedResult(OrderId);
    if (Existing.IsCommitted()) return Existing;

    FCheckoutResult Result;
    ExecuteAtomic(Result, [&]
    {
        Order.State = ECheckoutState::Processing;
        TArray<FStockReservationLine> Lines;
        TArray<FStockDeduction> Deductions;
        FBackpackPlacementPlan Placement;

        if (!ValidateActiveReservation(Order, Lines)
         || !ValidateLockedTotal(Order, Lines)
         || !ValidateAggregatedOnHand(Lines)
         || !ValidateBuyerFunds(Order.BuyerId, Order.AmountDue)
         || !TryPlanFifoStockDeductions(Order.ReservationId, Lines, Deductions)
         || !TryPlanWholeBasketPlacement(Order.BuyerId, Deductions, Placement))
        {
            Result = FCheckoutResult::ValidationFailed();
            ReleaseWholeReservation(Order.ReservationId);
            Order.State = ECheckoutState::Failed;
            return;
        }

        TransferBuyerFundsToShop(Order.BuyerId, Order.AmountDue);
        ConsumeReservation(Order.ReservationId);
        ApplyPlannedStockDeductionsAndDelivery(Order.BuyerId, Deductions, Placement);
        AppendSaleTransaction(Order, Lines);

        Order.State = ECheckoutState::Completed;
        Result = FCheckoutResult::Committed();
        NotifyStockChanged(Order.SellerBusinessEntityId, Deductions);   // 触发阈值评估，见下
    });

    BroadcastCheckoutResultAfterAtomic(OrderId, Result);
    return Result;
}
```

- 预留只保存店铺 + 商品 + 数量 + 锁价；扣货计划在履约时按 FIFO 生成。
- **补货阈值评估是状态驱动的，不是只在收银后**：`NotifyStockChanged` 与「货架放置/移除」「商品目录阈值修改」共用同一个 `EvaluateStockThresholds(BusinessEntityId)`。判定基数是**全仓库合计**——该商品在店铺范围内全部储物设施（可售货架 + 后备储物，不分 `bForSale`）的 OnHand 之和；低于阈值即库存不足：
  - 后备有货 → 发布 `Inventory.StockThresholdCrossed`，由工单索引器派生 `AI.Work.Restock`（从后备搬到可售货架）；
  - 后备为空 → 不生成 Restock，只给「库存不足、需要进货」提醒（V1 不自动采购）。
  
  因此空货架开局、货架新建、上架新商品都会立即评估，不依赖先成交一次。不做周期轮询。
- 背包槽位固定 16，槽内只存 `FItemInstanceId`；容量走 `GetBackpackCapacity()`。
- 失败/取消/离店统一释放整单预留，不部分扣款、不部分交付。
- 商品目录不声明购买效果；食品/饮料的使用效果在实际 `UseItem` 时结算。

### 11.13 认知增长

```cpp
// V1 路过发现：空间上靠近（路过/导航经空间），认知集合记录经营实体 ID
void TryDiscoverShopsOnPassBy(AVoxelIntelligentCreatureInstance* NPC)
{
    for (auto* Shop : GetShopsNear(NPC->GetActorLocation()))
    {
        if (NPC->Customer->KnownShopIds.Contains(Shop->GetBusinessEntityId())) continue;

        const float DiscoverDistance = Shop->GetConfiguredDiscoveryDistance();
        if (DistTo(NPC, Shop) <= DiscoverDistance)
            NPC->Customer->KnownShopIds.Add(Shop->GetBusinessEntityId());
    }
}
```

### 11.14 性能预算

- 图外 NPC 零决策开销；决策事件驱动，不逐帧。
- 单次决策评分 5–10 候选 < 0.1 ms；候选扫描 Layer 1+2 < 0.5 ms。
- 需求推进 1 游戏秒一次；看板按区域分片；索引器增量更新。
- 找活冷却/退避/分帧预算；可达性只对前 K 个候选做短时缓存查询。

### 11.15 蓝图与调试

- `UUtilityEvaluatorConfig`、`UFlowConfigAsset`、家具 `FunctionFragment` 全部 DataAsset/DataTable 驱动。
- 决策组件 `OnTriggerEvent` 可蓝图调用；执行组件 `OnFlowFinished` 可蓝图绑定；日程 `OnScheduleSlotChanged / OnAppointmentTriggered` 可蓝图绑定。
- 调试面板显示：待执行意图、执行组、主/次槽 `FFlowState`、看板工单、认领、需求条；每次决策输出“候选 × 各因子 × 最终分”分解日志。

---

## 12. 约束

1. 禁止裸 Tick、裸 Flow 节点、UI 直接写业务状态。
2. 禁止在区域类型切换时销毁领域组件。
3. 禁止用工单认领代替店铺租约，或反向混用。
4. 禁止把排班、工资、优先级复制到多个记录。
5. 禁止读档重放事件、重摇报价、重算既有结算时点。
6. 禁止引入第二套执行层（StateTree/BehaviorTree/GOAP）。
7. 禁止硬编码背包容量、队列容量、扣负白名单。
