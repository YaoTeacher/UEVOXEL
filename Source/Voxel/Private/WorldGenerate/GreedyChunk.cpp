// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldGenerate/GreedyChunk.h"
#include "ProceduralMeshComponent.h"

AGreedyChunk::AGreedyChunk()
{
	Mesh->SetCollisionProfileName(TEXT("BuildGround"));
}

void AGreedyChunk::Setup()
{
}

void AGreedyChunk::ResetForPool()
{
	Super::ResetForPool();
	ResetChunkState();
}

void AGreedyChunk::ResetChunkState()
{
	ChunkSize = FIntVector::ZeroValue;
}

// 网格生成由 UVoxelBlockWorldSubsystem::GenerateMeshData + FChunkGenContext 统一处理
// ChunkActor 只负责接收 MeshData (ModifyChunkMesh) 并渲染 (ApplyMeshRender)
void AGreedyChunk::GenerateMesh()
{
	// 废弃：实际网格生成由 BlockWorldSubsystem 处理
}
