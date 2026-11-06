// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "M8_AreaInfoModule.generated.h"

class UVoxelAreaItemWidget;
class UTextBlock;
class UVerticalBox;

/**
 * M8: 区域信息显示模块
 * 职责：显示区域基本信息和子区域列表
 *
 * 显示条件: Tool.AreaEdit.*
 *
 * UI控件:
 * - AreaNameText: 区域名称显示
 * - AreaTypeText: 区域类型显示
 * - AreaOwnerText: 区域所有人显示
 * - AreaStatusText: 区域状态显示
 * - SubAreaListContainer: 子区域列表容器
 *
 * 数据来源:
 * - 从 UVoxelPlayerStateCacheComponent 获取当前区域信息
 * - 从 AVoxelAreaBaseInstance::SubAreasIndex 获取子区域列表
 * - 从 UVoxelAreaWorldSubsystem 获取顶级区域列表
 */
UCLASS()
class VOXEL_API UM8_AreaInfoModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	// === 模块接口 ===

	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;

protected:
	virtual void NativeConstruct() override;

	// === UI 控件绑定 ===

	/** 区域名称显示 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* AreaNameText;

	/** 区域类型显示 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* AreaTypeText;

	/** 区域所有人显示 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* AreaOwnerText;

	/** 区域状态显示 */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* AreaStatusText;

	/** 子区域列表容器 */
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* SubAreaListContainer;

	/** 子列表项的样式类 (WBP_SubAreaItem) - 从 AreaModifyWidget 迁移 */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Class")
	TSubclassOf<UVoxelAreaItemWidget> SubAreaItemClass;

private:
	/** 更新子区域列表（从 AreaModifyWidget 迁移的完整逻辑） */
	void UpdateSubAreaList();
};
