// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/Tools/VoxelBatchEditTool.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Voxel/Editor/VoxelPreviewActor.h"
#include "Math/UnrealMathUtility.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Core/CoreTags.h"

namespace
{
	/**
	 * 壳体面归属过滤（仅壳模式 Thickness != 0 且 ShellFaces 非空时由调用方启用）
	 *
	 * 面归属按形状定义（采样点已在形状局部空间，旋转天然支持）：
	 * - Box：slab 并集——体素局部坐标沿某轴进入该面条带层（超过 FaceBoundary）即归属该面，
	 *        棱/角体素归相邻面共有，任一勾选面命中即保留。
	 *        FaceBoundary：内壳（Thickness>0）传 InnerExtents，外壳（Thickness<0）传 BaseExtents。
	 * - Sphere：dominant axis 三维扇区（|X|,|Y|,|Z| 最大分量的轴+符号），唯一归属。
	 * - Cylinder：顶/底盖按 Z slab 判定（超过 FaceBoundary.Z → Up/Down），
	 *        侧壁按水平 dominant axis 分 4 个 90° 扇区（前/后/左/右）。
	 *
	 * 方向-轴映射（与 CalculateWallBlocks 一致）：Forward=+X Backward=-X Right=+Y Left=-Y Up=+Z Down=-Z
	 */
	bool IsShellVoxelInFaces(const FVector& LocalPos, EBatchShapeType ShapeType,
		const FVector& FaceBoundary, const TSet<EActorDirection>& ShellFaces)
	{
		if (ShapeType == EBatchShapeType::Box)
		{
			if (ShellFaces.Contains(EActorDirection::Forward)  && LocalPos.X >  FaceBoundary.X) return true;
			if (ShellFaces.Contains(EActorDirection::Backward) && LocalPos.X < -FaceBoundary.X) return true;
			if (ShellFaces.Contains(EActorDirection::Right)    && LocalPos.Y >  FaceBoundary.Y) return true;
			if (ShellFaces.Contains(EActorDirection::Left)     && LocalPos.Y < -FaceBoundary.Y) return true;
			if (ShellFaces.Contains(EActorDirection::Up)       && LocalPos.Z >  FaceBoundary.Z) return true;
			if (ShellFaces.Contains(EActorDirection::Down)     && LocalPos.Z < -FaceBoundary.Z) return true;
			return false;
		}

		const float AX = FMath::Abs(LocalPos.X);
		const float AY = FMath::Abs(LocalPos.Y);
		const float AZ = FMath::Abs(LocalPos.Z);

		EActorDirection Belong;
		if (ShapeType == EBatchShapeType::Cylinder)
		{
			// 顶/底盖优先按 Z slab 判定；侧壁按水平 dominant axis 分扇区
			if (LocalPos.Z > FaceBoundary.Z)       Belong = EActorDirection::Up;
			else if (LocalPos.Z < -FaceBoundary.Z) Belong = EActorDirection::Down;
			else if (AX >= AY)                     Belong = LocalPos.X > 0 ? EActorDirection::Forward : EActorDirection::Backward;
			else                                   Belong = LocalPos.Y > 0 ? EActorDirection::Right : EActorDirection::Left;
		}
		else // Sphere：纯 dominant axis 扇区
		{
			if (AZ >= AX && AZ >= AY) Belong = LocalPos.Z > 0 ? EActorDirection::Up : EActorDirection::Down;
			else if (AX >= AY)        Belong = LocalPos.X > 0 ? EActorDirection::Forward : EActorDirection::Backward;
			else                      Belong = LocalPos.Y > 0 ? EActorDirection::Right : EActorDirection::Left;
		}
		return ShellFaces.Contains(Belong);
	}
}

// ==================== UVoxelToolBase 接口实现 ====================

FText UVoxelBatchEditTool::GetToolDisplayName() const
{
	return NSLOCTEXT("VoxelTools", "BatchEdit", "批量编辑");
}

FText UVoxelBatchEditTool::GetToolDescription() const
{
	return NSLOCTEXT("VoxelTools", "BatchEditDesc",
		"批量编辑工具：支持盒体、球形、圆柱等多种形状的体素编辑\n"
		"快捷键：B"
	);
}

UTexture2D* UVoxelBatchEditTool::GetToolIcon() const
{
	// TODO: 返回工具图标资源
	// return LoadObject<UTexture2D>(nullptr, TEXT("/Path/To/Icon"));
	return nullptr;
}

