// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/VoxelPreviewActor.h"
#include "Voxel/Utils/ChunkMeshData.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Voxel/Editor/UVoxelTemplateUtils.h"
#include "Voxel/Editor/Tools/VoxelSelectionTool.h"
#include "Core/UI/Interaction/Voxel3DUIActor.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/Enum/GameStateEnum.h"
#include "Core/VoxelGameInstance.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "GameManager/GameInstanceManager/VoxelTextureRegistry.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "Item/VoxelBlockItemBase.h"
#include "Data/Item/VoxelItemBase.h"
#include "Utils/ChunkBase.h"
#include "Core/CoreTags.h"
#include "Kismet/GameplayStatics.h"
#include "Voxel/VoxelPlayerController.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"

// === 构造函数 ===
AVoxelPreviewActor::AVoxelPreviewActor()
	: CurrentMode(EPreviewMode::None)
{
	// 预览 Actor 不需要 Tick，优化性能
	PrimaryActorTick.bCanEverTick = false;

	// 创建根组件
	USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootComp);

	// 创建外框网格组件
	WireframeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WireframeMesh"));
	WireframeMesh->SetupAttachment(RootComponent);
	WireframeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WireframeMesh->SetCastShadow(false);
	WireframeMesh->SetHiddenInGame(true);
	WireframeMesh->SetCanEverAffectNavigation(false);

	// 创建内部网格组件
	InnerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InnerMesh"));
	InnerMesh->SetupAttachment(RootComponent);
	InnerMesh->SetCastShadow(false);
	InnerMesh->SetHiddenInGame(true);
	InnerMesh->SetCanEverAffectNavigation(false);

	// 创建厚度网格组件（用于显示挖空或扩张边界）
	ThicknessMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThicknessMesh"));
	ThicknessMesh->SetupAttachment(RootComponent);
	ThicknessMesh->SetCastShadow(false);
	ThicknessMesh->SetHiddenInGame(true);
	ThicknessMesh->SetCanEverAffectNavigation(false);

	// 创建程序化网格组件（用于复杂蓝图）
	BlueprintMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BlueprintMesh"));
	BlueprintMesh->SetupAttachment(RootComponent);
	BlueprintMesh->SetCastShadow(false);
	BlueprintMesh->SetHiddenInGame(true);
	BlueprintMesh->SetCanEverAffectNavigation(false);

	// 创建笔刷预览网格组件（独立于 BlueprintMesh，避免覆盖选区数据）
	BrushPreviewMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BrushPreviewMesh"));
	BrushPreviewMesh->SetupAttachment(RootComponent);
	BrushPreviewMesh->SetCastShadow(false);
	BrushPreviewMesh->SetHiddenInGame(true);
	BrushPreviewMesh->SetCanEverAffectNavigation(false);

	// 注意：家具预览组件池（FurniturePreviewMeshPool）在运行时按需创建，不在构造函数中初始化
}

// === BeginPlay ===
void AVoxelPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	// 加载默认 StaticMesh 资产（如果未配置）
	if (!BoxMesh)
	{
		BoxMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	if (!SphereMesh)
	{
		SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}
	if (!CylinderMesh)
	{
		CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	}
	if (!WireframeBoxMesh)
	{
		WireframeBoxMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}

	UE_LOG(LogTemp, Log, TEXT("AVoxelPreviewActor::BeginPlay: Preview components initialized"));
}

// === SetPreviewMode ===
void AVoxelPreviewActor::SetPreviewMode(EPreviewMode Mode)
{
	CurrentMode = Mode;

	// 如果切换到 None 模式，清除预览
	if (Mode == EPreviewMode::None)
	{
		ClearPreview();
	}
	// 更新组件可见性
	UpdatePreviewComponentVisibility();
}

// === UpdateSingleVoxelPreview（模式1） ===
void AVoxelPreviewActor::UpdateSingleVoxelPreview(
	FIntVector BlockIndex,
	int32 BlockType,
	FLinearColor Color,
	float Size)
{
	// 切换到单体素模式
	SetPreviewMode(EPreviewMode::SingleVoxel);

	if (!InnerMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelPreviewActor::UpdateSingleVoxelPreview: InnerMesh is nullptr"));
		return;
	}

	// 1. 将体素索引转换为中心世界坐标
	FVector CenterPosition = (FVector(BlockIndex) + FVector(0.5f)) * Size;
	SetActorLocation(CenterPosition);

	// 2. 尝试根据方块类型获取实际材质
	//UMaterialInterface* BlockMaterial = GetMaterialByBlockType(BlockType);

	if (InnerDynamicMaterial)
	{
		// 使用实际材质
		InnerMesh->SetStaticMesh(BoxMesh);
		InnerMesh->SetMaterial(0, InnerDynamicMaterial);

		// 设置缩放
		const float DefaultCubeSize = 100.0f;
		const float Scale = Size / DefaultCubeSize;
		InnerMesh->SetRelativeScale3D(FVector(Scale));

		UE_LOG(LogTemp, Log, TEXT("AVoxelPreviewActor::UpdateSingleVoxelPreview: Using block material"));
	}
	else
	{
		// 最后回退：使用默认材质
		InnerMesh->SetStaticMesh(BoxMesh);
		const float DefaultCubeSize = 100.0f;
		const float Scale = Size / DefaultCubeSize;
		InnerMesh->SetRelativeScale3D(FVector(Scale));
	}

	//UE_LOG(LogTemp, Log, TEXT("AVoxelPreviewActor::UpdateSingleVoxelPreview: Single voxel preview at %s, size %.2f"),
	//	*Position.ToString(), Size);
}

// === UpdateSimpleGeometryPreview（模式2）- 不带选区数据版本 ===
void AVoxelPreviewActor::UpdateSimpleGeometryPreview(
	const FVoxelEditRequest& Request,
	float BlockLength)
{
	UpdateSimpleGeometryPreviewWithSelection(Request, BlockLength, TArray<FIntVector>());
}

