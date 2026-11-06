
#include "Core/CoreTags.h"
namespace FCoreTags
{
    // =========================================================
    // 1. Global & World Environment
    // =========================================================
    UE_DEFINE_GAMEPLAY_TAG(Faction,            "Faction");
    
    UE_DEFINE_GAMEPLAY_TAG(Environment,        "Environment");
    UE_DEFINE_GAMEPLAY_TAG(Environment_Weather,"Environment.Weather");
    UE_DEFINE_GAMEPLAY_TAG(Environment_Event,  "Environment.Event");

    // --- 稀有度 (Rarity) 定义 ---
    UE_DEFINE_GAMEPLAY_TAG(Rarity,           "Rarity");
    UE_DEFINE_GAMEPLAY_TAG(Rarity_Trash,     "Rarity.Trash");     // 灰色
    UE_DEFINE_GAMEPLAY_TAG(Rarity_Common,    "Rarity.Common");    // 白色
    UE_DEFINE_GAMEPLAY_TAG(Rarity_Rare,      "Rarity.Rare");      // 蓝色
    UE_DEFINE_GAMEPLAY_TAG(Rarity_Epic,      "Rarity.Epic");      // 紫色
    UE_DEFINE_GAMEPLAY_TAG(Rarity_Legendary, "Rarity.Legendary"); // 橙色
    
    UE_DEFINE_GAMEPLAY_TAG(Rarity_Unique,    "Rarity.Unique");    // 暗金/特殊

    // --- 质量 (Quality) 定义 ---
    UE_DEFINE_GAMEPLAY_TAG(Quality,          "Quality");
    UE_DEFINE_GAMEPLAY_TAG(Quality_Broken,   "Quality.Broken");   // 
    UE_DEFINE_GAMEPLAY_TAG(Quality_Poor,     "Quality.Poor");     // 
    UE_DEFINE_GAMEPLAY_TAG(Quality_Normal,   "Quality.Normal");   // 标准
    UE_DEFINE_GAMEPLAY_TAG(Quality_High,     "Quality.High");     // 强化过
    UE_DEFINE_GAMEPLAY_TAG(Quality_Master,   "Quality.Master");   // 大师工艺
    UE_DEFINE_GAMEPLAY_TAG(Quality_Mythic,   "Quality.Mythic");   // 神话级工艺

    // --- 性质 (Property) 定义 ---
    // 注意：这里使用了三层结构 (Property -> Element -> Fire)，方便在编辑器里折叠查看
    UE_DEFINE_GAMEPLAY_TAG(Property,           "Property");