EVoxelToolCategory UVoxelBatchEditTool::GetToolCategory() const
{
	return EVoxelToolCategory::Edit;
}

FName UVoxelBatchEditTool::GetToolID() const
{
	return FName("BatchEdit");
}

FText UVoxelBatchEditTool::GetShortcutHint() const
{
	return NSLOCTEXT("VoxelTools", "BatchEditShortcut", "B");
}

bool UVoxelBatchEditTool::ExecuteEdit(const FVoxelEditRequest& Request)
{
	// 获取 BlockWorld 子系统
	if (UWorld* World = GetWorld())
	{
		if (UVoxelBlockWorldSubsystem* BlockWorld = World->GetSubsystem<UVoxelBlockWorldSubsystem>())
		{
			// 使用新架构：计算完整的编辑请求 → 执行
			FSingleEditRequest SingleEditRequest = CalculateSingleEditRequest(Request, BlockWorld);

			if (SingleEditRequest.BlockIndex.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("BatchEditTool::ExecuteEdit: No valid blocks to edit"));
				return false;
			}

			// 执行编辑
			BlockWorld->BatchModifyVoxelData_Single(SingleEditRequest);
			return true;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("BatchEditTool::ExecuteEdit: Failed to get BlockWorld"));
	return false;
}

bool UVoxelBatchEditTool::ValidateEditRequest(const FVoxelEditRequest& Request) const
{
	return ValidateBatchEditParams(Request);
}

bool UVoxelBatchEditTool::CanExecuteEdit() const
{
	return true;
}

void UVoxelBatchEditTool::OnToolActivated()
{
	Super::OnToolActivated();
	UE_LOG(LogTemp, Log, TEXT("BatchEditTool activated: Supports Box, Sphere, Cylinder shapes"));
}

void UVoxelBatchEditTool::OnToolDeactivated()
{
	Super::OnToolDeactivated();
	// 无状态工具，无需清理缓存
}

FString UVoxelBatchEditTool::GetDebugInfo() const
{
	return FString::Printf(TEXT("BatchEditTool - Stateless Tool"));
}

// ==================== 静态工具方法（纯数学计算，无状态） ====================

FSingleEditRequest UVoxelBatchEditTool::CalculateSingleEditRequest(
	const FVoxelEditRequest& Request,
	UVoxelBlockWorldSubsystem* BlockWorld)
{
	FSingleEditRequest Result;
	Result.EditCommand = Request.EditCommand;
	Result.NewHandleId = Request.HandleId;

	if (!BlockWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("BatchEditTool::CalculateSingleEditRequest: BlockWorld is null"));
		return Result;
	}

	// 1. 计算受影响的 BlockIndex（数学计算：形状 → 体素）
	TArray<FIntVector> AffectedVoxels;

	// 特殊处理：单点编辑（FirstP == SecondP，或 ShapeType == Null）
	if (Request.Geometry.Box.isSimilar() && Request.Geometry.ShapeType == EBatchShapeType::Null)
	{
		// 单点编辑：直接使用 TargetPosition 转换的体素索引
		const FIntVector VoxelIndex=Request.Geometry.Box.MinIndex;
		AffectedVoxels.Add(VoxelIndex);
	}
	else
	{
		// 批量编辑：使用形状计算
		AffectedVoxels = CalculateAffectedVoxelsForPreview(Request, BlockWorld->BlockLength);
	}

	if (AffectedVoxels.Num() == 0)
	{
		return Result;
	}

	// 2. 预分配内存
	Result.BlockIndex.Reserve(AffectedVoxels.Num());
	Result.ChunkIndex.Reserve(AffectedVoxels.Num() * 2); // 平均每个方块影响1-2个区块

	// 3. 计算 ChunkIndex（含边界检测）
	for (const FIntVector& BlockIndex : AffectedVoxels)
	{
		if (!BlockWorld->IsBlockIndexInScene(BlockIndex))
		{
			continue;
		}

		// 记录旧方块类型（用于撤销/重做历史记录）
		int32 OldBlockType = BlockWorld->GetBlockType(BlockIndex);
		Result.PreviousBlocks.Add(BlockIndex, OldBlockType);

		// 添加到 BlockIndex 集合（自动去重）
		Result.BlockIndex.Add(BlockIndex);

		// 计算 ChunkIndex
		const FIntVector ChunkIndex = UChunkUtilsFunction::Get3DChunkIndexByBlockIndex(
			BlockIndex,
			BlockWorld->ChunkSize
		);

		// 添加到 ChunkIndex 集合（自动去重）
		Result.ChunkIndex.Add(ChunkIndex);

		// 边界检测（可能影响相邻区块）
		BlockWorld->IsBlockOnBorder(BlockIndex, ChunkIndex, Result.ChunkIndex);
	}

	return Result;
}

