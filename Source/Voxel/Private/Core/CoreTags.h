// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "NativeGameplayTags.h"

/**
 * 体素方块 ItemId 特殊值
 * Air=0 为唯一硬编码值，贯穿存档/渲染/网格合并
 * 其他方块类型 ID 由配置表分配，不硬编码
 */
namespace EBlockId
{
    constexpr int32 Air = 0;       // 空气/无方块
    constexpr int32 Invalid = -1;  // 无效/未初始化
}

/**
 * 核心 Gameplay Tags 定义
 * 包含：世界环境、物品系统、区域系统、生物属性与状态
 */
namespace FCoreTags
{
    // =========================================================
    // 1. 全局与世界环境 (Global & World Environment)
    // =========================================================
    
    // 阵营/势力 (e.g., Faction.Empire, Faction.Player)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Faction);

    // 环境系统根节点
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Environment);
    
    // 天气状态 (e.g., Environment.Weather.Rain, .Sunny)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Environment_Weather);
    
    // 世界事件 (e.g., Environment.Event.Festival, .Fire)
    // 用于驱动突发事件逻辑
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Environment_Event);

    // --- 稀有度描述 (Rarity) ---
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rarity);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rarity_Trash);     // 灰色
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rarity_Common);    // 普通
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rarity_Rare);      // 稀有
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rarity_Epic);      // 史诗
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rarity_Legendary); // 传说
    
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rarity_Unique);    // 独特
    // --- 质量描述 (Quality) ---
    // 通常指物品的耐久度、做工或成色
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quality);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quality_Broken);   // 破损
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quality_Poor);     // 劣质
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quality_Normal);   // 标准
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quality_High);     // 优质
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quality_Master);   // 大师
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Quality_Mythic);   // 神话

    // --- 性质描述 (Property / Element) ---
    // 通常指攻击属性、元素类型或物理特性
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Property);

    // =========================================================
    // 2. 物品与方块系统 (Item & Block)
    // =========================================================
    
    // 物品根节点
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item);

    // 物品大类
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type);

    // >> 方块物理属性 (Block Physics) - 用于体素放置逻辑
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Block);        // 方块类物品根
    
    // 1. 气体 (Gas)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Block_Gas);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Block_Gas_Air);
    
    // 2. 固体 (Solid) - 石头、木材等
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Block_Solid);
    
    // 3. 液体 (Liquid) - 水、岩浆
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Block_Liquid);
    
    // 4. 空固体/虚空 (Hollow) - 特殊逻辑方块
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Block_Hollow);

    // 5. 辅助方块 (Marker) - 模板编辑用，保存模板时跳过
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Block_Marker);

	//建造物品
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_BuildItem);
	// 吧台 (BarCounter)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_BuildItem_BarCounter);
	// 桌子 (Table)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_BuildItem_Table);
	// 椅子 (Chair)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_BuildItem_Chair);
	// 入口 (EntryPoint)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_BuildItem_EntryPoint);
	// 床 (Bed)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_BuildItem_Bed);
	// 浴缸 (Bathtub)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_BuildItem_Bathtub);
	// 货架 (Shelf)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_BuildItem_Shelf);
	// 储物柜 (Storage)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_BuildItem_Storage);

	
	//消耗品
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Consume);
	//装备
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Equipment);

	// 模板（玩家创建的体素组合，可分享）
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Template);


    // =========================================================
    // 3. 区域系统 (Area Management)
    // =========================================================
    
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Area);
    
    // 区域功能类型
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Area_Type);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Area_Type_Default);          // 默认未划分类
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Area_Type_Spawn);            // NPC/玩家出生点
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Area_Type_BusinessCluster);  // 商业综合区
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Area_Type_Shop);             // 具体店铺
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Area_Type_SubFunction);      // 店铺附属区(厨房、仓库)

    // =========================================================
    // 4. 生物：静态特征 (Creature Static Identity)
    // 定义 "它是谁"、"基础属性" (Data Asset 常用)
    // =========================================================
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature);

    // -------- 身份画像 (Identity) --------
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_Race);         // 种族 (Human, Elf)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_Class);        // 阶级 ()
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_Profession);   // 生活职业 (Chef, Miner)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_CombatClass);  // 战斗职业 (Warrior, Mage)

	// -------- 性别 (Gender) --------
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_Gender);        // 性别
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_Gender_Male);   
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_Gender_Female);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_Gender_Null);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_Gender_Uncertain);

    // [新增] 性格维度 (Personality Dimensions)
    // 用于在 PersistentBasicValues 中作为 Key 使用（生成时 roll 底子，消费端按命名空间分支）
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_PersonalityDim);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_PersonalityDim_Sociality);        // 社交性
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_PersonalityDim_Conscientiousness);// 尽责性
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_PersonalityDim_Exploratory);      // 探索性
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_PersonalityDim_SelfConsistency);  // 自洽性
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_PersonalityDim_Competitiveness);  // 竞争性
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_PersonalityDim_Greed);            // 贪婪性
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_PersonalityDim_Patience);         // 耐心 ↔ 急躁
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_PersonalityDim_Fastidiousness);   // 挑剔 ↔ 随和
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_PersonalityDim_Altruism);         // 利他 ↔ 利己
	
    // -------- 特质 (Traits) --------
    // 程序化生成的核心差异化标签
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_Trait);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_Trait_Physical);    // 生理特质 (Strong, Blind)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_Trait_Personality); // 性格特质 (Greedy, Lazy)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Identity_Trait_Special);  //特殊

    // -------- 身体部位 (Body) --------
    // 用于战棋部位打击、装备槽位、脸盲特征识别
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Body);           // Head, Arm
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Body_Part);    // Hair, Beard
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Visual_Type); // 
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Visual_Feature); // Scar, Glasses
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Visual_Part);    // Hair, Beard

    // -------- 属性定义 (Attributes) --------
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Attribute);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Attribute_Primary); // 基础七围 (Str, Agi, Con, Int, Per, Cha, Spirit)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Attribute_Dynamic); // 动态值 
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Attribute_Combat);  // 战斗衍生 (HP, Atk, MoveRange)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Attribute_Social);  // 经营衍生 (TipRate, WorkEfficiency)

    // -------- 偏好 (Preferences) --------
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Preference);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Preference_Flavor);      // 口味偏好
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Preference_Environment); // 环境偏好

    // =========================================================
    // 5. 生物：动态状态 (Creature Dynamic State)
    // 定义 "它在做什么"、"状态如何" (Runtime 常用)
    // =========================================================

    // -------- 行为动作 (Action) --------
    // 对应 State Tree 或 GOAP 的执行节点
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Action);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Action_Battle);  // 战斗 (Attack, Skill)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Action_Common);  // 日常 (Eat, Sleep, Work)

    // -------- 需求驱动 (Demand) --------
    // 驱动 AI 决策的核心数值 (Hunger, Energy)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Demand);

    // -------- 健康状态 (Health State) --------
    // 宏观生命状态
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_State_Health); 
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_State_Health_Healthy);      // 健康
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_State_Health_Injured);      // 受伤
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_State_Health_Critical);     // 濒死
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_State_Health_Dead);         // 死亡

    // -------- 具体状态条件 (Status Conditions) --------
    // 具体的 Buff/Debuff，影响属性计算
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Status);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Status_Condition_Physical);      // 物理 (流血, 骨折)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Status_Condition_Physiological); // 生理 (生病, 中毒)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Status_Condition_Mental);        // 精神 (抑郁, 狂暴)
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Status_Condition_Special);       // 特殊 (眩晕, 冰冻)
    
    // 免疫标记
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Creature_Status_Immunity);

    // =========================================================
    // 6. 编辑系统 (Voxel Editing System)
    // =========================================================

    // 编辑工具根节点
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool);

    // 选区工具
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Selection);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Selection_SimpleGeometry);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Selection_Magic);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Selection_Brush);

    // 编辑工具
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Edit);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Edit_SingleVoxel);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Edit_SimpleGeometry);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Edit_Selection);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Edit_Template);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Edit_Furniture);

	// 采样工具
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Sample);

    // 区域编辑工具
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_AreaEdit);

    // 透明区域框工具
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Cutout);

    // =========================================================
    // 8. 渲染图层 (Rendering Layer Overlay)
    // 驱动左上管理栏的图层切换区；图标/命名配置于 DT_TagUIDefConfig
    // =========================================================

    // 图层根节点（直接子级 = 可切换的渲染图层）
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Layer);

    // 透视框图层（本期实现）
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Layer_Cutout);

    // 地面热力图（预留）
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Layer_Heatmap);

    // 风向图（预留）
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Layer_Wind);

    // =========================================================
    // 7. 事件结果类型 (Event Outcome Type)
    // =========================================================
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Instant);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Task);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Task_Self);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Task_Player);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Chain);
};