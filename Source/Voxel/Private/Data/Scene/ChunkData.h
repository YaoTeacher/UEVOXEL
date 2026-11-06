// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Engine/DataTable.h"
#include "Voxel/Utils/ChunkMeshData.h"
#include "ChunkData.generated.h"

UENUM(BlueprintType)
enum class EChunkState : uint8
{
	WToLoad				UMETA(DisplayName = "Loading"),
	WToGenerate			UMETA(DisplayName = "Generating"),
	WToCalculate		UMETA(DisplayName = "Calculating"),
	WToRender			UMETA(DisplayName = "Rendering"),
	WToUnload		UMETA(DisplayName = "Unloading"),
	Active			UMETA(DisplayName = "Active"),
	Unloaded		UMETA(DisplayName = "Unloaded"),
	Any				UMETA(DisplayName = "Any")
};
UENUM(BlueprintType)
enum class EChunkLoadPriority : uint8
{
	Critical    UMETA(DisplayName = "Critical"),
	High        UMETA(DisplayName = "High"),
	Medium      UMETA(DisplayName = "Medium"),
	Low         UMETA(DisplayName = "Low")
};
/**
 * 
 */
USTRUCT(BlueprintType)
struct FChunkData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Chunk")
	FString ChunkActorName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Chunk")
	int ChunkId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Chunk")
	FIntVector ChunkIndex;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Chunk")
	int ParentScenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	EChunkState State = EChunkState::Unloaded;
	
};

USTRUCT(BlueprintType)
struct FChunkIstance : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WorldGenerateData/Chunk")
	FIntVector ChunkIndex;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	EChunkState State = EChunkState::Unloaded;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	FDateTime LastAccessTime;
    
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	// FBox DirtyRegion;
    
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	int32 UpdateCount = 0;
    
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	float LastUpdateDuration = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	FChunkMeshData MeshData;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	FIntVector ChunkSize;
	
};