#pragma once

#include "CoreMinimal.h"
#include "CreatureDataDefinitions.h"
#include "CreatureEventDefinitions.h"
#include "GameplayTagContainer.h"
#include "CreatureBase.generated.h"

// ==========================================
// [Sub-Struct] 基础组件
// ==========================================

/** 物品栏单项 */
USTRUCT(BlueprintType)
struct FInventoryItemData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTag ItemID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Count = 1;
};

/** 社交记忆单元 */
USTRUCT(BlueprintType)
struct FSocialMemory
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    int32 TargetID = -1;

    // 关系亲密度 (-100 ~ 100)
    UPROPERTY(VisibleAnywhere)
    float Affinity = 0.0f;

    // 记忆标签 (e.g. "Generous", "AttackedMe")
    UPROPERTY(VisibleAnywhere)
    FGameplayTagContainer MemoryTags;
};

/** 运行时装备槽 */
USTRUCT(BlueprintType)
struct FRuntimeEquipmentSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTag SlotTag; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTag ItemID;  
};

USTRUCT(BlueprintType)
struct FLifeHistoryEntry
{
    GENERATED_BODY()

    // 发生时的年龄
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly)
    int32 OccuredAge = 0;

    // 关联的事件资产 (Soft Pointer 用于运行时加载显示详细信息)
    // 注意：类型改为 UCreatureLifeEvent
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UCreatureEvent> SourceEvent; 

    // 资产 ID (用于 AssetManager 查找)
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly)
    FPrimaryAssetId EventId;

    // [新增] 当时做出的选择索引 (对应 Event->Choices 数组的下标)
    // -1 代表走了 DefaultOutcome (没选或没得选)
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly)
    int32 SelectedChoiceIndex = -1;

    // [新增] 结果快照文本
    // 为什么要存文本？因为策划可能会修改 Event 表里的文案。
    // 如果不存快照，读档时可能发现"捡钱包"变成了"捡炸弹"，历史就崩了。
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly)
    FText SnapshotOutcomeText;

    // [保留] 当时获得的特质 (方便 UI 快速显示图标，不用加载资产)
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly)
    FGameplayTag GainedTraitTag;
    
    // (可选) 如果你甚至不想存 Tag，可以在 UI 显示时去 SourceEvent 里查 GrantTrait->TraitTag
    // 但存下来比较安全，防止 Event 资产被修改后 Tag 变了
};

// =========================================================
// [Sub-Struct] 视觉与身体状态
// =========================================================

/** 单个部位的运行时状态 
 * 描述：记录血量、断裂状态以及加载的 Mesh 引用
 */
USTRUCT(BlueprintType)
struct FRuntimeBodyPartInstance
{
    GENERATED_BODY()

    // =========================================================
    // 1. 存档数据 (SaveGame) - 决定了"它长什么样"
    // =========================================================

    // [核心] 原始定义资产的软引用
    // 必须保存！否则读档时我们只知道 ID 是 "Red_Coat"，但不知道是人类的还是兽人的。
    // 使用 SoftPtr 是为了延迟加载和安全存档。
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
    TSoftObjectPtr<UBodyPartDefinition> Definition;

    // [核心] 选中的变体 ID (对应 UBodyPartDefinition::Variants Map 的 Key)
    // 例如: "Leather_Armor_01", "Cyber_Arm_T2"
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
    FName SelectedVariantId;

    // =========================================================
    // 2. 游戏状态 (SaveGame) - 决定了"它状态如何"
    // =========================================================

    // 当前健康值 (部位血量)
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadWrite, Category = "Status")
    float CurrentHealth = 100.0f;

    // 状态：是否已断裂/缺失 (如果为 true，组装时会跳过生成，实现断肢)
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadWrite, Category = "Status")
    bool bIsSevered = false;
    
    // 状态：是否是义肢 (影响治疗逻辑：维修 vs 治疗)
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadWrite, Category = "Status")
    bool bIsProsthetic = false;

    // 装备槽位数据 (保留你原本的逻辑，用于记录戒指/手套插槽内容)
    // UPROPERTY(SaveGame, VisibleAnywhere, Category = "Equipment")
    // TArray<FRuntimeEquipmentSlot> EquipmentSlots; 

    // =========================================================
    // 3. 运行时缓存 (Transient) - 不存盘，仅内存使用
    // =========================================================

    // [Runtime Only] 生成出来的组件指针
    // 作用：让子节点能找到父节点。例如 'Hand' 生成时，需要拿到 'Arm' 的这个指针来 Attach。
    // 标记 Transient 防止被序列化进存档（指针重启后会变）。
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Runtime")
    USkeletalMeshComponent* BodyPartComponent = nullptr;

    // [可选] 缓存加载后的 Mesh 资源引用
    // 作用：防止软引用加载后被过早 GC，或者方便快速访问。
    UPROPERTY(Transient)
    USkeletalMesh* LoadedMesh = nullptr;
};

/** 视觉基因 (Visual DNA)
 * 描述：存档/生成专用。记录"决定"（选了哪个鼻子），不存动态指针。
 */
USTRUCT(BlueprintType)
struct FCreatureBodyVisual
{
    GENERATED_BODY()

    // 基础架构 (Asset Path 方便存档)
    UPROPERTY(SaveGame, VisibleAnywhere)
    TSoftObjectPtr<UBodyTypeDefinition> BodyType;

