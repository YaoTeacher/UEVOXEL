#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CreatureCoreDefinitions.generated.h"

// =========================================================
// [Enums] 核心枚举
// =========================================================

/** 属性运算符号 */
UENUM(BlueprintType)
enum class EAttributeOp : uint8
{
    Override,   // 覆盖
    Add,        // 加法
    Multiply    // 乘法
};

/** 阈值比较运算符 */
UENUM(BlueprintType)
enum class EThresholdOp : uint8
{
    LessThanOrEqual,    // <=
    GreaterThanOrEqual, // >=
};

/** 特质获取方式 */
UENUM(BlueprintType)
enum class ETraitAcquisitionType : uint8
{
    Both,       
    InnateOnly, 
    AcquiredOnly 
};

/** 身体部位重要性 */
UENUM(BlueprintType)
enum class EBodyPartImportance : uint8
{
    Core,       
    Vital,      
    Standard,   
    Cosmetic    
};

// =========================================================
// [Structs] 基础数值模型
// =========================================================

/** 属性修正器 (Modifier) */
USTRUCT(BlueprintType)
struct FAttributeModifier
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly)
    EAttributeOp Op = EAttributeOp::Add;

    UPROPERTY(EditDefaultsOnly)
    float Value = 0.0f;
};

/** 属性因子 (用于衍生公式) */
USTRUCT(BlueprintType)
struct FAttributeFactor
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Attribute"))
    FGameplayTag SourceAttribute;

    UPROPERTY(EditDefaultsOnly)
    float Coefficient = 1.0f;
};

/** 单条衍生公式定义 */
USTRUCT(BlueprintType)
struct FDerivedAttributeDef
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly)
    float BaseConstant = 0.0f;

    UPROPERTY(EditDefaultsOnly)
    TArray<FAttributeFactor> Factors;
};

// =========================================================
// [Structs] 规则与需求 (Rules & Demands)
// =========================================================

/** 需求阈值规则 (Inner Rule) */
USTRUCT(BlueprintType)
struct FDemandThresholdRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EThresholdOp Operator = EThresholdOp::LessThanOrEqual;

    UPROPERTY(EditDefaultsOnly)
    float ThresholdValue = 20.0f;
    
    UPROPERTY(EditDefaultsOnly)
    TMap<FGameplayTag, FAttributeModifier> EffectModifiers;
};

/** 偏好与需求配置包 (Outer Config) */
USTRUCT(BlueprintType)
struct FPreferenceRuleConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAttributeOp ConfigOp = EAttributeOp::Override;

    UPROPERTY(EditDefaultsOnly)
    float DecayRate = 1.0f;       

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Importance = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Max = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float VetoThreshold = -1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FDemandThresholdRule> ThresholdRules;
};

// =========================================================
// [Structs] 通用判定条件 (Conditions)
// =========================================================

/** 基础 Tag 触发条件 (与/或/非) */
USTRUCT(BlueprintType)
struct FTriggerCondition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer RequiredTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer OneOfTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer ForbiddenTags;
    
    bool IsMet(const FGameplayTagContainer& TargetTags) const
    {
        if (!RequiredTags.IsEmpty() && !TargetTags.HasAll(RequiredTags)) return false;
        if (!ForbiddenTags.IsEmpty() && TargetTags.HasAny(ForbiddenTags)) return false;
        if (!OneOfTags.IsEmpty() && !TargetTags.HasAny(OneOfTags)) return false;
        return true;
    }
};

/** 性格数值范围规则 */
USTRUCT(BlueprintType)
struct FPersonalityRangeRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Creature.PersonalityDim"))
    FGameplayTag DimensionTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EThresholdOp Op = EThresholdOp::GreaterThanOrEqual; // 默认：大于等于某值

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = -1.0, ClampMax = 1.0))
    float Threshold = 0.0f;
    
    bool IsMatch(float CurrentValue) const
    {
        if (Op == EThresholdOp::GreaterThanOrEqual) return CurrentValue >= Threshold;
        if (Op == EThresholdOp::LessThanOrEqual) return CurrentValue <= Threshold;
        return false;
    }
};

/** 复合触发条件集 (Tag + Personality) */
USTRUCT(BlueprintType)
struct FEventTriggerCondition
{
    GENERATED_BODY()

    // 基础 Tag 检查
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTriggerCondition TagCondition;

    // 性格数值检查
    UPROPERTY(EditDefaultsOnly, Category = "Personality", meta = (Categories = "Creature.PersonalityDim"))
    TArray<FPersonalityRangeRule> PersonalityRequirements;
};