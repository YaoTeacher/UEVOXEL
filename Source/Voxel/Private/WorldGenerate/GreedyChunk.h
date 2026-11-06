// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Utils/ChunkBase.h"
#include "GreedyChunk.generated.h"

class UProceduralMeshComponent;

UCLASS(Blueprintable)
class AGreedyChunk final : public AChunkBase
{
	GENERATED_BODY()
	AGreedyChunk();

	struct FMask
	{
		int32 TypeId;
		int Normal;
		FIntVector3 maskId;

	};
	// 区块功能
	virtual void Setup() override;
	virtual void GenerateMesh() override;

	// 池化接口
	virtual void ResetForPool() override;
	void ResetChunkState();
public:


protected:

private:
	// 注：旧的 CreateQuad/GetTextureIndex/GetBlockType/CheckBlockOpaque 等方法已废弃
	// 网格生成由 UVoxelBlockWorldSubsystem::GenerateMeshData + FChunkGenContext 统一处理
	// ApplyMeshRender 只负责将子系统计算的 MeshData 应用到 ProceduralMeshComponent
};