// === UpdateSimpleGeometryPreviewWithSelection（模式2）- 带选区数据版本 ===
void AVoxelPreviewActor::UpdateSimpleGeometryPreviewWithSelection(
	const FVoxelEditRequest& Request,
	float BlockLength,
	const TArray<FIntVector>& SelectedVoxels,
	int32 BlockType)
{
	// 切换到简单几何体模式
	SetPreviewMode(EPreviewMode::SimpleGeometry);

	// 计算边界
	CurrentBounds = Request.Geometry.GetBound(BlockLength);
	FVector BaseMin = CurrentBounds.Min;
	FVector BaseMax = CurrentBounds.Max;

	// === 处理 REGULAR 模式（内接正方体） ===
	if (Request.Geometry.bIsRegular)
	{
		FVector BaseSize = BaseMax - BaseMin;
		float MinSide = FMath::Min3(BaseSize.X, BaseSize.Y, BaseSize.Z);
		FVector Center = CurrentBounds.GetCenter();
		BaseMin = Center - FVector(MinSide * 0.5f);
		BaseMax = Center + FVector(MinSide * 0.5f);
	}

	FBox BaseBounds(BaseMin, BaseMax);
	FVector GeometryCenter = Request.Geometry.GetGeometricCenter(BlockLength);
	FRotator GeometryRotation = Request.Geometry.Rotation;

	// === 将 Actor 定位到旋转中心，自身不旋转 ===
	// WireframeMesh 通过 SetWorldLocation/Rotation 单独处理世界变换
	// BlueprintMesh 使用相对于 GeometryCenter 的局部坐标，因此 Actor 必须位于 GeometryCenter 且无旋转
	SetActorLocation(GeometryCenter);
	SetActorRotation(FRotator::ZeroRotator);

	// ==================== 1. 更新WireframeMesh（外框） ====================
	UpdateWireframeMesh(BaseBounds, GeometryCenter, GeometryRotation, BlockLength);

	// ==================== 2. 更新 InnerMesh 和 ThicknessMesh（几何体填充） ====================
	// 几何体编辑工具的内部填充由 InnerMesh 直接渲染（按 ShapeType 选 Box/Sphere/Cylinder Mesh，
	// 按 Thickness 缩放，按 GeometryCenter/Rotation 定位），与 Request.Geometry 完全一致。
	// 不能隐藏 InnerMesh，否则范围预览会缺失；也不能依赖 SelectionMesh，因为 SelectionTool
	// 会把连续几何体离散化成体素集合，导致球/柱形状阶梯化，且 Thickness/bIsRegular/Rotation 全部丢失。
	UpdateInnerMesh(Request.Geometry, BaseBounds, GeometryCenter, GeometryRotation, BlockLength);

	// ==================== 3. 更新ProceduralMesh（体素高亮） ====================
	if (SelectedVoxels.Num() > 0)
	{
		// 选区预览：原点为 GeometryCenter（局部坐标），BlockType 决定 Chunk 材质与纹理
		UpdateProceduralMesh(SelectedVoxels, BlockLength, GeometryCenter, BlockType);
	}
	else
	{
		// 隐藏ProceduralMesh
		if (BlueprintMesh)
		{
			BlueprintMesh->ClearMeshSection(0);
			BlueprintMesh->SetHiddenInGame(true);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AVoxelPreviewActor::UpdateSimpleGeometryPreviewWithSelection: Shape=%d, Thickness=%d, Rotation=%s, bIsRegular=%d, SelectedVoxels=%d"),
		(int32)Request.Geometry.ShapeType, Request.Geometry.Thickness, *Request.Geometry.Rotation.ToString(), Request.Geometry.bIsRegular ? 1 : 0,
		SelectedVoxels.Num());
}

// === UpdateWireframeMesh（独立方法） ===
void AVoxelPreviewActor::UpdateWireframeMesh(
	const FBox& Bounds,
	const FVector& GeometryCenter,
	const FRotator& GeometryRotation,
	float BlockLength)
{
	if (!WireframeMesh) return;

	UpdateMeshByShape(WireframeMesh, EBatchShapeType::Box, Bounds, GeometryCenter, GeometryRotation);

	// WireframeMesh 只用框架材质（懒创建 WireframeDynamicMaterial）
	if (!WireframeDynamicMaterial && WireframeMaterial)
	{
		WireframeDynamicMaterial = UMaterialInstanceDynamic::Create(WireframeMaterial, this);
	}
	if (WireframeDynamicMaterial)
	{
		WireframeMesh->SetMaterial(0, WireframeDynamicMaterial);
	}
	WireframeMesh->SetHiddenInGame(false);
}

// === UpdateInnerMesh（独立方法，包含ThicknessMesh逻辑） ===
void AVoxelPreviewActor::UpdateInnerMesh(
	const FVoxelGeometryDesc& Geometry,
	const FBox& BaseBounds,
	const FVector& GeometryCenter,
	const FRotator& GeometryRotation,
	float BlockLength)
{
	FVector BaseMin = BaseBounds.Min;
	FVector BaseMax = BaseBounds.Max;

	// ==================== 1. 更新InnerMesh ====================
	if (InnerMesh)
	{
		FBox FillBounds = BaseBounds;

		// Thickness > 0 时，内部填充区域需要缩小
		if (Geometry.Thickness > 0)
		{
			float ShrinkAmount = Geometry.Thickness * BlockLength;
			FVector ShrinkVector(ShrinkAmount);
			FillBounds = FBox(BaseMin + ShrinkVector, BaseMax - ShrinkVector);

			// 确保不会翻转
			if (FillBounds.Min.X > FillBounds.Max.X) FillBounds.Min.X = FillBounds.Max.X;
			if (FillBounds.Min.Y > FillBounds.Max.Y) FillBounds.Min.Y = FillBounds.Max.Y;
			if (FillBounds.Min.Z > FillBounds.Max.Z) FillBounds.Min.Z = FillBounds.Max.Z;
		}

		UpdateMeshByShape(InnerMesh, Geometry.ShapeType, FillBounds, GeometryCenter, GeometryRotation);

		if (InnerDynamicMaterial)
		{
			InnerMesh->SetMaterial(0, InnerDynamicMaterial);
		}
		InnerMesh->SetHiddenInGame(false);
	}

	// ==================== 2. 更新ThicknessMesh（包含在InnerMesh逻辑中） ====================
	if (ThicknessMesh)
	{
		if (Geometry.Thickness != 0)
		{
			FBox ThicknessBounds;

			if (Geometry.Thickness > 0)
			{
				// 正厚度：显示挖空边界（内部空区域）
				float ShrinkAmount = Geometry.Thickness * BlockLength;
				FVector ShrinkVector(ShrinkAmount);
				ThicknessBounds = FBox(BaseMin + ShrinkVector, BaseMax - ShrinkVector);
			}
			else
			{
				// 负厚度：显示扩张边界
				float ExpandAmount = FMath::Abs(Geometry.Thickness) * BlockLength;
				ThicknessBounds = BaseBounds.ExpandBy(ExpandAmount);
			}

			UpdateMeshByShape(ThicknessMesh, EBatchShapeType::Box, ThicknessBounds, GeometryCenter, GeometryRotation);

			if (ThicknessDynamicMaterial)
			{
				ThicknessMesh->SetMaterial(0, ThicknessDynamicMaterial);
			}

			ThicknessMesh->SetHiddenInGame(false);
		}
		else
		{
			ThicknessMesh->SetHiddenInGame(true);
		}
	}
}

// === SetInnerMeshVisible ===
void AVoxelPreviewActor::SetInnerMeshVisible(bool bVisible)
{
	if (InnerMesh)
	{
		InnerMesh->SetHiddenInGame(!bVisible);
	}
	if (ThicknessMesh)
	{
		ThicknessMesh->SetHiddenInGame(!bVisible);
	}
}

// === UpdateProceduralMesh（独立方法） ===
void AVoxelPreviewActor::UpdateProceduralMesh(
	const TArray<FIntVector>& SelectedVoxels,
	float BlockLength,
	const FVector& Origin,
	int32 BlockType)
{
	if (!BlueprintMesh) return;

	BlueprintMesh->ClearMeshSection(0);

	if (SelectedVoxels.Num() == 0)
	{
		BlueprintMesh->SetHiddenInGame(true);
		return;
	}

	// 构建快速查找集合，用于面剔除（类似 Chunk 贪婪网格的面可见性判断）
	TSet<FIntVector> VoxelSet(SelectedVoxels);
	auto IsVoxelSelected = [&](const FIntVector& Pos) -> bool
	{
		return VoxelSet.Contains(Pos);
	};

	// 6 个邻居方向：-X, +X, -Y, +Y, -Z, +Z
	const FIntVector NeighborDirs[6] = {
		FIntVector(-1,  0,  0),
		FIntVector( 1,  0,  0),
		FIntVector( 0, -1,  0),
		FIntVector( 0,  1,  0),
		FIntVector( 0,  0, -1),
		FIntVector( 0,  0,  1)
	};

	// 查找 BlockType 对应的纹理索引（通过 TextureRegistry 查询，与 BuildChunkContext 一致）
	int32 TexIndices[6] = {0, 0, 0, 0, 0, 0};
	bool bHasTexture = false;
	if (GetWorld())
	{
		if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
		{
			if (UConfigDataSubsystem* ConfigData = GI->GetSubsystem<UConfigDataSubsystem>())
			{
				if (FVoxelItemBase* const * ItemPtr = ConfigData->ItemMap.Find(BlockType))
				{
					if (FVoxelBlockItemBase* BlockItem = static_cast<FVoxelBlockItemBase*>(*ItemPtr))
					{
						if (UVoxelTextureRegistry* TexRegistry = GI->GetSubsystem<UVoxelTextureRegistry>())
						{
							const FVoxelBlockTextureSetup& TexSetup = BlockItem->BlockFragment.TextureSetup;
						TexIndices[0] = TexRegistry->GetTextureIndex(TexSetup.Side);    // -X
						TexIndices[1] = TexRegistry->GetTextureIndex(TexSetup.Side);    // +X
						TexIndices[2] = TexRegistry->GetTextureIndex(TexSetup.Side);    // -Y
						TexIndices[3] = TexRegistry->GetTextureIndex(TexSetup.Side);    // +Y
						TexIndices[4] = TexRegistry->GetTextureIndex(TexSetup.Bottom);  // -Z
						TexIndices[5] = TexRegistry->GetTextureIndex(TexSetup.Top);     // +Z
							bHasTexture = true;
						}
					}
				}
			}
		}
	}

	FChunkMeshData MeshData;

	for (const FIntVector& VoxelIndex : SelectedVoxels)
	{
		// 使用相对于 Origin（GeometryCenter）的局部坐标
		FVector VoxelLocalPos = FVector(VoxelIndex) * BlockLength - Origin;
		FVector BoxMin = VoxelLocalPos;
		FVector BoxMax = VoxelLocalPos + FVector(BlockLength);

		// 只生成暴露在选区外部的面（类似 Chunk 的面剔除）
		for (int32 Face = 0; Face < 6; ++Face)
		{
			FIntVector NeighborIndex = VoxelIndex + NeighborDirs[Face];
			if (!IsVoxelSelected(NeighborIndex))
			{
				// 顶点颜色 alpha = 纹理索引（与 Chunk 的 CreateQuad 一致）
				FColor VertexColor = bHasTexture
					? FColor(0, 0, 0, TexIndices[Face])
					: FColor::White;
				AddFaceMeshData(MeshData, BoxMin, BoxMax, Face, VertexColor);
			}
		}
	}

	// 创建网格
	if (MeshData.Vertices.Num() > 0 && MeshData.Triangles.Num() > 0)
	{
		BlueprintMesh->CreateMeshSection(
			0,
			MeshData.Vertices,
			MeshData.Triangles,
			MeshData.Normals,
			MeshData.UV0,
			MeshData.Colors,
			TArray<FProcMeshTangent>(),
			false
		);
		BlueprintMesh->SetHiddenInGame(false);

		// 设置材质：优先使用 BlockType 对应的 Chunk 材质，否则回退到 InnerDynamicMaterial
		UMaterialInterface* BlockMaterial = GetMaterialByBlockType(BlockType);
		if (BlockMaterial)
		{
			BlueprintMesh->SetMaterial(0, BlockMaterial);
		}
		else if (InnerDynamicMaterial)
		{
			BlueprintMesh->SetMaterial(0, InnerDynamicMaterial);
		}
	}
	else
	{
		BlueprintMesh->SetHiddenInGame(true);
	}
}

// === UpdateComplexBlueprintPreview（模式3） ===
void AVoxelPreviewActor::UpdateComplexBlueprintPreview(
	FBox Bounds,
	const FChunkMeshData& MeshData,
	int32 BlockType,
	float BlockLength)
{
	// 切换到复杂蓝图模式
	SetPreviewMode(EPreviewMode::ComplexBlueprint);

	// === 1. 配置外框（WireframeMesh 只用框架材质） ===
	if (WireframeMesh)
	{
		// ComplexBlueprint模式不使用旋转，传递默认值
		UpdateMeshByShape(WireframeMesh, EBatchShapeType::Box, Bounds, Bounds.GetCenter(), FRotator::ZeroRotator);

		// 懒创建 WireframeDynamicMaterial 并应用
		if (!WireframeDynamicMaterial && WireframeMaterial)
		{
			WireframeDynamicMaterial = UMaterialInstanceDynamic::Create(WireframeMaterial, this);
		}
		if (WireframeDynamicMaterial)
		{
			WireframeMesh->SetMaterial(0, WireframeDynamicMaterial);
		}
	}

	// === 2. 应用网格数据（BlueprintMesh 专用于模板/蓝图预览，使用半透明 Chunk 材质） ===
	if (BlueprintMesh)
	{
		// 清除旧网格
		BlueprintMesh->ClearAllMeshSections();

		// 如果有网格数据，创建新网格
		if (MeshData.Vertices.Num() > 0 && MeshData.Triangles.Num() > 0)
		{
			// 创建程序化网格（不创建碰撞）
			BlueprintMesh->CreateMeshSection(
				0,                          // Section Index
				MeshData.Vertices,
				MeshData.Triangles,
				MeshData.Normals,
				MeshData.UV0,
				MeshData.Colors,
				TArray<FProcMeshTangent>(),
				false                       // 不创建碰撞
			);

			// 设置材质：根据 bPreviewValid 选择正常或无效材质
			UMaterialInterface* UseMaterial = nullptr;
			if (!bPreviewValid && TemplatePreviewInvalidMaterial)
			{
				UseMaterial = TemplatePreviewInvalidMaterial;
			}
			else
			{
				UseMaterial = TemplatePreviewMaterial ? TemplatePreviewMaterial.Get() : GetMaterialByBlockType(BlockType);
			}
			if (UseMaterial)
			{
				if (!TemplatePreviewDynamicMaterial)
				{
					TemplatePreviewDynamicMaterial = UMaterialInstanceDynamic::Create(UseMaterial, this);
					// 首次创建时设置 Texture2DArray 参数
					ApplyTextureArrayParams(TemplatePreviewDynamicMaterial);
				}
				if (TemplatePreviewDynamicMaterial)
				{
					if (BlockLength > 0.0f)
					{
						TemplatePreviewDynamicMaterial->SetScalarParameterValue(FName(TEXT("BlockLength")), BlockLength);
					}
					UseMaterial = TemplatePreviewDynamicMaterial;
				}
			}

			UE_LOG(LogTemp, Log, TEXT("[MeshChain] UpdateComplexBlueprintPreview: CreateMeshSection V=%d T=%d, Material=%s"),
				MeshData.Vertices.Num(), MeshData.Triangles.Num(),
				UseMaterial ? TEXT("Valid") : TEXT("NULL"));

			if (UseMaterial)
			{
				BlueprintMesh->SetMaterial(0, UseMaterial);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[MeshChain] UpdateComplexBlueprintPreview: MeshData empty (V=%d T=%d), no mesh created"),
				MeshData.Vertices.Num(), MeshData.Triangles.Num());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[MeshChain] UpdateComplexBlueprintPreview: BlueprintMesh is NULL"));
	}

	UE_LOG(LogTemp, Log, TEXT("AVoxelPreviewActor::UpdateComplexBlueprintPreview: Complex blueprint preview at %s, vertices: %d"),
		*Bounds.GetCenter().ToString(), MeshData.Vertices.Num());
}

// === UpdateTemplatePreview（封装模板预览的位置/旋转中心/padding 计算） ===
void AVoxelPreviewActor::UpdateTemplatePreview(
	const FVoxelTemplate& Tmpl,
	const FVoxelGeometryDesc& Geometry,
	const FChunkMeshData& MeshData,
	int32 BlockType,
	float BlockLength)
{
	if (Tmpl.Entries.Num() == 0)
	{
		return;
	}

	// 1. 计算模板局部包围盒（Entries 的 key 是相对坐标）
	FIntVector TmplMin = Tmpl.Size;
	FIntVector TmplMax = FIntVector::ZeroValue;
	for (const auto& Pair : Tmpl.Entries)
	{
		const FIntVector& Pos = Pair.Key;
		TmplMin = FIntVector(FMath::Min(TmplMin.X, Pos.X), FMath::Min(TmplMin.Y, Pos.Y), FMath::Min(TmplMin.Z, Pos.Z));
		TmplMax = FIntVector(FMath::Max(TmplMax.X, Pos.X), FMath::Max(TmplMax.Y, Pos.Y), FMath::Max(TmplMax.Z, Pos.Z));
	}
	const FIntVector TmplSize = TmplMax - TmplMin + FIntVector(1);

	// 2. 计算放置原点（XY 中心对齐鼠标点）
	const FIntVector Origin = UVoxelTemplateUtils::GetCenteredPlacementOrigin(Tmpl, Geometry.Box.MinIndex);

	// 3. 旋转中心 = 模板 XY 底面中心（世界坐标）
	//    X/Y: Origin + TmplMin + TmplSize/2（XY 中心对齐鼠标点）
	//    Z:   Origin + TmplMin（底面，Z 不偏移）
	const FVector RotationCenterWorld = FVector(Origin + TmplMin + FIntVector(TmplSize.X / 2, TmplSize.Y / 2, 0)) * BlockLength;

	// 4. 模板局部包围盒的世界 AABB（未旋转）
	const FBox Bounds(
		FVector(Origin + TmplMin) * BlockLength,
		FVector(Origin + TmplMax + FIntVector(1)) * BlockLength);

	// 5. Actor 位于旋转中心，旋转绕此点
	SetActorLocation(RotationCenterWorld);
	SetActorRotation(Geometry.Rotation);

	// 6. 调用 UpdateComplexBlueprintPreview 创建网格（内部 SetPreviewMode + CreateMeshSection）
	UpdateComplexBlueprintPreview(Bounds, MeshData, BlockType, BlockLength);

	// 7. BlueprintMesh 相对位置
	//    数据流：
	//    - BuildTemplateContext 把 Pos 存入 padded[Pos - TmplMin + 1]
	//    - GenerateMeshData 遍历 ChunkItr∈[0,Size-1]，调 GetBlockId(ChunkItr)
	//      内部 LocalX = ChunkItr + 1，所以 ChunkItr = Pos - TmplMin 时读到 Pos 的方块
	//    - CreateQuad 顶点 = FVector(ChunkItr) * BlockLength = (Pos - TmplMin) * BlockLength
	//    所以顶点局部坐标 = (Pos - TmplMin) * BlockLength（不含 padding +1）
	//
	//    期望顶点世界位置 = (Origin + Pos) * BlockLength（与 PlaceTemplate 的 WorldPos 一致）
	//    VertexWorld = ActorLocation + MeshRelativeOffset + VertexLocal
	//    => MeshRelativeOffset = (Origin + Pos) * BlockLength - RotationCenterWorld - (Pos - TmplMin) * BlockLength
	//                          = (Origin + TmplMin) * BlockLength - RotationCenterWorld
	const FVector MeshRelativeOffset = FVector(Origin + TmplMin) * BlockLength - RotationCenterWorld;
	if (BlueprintMesh)
	{
		BlueprintMesh->SetRelativeLocation(MeshRelativeOffset);
	}

	// 8. WireframeMesh 相对位置 + 相对旋转（跟随 Actor 旋转，绕旋转中心）
	//    UpdateComplexBlueprintPreview 内部用 SetWorldLocation 设到 Bounds.GetCenter()
	//    这里改为相对位置，让旋转时 WireframeMesh 跟着 Actor 转
	if (WireframeMesh)
	{
		const FVector WireframeRelative = Bounds.GetCenter() - RotationCenterWorld;
		WireframeMesh->SetRelativeLocation(WireframeRelative);
		WireframeMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}

	UE_LOG(LogTemp, Log, TEXT("[TemplatePreview] Origin=%s, TmplSize=%s, RotCenter=%s, Rot=%s, V=%d T=%d"),
		*Origin.ToString(), *TmplSize.ToString(), *RotationCenterWorld.ToString(),
		*Geometry.Rotation.ToString(), MeshData.Vertices.Num(), MeshData.Triangles.Num());
}

// === UpdateBrushPreview（模式4） ===
// 【重构】笔刷预览使用独立的 BrushPreviewMesh，不再切换 PreviewMode，不再移动 Actor
// 这样选区渲染（WireframeMesh + BlueprintMesh）和笔刷预览完全独立，互不干扰
void AVoxelPreviewActor::UpdateBrushPreview(
	const FIntVector& CenterIndex,
	int32 BrushSize,
	EBatchShapeType Shape,
	EBrushDimension Dimension,
	float BlockLength,
	FLinearColor Color)
{
	if (!BrushPreviewMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelPreviewActor::UpdateBrushPreview: BrushPreviewMesh is NULL"));
		return;
	}

	// === 像 SingleVoxel 一样定位 BrushPreviewMesh 到笔刷中心 ===
	// SingleVoxel 用 SetActorLocation((BlockIndex + 0.5) * Size) 把 Actor 移到体素中心
	// 这里只移动 BrushPreviewMesh 组件（不移动整个 Actor），避免破坏选区已建立的位置
	const FVector CenterPosition = (FVector(CenterIndex) + FVector(0.5f)) * BlockLength;
	BrushPreviewMesh->SetWorldLocation(CenterPosition);
	BrushPreviewMesh->SetWorldRotation(FRotator::ZeroRotator);

	// 确保 Actor 可见（之前可能被 ClearPreview / SetPreviewMode(None) 通过 SetActorHiddenInGame 隐藏）
	// Actor 隐藏会强制所有子组件不可见，必须解除
	SetActorHiddenInGame(false);

	// 计算笔刷影响范围
	TArray<FIntVector> BrushVoxels = UVoxelSelectionTool::CalculateBrushSelection(
		CenterIndex, static_cast<float>(BrushSize), Shape, Dimension);

	if (BrushVoxels.Num() == 0)
	{
		BrushPreviewMesh->ClearMeshSection(0);
		BrushPreviewMesh->SetHiddenInGame(true);
		return;
	}

	// 顶点用相对于 BrushPreviewMesh（笔刷中心）的局部坐标
	// VoxelLocalPos = VoxelIndex * BlockLength - BrushPreviewMesh 世界位置
	// 顶点世界位置 = BrushPreviewMesh 位置 + VoxelLocalPos = VoxelIndex * BlockLength ✓
	const FVector Origin = BrushPreviewMesh->GetComponentLocation();

	// 更新独立的笔刷预览网格（不影响 BlueprintMesh section 0）
	UpdateBrushPreviewMesh(BrushVoxels, BlockLength, Origin);

	// 覆盖材质为 BrushPreviewDynamicMaterial（懒创建）
	if (!BrushPreviewDynamicMaterial && BrushPreviewMaterial)
	{
		BrushPreviewDynamicMaterial = UMaterialInstanceDynamic::Create(BrushPreviewMaterial, this);
	}
	if (BrushPreviewDynamicMaterial)
	{
		BrushPreviewDynamicMaterial->SetVectorParameterValue(FName(TEXT("Color")), Color);
		BrushPreviewMesh->SetMaterial(0, BrushPreviewDynamicMaterial);
	}
}

// === UpdateBrushPreviewMesh（内部） ===
// 独立更新 BrushPreviewMesh，复用面剔除逻辑，但写入独立组件的 section 0
void AVoxelPreviewActor::UpdateBrushPreviewMesh(
	const TArray<FIntVector>& BrushVoxels,
	float BlockLength,
	const FVector& Origin)
{
	if (!BrushPreviewMesh) return;

	BrushPreviewMesh->ClearMeshSection(0);

	if (BrushVoxels.Num() == 0)
	{
		BrushPreviewMesh->SetHiddenInGame(true);
		return;
	}

	// 构建快速查找集合，用于面剔除
	TSet<FIntVector> VoxelSet(BrushVoxels);
	auto IsVoxelSelected = [&](const FIntVector& Pos) -> bool
	{
		return VoxelSet.Contains(Pos);
	};

	// 6 个邻居方向：-X, +X, -Y, +Y, -Z, +Z
	const FIntVector NeighborDirs[6] = {
		FIntVector(-1,  0,  0),
		FIntVector( 1,  0,  0),
		FIntVector( 0, -1,  0),
		FIntVector( 0,  1,  0),
		FIntVector( 0,  0, -1),
		FIntVector( 0,  0,  1)
	};

	FChunkMeshData MeshData;

	// 笔刷预览不需要纹理，用白色顶点（材质通过 BrushPreviewDynamicMaterial 控制颜色）
	const FColor VertexColor = FColor::White;

	for (const FIntVector& VoxelIndex : BrushVoxels)
	{
		// 顶点坐标：相对于 BrushPreviewMesh 世界位置（笔刷中心）的局部坐标
		// 顶点世界位置 = BrushPreviewMesh 位置 + VoxelLocalPos = VoxelIndex * BlockLength
		const FVector VoxelLocalPos = FVector(VoxelIndex) * BlockLength - Origin;
		const FVector BoxMin = VoxelLocalPos;
		const FVector BoxMax = VoxelLocalPos + FVector(BlockLength);

		// 面剔除：只生成暴露在笔刷外部的面
		for (int32 Face = 0; Face < 6; ++Face)
		{
			const FIntVector NeighborIndex = VoxelIndex + NeighborDirs[Face];
			if (!IsVoxelSelected(NeighborIndex))
			{
				AddFaceMeshData(MeshData, BoxMin, BoxMax, Face, VertexColor);
			}
		}
	}

	if (MeshData.Vertices.Num() > 0 && MeshData.Triangles.Num() > 0)
	{
		BrushPreviewMesh->CreateMeshSection(
			0,
			MeshData.Vertices,
			MeshData.Triangles,
			MeshData.Normals,
			MeshData.UV0,
			MeshData.Colors,
			TArray<FProcMeshTangent>(),
			false
		);
		BrushPreviewMesh->SetHiddenInGame(false);
	}
	else
	{
		BrushPreviewMesh->SetHiddenInGame(true);
	}
}

// === ClearPreview ===
void AVoxelPreviewActor::ClearPreview()
{
	// 【DEBUG】打印调用栈
	UE_LOG(LogTemp, Log, TEXT("[DEBUG] ClearPreview called! CurrentMode=%d"),
		(int32)CurrentMode);

	// 直接设置模式（避免递归调用）
	CurrentMode = EPreviewMode::None;

	// 隐藏所有组件并重置变换状态（避免旧工具的旋转/缩放残留）
	if (WireframeMesh) { WireframeMesh->SetHiddenInGame(true); WireframeMesh->SetRelativeScale3D(FVector::OneVector); WireframeMesh->SetWorldRotation(FRotator::ZeroRotator); }
	if (InnerMesh) { InnerMesh->SetHiddenInGame(true); InnerMesh->SetRelativeScale3D(FVector::OneVector); InnerMesh->SetWorldRotation(FRotator::ZeroRotator); }
	if (ThicknessMesh) { ThicknessMesh->SetHiddenInGame(true); ThicknessMesh->SetRelativeScale3D(FVector::OneVector); ThicknessMesh->SetWorldRotation(FRotator::ZeroRotator); }
	if (BlueprintMesh) { BlueprintMesh->SetHiddenInGame(true); BlueprintMesh->ClearMeshSection(0); }
	if (BrushPreviewMesh) { BrushPreviewMesh->SetHiddenInGame(true); BrushPreviewMesh->ClearMeshSection(0); }
	ClearFurniturePreviewPool();
	// 注意：选区渲染已迁移到独立的 AVoxelSelectionPreviewActor，不再由 PreviewActor 管理

	UE_LOG(LogTemp, Log, TEXT("AVoxelPreviewActor::ClearPreview: Preview cleared"));

	// 重置预览有效性
	bPreviewValid = true;
}

// === SetPreviewValid ===
// 切换预览材质：正常半透明 / 红色无效
void AVoxelPreviewActor::SetPreviewValid(bool bValid)
{
	if (bPreviewValid == bValid) return;
	bPreviewValid = bValid;

	// 切换家具预览池中活跃组件的材质
	UMaterialInterface* FurnitureMat = bValid ? FurniturePreviewMaterial : FurniturePreviewInvalidMaterial;
	if (FurnitureMat)
	{
		for (int32 i = 0; i < FurniturePreviewActiveCount; ++i)
		{
			if (UStaticMeshComponent* Comp = FurniturePreviewMeshPool.IsValidIndex(i) ? FurniturePreviewMeshPool[i] : nullptr)
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FurnitureMat, this);
				if (MID)
				{
					Comp->SetMaterial(0, MID);
				}
			}
		}
	}

	// 切换模板预览（BlueprintMesh）的材质
	UMaterialInterface* TemplateMat = bValid
		? (TemplatePreviewMaterial ? TemplatePreviewMaterial.Get() : nullptr)
		: (TemplatePreviewInvalidMaterial ? TemplatePreviewInvalidMaterial.Get() : nullptr);
	if (TemplateMat && BlueprintMesh)
	{
		// 清空缓存，让下次 UpdateComplexBlueprintPreview 根据 bPreviewValid 重新创建
		TemplatePreviewDynamicMaterial = nullptr;
		// 立即创建临时 MID 设置到组件，确保即时切换
		UMaterialInstanceDynamic* TempMID = UMaterialInstanceDynamic::Create(TemplateMat, this);
		if (TempMID)
		{
			ApplyTextureArrayParams(TempMID);
			BlueprintMesh->SetMaterial(0, TempMID);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PreviewActor] SetPreviewValid: %s"), bValid ? TEXT("Valid") : TEXT("Invalid"));
}
// 仅清理笔刷预览，不影响选区（用于工具切换、鼠标离开等场景）
void AVoxelPreviewActor::ClearBrushPreview()
{
	if (BrushPreviewMesh)
	{
		BrushPreviewMesh->ClearMeshSection(0);
		BrushPreviewMesh->SetHiddenInGame(true);
	}
}

// === UpdateFurniturePreview ===
// 渲染半透明包围盒（WireframeMesh）+ 家具 StaticMesh 半透明预览（FurniturePreviewMeshPool）
void AVoxelPreviewActor::UpdateFurniturePreview(
	const FIntVector& PlaceIndex,
	EActorDirection Direction,
	UStaticMesh* FurnitureMesh,
	const FBox& Bounds,
	float BlockLength,
	const FVoxelTextureSet& TextureSet)
{
	// 必须在蓝图配置 FurniturePreviewMaterial（半透明预览材质）
	if (!FurniturePreviewMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelPreviewActor::UpdateFurniturePreview: FurniturePreviewMaterial is null! Must configure in Blueprint."));
		return;
	}

	// 切换到家具预览模式（驱动组件可见性）
	SetPreviewMode(EPreviewMode::Furniture);

	// --- 1. 包围盒线框（WireframeMesh）---
	// Bounds 是体素坐标，转世界坐标
	const FBox WorldBounds(
		FVector(Bounds.Min) * BlockLength,
		FVector(Bounds.Max) * BlockLength);

	if (WireframeMesh && BoxMesh)
	{
		WireframeMesh->SetStaticMesh(BoxMesh);
		// 包围盒中心
		const FVector Center = WorldBounds.GetCenter();
		// 缩放：Bounds 的尺寸 / BoxMesh 默认尺寸（100cm 立方体）
		const FVector Scale = WorldBounds.GetSize() / 100.0f;
		WireframeMesh->SetWorldLocation(Center);
		WireframeMesh->SetWorldScale3D(Scale);
		// 材质：用 WireframeDynamicMaterial 或 WireframeMaterial
		if (WireframeDynamicMaterial)
		{
			WireframeMesh->SetMaterial(0, WireframeDynamicMaterial);
		}
		else if (WireframeMaterial)
		{
			WireframeMesh->SetMaterial(0, WireframeMaterial);
		}
		WireframeMesh->SetHiddenInGame(false);
	}

	// --- 2. 家具 StaticMesh 半透明预览（使用池中 Index=0 的组件）---
	ClearFurniturePreviewPool();
	if (FurnitureMesh)
	{
		UStaticMeshComponent* Comp = AcquireFurnitureMeshComponent(0);
		if (Comp)
		{
			Comp->SetStaticMesh(FurnitureMesh);

			// 位置：家具原点 = PlaceIndex * BlockLength（与 SpawnItemAtGrid 一致）
			const FVector WorldPos = FVector(PlaceIndex) * BlockLength;
			Comp->SetWorldLocation(WorldPos);

			// 旋转：Direction → Yaw（与 SpawnItemAtGrid 一致）
			FRotator Rot = FRotator::ZeroRotator;
			switch (Direction)
			{
			case EActorDirection::Forward:  Rot = FRotator(0, 0, 0);   break;
			case EActorDirection::Right:    Rot = FRotator(0, 90, 0);  break;
			case EActorDirection::Backward: Rot = FRotator(0, 180, 0); break;
			case EActorDirection::Left:     Rot = FRotator(0, 270, 0); break;
			}
			Comp->SetWorldRotation(Rot);

			// 材质：根据 bPreviewValid 选择正常或无效材质
			UMaterialInterface* FurnitureMat = bPreviewValid ? FurniturePreviewMaterial : (FurniturePreviewInvalidMaterial ? FurniturePreviewInvalidMaterial : FurniturePreviewMaterial);
			UMaterialInstanceDynamic* FurnitureMID = UMaterialInstanceDynamic::Create(FurnitureMat, this);
			if (FurnitureMID)
			{
				// 设置 PBR 贴图参数（M_FurniturePreview 需包含 BaseColorMap/NormalMap/ORMMap 纹理参数节点）
				if (TextureSet.BaseColorMap.IsValid())
				{
					if (UTexture2D* Tex = TextureSet.BaseColorMap.LoadSynchronous())
					{
						FurnitureMID->SetTextureParameterValue(FName(TEXT("BaseColorMap")), Tex);
					}
				}
				if (TextureSet.NormalMap.IsValid())
				{
					if (UTexture2D* Tex = TextureSet.NormalMap.LoadSynchronous())
					{
						FurnitureMID->SetTextureParameterValue(FName(TEXT("NormalMap")), Tex);
					}
				}
				if (TextureSet.ORMMap.IsValid())
				{
					if (UTexture2D* Tex = TextureSet.ORMMap.LoadSynchronous())
					{
						FurnitureMID->SetTextureParameterValue(FName(TEXT("ORMMap")), Tex);
					}
				}
				Comp->SetMaterial(0, FurnitureMID);
			}

			Comp->SetHiddenInGame(false);
			FurniturePreviewActiveCount = 1;
		}
	}
}

// === AcquireFurnitureMeshComponent ===
UStaticMeshComponent* AVoxelPreviewActor::AcquireFurnitureMeshComponent(int32 Index)
{
	// 池不够大时创建新组件
	while (FurniturePreviewMeshPool.Num() <= Index)
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this,
			FName(*FString::Printf(TEXT("FurniturePool_%d"), FurniturePreviewMeshPool.Num())));
		Comp->SetupAttachment(RootComponent);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(false);
		Comp->SetHiddenInGame(true);
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->RegisterComponent();
		FurniturePreviewMeshPool.Add(Comp);
	}
	return FurniturePreviewMeshPool[Index];
}

