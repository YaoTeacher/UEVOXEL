// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/VoxelSelectionPreviewActor.h"
#include "Voxel/Utils/ChunkMeshData.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"

// === 构造函数 ===
AVoxelSelectionPreviewActor::AVoxelSelectionPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootComp);

	// 选区高亮网格组件（程序化网格）
	SelectionMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SelectionMesh"));
	SelectionMesh->SetupAttachment(RootComponent);
	SelectionMesh->SetCastShadow(false);
	SelectionMesh->SetHiddenInGame(true);
	SelectionMesh->SetCanEverAffectNavigation(false);

	// 选区外框组件（静态网格）
	SelectionFrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionFrameMesh"));
	SelectionFrameMesh->SetupAttachment(RootComponent);
	SelectionFrameMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SelectionFrameMesh->SetCastShadow(false);
	SelectionFrameMesh->SetHiddenInGame(true);
	SelectionFrameMesh->SetCanEverAffectNavigation(false);
}

// === BeginPlay ===
void AVoxelSelectionPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	// 加载默认 BoxMesh（如果未配置）
	if (!BoxMesh)
	{
		BoxMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
}

// === UpdateSelectionPreview ===
void AVoxelSelectionPreviewActor::UpdateSelectionPreview(const TArray<FIntVector>& SelectedVoxels, float BlockLength)
{
	if (!SelectionMesh || !SelectionFrameMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[SelectionPreview] SelectionMesh or SelectionFrameMesh is NULL"));
		return;
	}

	SelectionMesh->ClearMeshSection(0);

	if (SelectedVoxels.Num() == 0)
	{
		SelectionMesh->SetHiddenInGame(true);
		SelectionFrameMesh->SetHiddenInGame(true);
		return;
	}

	// 确保 Actor 可见
	SetActorHiddenInGame(false);

	UVoxelBlockWorldSubsystem* BlockSys = GetWorld() ? GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>() : nullptr;
	if (!BlockSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SelectionPreview] BlockWorldSubsystem not available"));
		SelectionMesh->SetHiddenInGame(true);
		return;
	}

	// 计算选区 Min/Max
	FIntVector Min = SelectedVoxels[0];
	FIntVector Max = SelectedVoxels[0];
	for (const FIntVector& Pos : SelectedVoxels)
	{
		Min = FIntVector(FMath::Min(Min.X, Pos.X), FMath::Min(Min.Y, Pos.Y), FMath::Min(Min.Z, Pos.Z));
		Max = FIntVector(FMath::Max(Max.X, Pos.X), FMath::Max(Max.Y, Pos.Y), FMath::Max(Max.Z, Pos.Z));
	}

	// 选区外框
	const FBox SelectionBounds(FVector(Min) * BlockLength, FVector(Max + FIntVector(1)) * BlockLength);
	if (BoxMesh && SelectionFrameMesh->GetStaticMesh() != BoxMesh)
	{
		SelectionFrameMesh->SetStaticMesh(BoxMesh);
	}
	const FVector MeshSize = BoxMesh ? BoxMesh->GetBoundingBox().GetSize() : FVector(100.0f);
	const FVector TargetSize = SelectionBounds.GetSize();
	const FVector NewScale = TargetSize / MeshSize;
	SelectionFrameMesh->SetRelativeScale3D(NewScale);
	SelectionFrameMesh->SetWorldLocation(SelectionBounds.GetCenter());
	SelectionFrameMesh->SetWorldRotation(FRotator::ZeroRotator);

	if (!WireframeDynamicMaterial && WireframeMaterial)
	{
		WireframeDynamicMaterial = UMaterialInstanceDynamic::Create(WireframeMaterial, this);
	}
	if (WireframeDynamicMaterial)
	{
		SelectionFrameMesh->SetMaterial(0, WireframeDynamicMaterial);
	}
	SelectionFrameMesh->SetHiddenInGame(false);

	// 生成选区体素网格
	FChunkGenContext Context;
	BlockSys->BuildSelectionContext(SelectedVoxels, Context);
	FChunkMeshData MeshData = BlockSys->GenerateMeshData(Context).Opaque;

	if (MeshData.Vertices.Num() == 0 || MeshData.Triangles.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[SelectionPreview] GenerateMeshData empty: V=%d T=%d"),
			MeshData.Vertices.Num(), MeshData.Triangles.Num());
		SelectionMesh->SetHiddenInGame(true);
		return;
	}

	// SelectionMesh 定位到选区 Min 的世界位置
	const FVector SelectionOrigin = FVector(Min) * BlockLength;
	SelectionMesh->SetWorldLocation(SelectionOrigin);
	SelectionMesh->SetWorldRotation(FRotator::ZeroRotator);

	SelectionMesh->CreateMeshSection(
		0,
		MeshData.Vertices,
		MeshData.Triangles,
		MeshData.Normals,
		MeshData.UV0,
		MeshData.Colors,
		TArray<FProcMeshTangent>(),
		false
	);

	// 材质
	UMaterialInterface* UseMaterial = SelectionMeshMaterial;
	if (UseMaterial)
	{
		if (!SelectionDynamicMaterial)
		{
			SelectionDynamicMaterial = UMaterialInstanceDynamic::Create(UseMaterial, this);
		}
		if (SelectionDynamicMaterial)
		{
			if (BlockLength > 0.0f)
			{
				SelectionDynamicMaterial->SetScalarParameterValue(FName(TEXT("BlockLength")), BlockLength);
			}
			SelectionMesh->SetMaterial(0, SelectionDynamicMaterial);
		}
		else
		{
			SelectionMesh->SetMaterial(0, UseMaterial);
		}
	}

	SelectionMesh->SetHiddenInGame(false);

	UE_LOG(LogTemp, Log, TEXT("[SelectionPreview] Updated: Voxels=%d, V=%d T=%d, MeshPos=%s"),
		SelectedVoxels.Num(), MeshData.Vertices.Num(), MeshData.Triangles.Num(),
		*SelectionMesh->GetComponentLocation().ToString());
}

// === ClearSelectionPreview ===
void AVoxelSelectionPreviewActor::ClearSelectionPreview()
{
	if (SelectionMesh)
	{
		SelectionMesh->ClearMeshSection(0);
		SelectionMesh->SetHiddenInGame(true);
	}
	if (SelectionFrameMesh)
	{
		SelectionFrameMesh->SetHiddenInGame(true);
		SelectionFrameMesh->SetWorldRotation(FRotator::ZeroRotator);
		SelectionFrameMesh->SetRelativeScale3D(FVector::OneVector);
	}
}
