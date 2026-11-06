#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "CreatureCoreDefinitions.h" // 依赖核心
#include "CreatureDataDefinitions.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UAnimInstance;
class UTexture2D;
class UGOAPAction; // 前置声明

// =========================================================
// [Body] 身体与外观
// =========================================================
UENUM(BlueprintType)
enum class EBodyPartMountType : uint8
{
    MasterPose,       // 紧身/蒙皮 (绝大多数人体部件用这个)
    SocketAttachment  // 挂点 (挂在背后的武器、翅膀用这个)
};

/** 部位变体 */
USTRUCT(BlueprintType)
struct FBodyPartVariant
{
    GENERATED_BODY()
    // [核心] 模型文件
    // 必须是与主骨骼共享骨架的 Skeletal Mesh
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
    TSoftObjectPtr<USkeletalMesh> MeshAsset;
    // 材质覆盖 (可选)
    // 用于换肤色或换衣服材质。如果为空，则使用 Mesh 自带的材质。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
    TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;

    UPROPERTY(EditDefaultsOnly, Category = "Identity", meta = (Categories = "Rarity"))
    FGameplayTag RarityTag;
    
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Visual.Type"))
    FGameplayTag PartTypeTag;

    UPROPERTY(EditDefaultsOnly, Category = "Stats")
    float MaxHealth = 50.0f;

    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Attribute"))
    TMap<FGameplayTag, FAttributeModifier> AttributeModifiers;

    UPROPERTY(EditDefaultsOnly, Category = "Stats")
    TMap<FGameplayTag, FPreferenceRuleConfig> DemandModifiers;

    UPROPERTY(EditDefaultsOnly, Category = "Randomization")
    float SpawnWeight = 1.0f;
    
    UPROPERTY(EditDefaultsOnly, Category = "Conditions")
    FTriggerCondition RequiredTraits;
};

/** 身体部位定义 */
UCLASS(BlueprintType)
class UBodyPartDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Body.Part"))
    FGameplayTag PartTag;
    // 兼容性标签 (可选)
    // 用于防止把兽人的头装在人类身上
    // e.g., Race.Human, Gender.Male
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FTriggerCondition CompatibilityTags;
    // 挂载方式
    // 对于你的8个人体部件，这里统统选 MasterPose
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
    EBodyPartMountType MountType = EBodyPartMountType::MasterPose;
    
    UPROPERTY(EditDefaultsOnly, Category = "Equipment")
    TArray<FGameplayTag> ProvidedEquipmentSlots;
    
    UPROPERTY(EditDefaultsOnly, Category = "Visuals")
    TMap<FName, FBodyPartVariant> Variants;
};

/** 身体构成规则 */
USTRUCT(BlueprintType)
struct FBodyStructureEntry
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Body.Part"))
    FGameplayTag ParentSlot;

    UPROPERTY(EditDefaultsOnly, Category = "Structure")
    EBodyPartImportance Importance = EBodyPartImportance::Standard;
    
    UPROPERTY(EditDefaultsOnly, Category = "Data")
    TSoftObjectPtr<UBodyPartDefinition> PartDefinition;

    UPROPERTY(EditDefaultsOnly, Category = "Identity", meta = (Categories = "Creature.Visual.Type"))
    FTriggerCondition RequiredType;
};

/** 体型模板 */
UCLASS(BlueprintType)
class UBodyTypeDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Identity.Race"))
    FGameplayTag BodyTypeTag;

    // 根节点 (告诉程序从哪里开始递归)
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Body.Part"))
    FGameplayTag RootSlot;

    // 结构表
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Body.Part"))
    TMap<FGameplayTag, FBodyStructureEntry> BodyStructure;

    // [新增] 辅助函数：按层级顺序获取槽位列表
    // 防止 TMap 乱序导致 Attach 失败
    void GetOrderedSlots(TArray<FGameplayTag>& OutSlots) const;
};

// =========================================================
// [Identity] 特质、种族、职业
// =========================================================