// === ClearFurniturePreviewPool ===
void AVoxelPreviewActor::ClearFurniturePreviewPool()
{
	for (UStaticMeshComponent* Comp : FurniturePreviewMeshPool)
	{
		if (Comp)
		{
			Comp->SetHiddenInGame(true);
			Comp->SetStaticMesh(nullptr);
			Comp->SetRelativeScale3D(FVector::OneVector);
			Comp->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}
	FurniturePreviewActiveCount = 0;
}

// === UpdateTemplateFurniturePreview ===
void AVoxelPreviewActor::UpdateTemplateFurniturePreview(
	const FVoxelTemplate& Tmpl,
	const FVoxelGeometryDesc& Geometry,
	const TMap<FString, FString>& Remap,
	UConfigDataSubsystem* CDS,
	float BlockLength)
{
	ClearFurniturePreviewPool();
	if (!CDS || Tmpl.Entries.Num() == 0) return;

	// 必须在蓝图配置 FurniturePreviewMaterial（半透明预览材质）
	if (!FurniturePreviewMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelPreviewActor::UpdateTemplateFurniturePreview: FurniturePreviewMaterial is null! Must configure in Blueprint."));
		return;
	}

	// 1. 计算模板局部包围盒（与 UpdateTemplatePreview 一致）
	FIntVector TmplMin = Tmpl.Size;
	FIntVector TmplMax = FIntVector::ZeroValue;
	for (const auto& Pair : Tmpl.Entries)
	{
		const FIntVector& Pos = Pair.Key;
		TmplMin = FIntVector(FMath::Min(TmplMin.X, Pos.X), FMath::Min(TmplMin.Y, Pos.Y), FMath::Min(TmplMin.Z, Pos.Z));
		TmplMax = FIntVector(FMath::Max(TmplMax.X, Pos.X), FMath::Max(TmplMax.Y, Pos.Y), FMath::Max(TmplMax.Z, Pos.Z));
	}
	const FIntVector TmplSize = TmplMax - TmplMin + FIntVector(1);

	// 2. 计算放置原点（与 UpdateTemplatePreview 一致）
	const FIntVector Origin = UVoxelTemplateUtils::GetCenteredPlacementOrigin(Tmpl, Geometry.Box.MinIndex);

	// 3. 旋转中心（与 UpdateTemplatePreview 一致，Actor 已位于此点）
	const FVector RotationCenterWorld = FVector(Origin + TmplMin + FIntVector(TmplSize.X / 2, TmplSize.Y / 2, 0)) * BlockLength;

	// 4. 遍历模板 Entries 中的家具
	int32 Index = 0;
	for (const auto& Pair : Tmpl.Entries)
	{
		const FIntVector& RelPos = Pair.Key;
		const FVoxelInstanceEntry& Entry = Pair.Value;
		if (!Entry.IsValid() || Entry.IsAir()) continue;

		// 应用素材替换
		FString FinalFullId = Entry.FullId;
		if (const FString* NewFullId = Remap.Find(Entry.FullId))
		{
			FinalFullId = *NewFullId;
		}

		const int32 Handle = CDS->GetHandleByFullId(FinalFullId);
		if (Handle == EBlockId::Invalid) continue;

		const FVoxelItemBase* Item = CDS->GetItemTypeById<FVoxelItemBase>(Handle);
		if (!Item) continue;

		// 只处理家具（方块由 BlueprintMesh 渲染）
		if (!Item->ItemTag.MatchesTag(FCoreTags::Item_Type_BuildItem)) continue;

		UStaticMesh* Mesh = Item->AssetFragment.Mesh;
		if (!Mesh) continue;

		// 获取/创建池中组件
		UStaticMeshComponent* Comp = AcquireFurnitureMeshComponent(Index);
		if (!Comp) continue;

		Comp->SetStaticMesh(Mesh);

		// 相对位置（未旋转，Actor 旋转会自动应用）
		// 与 BlueprintMesh 的逻辑一致：家具世界位置 = (Origin + RelPos) * BlockLength
		// 相对位置 = 世界位置 - Actor位置(RotationCenterWorld)
		const FVector RelativePos = FVector(Origin + RelPos) * BlockLength - RotationCenterWorld;
		Comp->SetRelativeLocation(RelativePos);

		// 相对旋转（只含家具自身朝向，Actor 旋转自动叠加 = ComposeDirection(Entry.Direction, BaseDir)）
		FRotator Rot = FRotator::ZeroRotator;
		switch (Entry.Direction)
		{
		case EActorDirection::Forward:  Rot = FRotator(0, 0, 0);   break;
		case EActorDirection::Right:    Rot = FRotator(0, 90, 0);  break;
		case EActorDirection::Backward: Rot = FRotator(0, 180, 0); break;
		case EActorDirection::Left:     Rot = FRotator(0, 270, 0); break;
		}
		Comp->SetRelativeRotation(Rot);

		// 材质：根据 bPreviewValid 选择正常或无效材质
		UMaterialInterface* FurnitureMat = bPreviewValid ? FurniturePreviewMaterial : (FurniturePreviewInvalidMaterial ? FurniturePreviewInvalidMaterial : FurniturePreviewMaterial);
		UMaterialInstanceDynamic* FurnitureMID = UMaterialInstanceDynamic::Create(FurnitureMat, this);
		if (FurnitureMID)
		{
			const FVoxelTextureSet& TexSet = Item->AssetFragment.TextureSet;
			if (TexSet.BaseColorMap.IsValid())
			{
				if (UTexture2D* Tex = TexSet.BaseColorMap.LoadSynchronous())
				{
					FurnitureMID->SetTextureParameterValue(FName(TEXT("BaseColorMap")), Tex);
				}
			}
			if (TexSet.NormalMap.IsValid())
			{
				if (UTexture2D* Tex = TexSet.NormalMap.LoadSynchronous())
				{
					FurnitureMID->SetTextureParameterValue(FName(TEXT("NormalMap")), Tex);
				}
			}
			if (TexSet.ORMMap.IsValid())
			{
				if (UTexture2D* Tex = TexSet.ORMMap.LoadSynchronous())
				{
					FurnitureMID->SetTextureParameterValue(FName(TEXT("ORMMap")), Tex);
				}
			}
			Comp->SetMaterial(0, FurnitureMID);
		}

		Comp->SetHiddenInGame(false);
		++Index;
	}
	FurniturePreviewActiveCount = Index;
}

// === SetPreviewVisible ===
void AVoxelPreviewActor::SetPreviewVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
}

