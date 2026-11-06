#include "Utils/CreatureGeneratorUtils.h"
#include "Core/CoreTags.h" 
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Kismet/GameplayStatics.h"
FCreatureGenerationResult UCreatureGeneratorUtils::GenerateCreatureData(
    const UObject* WorldContextObject, 
    const FCreatureGenerationSettings& Settings)
{
    // 1. 第一步：生成"灵魂" (存档数据：身份 + 随机底子 + 履历)
    FCreatureSaveData NewSaveData = GenerateNewCreatureSaveData(WorldContextObject, Settings);

    // 2. 准备结果容器
    FCreatureGenerationResult Result;
    Result.bIsValid = true; // 只要 SaveData 生成成功，Result 默认有效
    
    // 3. 填充身份数据 (TrueData)
    Result.TrueData = NewSaveData.CreatureData;

    // 4. 第二步：构建"肉体" (运行时状态：基于存档计算属性)
    // 利用 Restore 接口，根据 TrueData 和 PersistentBasicValues 计算出 InitialState
    RestoreRuntimeFromSaveData(NewSaveData, Result.InitialState, WorldContextObject);

    // 此时 Result.InitialState 里的：
    // - CachedBaseAttributes 已经是 (随机底子 + 种族 + 职业) 的总和
    // - FinalAttributes 已经是 (Base + 装备 + 衍生公式) 的最终值
    // - Dynamics 已经是 (满血/满状态) 的初始值

    return Result;
}
// ============================================================================
// 1. 生成新的存档数据 (Create Save Data)
// ============================================================================