TArray<FIntVector> UVoxelBatchEditTool::CalculateAffectedVoxelsForPreview(
	const FVoxelEditRequest& Request,
	float BlockSize)
{
	TArray<FIntVector> AffectedVoxels;

	// 根据形状类型调用不同的计算方法
	switch (Request.Geometry.ShapeType)
	{
	case EBatchShapeType::Null:
		
	case EBatchShapeType::Box:
		// 盒体形状（支持旋转、厚度、正方体等高级功能）
		AffectedVoxels = CalculateBoxVoxels(
		Request.Geometry, BlockSize
		);
		break;

	case EBatchShapeType::Sphere:
		{
			// 球形形状
			AffectedVoxels = CalculateSphereVoxels(
			Request.Geometry, BlockSize
			);
			break;
		}

	case EBatchShapeType::Cylinder:
		{
			// 圆柱形状
			AffectedVoxels = CalculateCylinderVoxels(
			Request.Geometry, BlockSize
			);
			break;
		}

	default:
		break;
	}

	return AffectedVoxels;
}

bool UVoxelBatchEditTool::ValidateBatchEditParams(const FVoxelEditRequest& Request)
{
	// 验证几何形状参数
	switch (Request.Geometry.ShapeType)
	{
	case EBatchShapeType::Box:
		// 盒体：MinIndex 和 MaxIndex 不能相同
		if (Request.Geometry.Box.isSimilar())
		{
			UE_LOG(LogTemp, Error, TEXT("ValidateBatchEditParams: Box shape has identical points - MinIndex=%s, MaxIndex=%s"),
				*Request.Geometry.Box.MinIndex.ToString(), *Request.Geometry.Box.MaxIndex.ToString());
			return false;
		}
		break;

	case EBatchShapeType::Sphere:
	case EBatchShapeType::Cylinder:
		// TODO: 添加更多验证
		break;

	default:
		UE_LOG(LogTemp, Error, TEXT("ValidateBatchEditParams: Unknown shape type=%d"), (int32)Request.Geometry.ShapeType);
		return false;
	}

	// 验证方块类型
	if (Request.HandleId == EBlockId::Invalid)
	{
		UE_LOG(LogTemp, Error, TEXT("ValidateBatchEditParams: Invalid block type - ID=%d"), Request.HandleId);
		return false;
	}

	return true;
}

