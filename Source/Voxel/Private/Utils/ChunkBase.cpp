// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/ChunkBase.h"

#include "ProceduralMeshComponent.h"
#include "NavigationSystem.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "Core/VoxelGameInstance.h"
#include "Materials/MaterialInstanceDynamic.h"

// Sets default values
AChunkBase::AChunkBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Mesh->SetCastShadow(false);
	Mesh->bNavigationRelevant = false; 
	Mesh->SetCanEverAffectNavigation(false);
	SetRootComponent(Mesh);

	// 半透明 Ghost 组件：渲染半透明体素 + 碰撞（挡 Pawn/物理，忽略选取射线 Channel1 与相机）
	// 碰撞通道与 Mesh 不同 -> 必须独立组件（PMC 通道响应是组件级统一）
	MeshGhost = CreateDefaultSubobject<UProceduralMeshComponent>("MeshGhost");
	MeshGhost->SetCastShadow(false);
	MeshGhost->SetupAttachment(Mesh);
	MeshGhost->SetCollisionProfileName(TEXT("BuildGroundGhost"));
	MeshGhost->bNavigationRelevant = false;
	MeshGhost->SetCanEverAffectNavigation(false);
	
}

// Called when the game starts or when spawned
void AChunkBase::BeginPlay()
{
	Super::BeginPlay();
	Setup();
}

void AChunkBase::ClearMeshData()
{
	VertexCount = 0;
	MeshData.Clear();
	TranslucentMeshData.Clear();
	
}
void AChunkBase::ClearMeshRender()
{
	if (Mesh && IsValid(Mesh)) 
	{
		Mesh->ClearAllMeshSections();
	}
	if (MeshGhost && IsValid(MeshGhost))
	{
		MeshGhost->ClearAllMeshSections();
	}
}

void AChunkBase::PrepareForReuse()
{
	// 确保在游戏线程执行
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [this]() {
			PrepareForReuse();
		});
		return;
	}
    
	// 重新注册组件
	ReregisterAllComponents();
	// 重新初始化
	PreInitializeComponents();
	InitializeComponents();
	PostInitializeComponents();
	// 模拟自然生成流程
	DispatchBeginPlay();
	
	
}


void AChunkBase::PrepareForApply()
{
}