// === MovePreview ===
void AVoxelPreviewActor::MovePreview(FVector DeltaPosition)
{
	FVector NewLocation = GetActorLocation() + DeltaPosition;
	SetActorLocation(NewLocation);
}

// === RotatePreview ===
void AVoxelPreviewActor::RotatePreview(FRotator Rotation)
{
	FRotator NewRotation = GetActorRotation() + Rotation;
	SetActorRotation(NewRotation);
}

// === UpdatePreviewComponentVisibility（内部） ===
// 注意：BrushPreviewMesh 的可见性独立管理，由 UpdateBrushPreview / ClearBrushPreview 控制
// 这里不操作 BrushPreviewMesh，避免切换选区模式时闪烁
void AVoxelPreviewActor::UpdatePreviewComponentVisibility()
{
	// 根据当前模式显示/隐藏组件
	switch (CurrentMode)
	{
	case EPreviewMode::None:
		SetPreviewVisible(false);
		if (WireframeMesh) WireframeMesh->SetHiddenInGame(true);
		if (InnerMesh) InnerMesh->SetHiddenInGame(true);
		if (ThicknessMesh) ThicknessMesh->SetHiddenInGame(true);
		if (BlueprintMesh) BlueprintMesh->SetHiddenInGame(true);
		break;

	case EPreviewMode::SingleVoxel:
		SetPreviewVisible(true);
		if (WireframeMesh) WireframeMesh->SetHiddenInGame(true);
		if (InnerMesh) InnerMesh->SetHiddenInGame(false);  // ✅ 显示内部
		if (ThicknessMesh) ThicknessMesh->SetHiddenInGame(true);
		if (BlueprintMesh) BlueprintMesh->SetHiddenInGame(true);
		break;

	case EPreviewMode::SimpleGeometry:
		SetPreviewVisible(true);
		if (WireframeMesh) WireframeMesh->SetHiddenInGame(false);  // ✅ 显示外框
		// InnerMesh / ThicknessMesh 默认隐藏，由 UpdateSimpleGeometryPreviewWithSelection
		// 内部调用 UpdateInnerMesh 后再决定显隐（几何体编辑工具需要显示填充和厚度）
		if (InnerMesh) InnerMesh->SetHiddenInGame(true);
		if (ThicknessMesh) ThicknessMesh->SetHiddenInGame(true);
		if (BlueprintMesh) BlueprintMesh->SetHiddenInGame(false);  // ✅ 显示体素高亮
		break;

	case EPreviewMode::ComplexBlueprint:
		SetPreviewVisible(true);
		if (WireframeMesh) WireframeMesh->SetHiddenInGame(false);  // ✅ 显示外框
		if (InnerMesh) InnerMesh->SetHiddenInGame(true);
		if (ThicknessMesh) ThicknessMesh->SetHiddenInGame(true);
		if (BlueprintMesh) BlueprintMesh->SetHiddenInGame(false);         // ✅ 显示蓝图
		break;

	case EPreviewMode::BrushPreview:
		// 【已废弃】UpdateBrushPreview 不再切换到此模式
		// 笔刷预览现在独立于 PreviewMode，通过 BrushPreviewMesh 直接渲染
		// 保留此分支以兼容枚举完整性，但不做任何操作
		break;

	case EPreviewMode::Furniture:
		// 家具预览：WireframeMesh（包围盒线框）+ FurniturePreviewMeshPool（半透明模型池）
		// 池中组件的显隐由 UpdateFurniturePreview / UpdateTemplateFurniturePreview 管理
		SetPreviewVisible(true);
		if (WireframeMesh) WireframeMesh->SetHiddenInGame(false);
		if (InnerMesh) InnerMesh->SetHiddenInGame(true);
		if (ThicknessMesh) ThicknessMesh->SetHiddenInGame(true);
		if (BlueprintMesh) BlueprintMesh->SetHiddenInGame(true);
		break;
	}
}

