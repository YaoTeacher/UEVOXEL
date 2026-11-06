// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Editor/VoxelHistoryData.h" // FVoxelEdit
#include "VoxelClipboardData.generated.h"

/**
 * 体素剪切板数据
 * 存储从选区复制的体素数据，支持粘贴到任意位置
 *
 * 设计参考：
 * - PS 的剪切板：存储像素数据 + 原始尺寸
 * - Axiom 的 Clipboard：可旋转/翻转/保存为蓝图
 *
 * 数据存储：
 * - Voxels 数组中 BlockIndex 为相对位置（相对于 SourceMin）
 * - OldType=Air, NewType=实际方块类型
 * - 粘贴时通过 TargetPosition + BlockIndex 计算世界位置
 */
USTRUCT(BlueprintType)
struct FVoxelClipboardData
{
    GENERATED_BODY()

    /** 体素数据（位置相对于 SourceMin，OldType=Air, NewType=实际类型） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Clipboard")
    TArray<FVoxelEdit> Voxels;

    /** 选区包围盒大小（X/Y/Z 维度） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Clipboard")
    FIntVector Size = FIntVector::ZeroValue;

    /** 复制时的选区 Min（世界索引，用于计算相对位置） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Clipboard")
    FIntVector SourceMin = FIntVector::ZeroValue;

    /** 是否有效 */
    bool IsValid() const { return Voxels.Num() > 0; }

    /** 清空 */
    void Clear()
    {
        Voxels.Empty();
        Size = FIntVector::ZeroValue;
        SourceMin = FIntVector::ZeroValue;
    }
};