FCreatureSaveData UCreatureGeneratorUtils::GenerateNewCreatureSaveData(
    const UObject* WorldContextObject, 
    const FCreatureGenerationSettings& Settings)
{
    FCreatureSaveData SaveData;
    
    // 0. 初始化
    UConfigDataSubsystem* Config = nullptr;
    if (UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject))
    {
        Config = GI->GetSubsystem<UConfigDataSubsystem>();
    }
    FRandomStream Stream(Settings.RandomSeed == 0 ? FMath::Rand() : Settings.RandomSeed);

    // =========================================================
    // A. 生成 TrueData (身份 Identity)
    // =========================================================
    FTrueCreatureData& TrueData = SaveData.CreatureData;
    
    // 确定模板
    UCreatureRaceTemplate* Race = Settings.ForcedRace;
    if (!Race && Config) 
    {
        FGameplayTag RaceTag = Config->GetRandomRaceTag();
        Race = Config->GetRaceTemplate(RaceTag);
    }
    
    UCreatureProfessionTemplate* Job = Settings.ForcedProfession; 
    if (!Job && Config && Config->CachedProfessionTags.Num() > 0)
    {
        int32 Idx = Stream.RandRange(0, Config->CachedProfessionTags.Num() - 1);
        Job = Config->GetProfessionTemplate(Config->CachedProfessionTags[Idx]);
    }

    TrueData.CreatureID = Stream.RandRange(10000, 99999);
    TrueData.RaceTag = Race ? Race->RaceTag : FGameplayTag();
    TrueData.ProfessionTag = Job ? Job->ProfessionTag : FGameplayTag();
    
    // =========================================================
    // 生成性别 (Gender)
    // =========================================================
    // 使用配置的权重生成性别
    TrueData.GenderTag = GenerateGender(Race, Stream);

    // =========================================================
    // 生成名字 (Name)
    // =========================================================
    if (Race && Race->NameConfig)
    {
        // 现在 TrueData.GenderTag 已经是根据种族特性生成的了
        // 它可以正确地去 NameConfig 里的 Map 查找对应的名字列表
        TrueData.RealName = Race->NameConfig->GenerateRandomName(TrueData.GenderTag, Stream);
    }
    else
    {
        FString ProfName = Job ? Config->GetTagDisplayName(Job->ProfessionTag).ToString() : TEXT("Creature");
        TrueData.RealName = FString::Printf(TEXT("%s-%d"), *ProfName, TrueData.CreatureID);
    }

    // 生成特质
    if (Config)
    {
        TMap<FGameplayTag, UCreatureTraitDefinition*> InnatePool;
        // 筛选先天特质
        for (auto& Pair : Config->GetAllTraitDefinitions())
        {
            if (Pair.Value && (Pair.Value->AcquisitionType == ETraitAcquisitionType::InnateOnly || Pair.Value->AcquisitionType == ETraitAcquisitionType::Both))
            {
                InnatePool.Add(Pair.Key, Pair.Value);
            }
        }
        GenerateTraits(TrueData, Race, Job, Settings.ForcedTraits, InnatePool, Stream, Config);
    }

    // 生成视觉
    GenerateBodyVisual(TrueData, Race);

    // =========================================================
    // B. 生成 PersistentBasicValues (不可复现的随机基础值)
    // 包含：属性随机波动 + 性格先天底子
    // =========================================================
    TMap<FGameplayTag, float>& BasicValues = SaveData.PersistentBasicValues;

    // 假设我们给所有 Primary 属性加波动 (需要知道哪些Tag是Primary，这里假设固定几个或从配置获取)
    // 这里使用临时方法：假设 "Creature.Attribute.Primary" 下的所有Tag
    if (Config)
    {
        // 简单模拟：给 Strength/Agility 等加波动。实际应从 Subsystem 获取 Tag 列表
        TArray<FGameplayTag> TargetStats; 
        TargetStats.Add(FGameplayTag::RequestGameplayTag("Creature.Attribute.Primary.Strength"));
        TargetStats.Add(FGameplayTag::RequestGameplayTag("Creature.Attribute.Primary.Agility"));
        TargetStats.Add(FGameplayTag::RequestGameplayTag("Creature.Attribute.Primary.Intelligence"));
        TargetStats.Add(FGameplayTag::RequestGameplayTag("Creature.Attribute.Primary.Charisma"));
        TargetStats.Add(FGameplayTag::RequestGameplayTag("Creature.Attribute.Primary.Spirit"));
        TargetStats.Add(FGameplayTag::RequestGameplayTag("Creature.Attribute.Primary.Constitution"));
        TargetStats.Add(FGameplayTag::RequestGameplayTag("Creature.Attribute.Primary.Perception"));

        for (const FGameplayTag& Tag : TargetStats)
        {
            int Variance = Stream.RandRange(-2, 2);
            BasicValues.Add(Tag, Variance);
        }
    }

    // 2. 性格随机底子 (-0.3 ~ +0.3)
    // 收集所有性格维度 Tag
    TSet<FGameplayTag> PersonalityDims;
    if (Race) for(auto& P : Race->PersonalityModelPresets) PersonalityDims.Add(P.Key);
    // 应该从 ConfigSubsystem 获取所有注册的性格 Tag，防止遗漏
    
    for (const FGameplayTag& DimTag : PersonalityDims)
    {
        float Variance = Stream.FRandRange(-0.3f, 0.3f);
        BasicValues.Add(DimTag, Variance);
    }

    // =========================================================
    // C. 模拟人生 (Life History)
    // =========================================================
    
    int32 TargetAge = Stream.RandRange(0, Race->MaxAge);
    // GenerateBackstory 可能会修改 BasicValues (例如事件永久增加某属性)
    GenerateBackstory(TrueData, BasicValues, TargetAge, Config, Stream, Race, Job);

    // =========================================================
    // D. 初始化动态值 (PersistentDynamicValues)
    // =========================================================
    // 基于当前数据跑一次完整计算，获取 Max 值作为初始 Current 值
    
    FCreatureRuntimeState TempRuntime;
    RestoreRuntimeFromSaveData(SaveData, TempRuntime, WorldContextObject);

    // 将计算出的初始 Dynamics (满血/满饥饿) 存入存档
    SaveData.PersistentDynamicValues = TempRuntime.Dynamics;

    return SaveData;
}

// ============================================================================
// 2. 从存档恢复运行时数据 (Restore Runtime)
// 逻辑：SaveData -> CachedBaseAttributes -> FinalAttributes -> Dynamics
// ============================================================================