// === UpdateMeshByShape（内部） - 复现 UpdateAreaByBox 逻辑 ===
void AVoxelPreviewActor::UpdateMeshByShape(
	UStaticMeshComponent* MeshComponent,
	EBatchShapeType ShapeType,
	FBox Bounds,
	FVector GeometryCenter,
	FRotator GeometryRotation)
{
	if (!MeshComponent)
	{
		return;
	}

	// 1. 设置 StaticMesh
	UStaticMesh* TargetMesh = nullptr;

	// 【重要】WireframeMesh 永远只用盒体展示边界
	if (MeshComponent == WireframeMesh)
	{
		TargetMesh = BoxMesh;
	}
	else
	{
		// InnerMesh 根据形状类型选择
		switch (ShapeType)
		{
		case EBatchShapeType::Sphere:
			TargetMesh = SphereMesh;
			break;
		case EBatchShapeType::Cylinder:
			TargetMesh = CylinderMesh;
			break;
		case EBatchShapeType::Box:
		case EBatchShapeType::Null:
		default:
			TargetMesh = BoxMesh;
			break;
		}
	}

	// 2. 空指针检查
	if (!TargetMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelPreviewActor: Target mesh is null for shape type %d"), (int32)ShapeType);
		return;
	}

	// 如果网格没变，就不必重复 Set（优化性能）
	if (MeshComponent->GetStaticMesh() != TargetMesh)
	{
		MeshComponent->SetStaticMesh(TargetMesh);
	}

	// 3. 计算缩放（核心修复）
	FBox MeshBounds = TargetMesh->GetBoundingBox();
	FVector MeshSize = MeshBounds.GetSize();
	FVector TargetSize = Bounds.GetSize();

	// 防止除以零的安全检查
	if (MeshSize.X < KINDA_SMALL_NUMBER || MeshSize.Y < KINDA_SMALL_NUMBER || MeshSize.Z < KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning, TEXT("AVoxelPreviewActor: Mesh %s has zero size bounds, cannot scale."), *TargetMesh->GetName());
		return;
	}

	// 使用向量直接计算，避免手写 XYZ 减法出错
	FVector NewScale = TargetSize / MeshSize;
	MeshComponent->SetRelativeScale3D(NewScale);

	// 4. 计算Mesh位置（基于旋转中心）
	// 注意：这里假设 Mesh 的枢轴点(Pivot)在中心。
	// Mesh的世界位置 = 旋转中心 + 旋转向量(Mesh中心 - 旋转中心)
	FVector BoundsCenter = Bounds.GetCenter();
	FVector OffsetFromCenter = BoundsCenter - GeometryCenter;
	FVector RotatedOffset = GeometryRotation.RotateVector(OffsetFromCenter);
	FVector MeshWorldLocation = GeometryCenter + RotatedOffset;

	// 将世界坐标中心转换为相对于 Actor 的局部坐标
	MeshComponent->SetWorldLocation(MeshWorldLocation);

	// 5. 应用旋转（Mesh跟随Actor旋转）
	MeshComponent->SetWorldRotation(GeometryRotation);
}

