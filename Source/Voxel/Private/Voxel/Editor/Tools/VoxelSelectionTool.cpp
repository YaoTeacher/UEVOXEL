// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/Tools/VoxelSelectionTool.h"
#include "Math/UnrealMathUtility.h"
#include "Voxel/Editor/Tools/VoxelBatchEditTool.h"

// ==================== 主要选择方法 ====================

TArray<FIntVector> UVoxelSelectionTool::CalculateSelection(
	const FVoxelEditRequest& Request,
	float BlockSize,
	const TArray<FIntVector>& ExistingSelection,
	ESelectionOperationType OperationType)
{
	TArray<FIntVector> ShapeSelection;

	// 根据形状类型计算基础选择集
	// 【统一算法】直接复用 BatchEditTool 的形状计算，确保选区工具与几何编辑工具
	// 生成的体素集合完全一致（支持椭球/椭圆柱/非等比盒体/旋转/bIsRegular/Thickness）。
	switch (Request.Geometry.ShapeType)
	{
	case EBatchShapeType::Null:
	case EBatchShapeType::Box:
		// 盒体形状（支持旋转、厚度、正方体等高级功能）
		ShapeSelection = UVoxelBatchEditTool::CalculateBoxVoxels(
			Request.Geometry,
			BlockSize
		);
		break;

	case EBatchShapeType::Sphere:
		{
			// 球形形状（椭球，基于 XYZ 范围生成最大椭球体）
			ShapeSelection = UVoxelBatchEditTool::CalculateSphereVoxels(
				Request.Geometry,
				BlockSize
			);
			break;
		}

	case EBatchShapeType::Cylinder:
		{
			// 圆柱形状（椭圆底圆柱，基于 XY 范围生成最大椭圆底圆柱）
			ShapeSelection = UVoxelBatchEditTool::CalculateCylinderVoxels(
				Request.Geometry,
				BlockSize
			);
			break;
		}

	default:
		break;
	}

	// 获取选择边界（用于反选操作）
	FIntVector BoundsMin, BoundsMax;
	GetSelectionBounds(ShapeSelection, BoundsMin, BoundsMax);

	// 应用选择操作
	return ApplySelectionOperation(
		ShapeSelection,
		ExistingSelection,
		OperationType,
		BoundsMin,
		BoundsMax
	);
}

TArray<FIntVector> UVoxelSelectionTool::CalculateSelection(
	const FVoxelEditRequest& Request,
	float BlockSize)
{
	// 默认为新建选择
	TArray<FIntVector> EmptySelection;
	return CalculateSelection(Request, BlockSize, EmptySelection, ESelectionOperationType::New);
}

// ==================== 选择操作 ====================

TArray<FIntVector> UVoxelSelectionTool::UnionSelection(
	const TArray<FIntVector>& SelectionA,
	const TArray<FIntVector>& SelectionB)
{
	// 使用 TSet 来快速去重
	TSet<FIntVector> ResultSet;
	ResultSet.Append(SelectionA);
	ResultSet.Append(SelectionB);

	return ResultSet.Array();
}

TArray<FIntVector> UVoxelSelectionTool::SubtractSelection(
	const TArray<FIntVector>& SelectionA,
	const TArray<FIntVector>& SelectionB)
{
	TSet<FIntVector> SetA(SelectionA);
	TSet<FIntVector> SetB(SelectionB);

	// 从 A 中移除 B 中的所有元素
	SetA = SetA.Difference(SetB);

	return SetA.Array();
}

TArray<FIntVector> UVoxelSelectionTool::IntersectSelection(
	const TArray<FIntVector>& SelectionA,
	const TArray<FIntVector>& SelectionB)
{
	TSet<FIntVector> SetA(SelectionA);
	TSet<FIntVector> SetB(SelectionB);

	// 计算交集
	SetA = SetA.Intersect(SetB);

	return SetA.Array();
}

TArray<FIntVector> UVoxelSelectionTool::InvertSelection(
	const TArray<FIntVector>& CurrentSelection,
	const FIntVector& BoundsMin,
	const FIntVector& BoundsMax)
{
	TSet<FIntVector> SelectionSet(CurrentSelection);
	TArray<FIntVector> InvertedSelection;

	// 遍历包围盒内的所有体素
	for (int32 X = BoundsMin.X; X <= BoundsMax.X; X++)
	{
		for (int32 Y = BoundsMin.Y; Y <= BoundsMax.Y; Y++)
		{
			for (int32 Z = BoundsMin.Z; Z <= BoundsMax.Z; Z++)
			{
				const FIntVector VoxelIndex(X, Y, Z);

				// 如果体素不在当前选择中，则添加到反选结果
				if (!SelectionSet.Contains(VoxelIndex))
				{
					InvertedSelection.Add(VoxelIndex);
				}
			}
		}
	}

	return InvertedSelection;
}

