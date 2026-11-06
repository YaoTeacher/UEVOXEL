// Fill out your copyright notice in the Description page of Project Settings.

/**
 * @file GameStateEnum.h
 *
 * 迁移历史（2026-01-25）:
 * - EVoxelControllerEditState 已删除
 * - 操作类型由 Voxel.Tool.* GameplayTag 管理
 * - 放置规则由 EVoxelPlacementMode 管理
 * - 选择模式由 ESelectionOpMode 管理
 * - 迁移文档: VoxelDesign/05-设计决策记录/编辑系统-EVoxelControllerEditState三维度拆分-决策记录.md
 */

#pragma once

#include "CoreMinimal.h"
#include "GameStateEnum.generated.h"

UENUM(BlueprintType)
enum class EBatchShapeType : uint8
{
	Box = 0				UMETA(DisplayName = "正方体"),
	Cylinder = 1		UMETA(DisplayName = "圆柱体"),
	Sphere = 2			UMETA(DisplayName = "球体"),
	Pyramid = 3			UMETA(DisplayName = "四棱柱"),
	HollowBox = 10		UMETA(DisplayName = "中空正方体"),
	HollowCylinder = 11 UMETA(DisplayName = "中空圆柱体"),
	HollowSphere = 12	UMETA(DisplayName = "中空球体"),
	HollowPyramid = 13	UMETA(DisplayName = "中空四棱柱"),
	Null =4				UMETA(DisplayName = "无"),
};

// UENUM(Blueprintable,BlueprintType)
// enum class EBatchEditType : uint8
// {
// 	Replace = 0		UMETA(DisplayName = "生成"),
// 	Intersect = 1	UMETA(DisplayName = "替换"),
// 	Complement = 2	UMETA(DisplayName = "补充"),
// 	Null =3			UMETA(DisplayName = "无"),
// };

UENUM(BlueprintType)
enum class EVoxelGameAuthority :uint8
{
    
	Developer = 0 UMETA(DisplayName = "开发者模式"),
    
    
	Creator = 1 UMETA(DisplayName = "创造者模式") ,
    
    
	Player = 2 UMETA(DisplayName = "玩家模式"),
	
};

UENUM(BlueprintType)
enum class EVoxelControllerMode :uint8
{
    
    AdventureMode = 0 UMETA(DisplayName = "冒险模式"),
    
    
    ManageMode = 1 UMETA(DisplayName = "管理模式") ,
    
    
    FilmMode = 2 UMETA(DisplayName = "电影模式"),
    
    
    SceneBuildMode = 3 UMETA(DisplayName = "场景建造模式"),
	
};
UENUM(BlueprintType)
enum class EVoxelControllerGameState :uint8
{
	InGame,
	Lodding,
	Died,
	Quit,
};
UENUM(BlueprintType)
enum class EVoxelBuildLimit :uint8
{
	Develop,
	Player,
	Null,
};

// EVoxelControllerEditState 已删除 (2026-01-25)
// 替代方案：
// - 操作类型：Voxel.Tool.* GameplayTag
// - 放置规则：EVoxelPlacementMode (在 EditCommandEnums.h 中定义)
// - 选择模式：ESelectionOpMode (在 EditCommandEnums.h 中定义)

UENUM(BlueprintType)
enum class EScenceTypeEnum: uint8
{
	TestScene			=0,
	StaticScene			=1,
	ProcedureScene		=2,
};

// EVoxelControllerBuildState 已删除 (2026-01-25) - 替换为 FGameplayTag (Voxel.Tool.*)
UENUM(BlueprintType)
enum class EUIStateEnum: uint8
{
	Normal				=0,
	Selected			=1,
	Error				=2,
};

UENUM(BlueprintType)
enum class EAcceptabilityEnum: uint8
{
	
	Prohibited UMETA(DisplayName = "禁止"), //-1 特殊 完全禁止
	
	StronglyDislike UMETA(DisplayName = "强烈排斥"), 
	
	Dislike UMETA(DisplayName = "排斥"),
	
	NeutralNegative UMETA(DisplayName = "中立偏排斥"),
	
	Neutral UMETA(DisplayName = "中立"),
    
	NeutralPositive UMETA(DisplayName = "中立偏接受"),
	
	Like UMETA(DisplayName = "接受"),
	
	UnconditionallyAccept UMETA(DisplayName = "无条件接受"), //999 特殊 完全权限

	Null UMETA(DisplayName = "无"),
};


UENUM(BlueprintType)
enum class EActorDirection:uint8
{
	Left      UMETA(DisplayName = "Left"),      // 面向左 西
	Right     UMETA(DisplayName = "Right"),     // 面向右 东
	Backward  UMETA(DisplayName = "Backward"),  // 面向后 南
	Forward   UMETA(DisplayName = "Forward"),    // 面向前 北
	Up        UMETA(DisplayName = "Up"),		// 面向顶
	Down      UMETA(DisplayName = "Down"),		// 面向底
	Null


};

/**
 * 编辑工具交互状态
 * 统一状态机：IDLE → PLACING → EDITING
 * - IDLE：空闲，无操作（对应原 HIDDEN）
 * - PLACING：预览跟随鼠标或画框中（合并原 HIDDEN+BASE，选物品直接进入）
 * - EDITING：Gizmo 调整或家具拖拽中（原 EDIT）
 */
UENUM(BlueprintType)
enum class EPreviewInteractionState : uint8
{
	IDLE     UMETA(DisplayName = "空闲状态"),
	PLACING  UMETA(DisplayName = "放置/画框状态"),
	EDITING  UMETA(DisplayName = "编辑/拖拽状态")
};

UENUM(BlueprintType)
enum class EVoxelPivotMode : uint8
{
	ObjectCenter    UMETA(DisplayName = "物体中心"),
	Cursor          UMETA(DisplayName = "游标")
};

/**
 * 切面过滤模式
 * - All：全局生效，所有体素/物体统一应用切面虚化
 * - PlayerManaged：仅当前选中区域或玩家管理/拥有的区域内生效；
 *   无自己区域时排除别人的区域（黑名单模式）
 */
UENUM(BlueprintType)
enum class ESliceFilterMode : uint8
{
	All             UMETA(DisplayName = "全局场景"),
	PlayerManaged   UMETA(DisplayName = "仅自己区域"),
};

UENUM(BlueprintType)
enum class EVoxelTransformSpace : uint8
{
	Global    UMETA(DisplayName = "绝对坐标系"),
	Local     UMETA(DisplayName = "局部轴")
};