// === GetMaterialByBlockType（内部） ===
// 获取 Chunk 使用的共享 MID（已设置 Texture2DArray 参数，所有方块共用）
UMaterialInterface* AVoxelPreviewActor::GetMaterialByBlockType(int32 BlockType)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	// 优先从 VoxelBlockWorldSubsystem 获取共享 Chunk MID（包含 Texture2DArray 参数）
	if (UVoxelBlockWorldSubsystem* BlockSubsystem = GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>())
	{
		if (UMaterialInstanceDynamic* SharedMID = BlockSubsystem->GetSharedChunkMID())
		{
			return SharedMID;
		}
	}

	// 回退：从已加载区块获取原始材质
	if (UVoxelBlockWorldSubsystem* BlockSubsystem = GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>())
	{
		for (const auto& Pair : BlockSubsystem->LoadedChunks)
		{
			if (AChunkBase* Chunk = Pair.Value.Get())
			{
				if (Chunk->Material)
				{
					return Chunk->Material;
				}
			}
		}
	}

	return nullptr;
}

// === ApplyTextureArrayParams ===
// 为 MID 设置 Texture2DArray 参数（从 VoxelTextureRegistry 获取运行时贴图数组）
void AVoxelPreviewActor::ApplyTextureArrayParams(UMaterialInstanceDynamic* MID)
{
	if (!MID || !GetWorld()) return;

	if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
	{
		if (UVoxelTextureRegistry* TexRegistry = GI->GetSubsystem<UVoxelTextureRegistry>())
		{
			if (UTexture2DArray* BaseColorArray = TexRegistry->GetBaseColorArray())
			{
				MID->SetTextureParameterValue(FName(TEXT("BaseColorArray")), BaseColorArray);
			}
			if (UTexture2DArray* NormalArray = TexRegistry->GetNormalArray())
			{
				MID->SetTextureParameterValue(FName(TEXT("NormalArray")), NormalArray);
			}
			if (UTexture2DArray* ORMArray = TexRegistry->GetORMArray())
			{
				MID->SetTextureParameterValue(FName(TEXT("ORMArray")), ORMArray);
			}
		}
	}
}

