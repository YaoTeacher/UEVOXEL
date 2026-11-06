// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Voxel/Editor/VoxelHistoryData.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "VoxelBatchEditTool.generated.h"

class UVoxelBlockWorldSubsystem;
class UTexture2D;

/**
 * 批量编辑工具
 *
 * 支持多种形状的批量体素编辑：
 * - 盒体（Box）
 * - 球形（Sphere）
 * - 圆柱（Cylinder）
 *
 * 功能：
 * - 添加、删除、替换、补全方块
 * - 支持中空形状（厚度参数）
 * - 提供预览显示
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelBatchEditTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	// ==================== UVoxelToolBase 接口实现 ====================

	virtual FText GetToolDisplayName() const override;
	virtual FText GetToolDescription() const override;
	virtual UTexture2D* GetToolIcon() const override;
	virtual EVoxelToolCategory GetToolCategory() const override;
	virtual FName GetToolID() const override;
	virtual FText GetShortcutHint() const override;

	virtual bool ExecuteEdit(const FVoxelEditRequest& Request) override;
	virtual bool ValidateEditRequest(const FVoxelEditRequest& Request) const override;
	virtual bool CanExecuteEdit() const override;

	virtual void OnToolActivated() override;
	virtual void OnToolDeactivated() override;

	virtual FString GetDebugInfo() const override;

	// ==================== 静态工具方法（纯数学计算，无状态） ====================

	/**
	 * 计算 FSingleEditRequest（完整的批量执行信息）
	 * 纯数学计算：将几何形状转换为 BlockIndex + ChunkIndex（含边界检测）
	 *
	 * @param Request 编辑请求（包含几何信息和编辑参数）
	 * @param BlockWorld 方块世界子系统（用于计算 ChunkIndex 和边界检测）
	 * @return FSingleEditRequest（包含 BlockIndex、ChunkIndex、EditMode、BlockType）
	 */
	static FSingleEditRequest CalculateSingleEditRequest(
		const FVoxelEditRequest& Request,
		UVoxelBlockWorldSubsystem* BlockWorld
	);

	/**
	 * 计算受影响的体素集合（仅用于预览，不用于实际编辑）
	 * @param Request 编辑请求（包含几何信息和参数）
	 * @param BlockSize 方块尺寸（用于坐标转换）
	 * @return 受影响的体素位置集合
	 */
	static TArray<FIntVector> CalculateAffectedVoxelsForPreview(
		const FVoxelEditRequest& Request,
		float BlockSize
	);

	/**
	 * 验证批量编辑参数
	 * @param Request 编辑请求
	 * @return true 参数有效，false 参数无效
	 */
	static bool ValidateBatchEditParams(
		const FVoxelEditRequest& Request
	);

	/**
	 * 从编辑请求创建完整的历史记录
	 *
	 * 职责：
	 * - 计算受影响的方块位置
	 * - 读取当前方块类型（OldType）
	 * - 记录目标方块类型（NewType）
	 * - 构造 FVoxelEditHistoryRecord
	 *
	 * @param EditRequest 编辑请求（位置、形状、方块类型、编辑模式）
	 * @param BlockWorld 体素世界引用（用于读取当前状态）
	 * @return 包含完整前后状态的历史记录
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Edit")
	static FVoxelEditHistoryRecord CreateHistoryRecord(
		const FVoxelEditRequest& EditRequest,
		UVoxelBlockWorldSubsystem* BlockWorld);

	// ==================== 形状体素计算（公开静态方法，供 SelectionTool 复用） ====================
	// 这些方法支持完整的几何特性：旋转、bIsRegular（正方体/正球/正圆柱）、Thickness（中空/外扩）、
	// 椭球/椭圆柱/非等比盒体。选区工具直接复用以确保与几何编辑工具完全一致的体素集合。

	/**
	 * 计算盒体形状的体素
	 * 支持旋转、厚度（中空/外扩）、正方体（内接）等高级功能
	 *
	 * @param Geometry 几何描述（包含Box、Rotation、Thickness、bIsRegular等）
	 * @param BlockLength 体素边长（单位：米）
	 * @return 受影响的体素索引集合
	 */
	static TArray<FIntVector> CalculateBoxVoxels(
		const FVoxelGeometryDesc& Geometry,
		float BlockLength
	);

	/**
	 * 计算球形形状的体素（椭球，基于 XYZ 范围生成最大椭球体）
	 */
	static TArray<FIntVector> CalculateSphereVoxels(
		const FVoxelGeometryDesc& Geometry, float BlockLength
	);

	/**
	 * 计算圆柱形状的体素（椭圆底圆柱，基于 XY 范围生成最大椭圆底圆柱）
	 */
	static TArray<FIntVector> CalculateCylinderVoxels(
		const FVoxelGeometryDesc& Geometry, float BlockLength
	);
};