TArray<FIntVector> UVoxelBatchEditTool::CalculateBoxVoxels(
	const FVoxelGeometryDesc& Geometry,
	const float BlockLength)
{
	TArray<FIntVector> Voxels;

	// 获取原始尺寸（体素单位 -> 世界单位）
	FVector RawSizeVoxel = FVector(Geometry.GetSize());
	FVector BaseSize = RawSizeVoxel * BlockLength;

	// --- 处理 IsRegular ---
	if (Geometry.bIsRegular)
	{
		float MinSide = FMath::Min3(BaseSize.X, BaseSize.Y, BaseSize.Z);
		BaseSize = FVector(MinSide);
	}

	FVector BaseExtents = BaseSize * 0.5f;

	// --- 处理 Thickness ---
	FVector OuterExtents;
	FVector InnerExtents;
	bool bCheckInner = false;
	float WorldThickness = Geometry.Thickness * BlockLength;

	if (Geometry.Thickness == 0)
	{
		OuterExtents = BaseExtents;
		bCheckInner = false;
	}
	else if (Geometry.Thickness > 0)
	{
		OuterExtents = BaseExtents;
		InnerExtents = BaseExtents - FVector(WorldThickness);
		bCheckInner = (InnerExtents.X > 0 && InnerExtents.Y > 0 && InnerExtents.Z > 0);
	}
	else // Thickness < 0
	{
		float AbsThickness = FMath::Abs(WorldThickness);
		OuterExtents = BaseExtents + FVector(AbsThickness);
		InnerExtents = BaseExtents;
		bCheckInner = true;
	}

	// --- 壳体面子集过滤（仅壳模式且 ShellFaces 非空时启用；空集合=全部面，兼容旧行为） ---
	const bool bFilterFaces = Geometry.Thickness != 0 && Geometry.ShellFaces.Num() > 0;
	// 面归属判定的边界：内壳（正厚度）用 InnerExtents，外壳（负厚度）用 BaseExtents
	const FVector FaceBoundary = (Geometry.Thickness > 0) ? InnerExtents : BaseExtents;

	// ==========================================
	// [核心修复] 构建正确的空间变换
	// ==========================================

	// 1. 获取旋转轴心 (Pivot) 和 几何中心 (Geometric Center)
	FVector Pivot = Geometry.GetGeometricCenter(BlockLength);
	FVector GeometricCenter = Geometry.GetGeometricCenter(BlockLength);
	
	// 2. 计算偏移量 (未旋转前)
	FVector OffsetFromPivot = GeometricCenter - Pivot;

	// 3. 计算旋转后，形状实体在世界空间中的实际中心位置
	//    这也是 PreviewActor 放置 Mesh 的位置
	FVector ShapeWorldCenter = Pivot + Geometry.Rotation.RotateVector(OffsetFromPivot);

	// 4. 构建变换矩阵：以形状实体中心为原点
	FTransform LocalToWorld(Geometry.Rotation, ShapeWorldCenter, FVector::OneVector);

	// ==========================================
	// 计算遍历范围 (AABB)
	// ==========================================
	FBox LocalBox(-OuterExtents, OuterExtents);
	FBox WorldAABB = LocalBox.TransformBy(LocalToWorld);

	constexpr float Epsilon = KINDA_SMALL_NUMBER;
	FIntVector LoopMin = FIntVector(FMath::FloorToInt((WorldAABB.Min.X / BlockLength) - Epsilon),
	                                FMath::FloorToInt((WorldAABB.Min.Y / BlockLength) - Epsilon),
	                                FMath::FloorToInt((WorldAABB.Min.Z / BlockLength) - Epsilon));

	FIntVector LoopMax = FIntVector(FMath::CeilToInt((WorldAABB.Max.X / BlockLength) + Epsilon),
	                                FMath::CeilToInt((WorldAABB.Max.Y / BlockLength) + Epsilon),
	                                FMath::CeilToInt((WorldAABB.Max.Z / BlockLength) + Epsilon));

	// ==========================================
	// 遍历与采样
	// ==========================================
	const int32 EstimatedCount = (LoopMax.X - LoopMin.X + 1) * (LoopMax.Y - LoopMin.Y + 1) * (LoopMax.Z - LoopMin.Z + 1);
	Voxels.Reserve(EstimatedCount);

	for (int32 Z = LoopMin.Z; Z <= LoopMax.Z; Z++)
	{
		for (int32 Y = LoopMin.Y; Y <= LoopMax.Y; Y++)
		{
			for (int32 X = LoopMin.X; X <= LoopMax.X; X++)
			{
				FVector VoxelWorldPos = FVector(X + 0.5f, Y + 0.5f, Z + 0.5f) * BlockLength;

				// [核心修复] 直接逆变换，不再需要手动偏移修正
				// 因为 LocalToWorld 已经包含了正确的中心位置
				FVector LocalPos = LocalToWorld.InverseTransformPosition(VoxelWorldPos);

				if (FMath::Abs(LocalPos.X) <= OuterExtents.X &&
				    FMath::Abs(LocalPos.Y) <= OuterExtents.Y &&
				    FMath::Abs(LocalPos.Z) <= OuterExtents.Z)
				{
					bool bInHole = false;
					if (bCheckInner)
					{
						if (FMath::Abs(LocalPos.X) < InnerExtents.X &&
						    FMath::Abs(LocalPos.Y) < InnerExtents.Y &&
						    FMath::Abs(LocalPos.Z) < InnerExtents.Z)
						{
							bInHole = true;
						}
					}

					if (!bInHole
						&& (!bFilterFaces || IsShellVoxelInFaces(LocalPos, EBatchShapeType::Box, FaceBoundary, Geometry.ShellFaces)))
					{
						Voxels.Add(FIntVector(X, Y, Z));
					}
				}
			}
		}
	}

	return Voxels;
}