// ==================== 工具基类接口实现 ====================

// === SetVoxelPositions ===
void AVoxelPreviewActor::SetVoxelPositions(const TArray<FIntVector>& VoxelPositions)
{
	// TODO: 实现体素位置的可视化
	// 可以使用以下方法：
	// 1. 在每个位置创建小型方块实例
	// 2. 使用程序化网格生成所有体素
	// 3. 使用粒子系统显示位置

	// 临时实现：更新包围盒以包含所有体素
	if (VoxelPositions.Num() > 0)
	{
		FVector MinWorldPos = FVector(VoxelPositions[0]);
		FVector MaxWorldPos = MinWorldPos;

		for (const FIntVector& VoxelPos : VoxelPositions)
		{
			FVector WorldPos = FVector(VoxelPos);
			MinWorldPos.X = FMath::Min(MinWorldPos.X, WorldPos.X);
			MinWorldPos.Y = FMath::Min(MinWorldPos.Y, WorldPos.Y);
			MinWorldPos.Z = FMath::Min(MinWorldPos.Z, WorldPos.Z);

			MaxWorldPos.X = FMath::Max(MaxWorldPos.X, WorldPos.X + 1);
			MaxWorldPos.Y = FMath::Max(MaxWorldPos.Y, WorldPos.Y + 1);
			MaxWorldPos.Z = FMath::Max(MaxWorldPos.Z, WorldPos.Z + 1);
		}

		CurrentBounds =FBox(MinWorldPos,MaxWorldPos) ;
	}
}

// === ClearPreviewDisplay ===
void AVoxelPreviewActor::ClearPreviewDisplay()
{
	// 清除所有预览显示
	SetPreviewVisible(false);
	CurrentBounds = FBox();
}

// ==================== 预览交互状态管理实现 ====================
bool AVoxelPreviewActor::CanTransitionTo(EPreviewInteractionState NewState) const
{
	// 当前实现：允许所有状态转换
	// 如果需要限制状态转换，可以在这里添加规则

	// 示例：禁止从 CONFIRM 状态转换到 EDIT 状态（必须先回到 BASE）
	// if (PreviewState == EPreviewInteractionState::IDLE && NewState == EPreviewInteractionState::EDITING)
	// {
	//     return false;
	// }

	return true;
}

// === SetPreviewState ===
void AVoxelPreviewActor::SetPreviewState(EPreviewInteractionState NewState)
{
	// 【重构后】PreviewActor 不再持有状态，只根据传入的状态做视觉响应
	// 真正的状态在 StateComponent->BuildPhase

	switch (NewState)
	{
	case EPreviewInteractionState::IDLE:
		// 不隐藏整个 Actor：BrushPreviewMesh 是独立组件，可能仍需要显示
		// （选区高亮已迁移到独立的 AVoxelSelectionPreviewActor，不再由 PreviewActor 管理）
		if (WireframeMesh) WireframeMesh->SetHiddenInGame(true);
		if (InnerMesh) InnerMesh->SetHiddenInGame(true);
		if (ThicknessMesh) ThicknessMesh->SetHiddenInGame(true);
		if (BlueprintMesh) BlueprintMesh->SetHiddenInGame(true);
		UE_LOG(LogTemp, Log, TEXT("AVoxelPreviewActor::SetPreviewState: IDLE - mode components hidden"));
		break;

	case EPreviewInteractionState::PLACING:
		SetActorHiddenInGame(false);
		UE_LOG(LogTemp, Log, TEXT("AVoxelPreviewActor::SetPreviewState: PLACING - Base selection box shown"));
		break;

	case EPreviewInteractionState::EDITING:
		SetActorHiddenInGame(false);
		UE_LOG(LogTemp, Log, TEXT("AVoxelPreviewActor::SetPreviewState: EDITING - Edit mode shown"));
		break;
	}
}

// ==================== Geometry 操作回调实现 ====================