/** 特质定义 */
UCLASS(BlueprintType)
class UCreatureTraitDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category = "Identity", meta = (Categories = "Creature.Identity.Trait"))
    FGameplayTag TraitTag;

    UPROPERTY(EditDefaultsOnly, Category = "Randomization")
    FTriggerCondition TraitsRule;

    UPROPERTY(EditDefaultsOnly, Category = "Stats")
    int32 ComplexityCost = 1; 

    UPROPERTY(EditDefaultsOnly, Category = "Randomization")
    ETraitAcquisitionType AcquisitionType = ETraitAcquisitionType::Both;

    UPROPERTY(EditDefaultsOnly, Category = "Randomization")
    float SpawnWeight = 1.0f;
    
    // --- 属性影响 ---
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Attribute"))
    TMap<FGameplayTag, FAttributeModifier> AttributeConfigs;
    
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Attribute"))
    TMap<FGameplayTag, FDerivedAttributeDef> DerivedFormulas;
    
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Demand"))
    TMap<FGameplayTag, FPreferenceRuleConfig> DemandConfigs;

    // --- 性格影响 ---
    UPROPERTY(EditDefaultsOnly, Category = "Personality", meta = (Categories = "Creature.PersonalityDim"))
    TMap<FGameplayTag, FAttributeModifier> PersonalityModelPresets; // 获得时修正性格

    UPROPERTY(EditDefaultsOnly, Category = "Personality")
    TArray<FPersonalityRangeRule> PersonalityRequirements; // 获得门槛
};

/** 种族模板 */
UCLASS(BlueprintType)
class UCreatureRaceTemplate : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category = "Identity", meta = (Categories = "Creature.Identity.Race"))
    FGameplayTag RaceTag;

    UPROPERTY(EditDefaultsOnly, Category = "Visuals")
    UBodyTypeDefinition* BodyArchitecture;

    UPROPERTY(EditDefaultsOnly, Category = "Lifecycle")
    int32 AdulthoodAge = 18;

    UPROPERTY(EditDefaultsOnly, Category = "Lifecycle")
    int32 OldAge = 60;

    UPROPERTY(EditDefaultsOnly, Category = "Lifecycle")
    int32 MaxAge = 60;
    
    UPROPERTY(EditDefaultsOnly, Category = "Lifecycle|Traits")
    int32 InitialTraitCount = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Identity")
    TObjectPtr<class UCreatureNameTemplate> NameConfig;
    
    // 概率配置...
    UPROPERTY(EditDefaultsOnly, Category = "Lifecycle|Traits")
    float YouthEventChance = 0.35f; 
    UPROPERTY(EditDefaultsOnly, Category = "Lifecycle|Traits")
    float StableEventChance = 0.25f; 
    UPROPERTY(EditDefaultsOnly, Category = "Lifecycle|Traits")
    float OldEventChance = 0.15f; 
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,meta = (Category = "Creature.Identity.Gender"))
    TMap<FGameplayTag, float> GenderConfig;
    UPROPERTY(EditDefaultsOnly, Category = "Rules")
    FTriggerCondition ForbiddenTraitsPool;

    // --- 基础数值 ---
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Attribute"))
    TMap<FGameplayTag, FAttributeModifier> AttributeConfigs;
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Attribute"))
    TMap<FGameplayTag, FDerivedAttributeDef> DerivedFormulas;
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Demand"))
    TMap<FGameplayTag, FPreferenceRuleConfig> DemandConfigs;

    // --- 性格预设 ---
    UPROPERTY(EditDefaultsOnly, Category = "Personality", meta = (Categories = "Creature.PersonalityDim"))
    TMap<FGameplayTag, FAttributeModifier> PersonalityModelPresets;

    float GetCurAgeChance(int CurAge) const
    {
        if (CurAge <= AdulthoodAge) return YouthEventChance;
        if (CurAge > AdulthoodAge && CurAge <= OldAge) return StableEventChance;
        return OldEventChance;
    }
};

/** 职业模板 */
UCLASS(BlueprintType)
class UCreatureProfessionTemplate : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category = "Identity", meta = (Categories = "Creature.Identity.Profession"))
    FGameplayTag ProfessionTag;

    UPROPERTY(EditDefaultsOnly, Category = "Rules")
    FTriggerCondition ForbiddenTraitsPool;
    
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Attribute"))
    TMap<FGameplayTag, FAttributeModifier> AttributeConfigs;
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Attribute"))
    TMap<FGameplayTag, FDerivedAttributeDef> DerivedFormulas;
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Demand"))
    TMap<FGameplayTag, FPreferenceRuleConfig> DemandConfigs;
};

/**
 * 姓名生成配置
 * 包含：姓氏库、名字库（分性别）、命名规则
 */