void UCreatureGeneratorUtils::RestoreRuntimeFromSaveData(
    const FCreatureSaveData& SaveData, 
    FCreatureRuntimeState& OutState, 
    const UObject* WorldContextObject)
{
    // 0. 清空缓存
    OutState.CachedBaseAttributes.Empty();
    OutState.FinalAttributes.Empty();
    OutState.Dynamics.Empty();
    OutState.CachedDemandRules.Empty();

    UConfigDataSubsystem* Config = nullptr;
    if (UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject))
    {
        Config = GI->GetSubsystem<UConfigDataSubsystem>();
    }
    if (!Config) return;

    const FTrueCreatureData& TrueData = SaveData.CreatureData;
    const UCreatureRaceTemplate* Race = Config->GetRaceTemplate(TrueData.RaceTag);
    const UCreatureProfessionTemplate* Job = Config->GetProfessionTemplate(TrueData.ProfessionTag);

    // =========================================================
    // Step 1: 重建 CachedBaseAttributes (基础属性缓存)
    // 公式: Base = PersistentBasicValues(随机底子) + Template(Race+Job+Traits)
    // =========================================================

    // A. 先填入存档里的"不可复现基础值" (随机底子 + 事件永久修正)
    OutState.CachedBaseAttributes = SaveData.PersistentBasicValues;

    // B. 叠加模板属性 (Race)
    if (Race) 
    {
        AccumulateAttributeConfigs(OutState.CachedBaseAttributes, Race->AttributeConfigs);
        // 种族也可能影响性格，现在统一视为 Attribute 处理
        for(auto& Pair : Race->PersonalityModelPresets) 
            ApplyAttributeModifier(OutState.CachedBaseAttributes, Pair.Key, Pair.Value);
            
        AccumulateDemandConfigs(OutState.CachedDemandRules, Race->DemandConfigs);
    }

    // C. 叠加职业属性 (Job)
    if (Job) 
    {
        AccumulateAttributeConfigs(OutState.CachedBaseAttributes, Job->AttributeConfigs);
        AccumulateDemandConfigs(OutState.CachedDemandRules, Job->DemandConfigs);
    }

    // D. 叠加特质属性 (Traits)
    for (const FGameplayTag& Tag : TrueData.TraitTags)
    {
        if (const UCreatureTraitDefinition* Def = Config->GetTraitDefinition(Tag))
        {
            AccumulateAttributeConfigs(OutState.CachedBaseAttributes, Def->AttributeConfigs);
            // 特质影响性格
            for(auto& Pair : Def->PersonalityModelPresets) 
                ApplyAttributeModifier(OutState.CachedBaseAttributes, Pair.Key, Pair.Value);
                
            AccumulateDemandConfigs(OutState.CachedDemandRules, Def->DemandConfigs);
        }
    }
    
    // E. 叠加身体部位属性
    ProcessBodyParts_AttributesAndRules(TrueData, OutState.CachedBaseAttributes, OutState.CachedDemandRules);

    // =========================================================
    // Step 2: 重建 FinalAttributes (最终静态值)
    // 公式: Final = CachedBase + Formulas + Equipment + Buffs
    // =========================================================
    
    // A. 复制 Base
    OutState.FinalAttributes = OutState.CachedBaseAttributes;

    // B. 应用全局衍生公式 (e.g., Strength * 10 -> MaxLoad)
    if (Config->CachedFormulaMap.Num() > 0)
    {
        CalculateDerivedStats(OutState.FinalAttributes, Config->CachedFormulaMap);
    }
    if (Race) CalculateDerivedStats(OutState.FinalAttributes, Race->DerivedFormulas);
    if (Job) CalculateDerivedStats(OutState.FinalAttributes, Job->DerivedFormulas);
    // 特质公式
    for (const FGameplayTag& Tag : TrueData.TraitTags)
    {
        if (const UCreatureTraitDefinition* Def = Config->GetTraitDefinition(Tag))
            CalculateDerivedStats(OutState.FinalAttributes, Def->DerivedFormulas);
    }

    // C. 应用装备加成 (Equipment) - 假设 ItemDef 也有 AttributeModifiers
    // (此处代码逻辑略，需获取 ItemDef)

    // =========================================================
    // Step 3: 重建 Dynamics (动态实时值)
    // 逻辑: 优先用 SaveData 覆盖，否则用 FinalAttributes 里的 Max 初始化
    // =========================================================

    // A. 默认初始化：填满 (Full State)
    // 遍历 FinalAttributes，找到所有 "Dynamic" 类型的属性
    // 假设 Tag 规则是: "Creature.Attribute.Dynamic.Health"
    FGameplayTag DynamicRoot = FGameplayTag::RequestGameplayTag("Creature.Attribute.Dynamic");
    
    for (const auto& Pair : OutState.FinalAttributes)
    {
        // 如果这个属性在 Dynamic 目录下，说明 FinalAttributes 存的是它的 Max 值
        if (Pair.Key.MatchesTag(DynamicRoot)) 
        {
            OutState.Dynamics.Add(Pair.Key, Pair.Value);
        }
    }
    // 需求值默认填满 (Hunger, Thirst)
    for (const auto& Pair : OutState.CachedDemandRules)
    {
        OutState.Dynamics.Add(Pair.Key, Pair.Value.FinalMax);
    }

    // B. 存档覆盖：应用 PersistentDynamicValues
    // 如果是读档，这里会有数据；如果是新生，这里可能是空的(或在生成时已填好)
    for (const auto& Pair : SaveData.PersistentDynamicValues)
    {
        OutState.Dynamics.Add(Pair.Key, Pair.Value);
    }
}

