// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "Core/UI/Theme/VoxelUITheme.h"
#include "VoxelTextBlock.generated.h"

/**
 * 统一文本控件：按文本角色（TextRole）从主题资产应用 字体/字号/颜色/字距
 *
 * 使用方式：在 WBP 中放置后选择 TextRole（PanelTitle/SectionTitle/Body/Hint/DataValue…），
 * 样式由 DA_VoxelUITheme 统一下发；设计器中改角色立即预览。
 * 特例需要手动调样式时，把 bUseThemeStyle 关掉即可。
 */
UCLASS()
class VOXEL_API UVoxelTextBlock : public UTextBlock
{
	GENERATED_BODY()

public:
	/** 文本角色：决定字体族/字号/颜色/字距 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Style")
	EVoxelTextRole TextRole = EVoxelTextRole::Body;

	/** 字号覆盖：<=0 使用角色字号 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Style")
	int32 SizeOverride = 0;

	/** false 时不应用主题样式（保留手工设置，用于特例） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Style")
	bool bUseThemeStyle = true;

	/** 运行时切换角色并立即生效 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Style")
	void SetTextRole(EVoxelTextRole NewRole);

protected:
	virtual void SynchronizeProperties() override;

	/** 从主题应用当前角色样式 */
	void ApplyThemeStyle();
};