// === GetPreviewColor ===
FLinearColor AVoxelPreviewActor::GetPreviewColor(const FVoxelEditCommand& EditCommand) const
{
	// 根据放置规则返回颜色
	switch (EditCommand.PlacementMode)
	{
	case EVoxelPlacementMode::Normal:
		return FLinearColor::Green;  // 普通建造：绿色
	case EVoxelPlacementMode::Complement:
		return FLinearColor::Blue;   // 填充：蓝色
	case EVoxelPlacementMode::Replace:
		return FLinearColor::Yellow; // 替换：黄色
	case EVoxelPlacementMode::ReplaceMask:
		return FLinearColor::Red;   // 指定替换：青色
	default:
		return FLinearColor::White;
	}
}

// === UpdateFromEditRequest ===
void AVoxelPreviewActor::UpdateFromEditRequest(const FVoxelEditRequest& Request)
{
	SetPreviewVisible(true);
	if (Request.Geometry.IsValid())
	{
		float BlockLength = 20.0f;
		if (UVoxelGameInstance* GameInstance = GetWorld()->GetGameInstance<UVoxelGameInstance>())
		{
			BlockLength = GameInstance->GetBlockLength();
		}
		UpdateSimpleGeometryPreview(Request, BlockLength);
	}
}

// === AddCubeMeshData（辅助方法） ===
void AVoxelPreviewActor::AddCubeMeshData(
	FChunkMeshData& OutMeshData,
	const FVector& BoxMin,
	const FVector& BoxMax)
{
	// 立方体的8个顶点
	const FVector Vertices[8] = {
		FVector(BoxMin.X, BoxMin.Y, BoxMin.Z),  // 0: 左下前
		FVector(BoxMax.X, BoxMin.Y, BoxMin.Z),  // 1: 右下前
		FVector(BoxMax.X, BoxMax.Y, BoxMin.Z),  // 2: 右上前
		FVector(BoxMin.X, BoxMax.Y, BoxMin.Z),  // 3: 左上前
		FVector(BoxMin.X, BoxMin.Y, BoxMax.Z),  // 4: 左下后
		FVector(BoxMax.X, BoxMin.Y, BoxMax.Z),  // 5: 右下后
		FVector(BoxMax.X, BoxMax.Y, BoxMax.Z),  // 6: 右上后
		FVector(BoxMin.X, BoxMax.Y, BoxMax.Z)   // 7: 左上后
	};

	// 6个面的三角形索引（每个面2个三角形，逆时针顺序）
	const int32 FaceTriangles[6][6] = {
		{0, 1, 2, 0, 2, 3},  // 前面 (Z = Min)
		{5, 4, 7, 5, 7, 6},  // 后面 (Z = Max)
		{3, 2, 6, 3, 6, 7},  // 上面 (Y = Max)
		{0, 4, 5, 0, 5, 1},  // 下面 (Y = Min)
		{4, 0, 3, 4, 3, 7},  // 左面 (X = Min)
		{1, 5, 6, 1, 6, 2}   // 右面 (X = Max)
	};

	// 6个面的法线
	const FVector FaceNormals[6] = {
		FVector(0, 0, -1),  // 前面
		FVector(0, 0,  1),  // 后面
		FVector(0, 1,  0),  // 上面
		FVector(0, -1, 0),  // 下面
		FVector(-1, 0, 0),  // 左面
		FVector(1,  0,  0)   // 右面
	};

	// 顶点颜色（白色）
	const FColor VertexColor = FColor::White;

	// UV坐标（简化）
	const FVector2D UV = FVector2D::ZeroVector;

	// 当前顶点数量（用于计算三角形索引）
	const int32 VertexCount = OutMeshData.Vertices.Num();

	// 添加顶点和三角形
	for (int32 Face = 0; Face < 6; ++Face)
	{
		// 添加4个顶点（每个面）
		for (int32 i = 0; i < 6; ++i)
		{
			int32 VertexIndex = FaceTriangles[Face][i];
			OutMeshData.Vertices.Add(Vertices[VertexIndex]);
			OutMeshData.Normals.Add(FaceNormals[Face]);
			OutMeshData.UV0.Add(UV);
			OutMeshData.Colors.Add(VertexColor);
		}

		// 添加2个三角形的索引
		OutMeshData.Triangles.Add(VertexCount + 0);
		OutMeshData.Triangles.Add(VertexCount + 1);
		OutMeshData.Triangles.Add(VertexCount + 2);

		OutMeshData.Triangles.Add(VertexCount + 3);
		OutMeshData.Triangles.Add(VertexCount + 4);
		OutMeshData.Triangles.Add(VertexCount + 5);
	}
}

// === AddFaceMeshData（辅助方法 - 单面，4顶点2三角形） ===
void AVoxelPreviewActor::AddFaceMeshData(
	FChunkMeshData& OutMeshData,
	const FVector& BoxMin,
	const FVector& BoxMax,
	int32 FaceIndex,
	const FColor& Color)
{
	const int32 VertexCount = OutMeshData.Vertices.Num();

	// 每个面 4 个顶点 + 2 个三角形（CCW 外向法线）
	FVector V0, V1, V2, V3, Normal;

	switch (FaceIndex)
	{
	case 0: // -X (Backward)
		V0 = FVector(BoxMin.X, BoxMin.Y, BoxMin.Z);
		V1 = FVector(BoxMin.X, BoxMin.Y, BoxMax.Z);
		V2 = FVector(BoxMin.X, BoxMax.Y, BoxMax.Z);
		V3 = FVector(BoxMin.X, BoxMax.Y, BoxMin.Z);
		Normal = FVector(-1, 0, 0);
		break;
	case 1: // +X (Forward)
		V0 = FVector(BoxMax.X, BoxMin.Y, BoxMax.Z);
		V1 = FVector(BoxMax.X, BoxMin.Y, BoxMin.Z);
		V2 = FVector(BoxMax.X, BoxMax.Y, BoxMin.Z);
		V3 = FVector(BoxMax.X, BoxMax.Y, BoxMax.Z);
		Normal = FVector(1, 0, 0);
		break;
	case 2: // -Y (Left)
		V0 = FVector(BoxMin.X, BoxMin.Y, BoxMin.Z);
		V1 = FVector(BoxMax.X, BoxMin.Y, BoxMin.Z);
		V2 = FVector(BoxMax.X, BoxMin.Y, BoxMax.Z);
		V3 = FVector(BoxMin.X, BoxMin.Y, BoxMax.Z);
		Normal = FVector(0, -1, 0);
		break;
	case 3: // +Y (Right)
		V0 = FVector(BoxMin.X, BoxMax.Y, BoxMin.Z);
		V1 = FVector(BoxMin.X, BoxMax.Y, BoxMax.Z);
		V2 = FVector(BoxMax.X, BoxMax.Y, BoxMax.Z);
		V3 = FVector(BoxMax.X, BoxMax.Y, BoxMin.Z);
		Normal = FVector(0, 1, 0);
		break;
	case 4: // -Z (Down)
		V0 = FVector(BoxMin.X, BoxMin.Y, BoxMin.Z);
		V1 = FVector(BoxMin.X, BoxMax.Y, BoxMin.Z);
		V2 = FVector(BoxMax.X, BoxMax.Y, BoxMin.Z);
		V3 = FVector(BoxMax.X, BoxMin.Y, BoxMin.Z);
		Normal = FVector(0, 0, -1);
		break;
	case 5: // +Z (Up)
		V0 = FVector(BoxMin.X, BoxMin.Y, BoxMax.Z);
		V1 = FVector(BoxMax.X, BoxMin.Y, BoxMax.Z);
		V2 = FVector(BoxMax.X, BoxMax.Y, BoxMax.Z);
		V3 = FVector(BoxMin.X, BoxMax.Y, BoxMax.Z);
		Normal = FVector(0, 0, 1);
		break;
	default:
		return;
	}

	// 添加 4 个顶点
	OutMeshData.Vertices.Add(V0);
	OutMeshData.Vertices.Add(V1);
	OutMeshData.Vertices.Add(V2);
	OutMeshData.Vertices.Add(V3);

	// 法线
	for (int32 i = 0; i < 4; ++i)
		OutMeshData.Normals.Add(Normal);

	// UV（简化为 0，纹理索引通过顶点颜色 Alpha 传递）
	for (int32 i = 0; i < 4; ++i)
		OutMeshData.UV0.Add(FVector2D::ZeroVector);

	// 顶点颜色（Alpha = 纹理索引）
	for (int32 i = 0; i < 4; ++i)
		OutMeshData.Colors.Add(Color);

	// 2 个三角形（CCW 缠绕，外向法线）
	OutMeshData.Triangles.Add(VertexCount + 0);
	OutMeshData.Triangles.Add(VertexCount + 1);
	OutMeshData.Triangles.Add(VertexCount + 2);

	OutMeshData.Triangles.Add(VertexCount + 0);
	OutMeshData.Triangles.Add(VertexCount + 2);
	OutMeshData.Triangles.Add(VertexCount + 3);
}


