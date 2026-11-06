// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ChunkMeshData.generated.h"



USTRUCT(BlueprintType)
struct FChunkMeshData
{
	GENERATED_BODY();

public:
	TArray<FVector> Vertices;
	TArray<int> Triangles;
	TArray<FVector> Normals;
	TArray<FColor> Colors;
	TArray<FVector2D> UV0;

	void Clear();

	
};

inline void FChunkMeshData::Clear()
{
	Vertices.Empty();
	Triangles.Empty();
	Normals.Empty();
	Colors.Empty();
	UV0.Empty();
}

// 双 Section 网格输出（Opaque + Translucent），用于切面三域分流
USTRUCT(BlueprintType)
struct FChunkMeshDataPair
{
	GENERATED_BODY();

	UPROPERTY(VisibleAnywhere, Category = "Chunk")
	FChunkMeshData Opaque;

	UPROPERTY(VisibleAnywhere, Category = "Chunk")
	FChunkMeshData Translucent;

	void Clear()
	{
		Opaque.Clear();
		Translucent.Clear();
	}
};