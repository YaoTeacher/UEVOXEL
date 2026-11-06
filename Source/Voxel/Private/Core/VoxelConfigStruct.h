// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Utils/ChunkBase.h"
#include "VoxelConfigStruct.generated.h"

USTRUCT(BlueprintType)
struct FWorldConfig
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World", meta = (ClampMin = 2, ClampMax = 128))
	int ChunkLength = 16;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "World", meta = (ClampMin = 2, ClampMax = 256))
	int ChunkHeight = 128;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Chunk")
	int BlockLength = 20;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
	int32 LoadDistance = 3;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
	int32 MaxLoadsPerFrame = 8;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
	int32 MaxUnloadsPerFrame =8;
    
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
	// float LOD0Distance = 2000.0f;
 //    
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
	// float LOD1Distance = 4000.0f;
 //    
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
	// float LOD2Distance = 6000.0f;
 //    
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
	// float LODUpdateInterval = 0.5f;
 //    
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
	// int32 MaxLODUpdatesPerFrame = 2;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
	int32 MaxGeneratesPerFrame = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	TSubclassOf<AChunkBase> ChunkActorClass;

	int InitialPoolSize()
	{
		return (LoadDistance*2)*(LoadDistance*2)*3;
	}
	
	FIntVector GetWorldDimensions(FIntVector WorldDimensionsInChunk) const
	{
		return FIntVector(WorldDimensionsInChunk.X*ChunkLength, WorldDimensionsInChunk.Y*ChunkLength, WorldDimensionsInChunk.Z*ChunkHeight);
	}
	FIntVector GetChunkSize()
	{
		return FIntVector(ChunkLength,ChunkLength,ChunkHeight);
	}
};

