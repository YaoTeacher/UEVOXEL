// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelToolBase.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "VoxelSelectionTool.generated.h"

/**
 * 选择操作类型
 */
UENUM(BlueprintType)
enum class ESelectionOperationType : uint8
{
	/** 新建选择（清除旧选择） */
	New UMETA(DisplayName = "New Selection"),

	/** 添加到选择（并集） */
	Add UMETA(DisplayName = "Add to Selection"),

	/** 从选择中移除（差集） */
	Remove UMETA(DisplayName = "Remove from Selection"),

	/** 替换选择 */
	Replace UMETA(DisplayName = "Replace Selection"),

	/** 交集选择 */
	Intersect UMETA(DisplayName = "Intersect Selection"),

	/** 反选（反转当前选择） */
	Invert UMETA(DisplayName = "Invert Selection"),
};

/**
 * 体素选择工具（无状态方法类）
 *
 * 设计原则：
 * - 只提供静态方法，不存储任何状态
 * - 所有参数通过 Request 或参数传递
 * - 可安全多线程调用
 *
 * 职责：
 * - 计算各种形状的选择区域
 * - 支持多种选择操作类型（新建、添加、移除、替换、交集、反选）
 * - 提供选择集的布尔运算
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelSelectionTool: public UVoxelToolBase
{
	GENERATED_BODY()

public:
	// ==================== 主要选择方法 ====================

	/**
	 * 计算选中的体素集合
	 * @param Request 编辑请求（包含几何信息和形状参数）
	 * @param BlockSize 方块尺寸（用于坐标转换）
	 * @param ExistingSelection 已存在的选择集合（用于 Add/Remove/Intersect 操作）
	 * @param OperationType 选择操作类型
	 * @return 选中的体素位置集合
	 */
	static TArray<FIntVector> CalculateSelection(
		const FVoxelEditRequest& Request,
		float BlockSize,
		const TArray<FIntVector>& ExistingSelection,
		ESelectionOperationType OperationType
	);

	/**
	 * 计算选中的体素集合（简化版本，默认为新建选择）
	 * @param Request 编辑请求
	 * @param BlockSize 方块尺寸
	 * @return 选中的体素位置集合
	 */
	static TArray<FIntVector> CalculateSelection(
		const FVoxelEditRequest& Request,
		float BlockSize
	);

	// ==================== 选择操作 ====================

	/**
	 * 合并两个选择集（并集）
	 * @param SelectionA 第一个选择集
	 * @param SelectionB 第二个选择集
	 * @return 合并后的选择集
	 */
	static TArray<FIntVector> UnionSelection(
		const TArray<FIntVector>& SelectionA,
		const TArray<FIntVector>& SelectionB
	);

	/**
	 * 从选择集中移除（差集）
	 * @param SelectionA 原始选择集
	 * @param SelectionB 要移除的选择集
	 * @return 移除后的选择集
	 */
	static TArray<FIntVector> SubtractSelection(
		const TArray<FIntVector>& SelectionA,
		const TArray<FIntVector>& SelectionB
	);

	/**
	 * 交集选择
	 * @param SelectionA 第一个选择集
	 * @param SelectionB 第二个选择集
	 * @return 交集后的选择集
	 */
	static TArray<FIntVector> IntersectSelection(
		const TArray<FIntVector>& SelectionA,
		const TArray<FIntVector>& SelectionB
	);

	/**
	 * 反转选择（基于包围盒内的所有体素）
	 * @param CurrentSelection 当前选择集
	 * @param Bounds 包围盒（用于定义反选的范围）
	 * @return 反转后的选择集
	 */
	static TArray<FIntVector> InvertSelection(
		const TArray<FIntVector>& CurrentSelection,
		const FIntVector& BoundsMin,
		const FIntVector& BoundsMax
	);

	// ==================== 选择查询 ====================

	/**
	 * 检查体素是否在选择集中
	 * @param VoxelIndex 体素位置
	 * @param Selection 选择集
	 * @return true 如果体素在选择集中
	 */
	static bool IsVoxelSelected(
		const FIntVector& VoxelIndex,
		const TArray<FIntVector>& Selection
	);

	/**
	 * 获取选择的包围盒
	 * @param Selection 选择集
	 * @return 包围盒的 Min 和 Max
	 */
	static void GetSelectionBounds(
		const TArray<FIntVector>& Selection,
		FIntVector& OutBoundsMin,
		FIntVector& OutBoundsMax
	);

	/**
	 * 获取选择的中心点
	 * @param Selection 选择集
	 * @return 选择集的中心点
	 */
	static FIntVector GetSelectionCenter(
		const TArray<FIntVector>& Selection
	);

	/**
	 * 获取选择的大小
	 * @param Selection 选择集
	 * @return 选择集的体素数量
	 */
	static int32 GetSelectionSize(
		const TArray<FIntVector>& Selection
	);

	/**
	 * 验证选择参数
	 * @param Request 编辑请求
	 * @return true 参数有效，false 参数无效
	 */
	static bool ValidateSelectionParams(
		const FVoxelEditRequest& Request
	);

	/**
	 * 计算画笔形状的体素（基于画笔大小、形状和维度）
	 * 供 Brush 选区工具外部调用
	 * @param Center 笔刷中心体素索引
	 * @param BrushSize 笔刷尺寸
	 * @param BrushShape 笔刷形状（Box=方形, Sphere=圆形）
	 * @param Dimension 笔刷维度（Flat=平面单层, Volumetric=立体体积）
	 */
	static TArray<FIntVector> CalculateBrushSelection(
		const FIntVector& Center,
		float BrushSize,
		EBatchShapeType BrushShape,
		EBrushDimension Dimension = EBrushDimension::Volumetric
	);

protected:
	// ==================== 形状选择方法（内部使用） ====================

	/**
	 * 计算盒体形状的体素
	 */
	static TArray<FIntVector> CalculateBoxSelection(
		const FIntVector& FirstP,
		const FIntVector& SecondP
	);

	/**
	 * 计算球形形状的体素
	 */
	static TArray<FIntVector> CalculateSphereSelection(
		const FIntVector& Center,
		float Radius
	);

	/**
	 * 计算圆柱形状的体素
	 */
	static TArray<FIntVector> CalculateCylinderSelection(
		const FIntVector& Center,
		float Radius,
		float Height
	);

private:
	/**
	 * 应用选择操作到形状选择集
	 */
	static TArray<FIntVector> ApplySelectionOperation(
		const TArray<FIntVector>& ShapeSelection,
		const TArray<FIntVector>& ExistingSelection,
		ESelectionOperationType OperationType,
		const FIntVector& BoundsMin,
		const FIntVector& BoundsMax
	);
};
