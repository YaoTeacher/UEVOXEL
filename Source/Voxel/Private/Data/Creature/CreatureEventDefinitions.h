#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CreatureCoreDefinitions.h" // 依赖核心
#include "Core/CoreTags.h"
#include "CreatureEventDefinitions.generated.h"

// =========================================================
// [Layer 3] 结果定义 (Outcome) - 多态系统
// =========================================================

/** 结果基类 (Abstract) */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, BlueprintType)
class UEventOutcomeBase : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common")
    FText ResultDescription; // 结果描述文本

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common")
    FEventTriggerCondition ExtraCondition; // 额外的随机/条件判定

    // 运行时类型识别 Tag
    virtual FGameplayTag GetOutcomeTypeTag() const { return FGameplayTag::EmptyTag; }
};

/** 类型 A: 瞬间结算 (属性/特质) */
UCLASS(DisplayName = "Outcome: Instant Effect")
class UEventOutcome_Instant : public UEventOutcomeBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Reward")
    TArray<FGameplayTag> GrantTraits; // 获得的特质 Tag

    UPROPERTY(EditAnywhere, Category = "Reward", meta = (Categories = "Creature.PersonalityDim"))
    TMap<FGameplayTag, FAttributeModifier> PersonalityShift; // 性格偏移

    UPROPERTY(EditAnywhere, Category = "Reward")
    TMap<FGameplayTag, FAttributeModifier> AttributeModifiers; // 属性直接修正

    virtual FGameplayTag GetOutcomeTypeTag() const override 
    { return FCoreTags::Effect_Instant; }
};

/** 类型 B: 触发任务 (NPC Self) */
UCLASS(DisplayName = "Outcome: Start Task (NPC)")
class UEventOutcome_Task : public UEventOutcomeBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
    FPrimaryAssetId TaskId; // 任务资产 ID

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
    FString TaskParams;

    virtual FGameplayTag GetOutcomeTypeTag() const override 
    { return FCoreTags::Effect_Task_Self; }
};

/** 类型 C: 发布任务 (Player Quest) */
UCLASS(DisplayName = "Outcome: Give Quest (Player)")
class UEventOutcome_Quest : public UEventOutcomeBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FPrimaryAssetId QuestId;

    virtual FGameplayTag GetOutcomeTypeTag() const override 
    { return FCoreTags::Effect_Task_Player; }
};

/** 类型 D: 事件链 (Follow Up) */
UCLASS(DisplayName = "Outcome: Trigger Chain")
class UEventOutcome_Chain : public UEventOutcomeBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Chain")
    TSoftObjectPtr<class UCreatureEvent> NextEvent;

    UPROPERTY(EditAnywhere, Category = "Chain")
    bool bImmediate = false;

    virtual FGameplayTag GetOutcomeTypeTag() const override 
    { return FCoreTags::Effect_Chain; }
};

// =========================================================
// [Layer 2] 选项定义 (Choice)
// =========================================================

USTRUCT(BlueprintType)
struct FEventChoice
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText ButtonText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEventTriggerCondition VisibilityCondition; // 可见性条件

    // 支持多个潜在结果 (按概率或条件顺次执行)
    UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite)
    UEventOutcomeBase* Outcome;
};

// =========================================================
// [Layer 1] 事件定义 (Event)
// =========================================================

UCLASS(BlueprintType)
class UCreatureEvent : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category = "Editor")
    FString DevComment;

    // --- 触发筛选 ---
    UPROPERTY(EditDefaultsOnly, Category = "Trigger")
    int32 MinAge = 0;
    UPROPERTY(EditDefaultsOnly, Category = "Trigger")
    int32 MaxAge = 100;

    UPROPERTY(EditDefaultsOnly, Category = "Trigger")
    bool bOnlyOnce = true;

    UPROPERTY(EditDefaultsOnly, Category = "Trigger")
    int32 Priority = 0;

    UPROPERTY(EditDefaultsOnly, Category = "Trigger")
    float BaseWeight = 1.0f;

    // 通用 Tag 检查 (种族、职业、特质等都放在这里)
    UPROPERTY(EditDefaultsOnly, Category = "Trigger")
    FTriggerCondition RequiredConditions;

    // --- 事件内容 ---
    UPROPERTY(EditDefaultsOnly, Category = "Content")
    TArray<FEventChoice> Choices;

    // 默认结果
    UPROPERTY(EditDefaultsOnly, Instanced, Category = "Content")
    UEventOutcomeBase* DefaultOutcome;

    // Asset ID Helper
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId("CreatureLifeEvent", GetFName());
    }
};