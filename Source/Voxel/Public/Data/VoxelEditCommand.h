// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/Enum/EditCommandEnums.h"
#include "VoxelEditCommand.generated.h"

/**
 * 体素编辑命令 - 双维度组合
 *
 * 设计原则：
 * - 不包含操作类型（由 Voxel.Tool.* GameplayTag 管理）
 * - 只包含放置规则和选择模式
 * - 提供便利构造函数
 * - 支持组合有效性验证
 */
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelEditCommand
{
	GENERATED_BODY()

	// === 核心维度 ===

	/** 放置规则（决定如何覆盖） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Editing")
	EVoxelPlacementMode PlacementMode = EVoxelPlacementMode::Normal;

	/** 选择模式（决定选区如何组合） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Editing")
	ESelectionOpMode SelectionMode = ESelectionOpMode::New;

	// === 便利构造函数 ===

	/**
	 * 构建标准建造命令
	 */
	static FVoxelEditCommand BuildNormal()
	{
		return FVoxelEditCommand{EVoxelPlacementMode::Normal, ESelectionOpMode::New};
	}

	/**
	 * 构建填充建造命令
	 */
	static FVoxelEditCommand BuildComplement()
	{
		return FVoxelEditCommand{EVoxelPlacementMode::Complement, ESelectionOpMode::New};
	}

	/**
	 * 构建涂色替换命令
	 */
	static FVoxelEditCommand PaintReplace()
	{
		return FVoxelEditCommand{EVoxelPlacementMode::Replace, ESelectionOpMode::New};
	}

	/**
	 * 构建新建选择命令
	 */
	static FVoxelEditCommand SelectNew()
	{
		return FVoxelEditCommand{EVoxelPlacementMode::Normal, ESelectionOpMode::New};
	}

	// === 验证 ===

	/**
	 * 验证命令组合的有效性
	 * @return true 如果组合有效
	 */
	bool IsValid() const
	{
		// 所有组合都是有效的
		return true;
	}
};
