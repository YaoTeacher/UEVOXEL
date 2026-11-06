// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "VoxelPlaceholderTools.generated.h"

/**
 * 采样工具（占位，尚未实现）
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelSampleTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UVoxelSampleTool() { bNeedsDetailPanel = true; }
	virtual FText GetToolDisplayName() const override { return FText::FromString(TEXT("采样工具（敬请期待）")); }
	virtual EVoxelToolCategory GetToolCategory() const override { return EVoxelToolCategory::Misc; }
	virtual FName GetToolID() const override { return FName("Sample"); }
};

/**
 * 魔棒选区工具（占位，尚未实现）
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelMagicWandTool : public UVoxelToolBase
{
	GENERATED_BODY()

public:
	UVoxelMagicWandTool() { bNeedsDetailPanel = true; }
	virtual FText GetToolDisplayName() const override { return FText::FromString(TEXT("魔棒选区（敬请期待）")); }
	virtual EVoxelToolCategory GetToolCategory() const override { return EVoxelToolCategory::Selection; }
	virtual FName GetToolID() const override { return FName("MagicWand"); }
};
