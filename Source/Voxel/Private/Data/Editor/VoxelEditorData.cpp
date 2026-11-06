// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/VoxelEditorData.h"
#include "Utils/ChunkUtilsFunction.h"

// 体素编辑器常量定义
namespace VoxelEditorConstants
{
	constexpr float HALF_SIZE = 0.5f;
}

// === FVoxelEditRequest 方法实现 ===

void FVoxelEditRequest::BuildWorldToLocalMatrix() const
{
	// 构建世界到局部空间的变换矩阵
	// 这将世界空间的位置转换到形状的局部空间，考虑了旋转和缩放
	// 注意：这里使用 100.0f 作为默认 BlockLength，实际使用时会传入正确的值
	const FTransform LocalTransform = Geometry.GetTransform(100.0f);
	WorldToLocal = LocalTransform.ToMatrixWithScale().Inverse();
	bIsInitialized = true;
}

bool FVoxelEditRequest::IsPointInside(const FVector& WorldPosition, float BlockSize) const
{
	// 快速包围盒检查
	const FBox LocalWorldBounds = Geometry.GetBound(BlockSize);
	if (!LocalWorldBounds.IsInside(WorldPosition))
	{
		return false;
	}

	// 对于盒形（Null 或 Box），直接返回 true（已经通过包围盒检查）
	if (Geometry.ShapeType == EBatchShapeType::Null || Geometry.ShapeType == EBatchShapeType::Box)
	{
		return true;
	}

	// 对于球形和圆柱形，需要更精确的检查
	// 将世界位置转换到局部空间
	const FTransform LocalTransform = Geometry.GetTransform(BlockSize);
	const FVector LocalPosition = LocalTransform.InverseTransformPosition(WorldPosition);

	const FVector Dimensions = Geometry.GetWorldSize(BlockSize);

	switch (Geometry.ShapeType)
	{
	case EBatchShapeType::Sphere:
		{
			// 球形：半径检查
			const float Radius = FMath::Max(Dimensions.X, Dimensions.Y) * VoxelEditorConstants::HALF_SIZE;
			const float DistanceSquared = LocalPosition.SizeSquared();
			return DistanceSquared <= (Radius * Radius);
		}

	case EBatchShapeType::Cylinder:
		{
			// 圆柱形：底面半径 + 高度检查
			const float Radius = Dimensions.X * VoxelEditorConstants::HALF_SIZE;
			const float HalfHeight = Dimensions.Z * VoxelEditorConstants::HALF_SIZE;

			// 检查 Z 轴（高度）
			if (FMath::Abs(LocalPosition.Z) > HalfHeight)
			{
				return false;
			}

			// 检查 XY 平面（圆形底面）
			const float DistanceSquared = LocalPosition.X * LocalPosition.X +
			                            LocalPosition.Y * LocalPosition.Y;
			return DistanceSquared <= (Radius * Radius);
		}

	default:
		return true;
	}
}



void FVoxelGeometryDesc::MoveGEO(FIntVector Delta)
{
	Box.UpdateOffset(Delta);
}

bool FVoxelGeometryDesc::ModifyCoverage(EAxis::Type Axis, bool bMaxSide, int32 Delta)
{
	int32 NewValue;
	int32 MinValue, MaxValue;

	switch (Axis)
	{
	case EAxis::X:
		MinValue = Box.MinIndex.X;
		MaxValue = Box.MaxIndex.X;
		NewValue = (bMaxSide ? MaxValue : MinValue) + Delta;

		// 约束检查：拒绝无效操作
		if (bMaxSide && NewValue < MinValue)
		{
			UE_LOG(LogTemp, Warning, TEXT("ModifyCoverage rejected: New Max.X (%d) < Min.X (%d)"), NewValue, MinValue);
			return false;
		}
		if (!bMaxSide && NewValue > MaxValue)
		{
			UE_LOG(LogTemp, Warning, TEXT("ModifyCoverage rejected: New Min.X (%d) > Max.X (%d)"), NewValue, MaxValue);
			return false;
		}

		// 应用修改
		if (bMaxSide)
			Box.MaxIndex.X = NewValue;
		else
			Box.MinIndex.X = NewValue;
		break;

	case EAxis::Y:
		MinValue = Box.MinIndex.Y;
		MaxValue = Box.MaxIndex.Y;
		NewValue = (bMaxSide ? MaxValue : MinValue) + Delta;

		if (bMaxSide && NewValue < MinValue)
		{
			UE_LOG(LogTemp, Warning, TEXT("ModifyCoverage rejected: New Max.Y (%d) < Min.Y (%d)"), NewValue, MinValue);
			return false;
		}
		if (!bMaxSide && NewValue > MaxValue)
		{
			UE_LOG(LogTemp, Warning, TEXT("ModifyCoverage rejected: New Min.Y (%d) > Max.Y (%d)"), NewValue, MaxValue);
			return false;
		}

		if (bMaxSide)
			Box.MaxIndex.Y = NewValue;
		else
			Box.MinIndex.Y = NewValue;
		break;

	case EAxis::Z:
		MinValue = Box.MinIndex.Z;
		MaxValue = Box.MaxIndex.Z;
		NewValue = (bMaxSide ? MaxValue : MinValue) + Delta;

		if (bMaxSide && NewValue < MinValue)
		{
			UE_LOG(LogTemp, Warning, TEXT("ModifyCoverage rejected: New Max.Z (%d) < Min.Z (%d)"), NewValue, MinValue);
			return false;
		}
		if (!bMaxSide && NewValue > MaxValue)
		{
			UE_LOG(LogTemp, Warning, TEXT("ModifyCoverage rejected: New Min.Z (%d) > Max.Z (%d)"), NewValue, MaxValue);
			return false;
		}

		if (bMaxSide)
			Box.MaxIndex.Z = NewValue;
		else
			Box.MinIndex.Z = NewValue;
		break;

	default:
		UE_LOG(LogTemp, Warning, TEXT("ModifyCoverage: Invalid Axis"));
		return false;
	}

	return true;
}

void FVoxelGeometryDesc::RotateGEO(FRotator DeltaRotation)
{
	Rotation += DeltaRotation;
}