TArray<FIntVector> UVoxelBatchEditTool::CalculateSphereVoxels(
    const FVoxelGeometryDesc& Geometry, 
    float BlockLength)
{
    TArray<FIntVector> Voxels;

    FVector BaseSize = FVector(Geometry.GetSize()) * BlockLength;
    if (Geometry.bIsRegular)
    {
        float MinDiameter = FMath::Min3(BaseSize.X, BaseSize.Y, BaseSize.Z);
        BaseSize = FVector(MinDiameter);
    }
    FVector BaseExtents = BaseSize * 0.5f;

    FVector OuterExtents, InnerExtents;
    bool bCheckInner = false;
    float WorldThickness = Geometry.Thickness * BlockLength;

    if (Geometry.Thickness == 0) {
        OuterExtents = BaseExtents;
    } else if (Geometry.Thickness > 0) {
        OuterExtents = BaseExtents;
        InnerExtents = BaseExtents - FVector(WorldThickness);
        bCheckInner = (InnerExtents.X > 0 && InnerExtents.Y > 0 && InnerExtents.Z > 0);
    } else {
        OuterExtents = BaseExtents + FVector(FMath::Abs(WorldThickness));
        InnerExtents = BaseExtents;
        bCheckInner = true;
    }
    
    // 安全检查
    OuterExtents = OuterExtents.ComponentMax(FVector(0.001f));
    if(bCheckInner) InnerExtents = InnerExtents.ComponentMax(FVector(0.001f));

    // --- 壳体面子集过滤（仅壳模式且 ShellFaces 非空时启用） ---
    const bool bFilterFaces = Geometry.Thickness != 0 && Geometry.ShellFaces.Num() > 0;
    const FVector FaceBoundary = (Geometry.Thickness > 0) ? InnerExtents : BaseExtents;

    // [核心修复] 使用 ShapeWorldCenter 构建变换
    FVector Pivot = Geometry.GetGeometricCenter(BlockLength);
    FVector GeometricCenter = Geometry.GetGeometricCenter(BlockLength);
    FVector ShapeWorldCenter = Pivot + Geometry.Rotation.RotateVector(GeometricCenter - Pivot);

    FTransform LocalToWorld(Geometry.Rotation, ShapeWorldCenter, FVector::OneVector);

    // 计算 AABB
    FBox LocalBox(-OuterExtents, OuterExtents);
    FBox WorldAABB = LocalBox.TransformBy(LocalToWorld);

    FIntVector LoopMin = FIntVector(FMath::FloorToInt((WorldAABB.Min.X / BlockLength) - 0.01f),
                                    FMath::FloorToInt((WorldAABB.Min.Y / BlockLength) - 0.01f),
                                    FMath::FloorToInt((WorldAABB.Min.Z / BlockLength) - 0.01f));
    FIntVector LoopMax = FIntVector(FMath::CeilToInt((WorldAABB.Max.X / BlockLength) + 0.01f),
                                    FMath::CeilToInt((WorldAABB.Max.Y / BlockLength) + 0.01f),
                                    FMath::CeilToInt((WorldAABB.Max.Z / BlockLength) + 0.01f));

    Voxels.Reserve((LoopMax.X - LoopMin.X) * (LoopMax.Y - LoopMin.Y));

    float InvOuterXSq = 1.0f / (OuterExtents.X * OuterExtents.X);
    float InvOuterYSq = 1.0f / (OuterExtents.Y * OuterExtents.Y);
    float InvOuterZSq = 1.0f / (OuterExtents.Z * OuterExtents.Z);

    float InvInnerXSq = bCheckInner ? 1.0f / (InnerExtents.X * InnerExtents.X) : 0.0f;
    float InvInnerYSq = bCheckInner ? 1.0f / (InnerExtents.Y * InnerExtents.Y) : 0.0f;
    float InvInnerZSq = bCheckInner ? 1.0f / (InnerExtents.Z * InnerExtents.Z) : 0.0f;

    for (int32 Z = LoopMin.Z; Z <= LoopMax.Z; Z++)
    {
        for (int32 Y = LoopMin.Y; Y <= LoopMax.Y; Y++)
        {
            for (int32 X = LoopMin.X; X <= LoopMax.X; X++)
            {
                FVector VoxelCenterWorldPos = FVector(X + 0.5f, Y + 0.5f, Z + 0.5f) * BlockLength;
                
                // [核心修复] 直接逆变换
                FVector LocalPos = LocalToWorld.InverseTransformPosition(VoxelCenterWorldPos);

                float DistSqOuter = (LocalPos.X * LocalPos.X) * InvOuterXSq + 
                                    (LocalPos.Y * LocalPos.Y) * InvOuterYSq +
                                    (LocalPos.Z * LocalPos.Z) * InvOuterZSq;

                if (DistSqOuter <= 1.0f)
                {
                    bool bInHole = false;
                    if (bCheckInner)
                    {
                        float DistSqInner = (LocalPos.X * LocalPos.X) * InvInnerXSq + 
                                            (LocalPos.Y * LocalPos.Y) * InvInnerYSq +
                                            (LocalPos.Z * LocalPos.Z) * InvInnerZSq;
                        if (DistSqInner < 1.0f) bInHole = true;
                    }

                    if (!bInHole
                        && (!bFilterFaces || IsShellVoxelInFaces(LocalPos, EBatchShapeType::Sphere, FaceBoundary, Geometry.ShellFaces)))
                    {
                        Voxels.Add(FIntVector(X, Y, Z));
                    }
                }
            }
        }
    }
    return Voxels;
}

