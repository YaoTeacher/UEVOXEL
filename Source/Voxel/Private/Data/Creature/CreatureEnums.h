// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

UENUM(Blueprintable,BlueprintType)
enum class ECreatureRace : uint8
{
	Human = 0,
	Fairy = 1,
	Elf = 2,
	Goblin = 3,
	
};
UENUM(Blueprintable,BlueprintType)
enum class EDemandEnum : uint8
{
	Hunger = 0 UMETA(DisplayName = "饱腹值"),
	Thirst = 1 UMETA(DisplayName = "口渴值"),
	Health = 2 UMETA(DisplayName = "身体健康"),
	Spirit = 3 UMETA(DisplayName = "精神健康"),
	Leisure = 4 UMETA(DisplayName = "休闲需求"),
	Cloth = 5 UMETA(DisplayName = "衣物需求"),
	Luxury = 6 UMETA(DisplayName = "奢侈需求"),
	
	
};

// 行为类型分层架构
UENUM(Blueprintable, BlueprintType, Category="AI|Behavior")
enum class ECreatureActionType : uint8
{
	ConsumeAction    = 0 UMETA(DisplayName="消费动作"),  // 进食、饮水、使用服务等
	ProductAction    = 1 UMETA(DisplayName="生产动作"),  // 烹饪、制作、建造等
	BattleAction     = 2 UMETA(DisplayName="战斗动作"),  // 攻击、防御、施法等
	SocialAction     = 3 UMETA(DisplayName="社交动作"),  // 交谈、交易
	MoveAction       = 4 UMETA(DisplayName="移动动作"),  // 行走、奔跑、传送等
	RestAction       = 5 UMETA(DisplayName="休息动作"),  // 睡眠、冥想、放松等
	LearnAction      = 6 UMETA(DisplayName="学习动作"),  // 阅读、训练、研究等
	NormalAction     = 7 UMETA(DisplayName="通用动作"),  // 组织、规划、分配等
	
	// // 消费者行为
	// Idle            UMETA(DisplayName="空闲"),
	// Stroll          UMETA(DisplayName="漫步"),
	// Eat             UMETA(DisplayName="进食"),
	// Sleep           UMETA(DisplayName="睡眠"),
	// Hygiene         UMETA(DisplayName="清洁"),
 //    
	// // 经济行为
	// Commerce        UMETA(DisplayName="交易"),
 //    
	// // 工作行为
	// Serve           UMETA(DisplayName="服务"),
	// Cook            UMETA(DisplayName="烹饪"),
	// Craft           UMETA(DisplayName="制作"),
	// Plant			UMETA(DisplayName="种植"),
	// Domesticate		UMETA(DisplayName="驯养"),
	//
	// // 通用行为
	// Equip           UMETA(DisplayName = "装备"),
	// Sit             UMETA(DisplayName = "坐下"),
	// Build           UMETA(DisplayName = "建造"),
	// Interact        UMETA(DisplayName = "交互"),
	// Destroy			UMETA(DisplayName = "销毁"),
	//
	// // 特殊行为
	// Plan            UMETA(DisplayName="计划"),
	//
	// Null            UMETA(DisplayName="无")

	
	
};

UENUM(Blueprintable,BlueprintType)
enum class ECreatureProfession : uint8
{
	Miner = 0,
	Butcher = 1,
	Adventure = 2,
	Farmer = 3,
	
};

UENUM(Blueprintable,BlueprintType,BlueprintInternalUseOnly)
enum class ECreatureClass : uint8
{
	Human = 0,
	Computer = 1,
	
};

#include "CreatureEnums.generated.h"