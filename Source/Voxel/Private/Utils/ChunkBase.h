// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Voxel/Utils/ChunkMeshData.h"
#include "VoxelGameState.h"
#include "Item/Interfaces/InteractionItemInterface.h"
#include "ChunkBase.generated.h"

struct FWorldConfig;
class UProceduralMeshComponent;
class UChunkUtilsFunction;
class UVoxelBlockWorldSubsystem;
struct FChunkMeshDataPair;

DECLARE_DELEGATE(FOnMeshClearedDelegate);
UCLASS(Blueprintable)
class AChunkBase : public AActor,public IInteractionItemInterface
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	AChunkBase();
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Chunk")
	TObjectPtr<UMaterialInterface> Material;

	/** 半透明 Section1 材质（M_ChunkTranslucent），留空则由 Subsystem 的 SharedChunkTranslucentMID 兜底 */
	UPROPERTY(EditDefaultsOnly, Category = "Chunk")
	TObjectPtr<UMaterialInterface> TranslucentMaterial;
	
	UPROPERTY(EditDefaultsOnly, Category = "Chunk")
	FIntVector ChunkIndex;

	UPROPERTY(EditDefaultsOnly, Category = "Chunk")
	int BlockLength;
	UPROPERTY(EditInstanceOnly, Category = "Chunk")
	TObjectPtr<UProceduralMeshComponent> Mesh;
	/** 半透明 Ghost 网格组件（Section1）：渲染半透明体素 + 碰撞（挡 Pawn/Visibility，忽略选取射线与相机） */
	UPROPERTY(VisibleAnywhere, Category = "Chunk")
	TObjectPtr<UProceduralMeshComponent> MeshGhost;
	// 对象池接口
	UFUNCTION(BlueprintCallable, Category = "Object Pool")
	virtual void PrepareForReuse();
	void PrepareForApply();

	UFUNCTION(BlueprintCallable, Category = "Object Pool")
	virtual void ResetForPool();

	// 区块功能

	UFUNCTION(BlueprintCallable, Category = "Chunk")
	void ModifyChunkMesh(FChunkMeshData Data);
	// 双 Section 版本（切面三域分流：Opaque + Translucent）
	void ModifyChunkMesh(const FChunkMeshDataPair& DataPair);
	// 在游戏线程调用
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	void ApplyMeshRender();
public:
	FChunkMeshData MeshData;          // Section0 不透明
	FChunkMeshData TranslucentMeshData; // Section1 半透明（切面超高体素）
	FIntVector ChunkSize;
protected:


	virtual void Setup() PURE_VIRTUAL(AChunkBase::Setup);
	//virtual void Generate2DHeightMap(const FVector Position) PURE_VIRTUAL(AChunkBase::Generate2DHeightMap);
	//virtual void Generate3DHeightMap(const FVector Position) PURE_VIRTUAL(AChunkBase::Generate3DHeightMap);
	virtual void GenerateMesh() PURE_VIRTUAL(AChunkBase::GenerateMesh);

	virtual void ModifyVoxelData(const FIntVector Position, const int TypeID) PURE_VIRTUAL(AChunkBase::RemoveVoxelData);
	// 修改为线程安全的ClearMeshData

	// 委托用于回调
	FOnMeshClearedDelegate OnMeshCleared;
	void ClearMeshData();
    
	// 游戏线程中清除实际网格
	UFUNCTION()
	void ClearMeshRender();
	int VertexCount = 0;
	// void GenerateHeightMap();
};