TArray<FIntVector> UVoxelBatchEditTool::CalculateCylinderVoxels(
    const FVoxelGeometryDesc& Geometry, 
    float BlockLength)
{
    TArray<FIntVector> Voxels;

    FVector BaseSize = FVector(Geometry.GetSize()) * BlockLength;
    if (Geometry.bIsRegular)
    {
        float Diameter = FMath::Min(BaseSize.X, BaseSize.Y);
        BaseSize.X = Diameter;
        BaseSize.Y = Diameter;
    }
    FVector BaseExtents = BaseSize * 0.5f;

    FVector OuterExtents, InnerExtents;
    bool bCheckInner = false;
    float WorldThickness = Geometry.Thickness * BlockLength;

    if (Geometry.Thickness == 0) {
        OuterExtents = BaseExtents;
    } else if (Geometry.Thickness > 0) {
        OuterExtents = BaseExtents;
        InnerExtents = BaseExtents - FVector(WorldThickness);
        bCheckInner = (InnerExtents.X > 0 && InnerExtents.Y > 0 && InnerExtents.Z > 0);
    } else {
        OuterExtents = BaseExtents + FVector(FMath::Abs(WorldThickness));
        InnerExtents = BaseExtents;
        bCheckInner = true;
    }
    
    OuterExtents = OuterExtents.ComponentMax(FVector(0.001f));
    if(bCheckInner) InnerExtents = InnerExtents.ComponentMax(FVector(0.001f));

    // --- 壳体面子集过滤（仅壳模式且 ShellFaces 非空时启用） ---
    const bool bFilterFaces = Geometry.Thickness != 0 && Geometry.ShellFaces.Num() > 0;
    const FVector FaceBoundary = (Geometry.Thickness > 0) ? InnerExtents : BaseExtents;

    // [核心修复] 使用 ShapeWorldCenter 构建变换
    FVector Pivot = Geometry.GetGeometricCenter(BlockLength);
    FVector GeometricCenter = Geometry.GetGeometricCenter(BlockLength);
    FVector ShapeWorldCenter = Pivot + Geometry.Rotation.RotateVector(GeometricCenter - Pivot);

    FTransform LocalToWorld(Geometry.Rotation, ShapeWorldCenter, FVector::OneVector);

    // 计算 AABB
    FBox LocalBox(-OuterExtents, OuterExtents);
    FBox WorldAABB = LocalBox.TransformBy(LocalToWorld);

    FIntVector LoopMin = FIntVector(FMath::FloorToInt((WorldAABB.Min.X / BlockLength) - 0.01f),
                                    FMath::FloorToInt((WorldAABB.Min.Y / BlockLength) - 0.01f),
                                    FMath::FloorToInt((WorldAABB.Min.Z / BlockLength) - 0.01f));
    FIntVector LoopMax = FIntVector(FMath::CeilToInt((WorldAABB.Max.X / BlockLength) + 0.01f),
                                    FMath::CeilToInt((WorldAABB.Max.Y / BlockLength) + 0.01f),
                                    FMath::CeilToInt((WorldAABB.Max.Z / BlockLength) + 0.01f));

    Voxels.Reserve((LoopMax.X - LoopMin.X) * (LoopMax.Y - LoopMin.Y));

    float InvOuterXSq = 1.0f / (OuterExtents.X * OuterExtents.X);
    float InvOuterYSq = 1.0f / (OuterExtents.Y * OuterExtents.Y);
    float InvInnerXSq = bCheckInner ? 1.0f / (InnerExtents.X * InnerExtents.X) : 0.0f;
    float InvInnerYSq = bCheckInner ? 1.0f / (InnerExtents.Y * InnerExtents.Y) : 0.0f;

    for (int32 Z = LoopMin.Z; Z <= LoopMax.Z; Z++)
    {
        for (int32 Y = LoopMin.Y; Y <= LoopMax.Y; Y++)
        {
            for (int32 X = LoopMin.X; X <= LoopMax.X; X++)
            {
                FVector VoxelCenterWorldPos = FVector(X + 0.5f, Y + 0.5f, Z + 0.5f) * BlockLength;
                
                // [核心修复] 直接逆变换
                FVector LocalPos = LocalToWorld.InverseTransformPosition(VoxelCenterWorldPos);

                if (FMath::Abs(LocalPos.Z) > OuterExtents.Z) continue;

                float DistSqOuter = (LocalPos.X * LocalPos.X) * InvOuterXSq + 
                                    (LocalPos.Y * LocalPos.Y) * InvOuterYSq;

                if (DistSqOuter <= 1.0f)
                {
                    bool bInHole = false;
                    if (bCheckInner)
                    {
                        if (FMath::Abs(LocalPos.Z) < InnerExtents.Z)
                        {
                            float DistSqInner = (LocalPos.X * LocalPos.X) * InvInnerXSq + 
                                                (LocalPos.Y * LocalPos.Y) * InvInnerYSq;
                            if (DistSqInner < 1.0f) bInHole = true;
                        }
                    }

                    if (!bInHole
                        && (!bFilterFaces || IsShellVoxelInFaces(LocalPos, EBatchShapeType::Cylinder, FaceBoundary, Geometry.ShellFaces)))
                    {
                        Voxels.Add(FIntVector(X, Y, Z));
                    }
                }
            }
        }
    }
    return Voxels;
}