// ============================================================================
// 3. 动态属性更新 (Tick)
// ============================================================================

void UCreatureGeneratorUtils::UpdateDynamicAttributes(FCreatureRuntimeState& OutState, UConfigDataSubsystem* Config)
{
    // 重置 FinalAttributes 为 Base (不包含上次 Tick 的临时 Buff)
    // 注意：如果有 Duration Buff 系统，这里需要重新累加 Duration Buff
    OutState.FinalAttributes = OutState.CachedBaseAttributes;
    
    // 重新应用衍生公式 (因为 Base 可能没变，但这步开销不大)
    if (Config && Config->CachedFormulaMap.Num() > 0)
    {
        CalculateDerivedStats(OutState.FinalAttributes, Config->CachedFormulaMap);
    }

    TMap<FGameplayTag, float>& FinalAttrs = OutState.FinalAttributes;

    // 检查需求阈值 (例如：饥饿度 < 20 -> 力量 - 5)
    for (const auto& Pair : OutState.CachedDemandRules)
    {
        FGameplayTag DemandTag = Pair.Key;
        float CurrentVal = OutState.Dynamics.FindRef(DemandTag); // 获取当前饥饿值
        
        for (const FDemandThresholdRule& Rule : Pair.Value.MergedThresholdRules)
        {
            bool bTriggered = (Rule.Operator == EThresholdOp::LessThanOrEqual && CurrentVal <= Rule.ThresholdValue) ||
                              (Rule.Operator == EThresholdOp::GreaterThanOrEqual && CurrentVal >= Rule.ThresholdValue);
            
            if (bTriggered)
            {
                for (const auto& Eff : Rule.EffectModifiers)
                    ApplyAttributeModifier(FinalAttrs, Eff.Key, Eff.Value);
            }
        }
    }
}

// ============================================================================
// 4. 人生模拟与事件 (Backstory)
// ============================================================================

void UCreatureGeneratorUtils::GenerateBackstory(
    FTrueCreatureData& OutData,
    TMap<FGameplayTag, float>& OutBasicValues,
    int32 TargetAge,
    UConfigDataSubsystem* Config,
    FRandomStream& Stream,
    const UCreatureRaceTemplate* Race,
    const UCreatureProfessionTemplate* Profession)
{
    if (!Config) return;

    OutData.CurrentAge = TargetAge;
    OutData.LifeHistory.Empty();

    TArray<UCreatureEvent*> AllEvents = Config->GetAllLifeEvents();
    
    for (int32 Age = 0; Age <= TargetAge; ++Age)
    {
        float Chance = Race ? Race->GetCurAgeChance(Age) : 0.25f;
        if (Stream.FRandRange(0.0f, 1.0f) > Chance) continue;

        // 筛选候选事件
        TMap<UCreatureEvent*, float> Candidates;
        float TotalWeight = 0.0f;
        int32 HighestPriority = -9999;

        for (UCreatureEvent* Evt : AllEvents)
        {
            if (!Evt) continue;
            if (Evt->bOnlyOnce)
            {
                bool bHasHappened = OutData.LifeHistory.ContainsByPredicate([&](const FLifeHistoryEntry& Entry){
                    return Entry.SourceEvent.Get() == Evt || Entry.EventId == Evt->GetPrimaryAssetId();
                });
                if (bHasHappened) continue;
            }

            // 传入 BasicValues 进行条件检查 (例如：性格是否满足)
            if (!CheckAllConditions(OutData, OutBasicValues, Age, Evt)) continue;

            if (Evt->Priority < HighestPriority) continue;
            if (Evt->Priority > HighestPriority)
            {
                HighestPriority = Evt->Priority;
                Candidates.Empty();
                TotalWeight = 0.0f;
            }

            Candidates.Add(Evt, Evt->BaseWeight);
            TotalWeight += Evt->BaseWeight;
        }
        
        if (TotalWeight > 0)
        {
            float Roll = Stream.FRandRange(0.0f, TotalWeight);
            UCreatureEvent* SelectedEvent = nullptr;
            for (auto& Pair : Candidates)
            {
                Roll -= Pair.Value;
                if (Roll <= 0.0f) { SelectedEvent = Pair.Key; break; }
            }
            if (!SelectedEvent) SelectedEvent = Candidates.Array().Last().Key;

            // 解析结果
            FEventResolutionResult Resolution = ResolveEventOutcome(OutData, OutBasicValues, SelectedEvent, Stream);

            // 应用结果
            if (Resolution.Outcome)
            {
                ApplyEventOutcome(OutData, OutBasicValues, SelectedEvent, Resolution, Age, Config);
            }
        }
    }
}