// ==================== 选择查询 ====================

bool UVoxelSelectionTool::IsVoxelSelected(
	const FIntVector& VoxelIndex,
	const TArray<FIntVector>& Selection)
{
	return Selection.Contains(VoxelIndex);
}

void UVoxelSelectionTool::GetSelectionBounds(
	const TArray<FIntVector>& Selection,
	FIntVector& OutBoundsMin,
	FIntVector& OutBoundsMax)
{
	if (Selection.Num() == 0)
	{
		OutBoundsMin = FIntVector::ZeroValue;
		OutBoundsMax = FIntVector::ZeroValue;
		return;
	}

	OutBoundsMin = Selection[0];
	OutBoundsMax = Selection[0];

	for (const FIntVector& VoxelIndex : Selection)
	{
		OutBoundsMin.X = FMath::Min(OutBoundsMin.X, VoxelIndex.X);
		OutBoundsMin.Y = FMath::Min(OutBoundsMin.Y, VoxelIndex.Y);
		OutBoundsMin.Z = FMath::Min(OutBoundsMin.Z, VoxelIndex.Z);

		OutBoundsMax.X = FMath::Max(OutBoundsMax.X, VoxelIndex.X);
		OutBoundsMax.Y = FMath::Max(OutBoundsMax.Y, VoxelIndex.Y);
		OutBoundsMax.Z = FMath::Max(OutBoundsMax.Z, VoxelIndex.Z);
	}
}

FIntVector UVoxelSelectionTool::GetSelectionCenter(
	const TArray<FIntVector>& Selection)
{
	if (Selection.Num() == 0)
	{
		return FIntVector::ZeroValue;
	}

	FIntVector Sum = FIntVector::ZeroValue;

	for (const FIntVector& VoxelIndex : Selection)
	{
		Sum += VoxelIndex;
	}

	return FIntVector(
		FMath::RoundToInt(static_cast<float>(Sum.X) / Selection.Num()),
		FMath::RoundToInt(static_cast<float>(Sum.Y) / Selection.Num()),
		FMath::RoundToInt(static_cast<float>(Sum.Z) / Selection.Num())
	);
}

int32 UVoxelSelectionTool::GetSelectionSize(
	const TArray<FIntVector>& Selection)
{
	return Selection.Num();
}

bool UVoxelSelectionTool::ValidateSelectionParams(
	const FVoxelEditRequest& Request)
{
	// 验证几何形状参数
	switch (Request.Geometry.ShapeType)
	{
	case EBatchShapeType::Box:
		// 盒体：max 和 min 不能相同
		if (Request.Geometry.Box.isSimilar())
		{
			return false;
		}
		break;

	case EBatchShapeType::Sphere:
	case EBatchShapeType::Cylinder:
		// 几何描述必须有效
		if (!Request.Geometry.IsValid())
		{
			return false;
		}
		break;

	default:
		break;
	}

	return true;
}

// ==================== 形状选择方法（内部使用） ====================

TArray<FIntVector> UVoxelSelectionTool::CalculateBoxSelection(
	const FIntVector& FirstP,
	const FIntVector& SecondP)
{
	TArray<FIntVector> Selection;

	// 计算包围盒的边界
	const FIntVector Min(
		FMath::Min(FirstP.X, SecondP.X),
		FMath::Min(FirstP.Y, SecondP.Y),
		FMath::Min(FirstP.Z, SecondP.Z)
	);
	const FIntVector Max(
		FMath::Max(FirstP.X, SecondP.X),
		FMath::Max(FirstP.Y, SecondP.Y),
		FMath::Max(FirstP.Z, SecondP.Z)
	);

	// 遍历包围盒内的所有体素
	for (int32 X = Min.X; X <= Max.X; X++)
	{
		for (int32 Y = Min.Y; Y <= Max.Y; Y++)
		{
			for (int32 Z = Min.Z; Z <= Max.Z; Z++)
			{
				Selection.Add(FIntVector(X, Y, Z));
			}
		}
	}

	return Selection;
}

TArray<FIntVector> UVoxelSelectionTool::CalculateSphereSelection(
	const FIntVector& Center,
	float Radius)
{
	TArray<FIntVector> Selection;

	const float RadiusSquared = Radius * Radius;
	const int32 RadiusInt = FMath::FloorToInt(Radius);

	// 遍历球体包围盒
	for (int32 X = -RadiusInt; X <= RadiusInt; X++)
	{
		for (int32 Y = -RadiusInt; Y <= RadiusInt; Y++)
		{
			for (int32 Z = -RadiusInt; Z <= RadiusInt; Z++)
			{
				const FIntVector Offset(X, Y, Z);
				const float DistanceSquared = FVector(Offset).SizeSquared();

				if (DistanceSquared <= RadiusSquared)
				{
					Selection.Add(Center + Offset);
				}
			}
		}
	}

	return Selection;
}