void AChunkBase::ResetForPool()
{
    
	// 确保在游戏线程执行
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [this]() {
			ResetForPool();
		});
		return;
	}
	ClearMeshData();
	ClearMeshRender();
	#if WITH_EDITOR
	SetActorLabel("Tmp");
	#endif
    
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
    
	// 【关键修改】先关闭导航相关性，再更新 NavOctree
	Mesh->SetCanEverAffectNavigation(false); 
	Mesh->bNavigationRelevant = false; // 显式关闭
	if (MeshGhost)
	{
		MeshGhost->SetCanEverAffectNavigation(false);
		MeshGhost->bNavigationRelevant = false;
	}
    
	// 更新导航系统，移除该 Actor（覆盖 Mesh 与 MeshGhost）
	UNavigationSystemV1::GetCurrent(GetWorld())->UpdateActorAndComponentsInNavOctree(*this);
    
	SetActorTickEnabled(false);
	SetActorLocation(FVector::ZeroVector);
    
	// 这里的 UpdateComponentData 可以保留，但上面的 UpdateActorAndComponentsInNavOctree 通常够了
	// FNavigationSystem::UpdateComponentData(*Mesh); 
    
	UnregisterAllComponents();
}
void AChunkBase::ApplyMeshRender()
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [this]() { ApplyMeshRender(); });
		return;
	};

	if (!Mesh || !Material) return;

	// 获取共享的 Chunk MID（Section0 不透明，已设置 Texture2DArray 参数）
	UMaterialInterface* UseMaterial = Material;
	UMaterialInterface* UseTranslucentMaterial = TranslucentMaterial;
	if (GetWorld())
	{
		if (UVoxelBlockWorldSubsystem* BlockSys = GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>())
		{
			if (UMaterialInstanceDynamic* SharedMID = BlockSys->GetSharedChunkMID())
			{
				UseMaterial = SharedMID;
			}
			if (UMaterialInstanceDynamic* SharedTranslucentMID = BlockSys->GetSharedChunkTranslucentMID())
			{
				UseTranslucentMaterial = SharedTranslucentMID;
			}
		}
	}

	// 1. 清理两个组件的 Section
	Mesh->ClearAllMeshSections();
	if (MeshGhost)
	{
		MeshGhost->ClearAllMeshSections();
	}

	// 2. Mesh（Section0）：不透明体素（含切面层顶面 + 区域外墙壁），开启碰撞（BuildGround，挡选取射线+Pawn）
	Mesh->SetMaterial(0, UseMaterial);
	Mesh->CreateMeshSection(
		0,
		MeshData.Vertices,
		MeshData.Triangles,
		MeshData.Normals,
		MeshData.UV0,
		MeshData.Colors,
		TArray<FProcMeshTangent>(),
		true  // bCreateCollision：切面层顶面可射线、区域外墙壁可碰撞
	);

	// 3. MeshGhost：半透明体素（切面超高部分）渲染 + 碰撞（BuildGroundGhost：挡 Pawn，忽略选取射线/相机）
	//    -> NPC 站真实地形不坠落；玩家选取射线穿透命中 Mesh 切面层顶面；相机可伸入剖切区
	if (MeshGhost && TranslucentMeshData.Vertices.Num() > 0 && UseTranslucentMaterial)
	{
		MeshGhost->SetMaterial(0, UseTranslucentMaterial);
		MeshGhost->CreateMeshSection(
			0,
			TranslucentMeshData.Vertices,
			TranslucentMeshData.Triangles,
			TranslucentMeshData.Normals,
			TranslucentMeshData.UV0,
			TranslucentMeshData.Colors,
			TArray<FProcMeshTangent>(),
			true  // bCreateCollision=true：Ghost 保留碰撞（挡 Pawn/物理），通道差异由 BuildGroundGhost profile 决定
		);
	}

	// 4. 激活 Actor 基本状态
	SetActorEnableCollision(true);
	SetActorHiddenInGame(false);
	Mesh->SetHiddenInGame(false);
	Mesh->SetVisibility(true);
	if (MeshGhost)
	{
		MeshGhost->SetHiddenInGame(false);
		MeshGhost->SetVisibility(true);
	}

	// 5. 强制更新包围盒，确保 Bounds 不为零
	Mesh->UpdateBounds();
	if (MeshGhost && TranslucentMeshData.Vertices.Num() > 0)
	{
		MeshGhost->UpdateBounds();
	}

	// 6. 导航：Mesh（不透明地面+切片平面）与 MeshGhost（Ghost 真实地形）各自按几何是否非空开启
	//    Ghost nav-relevant -> NPC 在被透视的上层仍保留导航地面（不受剖切影响）
	if (MeshData.Vertices.Num() > 0)
	{
		Mesh->bNavigationRelevant = true;
		Mesh->SetCanEverAffectNavigation(true);
		FNavigationSystem::UpdateComponentData(*Mesh);
	}
	else
	{
		Mesh->bNavigationRelevant = false;
		Mesh->SetCanEverAffectNavigation(false);
	}

	if (MeshGhost)
	{
		if (TranslucentMeshData.Vertices.Num() > 0)
		{
			MeshGhost->bNavigationRelevant = true;
			MeshGhost->SetCanEverAffectNavigation(true);
			FNavigationSystem::UpdateComponentData(*MeshGhost);
		}
		else
		{
			MeshGhost->bNavigationRelevant = false;
			MeshGhost->SetCanEverAffectNavigation(false);
		}
	}
	UE_LOG(LogTemp, Verbose, TEXT("Chunk prepared for reuse: %s"), *GetName());
}

void AChunkBase::ModifyChunkMesh(FChunkMeshData Data)
{
	ClearMeshData();
	MeshData = Data;
}

void AChunkBase::ModifyChunkMesh(const FChunkMeshDataPair& DataPair)
{
	ClearMeshData();
	MeshData = DataPair.Opaque;
	TranslucentMeshData = DataPair.Translucent;
}