bool UCreatureGeneratorUtils::CheckAllConditions(const FTrueCreatureData& Data, const TMap<FGameplayTag, float>& BasicValues, int32 Age, const UCreatureEvent* Evt)
{
    if (Age < Evt->MinAge || Age > Evt->MaxAge) return false;

    FGameplayTagContainer AllTags = Data.TraitTags;
    if (Data.RaceTag.IsValid()) AllTags.AddTag(Data.RaceTag);
    if (Data.ProfessionTag.IsValid()) AllTags.AddTag(Data.ProfessionTag);

    if (!Evt->RequiredConditions.IsMet(AllTags)) return false;
    return true;
}

FEventResolutionResult UCreatureGeneratorUtils::ResolveEventOutcome(
    const FTrueCreatureData& Data,
    const TMap<FGameplayTag, float>& BasicValues,
    const UCreatureEvent* Event,
    FRandomStream& Stream)
{
    FEventResolutionResult Result;
    if (!Event) return Result;

    for (int32 i = 0; i < Event->Choices.Num(); ++i)
    {
        const FEventChoice& Choice = Event->Choices[i];
        
        // 检查 Trigger (传入 BasicValues 以支持性格检查)
        if (CheckTriggerCondition(Data, BasicValues, Choice.VisibilityCondition))
        {
            if (Choice.Outcome)
            {
                if (CheckTriggerCondition(Data, BasicValues, Choice.Outcome->ExtraCondition))
                {
                    Result.Outcome = Choice.Outcome;
                    Result.ChoiceIndex = i;
                    return Result;
                }
            }
        }
    }

    if (Event->DefaultOutcome && CheckTriggerCondition(Data, BasicValues, Event->DefaultOutcome->ExtraCondition))
    {
        Result.Outcome = Event->DefaultOutcome;
        Result.ChoiceIndex = -1;
    }

    return Result;
}

bool UCreatureGeneratorUtils::CheckTriggerCondition(
    const FTrueCreatureData& Data, 
    const TMap<FGameplayTag, float>& BasicValues, 
    const FEventTriggerCondition& Condition)
{
    // Tag 检查
    FGameplayTagContainer AllTags = Data.TraitTags;
    if (Data.RaceTag.IsValid()) AllTags.AddTag(Data.RaceTag);
    if (Data.ProfessionTag.IsValid()) AllTags.AddTag(Data.ProfessionTag);

    if (!Condition.TagCondition.IsMet(AllTags)) return false;

    // 性格检查 (此时需要计算临时性格值：BasicValues + Race/Job Modifiers)
    // 简化处理：这里假设 BasicValues 里已经包含了主要的性格数值 (生成时已初始化)
    // 如果需要更精确，这里需要把 Race/Job/Trait 对性格的加成临时算进去
    for (const FPersonalityRangeRule& Rule : Condition.PersonalityRequirements)
    {
        float Val = BasicValues.FindRef(Rule.DimensionTag); 
        // 实际上这里应该算上 Race Modifier，但为了性能，假设 BasicValues 已经足够代表
        if (!Rule.IsMatch(Val)) return false;
    }
    return true;
}