// ==================== 历史记录支持 ====================

FVoxelEditHistoryRecord UVoxelBatchEditTool::CreateHistoryRecord(
    const FVoxelEditRequest& EditRequest,
    UVoxelBlockWorldSubsystem* BlockWorld)
{
    if (!BlockWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateHistoryRecord: BlockWorld is null"));
        return FVoxelEditHistoryRecord();
    }

    // === 阶段 1: 计算受影响的方块 ===
    // 复用现有的计算逻辑
    FSingleEditRequest SingleEditRequest = CalculateSingleEditRequest(EditRequest, BlockWorld);

    // === 阶段 2: 构造历史记录 ===
    FVoxelEditHistoryRecord HistoryRecord;

    // 设置元数据
    // EditMode 已废弃，不再单独记录（EditCommand 已包含在 Edits 中）
    HistoryRecord.ToolTag = EditRequest.ToolTag;
    HistoryRecord.EditCommand = EditRequest.EditCommand;
    HistoryRecord.Description = EditRequest.GetDescription();
    HistoryRecord.Timestamp = FDateTime::Now();

    // === 性能优化: 预分配空间 ===
    const int32 EstimatedSize = SingleEditRequest.PreviousBlocks.Num();
    HistoryRecord.Reserve(EstimatedSize);  // 同时预分配 Edits 和 ChunkIndices

    // === 阶段 3: 填充编辑数据 ===
    for (const auto& Pair : SingleEditRequest.PreviousBlocks)
    {
        const FIntVector& BlockIndex = Pair.Key;           // 方块位置
        int32 OldType = Pair.Value;          // 当前类型(已读取)
        int32 NewType = EditRequest.HandleId; // 目标类型

        // 添加到 TSet(自动去重)
        HistoryRecord.Add(FVoxelEdit(BlockIndex, OldType, NewType));

        // 计算并记录 Chunk 索引
        const FIntVector ChunkIndex = UChunkUtilsFunction::Get3DChunkIndexByBlockIndex(
	        BlockIndex,
	        BlockWorld->ChunkSize
        );
        HistoryRecord.AddChunkIndex(ChunkIndex);
    }

    // === 阶段 4: 验证 ===
    if (HistoryRecord.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateHistoryRecord: No edits generated"));
    }

    return HistoryRecord;
}