    // =========================================================
    // 2. Item & Block System
    // =========================================================
    UE_DEFINE_GAMEPLAY_TAG(Item,                       "Item");
    UE_DEFINE_GAMEPLAY_TAG(Item_Type,                  "Item.Type");
    
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_Block,            "Item.Type.Block");
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_Block_Gas,        "Item.Type.Block.Gas");
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_Block_Gas_Air,    "Item.Type.Block.Gas.Air");
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_Block_Solid,      "Item.Type.Block.Solid");
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_Block_Liquid,     "Item.Type.Block.Liquid");
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_Block_Hollow,     "Item.Type.Block.Hollow");
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_Block_Marker,    "Item.Type.Block.Marker");

    // 建造物品 (Item.Type.BuildItem)
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_BuildItem, "Item.Type.BuildItem");

    // =========================================================
    // [BuildItems] 建筑物品/家具子类
    // =========================================================
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_BuildItem_BarCounter, "Item.Type.BuildItem.BarCounter");// 吧台
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_BuildItem_Table,      "Item.Type.BuildItem.Table");// 桌子
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_BuildItem_Chair,      "Item.Type.BuildItem.Chair");// 椅子
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_BuildItem_EntryPoint, "Item.Type.BuildItem.EntryPoint");// 入口
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_BuildItem_Bed,        "Item.Type.BuildItem.Bed");// 床
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_BuildItem_Bathtub,    "Item.Type.BuildItem.Bathtub");// 浴缸
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_BuildItem_Shelf,      "Item.Type.BuildItem.Shelf");// 货架
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_BuildItem_Storage,    "Item.Type.BuildItem.Storage");// 储物柜

    // 消耗品 (Item.Type.Consume)
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_Consume, "Item.Type.Consume");

    // 装备 (Item.Type.Equipment)
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_Equipment, "Item.Type.Equipment");

    // 模板 (Item.Type.Template)
    UE_DEFINE_GAMEPLAY_TAG(Item_Type_Template, "Item.Type.Template");

    // =========================================================
    // 3. Area System
    // =========================================================
    UE_DEFINE_GAMEPLAY_TAG(Area,                       "Area");
    UE_DEFINE_GAMEPLAY_TAG(Area_Type,                  "Area.Type");
    UE_DEFINE_GAMEPLAY_TAG(Area_Type_Default,          "Area.Type.Default");
    UE_DEFINE_GAMEPLAY_TAG(Area_Type_Spawn,            "Area.Type.Spawn");
    UE_DEFINE_GAMEPLAY_TAG(Area_Type_BusinessCluster,  "Area.Type.BusinessCluster");
    UE_DEFINE_GAMEPLAY_TAG(Area_Type_Shop,             "Area.Type.Shop");
    UE_DEFINE_GAMEPLAY_TAG(Area_Type_SubFunction,      "Area.Type.SubFunction");

    // =========================================================
    // 4. Creature Static Identity
    // =========================================================
    UE_DEFINE_GAMEPLAY_TAG(Creature, "Creature");

    // Identity
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity,             "Creature.Identity");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_Race,        "Creature.Identity.Race");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_Class,       "Creature.Identity.Class");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_Profession,  "Creature.Identity.Profession");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_CombatClass, "Creature.Identity.CombatClass");

    //Gender
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_Gender,           "Creature.Identity.Gender");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_Gender_Male,      "Creature.Identity.Gender.Male");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_Gender_Female,    "Creature.Identity.Gender.Female");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_Gender_Null,      "Creature.Identity.Gender.Null");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_Gender_Uncertain, "Creature.Identity.Gender.Uncertain");

    //六维性格模型注册
    UE_DEFINE_GAMEPLAY_TAG(Creature_PersonalityDim, "Creature.PersonalityDim");
    UE_DEFINE_GAMEPLAY_TAG(Creature_PersonalityDim_Sociality, "Creature.PersonalityDim.Sociality");
    UE_DEFINE_GAMEPLAY_TAG(Creature_PersonalityDim_Conscientiousness, "Creature.PersonalityDim.Conscientiousness");
    UE_DEFINE_GAMEPLAY_TAG(Creature_PersonalityDim_Exploratory, "Creature.PersonalityDim.Exploratory");
    UE_DEFINE_GAMEPLAY_TAG(Creature_PersonalityDim_SelfConsistency, "Creature.PersonalityDim.SelfConsistency");
    UE_DEFINE_GAMEPLAY_TAG(Creature_PersonalityDim_Competitiveness, "Creature.PersonalityDim.Competitiveness");
    UE_DEFINE_GAMEPLAY_TAG(Creature_PersonalityDim_Greed, "Creature.PersonalityDim.Greed");
    UE_DEFINE_GAMEPLAY_TAG(Creature_PersonalityDim_Patience, "Creature.PersonalityDim.Patience");
    UE_DEFINE_GAMEPLAY_TAG(Creature_PersonalityDim_Fastidiousness, "Creature.PersonalityDim.Fastidiousness");
    UE_DEFINE_GAMEPLAY_TAG(Creature_PersonalityDim_Altruism, "Creature.PersonalityDim.Altruism");

    // Traits
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_Trait,             "Creature.Identity.Trait");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_Trait_Physical,    "Creature.Identity.Trait.Physical");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_Trait_Personality, "Creature.Identity.Trait.Personality");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Identity_Trait_Special,  "Creature.Identity.Trait.Special");

    // Body
    UE_DEFINE_GAMEPLAY_TAG(Creature_Body, "Creature.Body");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Body_Part, "Creature.Body.Part");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Visual_Type, "Creature.Visual.Type");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Visual_Feature, "Creature.Visual.Feature");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Visual_Part, "Creature.Visual.Part");

    // Attributes
    UE_DEFINE_GAMEPLAY_TAG(Creature_Attribute,         "Creature.Attribute");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Attribute_Primary, "Creature.Attribute.Primary");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Attribute_Dynamic, "Creature.Attribute.Dynamic");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Attribute_Combat,  "Creature.Attribute.Combat");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Attribute_Social,  "Creature.Attribute.Social");

    // Preferences
    UE_DEFINE_GAMEPLAY_TAG(Creature_Preference,             "Creature.Preference");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Preference_Flavor,      "Creature.Preference.Flavor");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Preference_Environment, "Creature.Preference.Environment");

    // =========================================================
    // 5. Creature Dynamic State
    // =========================================================

    // Action
    UE_DEFINE_GAMEPLAY_TAG(Creature_Action,        "Creature.Action");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Action_Battle, "Creature.Action.Battle");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Action_Common, "Creature.Action.Common");

    // Health State
    UE_DEFINE_GAMEPLAY_TAG(Creature_State_Health,          "Creature.State.Health");
    UE_DEFINE_GAMEPLAY_TAG(Creature_State_Health_Healthy,  "Creature.State.Health.Healthy");
    UE_DEFINE_GAMEPLAY_TAG(Creature_State_Health_Injured,  "Creature.State.Health.Injured");
    UE_DEFINE_GAMEPLAY_TAG(Creature_State_Health_Critical, "Creature.State.Health.Critical");
    UE_DEFINE_GAMEPLAY_TAG(Creature_State_Health_Dead,     "Creature.State.Health.Dead");

    // Status Conditions
    UE_DEFINE_GAMEPLAY_TAG(Creature_Status,                          "Creature.Status");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Status_Condition_Physical,       "Creature.Status.Condition.Physical");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Status_Condition_Physiological,  "Creature.Status.Condition.Physiological");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Status_Condition_Mental,         "Creature.Status.Condition.Mental");
    UE_DEFINE_GAMEPLAY_TAG(Creature_Status_Condition_Special,        "Creature.Status.Condition.Special");
    
    // Immunity
    UE_DEFINE_GAMEPLAY_TAG(Creature_Status_Immunity, "Creature.Status.Immunity");

    // Demand
    UE_DEFINE_GAMEPLAY_TAG(Creature_Demand, "Creature.Demand");

    // =========================================================
    // 6. Voxel Editing System
    // =========================================================

    // Tool Root
    UE_DEFINE_GAMEPLAY_TAG(Tool, "Tool");

    // Selection Tools
    UE_DEFINE_GAMEPLAY_TAG(Tool_Selection, "Tool.Selection");
    UE_DEFINE_GAMEPLAY_TAG(Tool_Selection_SimpleGeometry, "Tool.Selection.SimpleGeometry");
    UE_DEFINE_GAMEPLAY_TAG(Tool_Selection_Magic, "Tool.Selection.Magic");
    UE_DEFINE_GAMEPLAY_TAG(Tool_Selection_Brush, "Tool.Selection.Brush");

    // Edit Tools
    UE_DEFINE_GAMEPLAY_TAG(Tool_Edit, "Tool.Edit");
    UE_DEFINE_GAMEPLAY_TAG(Tool_Edit_SingleVoxel, "Tool.Edit.SingleVoxel");
    UE_DEFINE_GAMEPLAY_TAG(Tool_Edit_SimpleGeometry, "Tool.Edit.SimpleGeometry");
    UE_DEFINE_GAMEPLAY_TAG(Tool_Edit_Selection, "Tool.Edit.Selection");
    UE_DEFINE_GAMEPLAY_TAG(Tool_Edit_Template, "Tool.Edit.Template");
    UE_DEFINE_GAMEPLAY_TAG(Tool_Edit_Furniture, "Tool.Edit.Furniture");

    // 采样工具
    UE_DEFINE_GAMEPLAY_TAG(Tool_Sample, "Tool.Sample");

    // Area Edit Tools
    UE_DEFINE_GAMEPLAY_TAG(Tool_AreaEdit, "Tool.AreaEdit");

    // Cutout Box Tool
    UE_DEFINE_GAMEPLAY_TAG(Tool_Cutout, "Tool.Cutout");

    // =========================================================
    // 8. 渲染图层 (Rendering Layer Overlay)
    // =========================================================
    UE_DEFINE_GAMEPLAY_TAG(Layer, "Voxel.Layer");
    UE_DEFINE_GAMEPLAY_TAG(Layer_Cutout, "Voxel.Layer.Cutout");
    UE_DEFINE_GAMEPLAY_TAG(Layer_Heatmap, "Voxel.Layer.Heatmap");
    UE_DEFINE_GAMEPLAY_TAG(Layer_Wind, "Voxel.Layer.Wind");

    // =========================================================
    // 7. 事件结果类型 (Event Outcome Type)
    // =========================================================
    UE_DEFINE_GAMEPLAY_TAG(Effect, "Effect");
    UE_DEFINE_GAMEPLAY_TAG(Effect_Instant, "Effect.Instant");
    UE_DEFINE_GAMEPLAY_TAG(Effect_Task, "Effect.Task");
    UE_DEFINE_GAMEPLAY_TAG(Effect_Task_Self, "Effect.Task.Self");
    UE_DEFINE_GAMEPLAY_TAG(Effect_Task_Player, "Effect.Task.Player");
    UE_DEFINE_GAMEPLAY_TAG(Effect_Chain, "Effect.Chain");
}