void UCreatureGeneratorUtils::ApplyEventOutcome(
    FTrueCreatureData& OutData,
    TMap<FGameplayTag, float>& OutBasicValues,
    const UCreatureEvent* SourceEvent,
    const FEventResolutionResult& Resolution,
    int32 Age,
    UConfigDataSubsystem* Config)
{
    UEventOutcomeBase* Outcome = Resolution.Outcome;
    if (!Outcome) return;

    FLifeHistoryEntry Entry;
    Entry.OccuredAge = Age;
    Entry.SourceEvent = const_cast<UCreatureEvent*>(SourceEvent);
    Entry.EventId = SourceEvent->GetPrimaryAssetId();
    Entry.SelectedChoiceIndex = Resolution.ChoiceIndex;
    Entry.SnapshotOutcomeText = Outcome->ResultDescription;

    if (UEventOutcome_Instant* Instant = Cast<UEventOutcome_Instant>(Outcome))
    {
        // 获得特质
        for (const FGameplayTag& NewTrait : Instant->GrantTraits)
        {
            if (!OutData.TraitTags.HasTag(NewTrait))
            {
                OutData.TraitTags.AddTag(NewTrait);
                Entry.GainedTraitTag = NewTrait;
            }
        }
        // 性格偏移 (直接修改 BasicValues)
        for (const auto& Pair : Instant->PersonalityShift)
        {
            ApplyAttributeModifier(OutBasicValues, Pair.Key, Pair.Value);
        }
        // 属性修改 (直接修改 BasicValues)
        for (const auto& Pair : Instant->AttributeModifiers)
        {
            ApplyAttributeModifier(OutBasicValues, Pair.Key, Pair.Value);
        }
    }
    
    // ... Task / Quest 处理 ...

    OutData.LifeHistory.Add(Entry);
}

// ============================================================================
// 5. 辅助与杂项
// ============================================================================

void UCreatureGeneratorUtils::GenerateTraits(
    FTrueCreatureData& OutData,
    const UCreatureRaceTemplate* Race,
    const UCreatureProfessionTemplate* Job,
    const TArray<UCreatureTraitDefinition*>& ForcedTraits,
    const TMap<FGameplayTag, UCreatureTraitDefinition*>& TraitPool,
    FRandomStream& Stream,
    UConfigDataSubsystem* Config)
{
    if (!Config) return;

    FGameplayTagContainer ForbiddenTags;
    if (Race) ForbiddenTags.AppendTags(Race->ForbiddenTraitsPool.ForbiddenTags);
    if (Job) ForbiddenTags.AppendTags(Job->ForbiddenTraitsPool.ForbiddenTags);

    for (const FGameplayTag& ExistingTag : OutData.TraitTags)
    {
        if (const UCreatureTraitDefinition* ExistingDef = Config->GetTraitDefinition(ExistingTag))
        {
            ForbiddenTags.AppendTags(ExistingDef->TraitsRule.ForbiddenTags);
        }
    }

    int32 TargetCount = Race ? Race->InitialTraitCount : 3;
    int32 CurrentCount = OutData.TraitTags.Num(); 

    struct FCandidate { UCreatureTraitDefinition* Def; float W; };

    while (CurrentCount < TargetCount)
    {
        TArray<FCandidate> Pool;
        float TotalW = 0.0f;

        for (auto& Pair : TraitPool)
        {
            if (OutData.TraitTags.HasTag(Pair.Key)) continue;
            if (ForbiddenTags.HasTag(Pair.Key)) continue;
            // 注意：TraitsRule 改为 ForbiddenTraitsPool 或 RequiredConditions，请检查你的定义
            // if (!Pair.Value->RequiredConditions.IsMet(OutData.TraitTags)) continue;
            
            Pool.Add({Pair.Value, Pair.Value->SpawnWeight});
            TotalW += Pair.Value->SpawnWeight;
        }
        if (Pool.IsEmpty() || TotalW <= 0.0f) break;

        float R = Stream.FRandRange(0.0f, TotalW);
        UCreatureTraitDefinition* Selected = nullptr;
        for (auto& C : Pool) { R -= C.W; if (R <= 0) { Selected = C.Def; break; } }
        if (!Selected) Selected = Pool.Last().Def;
        
        OutData.TraitTags.AddTag(Selected->TraitTag);
        ForbiddenTags.AppendTags(Selected->TraitsRule.ForbiddenTags);
        CurrentCount++;
    }
}

