// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/Enum/GameStateEnum.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChunkUtilsFunction.generated.h"

/**
 * 
 */


UCLASS()
class UChunkUtilsFunction : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	public:

	//TAG
	static  bool CompareTagAtLevel(const FGameplayTag& TagA, const FGameplayTag& TagB, int32 Level)
	{
		if (Level < 1) return false;  // 层数从1开始
    
		TArray<FString> PartsA, PartsB;
		TagA.GetTagName().ToString().ParseIntoArray(PartsA, TEXT("."));
		TagB.GetTagName().ToString().ParseIntoArray(PartsB, TEXT("."));
    
		int32 Index = Level - 1;  // 转换为0-based索引
    
		// 确保两个标签都有足够的层数
		if (PartsA.Num() <= Index || PartsB.Num() <= Index)
		{
			return false;
		}
    
		return PartsA[Index] == PartsB[Index];
	}
	
	
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static FIntVector GetBlockIndexByPosition(const FVector Position, const float BlockLength);
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static int GetBlockIdByIndex(const FIntVector BlockIndex, const  FIntVector WorldSize);	
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static FVector GetPositionByBlockIndex(const FIntVector BlockIndex, const float BlockLength);
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static FIntVector GetBlockIndexById(const int BlockId, const  FIntVector WorldSize);
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static FVector2D GetChunkIndexByBlockIndex(FIntVector BlockIndex, FIntVector ChunkDimensions);
	/**
	 * 通过位运算快速获取 Chunk Index (支持负数坐标)
	 * @param BlockIndex 体素的世界坐标 (Global Block Position)
	 * @param ChunkBitShift 注意：这里传的是位移量 (Log2(Size))，而不是 Size 本身！
	 * 例如 ChunkSize=32，这里传 5。
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel Utils")
	static FIntVector GetChunkIndexByShift(const FIntVector BlockIndex, int32 ChunkBitShift);
    
	/** * 辅助函数：计算 ChunkSize 对应的位移量
	 * 用于在初始化时将 32 转换为 5
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel Utils")
	static FIntVector CalculateBitShift(FIntVector ChunkSize);
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static FIntVector Get3DChunkIndexByBlockIndex(FIntVector BlockIndex, FIntVector ChunkSize);
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static int GetChunkIdByBlockIndex(FIntVector BlockIndex, FIntVector ChunkSize, FIntVector WorldDimension, FIntVector ChunkDimension);
	
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static FIntVector GetBlockWorldIndexByBlockChunkIndex(FIntVector BlockChunkIndex, FIntVector ChunkIndex, FIntVector ChunkSize);	

	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static FBox ChangeArea(FIntVector Min, FIntVector Max, FBox WorldSize);
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static FIntVector CheckInputVector(FIntVector Vector, FIntVector WorldSize);
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static FRotator GetSpawnDirection(EActorDirection InputDirection);

	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static FVector GetRotationVolume(EActorDirection InputDirection, FVector InputVolume);

	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static FBox UpdateBox(FBox CurArea, FVector Input);
	
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static FBox CheckBox(FBox CheckBox);

	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static EActorDirection UpdateDir(bool isTurnLeft,EActorDirection CurDir);

	/**
	 * 将旋转角度转换为最近的90度方向（FRotator → EActorDirection）
	 * @param Rotation 输入旋转角度
	 * @return 最近的 EActorDirection (Forward/Right/Backward/Left)
	 */
	UFUNCTION(BlueprintCallable, Category = "Chunk")
	static EActorDirection RotationToDirection(FRotator Rotation);

	// ==================== 坐标转换工具（新增）====================

	/**
	 * 从 World 获取 BlockLength（统一访问点，消除 20.0f 魔数与多数据源问题）
	 * @param World 世界上下文
	 * @param Fallback 获取失败时的回退值（默认 20.0f）
	 * @return 体素边长
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel Utils")
	static float GetBlockLength(const UObject* WorldContext, float Fallback = 20.0f);

	/**
	 * 将世界坐标对齐到体素网格
	 * @param WorldPosition 世界坐标
	 * @param BlockLength 体素边长
	 * @return 对齐后的世界坐标（体素中心）
	 *
	 * @note 例如：BlockLength=100, WorldPosition=(150, 250, 350) → (200, 300, 400)
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel Utils|Coordinate")
	static FVector AlignPositionToVoxelGrid(FVector WorldPosition, float BlockLength);

	/**
	 * 世界坐标增量转换为体素索引增量
	 * @param WorldDelta 世界坐标增量（例如拖拽距离）
	 * @param BlockLength 体素边长
	 * @return 体素索引增量
	 *
	 * @note 用于 Gizmo 拖拽操作，将世界空间的拖拽距离转换为体素数量
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel Utils|Coordinate")
	static FIntVector WorldDeltaToIndexDelta(FVector WorldDelta, float BlockLength);

	/**
	 * 单轴世界坐标增量转换为体素索引增量
	 * @param WorldDelta 世界坐标增量（单轴）
	 * @param BlockLength 体素边长
	 * @return 体素索引增量
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel Utils|Coordinate")
	static int32 WorldDeltaToIndexDelta_SingleAxis(float WorldDelta, float BlockLength);


};
