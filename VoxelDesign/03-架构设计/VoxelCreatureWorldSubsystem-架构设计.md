# VoxelCreatureWorldSubsystem-架构设计

**定位**: 名册驱动的生物生命周期管理；生物实体是组件容器，经营与 AI 数据在组件中。

---

## 1. 职责

- 维护固定名册（30–50 人），CreatureID 终身稳定。
- 按名册点名生成、回收生物实例；对象池只复用 Actor，不复用身份。
- 提供按 CreatureID 的查询与名字解析（图内图外都可解析）。
- 不承担需求、决策、劳动、经济等业务逻辑；这些在生物组件与对应子系统中。

**不负责**: 需求推进、决策、工单、雇佣、钱包、日程。

---

## 2. 名册模型

```cpp
USTRUCT()
struct FRosterRecord
{
    int32 CreatureID = INDEX_NONE;
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
```

规则：

1. 新游戏开局一次性生成名册；画像、种族、职业、特质、CreatureID 在此确定。
2. CreatureID 在名册创建时分配，终身不变；对象池复用 Actor 不改变 CreatureID。
3. 图外名册数据完整保存；入场时用名册数据初始化 Actor，退场时回收 Actor。
4. 名册是唯一身份来源；任何系统不得用 Actor 指针作为跨系统引用。

---

## 3. 生成与回收

```cpp
class UVoxelCreatureWorldSubsystem : public UWorldSubsystem
{
public:
    const TMap<int32, FRosterRecord>& GetRoster() const;

    AVoxelIntelligentCreatureInstance* SpawnFromRoster(int32 CreatureId);
    void RecycleToRoster(int32 CreatureId, ERecycleReason Reason);

    AVoxelNpcBaseInstance* GetCreatureById(int32 CreatureId) const;
    bool IsInMap(int32 CreatureId) const;
    TArray<int32> GetAllCreatureIds() const;

    FText ResolveCreatureName(int32 CreatureId) const;
};
```

- `SpawnFromRoster`：从 `NpcPool` 获取 Actor → 用名册记录初始化 → 注册到 `InSceneCreatureList` → 设置出生点与朝向。
- `RecycleToRoster`：把 Actor 当前状态回写到名册 → 从 `InSceneCreatureList` 注销 → `Pool->Release`。
- `CreatureNumLimit` 只作对象池容量与同屏上限，不再决定人口数量。
- 取消定时器随机生成路径；入场由日程/事件驱动，退场由流程/日程驱动。
- 名字解析同时查在场列表与名册，图外也能返回名字。

---

## 4. 生物实体与组件

```text
AVoxelNpcBaseInstance                       // 基类：身份、身体、生命周期
 └─ AVoxelIntelligentCreatureInstance       // V1 统一智能生物
      ├─ UVoxelLaborComponent
      ├─ UVoxelNeedsComponent
      ├─ UVoxelScheduleComponent
      ├─ UVoxelDecisionComponent
      ├─ UVoxelExecutorComponent
      ├─ UVoxelMemoryComponent
      ├─ UVoxelCustomerComponent
      └─ UVoxelAccountRefComponent
```

- 组件在名册实例化（或首次生成）时一次性创建，之后只做状态重置。
- 组件 `BeginPlay` 注册到对应子系统，`EndPlay` 注销。
- 基类保持最小，不挂经营组件；未来其他生物类型可直接继承基类。
- 取消 `UConsumComponent`，需求统一走 `UVoxelNeedsComponent`。
- 取消 `LiveTime / MaxLiveTime / UpdateLiveTime` 自动 despawn；退场由日程与流程决定。

---

## 5. 数据模型

### 5.1 身份与画像

```cpp
USTRUCT()
struct FTrueCreatureData
{
    int32 CreatureID = 0;
    FString RealName;
    int32 HomeAreaID = -1;
    int32 SpawnAreaID = -1;
    FGameplayTag RaceTag;
    FGameplayTag ProfessionTag;
    FGameplayTagContainer TraitTags;
    FGameplayTag GenderTag;
    int32 CurrentAge = 0;
    TArray<FLifeHistoryEntry> LifeHistory;
    FCreatureBodyVisual BodyVisual;
    TMap<FGameplayTag, FRuntimeEquipmentSlot> Equipment;
    TArray<FItemInstanceId> BackpackSlots;   // 固定 16 槽，容量由 GetBackpackCapacity() 返回
    TArray<FSocialMemory> SocialMemories;
};
```

### 5.2 运行时缓存（不入档）

```cpp
USTRUCT()
struct FCreatureRuntimeState
{
    TMap<FGameplayTag, float> CachedBaseAttributes;
    TMap<FGameplayTag, float> FinalAttributes;
    TMap<FGameplayTag, FRuntimeDemandRule> CachedDemandRules;
};
```

- `Dynamics` 迁入 `UVoxelNeedsComponent`。

### 5.3 存档记录

生物存档记录 = 名册记录（`FRosterRecord`），包含身份、认知与各组件载荷；不再单独维护一份 `FCreatureSaveData` 平铺结构。

---

## 6. 生命周期