void UCreatureGeneratorUtils::GenerateBodyVisual(FTrueCreatureData& OutData, const UCreatureRaceTemplate* Race)
{
    // 1. 初始化
    if (!Race || !Race->BodyArchitecture) return;
    OutData.BodyVisual.BodyType = Race->BodyArchitecture;
    OutData.BodyVisual.BodyState.Empty(); // 清空旧数据

    const UBodyTypeDefinition* BodyType = Race->BodyArchitecture;

    // 2. 准备上下文 (用于条件过滤)
    FGameplayTagContainer ContextTags;
    ContextTags.AddTag(OutData.RaceTag);
    ContextTags.AddTag(OutData.GenderTag);
    ContextTags.AppendTags(OutData.TraitTags);

    // 3. 遍历每一个身体槽位 (Head, Chest, Leg...)
    for (const auto& Elem : BodyType->BodyStructure)
    {
        FGameplayTag SlotTag = Elem.Key;
        const FBodyStructureEntry& Entry = Elem.Value;

        // 加载部件定义
        UBodyPartDefinition* PartDef = Entry.PartDefinition.LoadSynchronous();
        if (!PartDef) continue;

        // --- 核心改造：基于 TMap<FName, Variant> 的加权随机 ---
        
        struct FCandidate { FName ID; float W; };
        TArray<FCandidate> Candidates;
        float TotalWeight = 0.0f;

        // 遍历 Map (Key=ID, Value=Variant)
        for (const auto& VariantPair : PartDef->Variants)
        {
            FName VariantID = VariantPair.Key;
            const FBodyPartVariant& VarData = VariantPair.Value;

            // A. 检查条件 (RequiredTraits)
            if (!VarData.RequiredTraits.IsMet(ContextTags)) continue;

            // C. 加入候选池
            Candidates.Add({VariantID, VarData.SpawnWeight});
            TotalWeight += VarData.SpawnWeight;
        }

        // --- 轮盘赌选择 ---
        if (Candidates.Num() > 0 && TotalWeight > 0.0f)
        {
            float Roll = FMath::FRandRange(0.0f, TotalWeight);
            FName SelectedID = NAME_None;

            for (const auto& Cand : Candidates)
            {
                Roll -= Cand.W;
                if (Roll <= 0.0f)
                {
                    SelectedID = Cand.ID;
                    break;
                }
            }
            if (SelectedID.IsNone()) SelectedID = Candidates.Last().ID;

            // --- 4. 存入结果 (FRuntimeBodyPartInstance) ---
            FRuntimeBodyPartInstance& NewInst = OutData.BodyVisual.BodyState.FindOrAdd(SlotTag);
            NewInst.Definition = PartDef;  // 存资产引用
            NewInst.SelectedVariantId = SelectedID; // 存刚才选中的 FName
            
            // 注意：这里我们还没生成 Mesh 组件，组件在 Rebuild 阶段生成
        }
    }
}

void UCreatureGeneratorUtils::AccumulateAttributeConfigs(TMap<FGameplayTag, float>& OutAttrs, const TMap<FGameplayTag, FAttributeModifier>& Configs)
{
    for (const auto& Pair : Configs) ApplyAttributeModifier(OutAttrs, Pair.Key, Pair.Value);
}

void UCreatureGeneratorUtils::ApplyAttributeModifier(TMap<FGameplayTag, float>& AttributeMap, FGameplayTag Target, const FAttributeModifier& Modifier)
{
    float& CurrentVal = AttributeMap.FindOrAdd(Target);
    switch (Modifier.Op)
    {
        case EAttributeOp::Add: CurrentVal += Modifier.Value; break;
        case EAttributeOp::Multiply: CurrentVal *= Modifier.Value; break;
        case EAttributeOp::Override: CurrentVal = Modifier.Value; break;
    }
    // 可选：如果是性格，Clamp -1~1
    if (Target.MatchesTag(FGameplayTag::RequestGameplayTag("Creature.PersonalityDim")))
    {
        CurrentVal = FMath::Clamp(CurrentVal, -1.0f, 1.0f);
    }
}