USTRUCT(BlueprintType)
struct FCreatureNameList
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> List;
};
UCLASS(BlueprintType)
class UCreatureNameTemplate : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 姓氏列表 (e.g., "Smith", "Lionheart")
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Names")
    TArray<FString> Surnames;

    // 男性名字列表 (e.g., "John", "Throg")
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Names")
     TMap<FGameplayTag,FCreatureNameList> FirstNameMap;
    
    // 兜底通用名 (当找不到对应 Tag 的配置时使用)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Names")
    TArray<FString> FallbackFirstNames;

    // 是否姓在名之前？ (东方命名法：True=李小龙, False=Bruce Lee)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
    bool bSurnameFirst = false;

    // 是否一定要有姓氏？ (有些种族可能只有名)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
    bool bUseSurname = true;

    // --- 核心生成函数 ---
    
    UFUNCTION(BlueprintCallable)
    FString GenerateRandomName(const FGameplayTag& GenderTag, FRandomStream& Stream) const
    {
        // 1. 决定名字 (First Name)
        const TArray<FString>* TargetList = nullptr;

        // A. 尝试在 Map 中精确查找 Key
        if (const FCreatureNameList* FoundList = FirstNameMap.Find(GenderTag))
        {
            if (FoundList->List.Num() > 0)
            {
                TargetList = &FoundList->List;
            }
        }

        // B. (可选) 如果精确查找失败，尝试模糊匹配
        // 如果你希望 "Gender.Male.Strong" 也能读到 "Gender.Male" 的配置
        if (!TargetList)
        {
            // 遍历 Map 寻找匹配的父级 Tag (性能稍低，但对于配置不多的情况可以接受)
            for (const auto& Pair : FirstNameMap)
            {
                if (GenderTag.MatchesTag(Pair.Key)) 
                {
                    if (Pair.Value.List.Num() > 0)
                    {
                        TargetList = &Pair.Value.List;
                        break; 
                    }
                }
            }
        }

        // C. 兜底逻辑
        if (!TargetList)
        {
            if (FallbackFirstNames.Num() > 0) TargetList = &FallbackFirstNames;
            else return TEXT("Unnamed"); // 彻底没救了
        }

        // 随机取名
        int32 NameIdx = Stream.RandRange(0, TargetList->Num() - 1);
        FString FirstName = (*TargetList)[NameIdx];

        // 2. 决定姓氏 (Surname)
        FString Surname = TEXT("");
        if (bUseSurname && Surnames.Num() > 0)
        {
            int32 SurIdx = Stream.RandRange(0, Surnames.Num() - 1);
            Surname = Surnames[SurIdx];
        }

        // 3. 组合
        if (Surname.IsEmpty()) return FirstName;

        if (bSurnameFirst)
        {
            return FString::Printf(TEXT("%s%s"), *Surname, *FirstName); // 东方：李明 (无空格)
        }
        else
        {
            return FString::Printf(TEXT("%s %s"), *FirstName, *Surname); // 西方：John Smith (有空格)
        }
    }
};

// =========================================================
// [Registry] 表格定义
// =========================================================

USTRUCT(BlueprintType)
struct FCreatureTraitsRegistryItem : public FTableRowBase
{
    GENERATED_BODY()
    
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Identity.Trait"))
    FGameplayTag Tag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UCreatureTraitDefinition> TraitTemplate;
};

USTRUCT(BlueprintType)
struct FCreatureRaceRegistryItem : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Identity.Race"))
    FGameplayTag Tag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UCreatureRaceTemplate> RaceTemplate;
};

USTRUCT(BlueprintType)
struct FCreatureProffesionRegistryItem : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Creature.Identity.Profession"))
    FGameplayTag Tag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UCreatureProfessionTemplate> ProfessionTemplate;
};
// =========================================================
// [Global Rules] 世界通用规则
// =========================================================

/** 全局属性公式表 (Global Formulas)
 * 作用：定义全游戏通用的衍生计算规则 (物理法则)
 * 例如：最大负重 = 力量 * 10 + 体质 * 5
 * 优先级：通常作为最后一道工序，或者最基础的工序执行
 */
UCLASS(BlueprintType)
class UGlobalAttributeFormulas : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    // 公式列表
    // Key: 目标属性 (e.g. Attribute.Derived.MaxWeight)
    // Value: 计算公式
    UPROPERTY(EditDefaultsOnly, Category = "Formulas", meta = (Categories = "Creature.Attribute"))
    TMap<FGameplayTag, FDerivedAttributeDef> Formulas;

    // 允许 AssetManager 获取 ID
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId("GlobalFormulas", GetFName());
    }
};
/** TAG UI 配置行 */
USTRUCT(BlueprintType)
struct FTagUIDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag Tag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText Abbreviation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FLinearColor UIColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SortOrder = 0;
};

