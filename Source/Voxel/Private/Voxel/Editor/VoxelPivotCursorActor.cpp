#include "Voxel/Editor/VoxelPivotCursorActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/EngineTypes.h"

AVoxelPivotCursorActor::AVoxelPivotCursorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 碰撞球（根组件）
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->SetSphereRadius(50.0f);
	CollisionSphere->SetCollisionProfileName("UI");
	// Gizmo 射线用 Channel6（3DUI），Cursor/GroundMarker 不应阻挡
	CollisionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel6, ECR_Ignore);
	RootComponent = CollisionSphere;

	// 地面点标记（始终显示）
	CursorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CursorMesh"));
	CursorMesh->SetupAttachment(RootComponent);
	CursorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 落地竖线（Pawn 浮空时显示，从 Pawn 向下到地面）
	DropLine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DropLine"));
	DropLine->SetupAttachment(RootComponent);
	DropLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DropLine->SetVisibility(false);
	DropLine->SetHiddenInGame(true);

	// 旋转环标记（默认隐藏，MMB 按住时显示）
	RotationRings = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RotationRings"));
	RotationRings->SetupAttachment(RootComponent);
	RotationRings->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RotationRings->SetVisibility(false);
	RotationRings->SetHiddenInGame(true);

	// 尝试加载默认网格（运行时可在蓝图子类覆盖）
	// 地面点：小球或十字
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		CursorMesh->SetStaticMesh(SphereMeshFinder.Object);
		CursorMesh->SetRelativeScale3D(FVector(0.2f));
	}

	// 落地竖线：细长圆柱（默认沿 Z 轴，运行时按高度缩放定位）
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		DropLine->SetStaticMesh(CylinderMeshFinder.Object);
		// 圆柱默认单位高度(100cm)、半径50，缩放为细线：半径0.02、高度按运行时设置
		DropLine->SetRelativeScale3D(FVector(0.02f, 0.02f, 0.01f));
	}

	// 旋转环：使用引擎自带的圆环网格（若有），否则由蓝图配置
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RingMeshFinder(TEXT("/Engine/BasicShapes/Torus"));
	if (RingMeshFinder.Succeeded())
	{
		RotationRings->SetStaticMesh(RingMeshFinder.Object);
		RotationRings->SetRelativeScale3D(FVector(2.0f));
	}
}

void AVoxelPivotCursorActor::BeginPlay()
{
	Super::BeginPlay();
}

void AVoxelPivotCursorActor::SetCursorWorldLocation(const FVector& WorldLocation)
{
	SetActorLocation(WorldLocation);
}

void AVoxelPivotCursorActor::SetDropLine(const FVector& TopWorld, const FVector& BottomWorld)
{
	if (!DropLine) return;

	const float Height = TopWorld.Z - BottomWorld.Z;
	if (Height <= 1.0f)
	{
		// 贴地或反向：隐藏竖线
		DropLine->SetVisibility(false);
		DropLine->SetHiddenInGame(true);
		return;
	}

	// 圆柱默认高 100cm，居中于原点。缩放 Z = Height/100，位置取中点（世界坐标）。
	constexpr float MeshHeight = 100.0f;
	const FVector MidPoint = (TopWorld + BottomWorld) * 0.5f;
	DropLine->SetVisibility(true);
	DropLine->SetHiddenInGame(false);
	// DropLine 挂在 RootComponent 下，但 RootComponent 位置由 SetActorLocation 定到 Top（白球位置）。
	// 因此用相对偏移定位到中点：相对位置 = MidPoint - ActorLocation。
	const FVector ActorLoc = GetActorLocation();
	DropLine->SetRelativeLocation(MidPoint - ActorLoc);
	DropLine->SetRelativeScale3D(FVector(0.02f, 0.02f, Height / MeshHeight));
}

void AVoxelPivotCursorActor::SetRotationRingsVisible(bool bVisible)
{
	if (RotationRings)
	{
		RotationRings->SetVisibility(bVisible);
		RotationRings->SetHiddenInGame(!bVisible);
	}
}