    // // 部位选型 (Key: PartSlot, Value: VariantTag)
    // UPROPERTY(SaveGame, VisibleAnywhere)
    // TMap<FGameplayTag, FGameplayTag> PartSelections;

    // 身体实时状态 (包含血量)
    UPROPERTY(VisibleAnywhere)
    TMap<FGameplayTag, FRuntimeBodyPartInstance> BodyState;
};

/** 完整的生物外观运行时 
 * 描述：挂载在 Actor 上，管理 Mesh 的加载和显示
 */
USTRUCT(BlueprintType)
struct FCreatureAppearanceRuntime
{
    GENERATED_BODY()

    // 所有的生效部位 (Key: PartSlot Tag)
    UPROPERTY(VisibleAnywhere)
    TMap<FGameplayTag, FRuntimeBodyPartInstance> ActiveBodyParts;

    // 渲染脏标记 (True = 需要重新 Merge Mesh)
    UPROPERTY(Transient)
    bool bRenderDirty = true;
};

// =========================================================
// [Runtime Cache] 规则预计算结构
// =========================================================

/** 运行时需求规则缓存
 * 描述：将 Race + Job + Traits 的所有 DemandConfig 合并后的结果。
 * 作用：极大地优化 Tick 性能，无需每帧查 Asset。
 */
USTRUCT(BlueprintType)
struct FRuntimeDemandRule
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float FinalMax = 1.0f;

    // 最终衰减速率 (Race.Decay * Trait.Decay ...)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float FinalDecayRate = 1.0f;

    // 最终重要性 (AI Priority)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float FinalImportance = 1.0f;

    // 预合并的触发规则列表 (Flattened Triggers)
    // Tick 中直接遍历此数组，判断 (CurrentVal <= Rule.Threshold)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FDemandThresholdRule> MergedThresholdRules;
};

// =========================================================
// [Model 1] 客观真值 (The Truth)
// 说明：服务器/逻辑层的权威数据，用于存档。
// =========================================================
USTRUCT(BlueprintType)
struct FTrueCreatureData
{
    GENERATED_BODY()

    // --- 唯一标识 ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CreatureID = 0; 

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FString RealName;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 HomeAreaID = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 SpawnAreaID = -1;

    // --- 核心身份 ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGameplayTag RaceTag;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGameplayTag ProfessionTag;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGameplayTagContainer TraitTags;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGameplayTag GenderTag;
    
    //当前生理年龄 (决定特质槽位上限)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CurrentAge = 0;

    //人生履历 (按时间顺序排列)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FLifeHistoryEntry> LifeHistory;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FCreatureBodyVisual BodyVisual;

    // --- 物品与资产 ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TMap<FGameplayTag, FRuntimeEquipmentSlot> Equipment;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FInventoryItemData> Inventory;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FSocialMemory> SocialMemories;
};

// =========================================================
// [Model 2] 主观认知 (The Perception)
// 说明：UI/客户端层使用，处理"脸盲"和"侦探"玩法。
// =========================================================
USTRUCT(BlueprintType)
struct FPerceivedCreatureData
{
    GENERATED_BODY()

    // 熟悉度 (0.0 - 100.0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FamiliarityLevel = 0.0f;

    // 玩家备注
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CustomNote;
    
    // 已识破的 Tag
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer KnownTags;

    // 视觉线索文本缓存
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FText> VisualClues;
};

// =========================================================
// [Model 3] 运行时缓存 (Runtime Cache)
// 说明：高频访问数据，不存档，由 TrueData + Time 计算得出。
// =========================================================
USTRUCT(BlueprintType)
struct FCreatureRuntimeState
{
    GENERATED_BODY()
    
    // [Type A + Type B] = 基础属性池
    // 这里存储的是：(种族 + 职业 + 特质) + (存档里的 AttributeVariance)
    // 作用：作为计算衍生属性的"纯净原料"。
    UPROPERTY(VisibleAnywhere)
    TMap<FGameplayTag, float> CachedBaseAttributes;

    // [Type C] = 最终静态面板
    // 这里存储的是：GlobalFormulas(CachedBaseAttributes)
    // 作用：提供 Health, Load, AttackPower 等上限值/固定值供 UI 和战斗读取。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TMap<FGameplayTag, float> FinalAttributes;

    // [Type D] = 动态实时值
    // 这里存储的是：当前血量, 当前饥饿。
    // 作用：Tick 中高频修改。
    UPROPERTY(VisibleAnywhere)
    TMap<FGameplayTag, float> Dynamics;

    // 预合并的需求规则缓存 (Decay + Thresholds)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TMap<FGameplayTag, FRuntimeDemandRule> CachedDemandRules;
    
    // --- AI 状态 ---
    UPROPERTY(Transient)
    TMap<FGameplayTag, int32> GOAPWorldState;
};

// =========================================================
// [Model 4] 存档数据 (Save Data)
// 说明：用于持久化存储，只存变化的和关键的。
// =========================================================
USTRUCT(BlueprintType)
struct FCreatureSaveData
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, VisibleAnywhere)
    FTrueCreatureData CreatureData;

    //认知状态
    UPROPERTY(SaveGame, VisibleAnywhere)
    FPerceivedCreatureData Perception;

    //
    UPROPERTY(SaveGame, VisibleAnywhere)
    TMap<FGameplayTag, float> PersistentBasicValues;
    
    // 动态数值 (Current Demands)
    UPROPERTY(SaveGame, VisibleAnywhere)
    TMap<FGameplayTag, float> PersistentDynamicValues;
    
};