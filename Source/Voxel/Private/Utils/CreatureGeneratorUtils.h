// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

// 引入拆分后的基础数据结构
#include "Data/Creature/CreatureCoreDefinitions.h"
#include "Data/Creature/CreatureDataDefinitions.h"
#include "Data/Creature/CreatureEventDefinitions.h"
#include "Data/Creature/CreatureBase.h"

#include "CreatureGeneratorUtils.generated.h"

// 前置声明
class UConfigDataSubsystem;

// =========================================================
// [Structs] 辅助结构
// =========================================================

/** * 事件解析结果包 
 * 用于在 Resolve 和 Apply 之间传递上下文
 */
struct FEventResolutionResult
{
    // 最终选定的结果逻辑对象
    UEventOutcomeBase* Outcome = nullptr;
    
    // 玩家/NPC 选择的选项索引 (-1 代表默认结果/无选项)
    int32 ChoiceIndex = -1;
};

// ... FCreatureGenerationSettings 和 FCreatureGenerationResult 保持不变 ...
USTRUCT(BlueprintType)
struct FCreatureGenerationSettings
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UCreatureRaceTemplate* ForcedRace = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UCreatureProfessionTemplate* ForcedProfession = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<UCreatureTraitDefinition*> ForcedTraits;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RandomSeed = 0;
};

USTRUCT(BlueprintType)
struct FCreatureGenerationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FTrueCreatureData TrueData;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FCreatureRuntimeState InitialState;

    UPROPERTY(BlueprintReadWrite)
    bool bIsValid = false;
};

/**
 * 生物生成与数据计算工具库 (重构版)
 */
UCLASS()
class UCreatureGeneratorUtils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // =========================================================
    // [Public API] 核心生成与恢复
    // =========================================================

    UFUNCTION(BlueprintCallable, Category = "Creature|Generation", meta = (WorldContext = "WorldContextObject"))
    static FCreatureGenerationResult GenerateCreatureData(const UObject* WorldContextObject,
                                                   const FCreatureGenerationSettings& Settings);
    /** * 生成全新的角色存档数据 
     * 包含：身份、履历、以及"不可复现"的随机基础值(PersistentBasicValues)
     */
    UFUNCTION(BlueprintCallable, Category = "Creature|Generation", meta = (WorldContext = "WorldContextObject"))
    static FCreatureSaveData GenerateNewCreatureSaveData(
        const UObject* WorldContextObject, 
        const FCreatureGenerationSettings& Settings
    );

    /**
     * 从存档恢复运行时状态 (Rebuild)
     * 逻辑：PersistentBasicValues -> CachedBaseAttributes -> FinalAttributes -> Dynamics
     */
    UFUNCTION(BlueprintCallable, Category = "Creature|Stats", meta = (WorldContext = "WorldContextObject"))
    static void RestoreRuntimeFromSaveData(
        const FCreatureSaveData& SaveData, 
        UPARAM(ref) FCreatureRuntimeState& OutState, 
        const UObject* WorldContextObject
    );

    // =========================================================
    // [Public API] 属性更新
    // =========================================================

    /** 更新动态属性 (Tick Update) */
    UFUNCTION(BlueprintCallable, Category = "Creature|Stats")
    static void UpdateDynamicAttributes(
        UPARAM(ref) FCreatureRuntimeState& OutState,
        UConfigDataSubsystem* ConfigSubsystem
    );

    // =========================================================
    // [Public API] UI 辅助
    // =========================================================

    UFUNCTION(BlueprintCallable, Category = "Creature|UI")
    static FText ResolveHistoryText(
        const FLifeHistoryEntry& Entry,
        const FString& CreatureName,
        UConfigDataSubsystem* Config
    );

private:
    // =========================================================
    // [Private] 内部逻辑
    // =========================================================
    static FGameplayTag GenerateGender(const UCreatureRaceTemplate* Race, FRandomStream& Stream);
    static void GenerateBackstory(
        FTrueCreatureData& OutData,
        TMap<FGameplayTag, float>& OutBasicValues, // 需要传入 BasicValues 以便事件修改永久属性
        int32 TargetAge,
        UConfigDataSubsystem* Config,
        FRandomStream& Stream,
        const UCreatureRaceTemplate* Race,
        const UCreatureProfessionTemplate* Profession
    );

    static void GenerateTraits(
        FTrueCreatureData& OutData,
        const UCreatureRaceTemplate* Race,
        const UCreatureProfessionTemplate* Job,
        const TArray<UCreatureTraitDefinition*>& ForcedTraits,
        const TMap<FGameplayTag, UCreatureTraitDefinition*>& TraitPool,
        FRandomStream& Stream,
        UConfigDataSubsystem* ConfigSubsystem
    );

    static void GenerateBodyVisual(FTrueCreatureData& OutData, const UCreatureRaceTemplate* Race);

    // --- 事件判定 ---
    static bool CheckAllConditions(const FTrueCreatureData& Data, const TMap<FGameplayTag, float>& BasicValues, int32 Age, const UCreatureEvent* Evt);
    static bool CheckTriggerCondition(const FTrueCreatureData& Data, const TMap<FGameplayTag, float>& BasicValues, const FEventTriggerCondition& Condition);
    static FEventResolutionResult ResolveEventOutcome(const FTrueCreatureData& Data, const TMap<FGameplayTag, float>& BasicValues, const UCreatureEvent* Event, FRandomStream& Stream);
    
    static void ApplyEventOutcome(
        FTrueCreatureData& OutData,
        TMap<FGameplayTag, float>& OutBasicValues,
        const UCreatureEvent* SourceEvent,
        const FEventResolutionResult& Resolution,
        int32 Age,
        UConfigDataSubsystem* Config
    );

    // --- 数值计算辅助 ---
    static void AccumulateAttributeConfigs(TMap<FGameplayTag, float>& OutAttrs, const TMap<FGameplayTag, FAttributeModifier>& Configs);
    static void ApplyAttributeModifier(TMap<FGameplayTag, float>& AttributeMap, FGameplayTag Target, const FAttributeModifier& Modifier);
    static void AccumulateDemandConfigs(TMap<FGameplayTag, FRuntimeDemandRule>& OutRules, const TMap<FGameplayTag, FPreferenceRuleConfig>& Configs);
    static void MergeDemandConfigToCache(const FPreferenceRuleConfig& Config, FRuntimeDemandRule& OutRule);
    static void ProcessBodyParts_AttributesAndRules(const FTrueCreatureData& Data, TMap<FGameplayTag, float>& OutAttrs, TMap<FGameplayTag, FRuntimeDemandRule>& OutRules);
    static void CalculateDerivedStats(TMap<FGameplayTag, float>& TargetAttrs, const TMap<FGameplayTag, FDerivedAttributeDef>& Formulas);
};