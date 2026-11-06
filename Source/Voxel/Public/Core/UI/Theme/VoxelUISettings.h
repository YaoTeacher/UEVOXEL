// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VoxelUISettings.generated.h"

class UVoxelUITheme;

/**
 * Voxel UI 项目设置
 * 配置全局 UI 主题资产（Project Settings → Game → Voxel UI Settings）
 */
UCLASS(Config = Game, defaultconfig)
class VOXEL_API UVoxelUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoxelUISettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	virtual FName GetSectionName() const override { return TEXT("Voxel UI Settings"); }

	/** 全局 UI 主题资产：统一控件经 UVoxelUITheme::Get() 读取 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSoftObjectPtr<UVoxelUITheme> DefaultTheme;
};
