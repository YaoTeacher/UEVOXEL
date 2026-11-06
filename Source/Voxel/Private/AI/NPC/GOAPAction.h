// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "GOAPAction.generated.h"

// 前置声明
//class UCreatureCoreComponent;
class AVoxelIntelligentCreatureInstance;

// =========================================================
// GOAP 状态原子 (Atom)
// 用途：描述一个单一的状态，例如 { State.Hungry : 0 }
// =========================================================
USTRUCT(BlueprintType)
struct FGOAPAtom
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FGameplayTag Key; // e.g., Creature.State.Internal.Hungry

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 Value = 1;  // 1 = True, 0 = False
};

// =========================================================
// GOAP 动作定义 (Action Definition)
// =========================================================
UCLASS(Blueprintable, BlueprintType)
class UGOAPAction : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // --- 1. 基础配置 ---
    
    UPROPERTY(EditDefaultsOnly, Category = "Config")
    FText ActionName;

    // 动作的基础代价 (Cost)
    // 代价越低，AI 越倾向于选择此动作。
    // e.g., "吃路边摊" Cost=5, "去五星饭店" Cost=50
    UPROPERTY(EditDefaultsOnly, Category = "Config")
    float BaseCost = 1.0f;

    // --- 2. 规划逻辑 (Planning) ---

    // 前提条件 (Preconditions)
    // 必须满足这些状态，才能执行此动作
    // e.g., { HasMoney: 1, AtShop: 1 }
    UPROPERTY(EditDefaultsOnly, Category = "Planning")
    TArray<FGOAPAtom> Preconditions;

    // 预期效果 (Effects)
    // 执行完后，世界状态会发生什么变化
    // e.g., { Hungry: 0, HasMoney: 0 }
    UPROPERTY(EditDefaultsOnly, Category = "Planning")
    TArray<FGOAPAtom> Effects;

    // --- 3. 执行逻辑 (Execution) ---

    // 绑定的 State Tree 任务 Tag
    // 当规划器决定执行此动作时，会把这个 Tag 发送给 State Tree Component
    // State Tree 里的 Transition 会监听此 Tag 并切换到对应状态
    UPROPERTY(EditDefaultsOnly, Category = "Execution")
    FGameplayTag StateTreeEventTag; // e.g., Creature.Action.Survival.Eat

    // --- 4. 动态逻辑 (C++ Virtuals) ---

    // 动态计算代价
    // e.g., 如果 NPC 有 "Greedy" 特质，Steal 的代价降低
    // e.g., 如果天气是 "Rain"，露天活动的代价增加
    UFUNCTION(BlueprintNativeEvent, Category = "AI")
    float GetCost(const AVoxelIntelligentCreatureInstance* OwnerNPC) const;
    virtual float GetCost_Implementation(const AVoxelIntelligentCreatureInstance* OwnerNPC) const
    {
        return BaseCost;
    }

    // 程序化前提检查 (Procedural Precondition)
    // 处理那些无法简单用 Tag 表示的逻辑
    // e.g., "虽然我有钱，但那个店现在真的开门了吗？" "目标位置可达吗？"
    UFUNCTION(BlueprintNativeEvent, Category = "AI")
    bool CheckProceduralPrecondition(const AVoxelIntelligentCreatureInstance* OwnerNPC) const;
    virtual bool CheckProceduralPrecondition_Implementation(const AVoxelIntelligentCreatureInstance* OwnerNPC) const
    {
        return true;
    }
};