```text
新游戏：生成名册 → 分配 CreatureID → 写入名册
入场：日程判据/事件 → SpawnFromRoster → Actor 初始化 → 组件注册
在场：需求/决策/执行由组件与子系统推进
退场：流程完成/日程结束 → 状态回写名册 → RecycleToRoster → 池回收
读档：恢复名册 → 按 bInMap 生成在场实例 → 恢复组件载荷 → 重建索引
```

- 对象池 `ResetFunc` 必须重置全部组件状态，避免状态泄漏。
- 读档不重放需求、进店、成交事件。

---

## 7. 性能

- 名册查询 O(1)（`TMap<int32, FRosterRecord>`）。
- 在场查询 O(1)（`TMap<int32, TWeakObjectPtr<AVoxelNpcBaseInstance>>`）。
- 生成/回收由事件驱动，不轮询。
- 需求推进 1 游戏秒一次，由需求组件自计游戏时间。
- 对象池容量 = 同屏上限 + 2 缓冲。

---

## 8. 与经营链的关系

| 模块 | 关系 |
|---|---|
| 劳动子系统 | 雇佣合同在 GameInstance（跨场景）；劳动安排数据在 `UVoxelLaborComponent`，随场景名册存档 |
| 区域/Shop | 顾客进店/离店事件；区域只引用 CreatureID |
| 经济 | 钱包 ID 在 `UVoxelAccountRefComponent`；工资/消费按 CreatureID 结算 |
| 日程/决策 | 组件挂在生物上，由生物子系统保证生命周期 |
| 存档 | 名册是生物数据的唯一存档根 |

---

## 9. 对象池与出生点实现

### 9.1 对象池

```text
池名称：NpcPool
缓冲大小：CreatureNumLimit + 2
创建位置：FVector(0, 0, -100000)（隐藏区）
CreateFunc：SpawnActor<AVoxelNpcBaseInstance>（隐藏位置，关闭碰撞与移动）
ResetFunc：ResetCreature
```

- `Acquire` 后设置世界位置、激活组件、注册到 `InSceneCreatureList`。
- `Release` 前回写名册状态、注销组件、关闭物理与可见性、回到隐藏位置。
- `ResetCreature` 必须覆盖：需求、意图、执行 Flow、日程、记忆、顾客认知、账户引用、劳动安排缓存。
- 对象池只复用 Actor，不复用 CreatureID。

### 9.2 出生点

```cpp
FTransform UVoxelCreatureWorldSubsystem::MakeSpawnTransform(int32 SpawnAreaId) const
{
    auto* AreaSys = GetWorld()->GetSubsystem<UVoxelAreaWorldSubsystem>();

    AVoxelAreaBaseInstance* Area = AreaSys->GetAreaActor(SpawnAreaId);
    if (!Area)
        Area = PickRandomSpawnArea();          // 名册记录的出生区无效时回退

    const FBox Bounds = Area->GetAreaBounds();
    const FVector Pos = GetRandomPositionInArea(
        FIntVector(Bounds.Min), FIntVector(Bounds.Max));

    FRotator Rot = FRotator::ZeroRotator;
    if (auto* SpawnComp = Area->SpawnAreaComponent)
        Rot = SpawnComp->GetSpawnRotation();

    return FTransform(Rot, Pos, FVector::OneVector);
}

FVector UVoxelCreatureWorldSubsystem::GetRandomPositionInArea(FIntVector Min, FIntVector Max)
{
    return FVector(
        FMath::RandRange(Min.X, Max.X),
        FMath::RandRange(Min.Y, Max.Y),
        FMath::RandRange(Min.Z, Max.Z));
}
```

- 出生区优先用名册记录的 `SpawnAreaID`；无效时回退到随机可用出生区。
- 落地由角色移动组件/贴地逻辑处理，不在出生函数里做射线。
- 模板编辑等无游戏场景不生成名册实例。

### 9.3 名字解析

```cpp
FText UVoxelCreatureWorldSubsystem::ResolveCreatureName(int32 CreatureId) const
{
    if (CreatureId == 0) return FText::FromString(TEXT("系统角色"));
    if (CreatureId < 0) return FText::FromString(TEXT("未指派"));
    if (CreatureId == GetMainCharacterId()) return FText::FromString(TEXT("玩家"));

    if (const FRosterRecord* Record = Roster.Find(CreatureId))
        return FText::FromString(Record->True.RealName);

    return FText::FromString(FString::Printf(TEXT("#%d"), CreatureId));
}
```

### 9.4 回收流程

```text
RecycleToRoster(CreatureId)
  → 强引用 Pin 在场实例
  → 组件状态回写名册（需求/日程/Flow/记忆/账户/劳动）
  → 组件 EndPlay 注销子系统
  → InSceneCreatureList.Remove
  → Pool->Release（ResetFunc 清状态、隐藏、关物理）
  → 名册 bInMap=false
```

---

## 10. 相关文档

- [经营与 AI-架构设计](/VoxelDesign/03-架构设计/AI决策系统-架构设计.md)
- [AI决策系统-NPC个体与决策-系统设计](/VoxelDesign/02-系统设计/AI决策系统-NPC个体与决策-系统设计.md)