TArray<FIntVector> UVoxelSelectionTool::CalculateCylinderSelection(
	const FIntVector& Center,
	float Radius,
	float Height)
{
	TArray<FIntVector> Selection;

	const float RadiusSquared = Radius * Radius;
	const int32 RadiusInt = FMath::FloorToInt(Radius);
	const int32 HalfHeight = FMath::FloorToInt(Height * 0.5f);

	// 遍历圆柱包围盒
	for (int32 X = -RadiusInt; X <= RadiusInt; X++)
	{
		for (int32 Y = -RadiusInt; Y <= RadiusInt; Y++)
		{
			// XY 平面圆形检测
			const float DistanceSquared = static_cast<float>(X * X + Y * Y);

			if (DistanceSquared <= RadiusSquared)
			{
				// Z 轴高度
				for (int32 Z = -HalfHeight; Z <= HalfHeight; Z++)
				{
					Selection.Add(Center + FIntVector(X, Y, Z));
				}
			}
		}
	}

	return Selection;
}

TArray<FIntVector> UVoxelSelectionTool::CalculateBrushSelection(
	const FIntVector& Center,
	float BrushSize,
	EBatchShapeType BrushShape,
	EBrushDimension Dimension)
{
	TArray<FIntVector> Selection;

	if (Dimension == EBrushDimension::Flat)
	{
		// 平面模式：只在 Center.Z 层涂抹（Z偏移=0，单层）
		const int32 Half = FMath::FloorToInt(BrushSize * 0.5f);

		switch (BrushShape)
		{
		case EBatchShapeType::Box:
			{
				// 平面方形：NxN 的正方形，单层
				for (int32 X = -Half; X <= Half; X++)
				{
					for (int32 Y = -Half; Y <= Half; Y++)
					{
						Selection.Add(Center + FIntVector(X, Y, 0));
					}
				}
				break;
			}
		case EBatchShapeType::Sphere:
			{
				// 平面圆形：圆盘（X²+Y² ≤ R²），单层
				const int32 R = FMath::FloorToInt(BrushSize);
				const float RSquared = BrushSize * BrushSize;
				for (int32 X = -R; X <= R; X++)
				{
					for (int32 Y = -R; Y <= R; Y++)
					{
						if (X * X + Y * Y <= RSquared)
						{
							Selection.Add(Center + FIntVector(X, Y, 0));
						}
					}
				}
				break;
			}
		default:
			break;
		}
	}
	else
	{
		// 立体模式：3D 体积涂抹
		switch (BrushShape)
		{
		case EBatchShapeType::Sphere:
			return CalculateSphereSelection(Center, BrushSize);

		case EBatchShapeType::Cylinder:
			return CalculateCylinderSelection(Center, BrushSize, BrushSize);

		case EBatchShapeType::Box:
			{
				const FIntVector HalfSize(FMath::FloorToInt(BrushSize * 0.5f));
				return CalculateBoxSelection(Center - HalfSize, Center + HalfSize);
			}

		default:
			break;
		}
	}

	return Selection;
}

// ==================== 私有方法 ====================

TArray<FIntVector> UVoxelSelectionTool::ApplySelectionOperation(
	const TArray<FIntVector>& ShapeSelection,
	const TArray<FIntVector>& ExistingSelection,
	ESelectionOperationType OperationType,
	const FIntVector& BoundsMin,
	const FIntVector& BoundsMax)
{
	switch (OperationType)
	{
	case ESelectionOperationType::New:
		// 新建选择，直接返回形状选择
		return ShapeSelection;

	case ESelectionOperationType::Add:
		// 添加到选择（并集）
		return UnionSelection(ExistingSelection, ShapeSelection);

	case ESelectionOperationType::Remove:
		// 从选择中移除（差集）
		return SubtractSelection(ExistingSelection, ShapeSelection);

	case ESelectionOperationType::Replace:
		// 替换选择，等同于新建
		return ShapeSelection;

	case ESelectionOperationType::Intersect:
		// 交集选择
		return IntersectSelection(ExistingSelection, ShapeSelection);

	case ESelectionOperationType::Invert:
		// 反选（基于包围盒）
		return InvertSelection(ExistingSelection, BoundsMin, BoundsMax);

	default:
		return ShapeSelection;
	}
}
