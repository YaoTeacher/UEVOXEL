// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditCommandEnums.generated.h"

/**
 * 体素放置规则 - 决定"新方块如何覆盖旧方块"
 *
 * 应用场景：
 * - 用于单体素编辑
 * - 用于几何形状编辑
 * - 用于模板放置
 * - 用于选区操作
 */
UENUM(BlueprintType)
enum class EVoxelPlacementMode : uint8
{
	/**
	 * 覆盖模式（默认）
	 * 行为：不管原位置有什么，直接写入新方块
	 * 适用：建造、替换、删除
	 */
	Normal      UMETA(DisplayName = "覆盖"),

	/**
	 * 填充模式
	 * 行为：只在空位（空气）放置，不破坏现有方块
	 * 适用：补全空洞、填充空缺
	 */
	Complement  UMETA(DisplayName = "填充"),

	/**
	 * 替换模式
	 * 行为：只替换非空方块，不填空气
	 * 适用：涂色、批量替换材质
	 */
	Replace     UMETA(DisplayName = "替换"),

	/**
	 * 指定替换模式
	 * 行为：只替换特定类型的方块
	 * 适用：精确替换某类材质
	 * TODO: 需要额外参数指定要替换的目标类型
	 */
	ReplaceMask UMETA(DisplayName = "指定替换")
};

/**
 * 选择运算模式 - 决定"新选区如何影响旧选区"
 *
 * 应用场景：
 * - 用于选区工具（框选、笔刷等）
 * 通常绑定按键修饰键：
 *   - 无修饰键 = New
 *   - Shift = Add
 *   - Alt = Subtract
 *   - Ctrl = Intersect
 */
UENUM(BlueprintType)
enum class ESelectionOpMode : uint8
{
	/**
	 * 新选模式（默认）
	 * 行为：清除旧选区，使用新选区
	 * 适用：首次选择、重新选择
	 */
	New         UMETA(DisplayName = "新选"),

	/**
	 * 加选模式
	 * 行为：新选区 ∪ 旧选区（并集）
	 * 适用：逐步扩大选区
	 */
	Add         UMETA(DisplayName = "加选"),

	/**
	 * 减选模式
	 * 行为：旧选区 - 新选区（差集）
	 * 适用：从选区移除部分
	 */
	Subtract    UMETA(DisplayName = "减选"),

	/**
	 * 交选模式
	 * 行为：新选区 ∩ 旧选区（交集）
	 * 适用：缩小选区到重叠部分
	 */
	Intersect   UMETA(DisplayName = "交选")
};

/**
 * 笔刷维度
 * 控制笔刷影响的范围是单层平面还是3D体积
 */
UENUM(BlueprintType)
enum class EBrushDimension : uint8
{
	/** 平面：只在鼠标命中层涂抹（Z轴固定，单层） */
	Flat        UMETA(DisplayName = "平面"),

	/** 立体：以命中点为中心的3D体积涂抹 */
	Volumetric  UMETA(DisplayName = "立体")
};
