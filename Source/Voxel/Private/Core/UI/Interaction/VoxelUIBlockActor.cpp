// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/UI/Interaction/VoxelUIBlockActor.h"

// Sets default values
AVoxelUIBlockActor::AVoxelUIBlockActor()
{
	PrimaryActorTick.bCanEverTick = false; // UI Actor 通常不需要 Tick，优化性能

	// 1. 创建独立的根组件
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 2. 形状网格（挂载到根）
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShapeMesh"));
	MeshComponent->SetupAttachment(RootComponent);
	// 禁用碰撞，纯视觉
	//MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); 

	// 3. 线框网格（挂载到根，独立控制）
	WireFrameMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WireFrameMesh"));
	WireFrameMeshComponent->SetupAttachment(RootComponent);
	//WireFrameMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AVoxelUIBlockActor::UpdateAreaByBox(EBatchShapeType InputShape,FBox Bound, float BlockLength)
{
	if (!MeshComponent || !WireFrameMeshComponent) return;

	// 1. 计算基础几何信息
	// 假设 Bound 是体素索引，例如 Min(0,0,0) Max(0,0,0) 代表 1 个块
	// 实际尺寸 = (Max - Min + 1) * BlockLength
	FVector BoxSize = (Bound.Max - Bound.Min + FVector(1.0f)) * BlockLength;
	FVector CenterPos = (Bound.Min * BlockLength) + (BoxSize * 0.5f);

	// 2. 更新 Actor 的世界位置（以中心点为锚点）
	// 注意：我们将 Actor 放在包围盒中心，子组件只需要处理相对缩放
	this->SetActorLocation(CenterPos); 
	this->SetActorRotation(FRotator::ZeroRotator);

	// 3. 处理线框 (WireFrame) - 始终是包围盒
	// 假设基础 Mesh（如 Cube）的大小是 100x100x100 (UE标准)
	// Scale = 目标尺寸 / 100.0f
	FVector WireFrameScale = BoxSize / 100.0f;
    
	WireFrameMeshComponent->SetStaticMesh(WireFrameCube);
	WireFrameMeshComponent->SetRelativeScale3D(WireFrameScale);
	WireFrameMeshComponent->SetRelativeLocation(CenterPos); // 居中

	// 4. 处理实体形状 (Shape Mesh)
	UStaticMesh* TargetMesh = Cube;
	FVector ShapeScale = WireFrameScale; // 默认跟随盒子
	

	switch (InputShape)
	{
	case EBatchShapeType::Sphere:
		if (Sphere) TargetMesh = Sphere;
		{
		// 策略：内切球 (Inscribed Sphere) - 以最短边为直径
		// 或者：拉伸球 (Ellipsoid) - 填满盒子
		// 这里演示“填满盒子”的拉伸球，这通常在体素编辑器里更直观
		ShapeScale = BoxSize / 100.0f; 
		}
		break;

	case EBatchShapeType::Cylinder:
		if (Tube) TargetMesh = Tube;
		{
		// 圆柱体填满盒子
		ShapeScale = BoxSize / 100.0f;
		}
		break;

	default: // Box
		if (Cube) TargetMesh = Cube;
		ShapeScale = BoxSize / 100.0f;
		break;
	}

	MeshComponent->SetStaticMesh(TargetMesh);
	MeshComponent->SetRelativeScale3D(ShapeScale);
	MeshComponent->SetRelativeLocation(CenterPos);
}
void AVoxelUIBlockActor::UpdateState(EUIStateEnum Input)
{
	UIState = Input;
	switch(UIState)
	{
	case EUIStateEnum::Normal:
		MeshComponent->SetMaterial(0,NorMaterial);
		break;
	case EUIStateEnum::Selected:
		MeshComponent->SetMaterial(0,SelecMaterial);
		break;
	case EUIStateEnum::Error:
		MeshComponent->SetMaterial(0,ErrorMaterial);
		break;
	default:
		MeshComponent->SetMaterial(0,NorMaterial);
		break;
		
	}
}

// Called when the game starts or when spawned
void AVoxelUIBlockActor::BeginPlay()
{
	Super::BeginPlay();
	
}