void UCreatureGeneratorUtils::AccumulateDemandConfigs(TMap<FGameplayTag, FRuntimeDemandRule>& OutRules, const TMap<FGameplayTag, FPreferenceRuleConfig>& Configs)
{
    for (const auto& Pair : Configs)
    {
        FRuntimeDemandRule& RuleCache = OutRules.FindOrAdd(Pair.Key);
        MergeDemandConfigToCache(Pair.Value, RuleCache);
    }
}

void UCreatureGeneratorUtils::MergeDemandConfigToCache(const FPreferenceRuleConfig& Config, FRuntimeDemandRule& OutRule)
{
    if (Config.ConfigOp == EAttributeOp::Override) {
        OutRule.FinalMax = Config.Max; OutRule.FinalDecayRate = Config.DecayRate; OutRule.FinalImportance = Config.Importance;
    } else if (Config.ConfigOp == EAttributeOp::Add) {
        OutRule.FinalMax += Config.Max; OutRule.FinalDecayRate += Config.DecayRate; OutRule.FinalImportance += Config.Importance;
    }
    OutRule.MergedThresholdRules.Append(Config.ThresholdRules);
}

void UCreatureGeneratorUtils::ProcessBodyParts_AttributesAndRules(const FTrueCreatureData& Data, TMap<FGameplayTag, float>& OutAttrs, TMap<FGameplayTag, FRuntimeDemandRule>& OutRules)
{
    // 遍历 BodyState (这里存的是我们生成的实例)
    for (const auto& Pair : Data.BodyVisual.BodyState)
    {
        const FRuntimeBodyPartInstance& Instance = Pair.Value;

        // 1. 加载定义资产
        const UBodyPartDefinition* Def = Instance.Definition.LoadSynchronous();
        if (!Def) continue;

        // 2. [核心改造] 直接通过 FName ID 查找变体数据 (O(1) 高效查找)
        if (const FBodyPartVariant* Var = Def->Variants.Find(Instance.SelectedVariantId))
        {
            // 累加属性
            AccumulateAttributeConfigs(OutAttrs, Var->AttributeModifiers);
            // 累加需求
            AccumulateDemandConfigs(OutRules, Var->DemandModifiers);
        }
    }
}

void UCreatureGeneratorUtils::CalculateDerivedStats(TMap<FGameplayTag, float>& TargetAttrs, const TMap<FGameplayTag, FDerivedAttributeDef>& Formulas)
{
    for (const auto& Pair : Formulas)
    {
        float Val = Pair.Value.BaseConstant;
        for (const FAttributeFactor& F : Pair.Value.Factors)
            Val += TargetAttrs.FindRef(F.SourceAttribute) * F.Coefficient;
        TargetAttrs.FindOrAdd(Pair.Key) += Val;
    }
}

FText UCreatureGeneratorUtils::ResolveHistoryText(const FLifeHistoryEntry& Entry, const FString& CreatureName, UConfigDataSubsystem* Config)
{
    return Entry.SnapshotOutcomeText;
}

FGameplayTag UCreatureGeneratorUtils::GenerateGender(const UCreatureRaceTemplate* Race, FRandomStream& Stream)
{
    // 1. 兜底：如果没有配置种族或性别池
    if (!Race || Race->GenderConfig.Num() == 0)
    {
        // 返回默认 Male 或者 None
        return FGameplayTag::RequestGameplayTag(FName("Creature.Identity.Gender.None"));
    }

    // 2. 计算总权重
    float TotalWeight = 0.0f;
    for (const auto& Pair : Race->GenderConfig)
    {
        TotalWeight += Pair.Value;
    }

    if (TotalWeight <= 0.0f) return Race->GenderConfig.Array()[0].Key; // 防止配置错误

    // 3. 轮盘赌选择
    float Roll = Stream.FRandRange(0.0f, TotalWeight);
    
    for (const auto& Pair : Race->GenderConfig)
    {
        Roll -= Pair.Value;
        if (Roll <= 0.0f)
        {
            return Pair.Key;
        }
    }

    // 浮点数误差兜底，返回最后一个
    TArray<FGameplayTag> Keys;
    Race->GenderConfig.GetKeys(Keys);
    return Keys.Last();
}
