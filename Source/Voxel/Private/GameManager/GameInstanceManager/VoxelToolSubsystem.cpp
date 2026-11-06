// Fill out your copyright notice in the Description page of Project Settings.

#include "GameManager/GameInstanceManager/VoxelToolSubsystem.h"

#include "Core/CoreTags.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "Voxel/Editor/Tools/VoxelSingleVoxelEditTool.h"
#include "Voxel/Editor/Tools/VoxelSimpleGeometryEditTool.h"
#include "Voxel/Editor/Tools/VoxelSelectionEditTool.h"
#include "Voxel/Editor/Tools/VoxelFurnitureEditTool.h"
#include "Voxel/Editor/Tools/VoxelTemplateEditTool.h"
#include "Voxel/Editor/Tools/VoxelSelectionShapeTool.h"
#include "Voxel/Editor/Tools/VoxelSelectionBrushTool.h"
#include "Voxel/Editor/Tools/VoxelPlaceholderTools.h"
#include "Voxel/Editor/Tools/VoxelAreaEditTool.h"
#include "Voxel/Editor/Tools/VoxelCutoutEditTool.h"

void UVoxelToolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisterTools();
	UE_LOG(LogTemp, Log, TEXT("[ToolSubsystem] Initialized with %d tools"), ToolRegistry.Num());
}

void UVoxelToolSubsystem::Deinitialize()
{
	ToolRegistry.Empty();
	Super::Deinitialize();
}

void UVoxelToolSubsystem::RegisterTools()
{
	ToolRegistry.Empty();

	// 编辑工具
	ToolRegistry.Add(FCoreTags::Tool_Edit_SingleVoxel, NewObject<UVoxelSingleVoxelEditTool>(this));
	ToolRegistry.Add(FCoreTags::Tool_Edit_SimpleGeometry, NewObject<UVoxelSimpleGeometryEditTool>(this));
	ToolRegistry.Add(FCoreTags::Tool_Edit_Selection, NewObject<UVoxelSelectionEditTool>(this));
	ToolRegistry.Add(FCoreTags::Tool_Edit_Furniture, NewObject<UVoxelFurnitureEditTool>(this));
	ToolRegistry.Add(FCoreTags::Tool_Edit_Template, NewObject<UVoxelTemplateEditTool>(this));

	// 选区工具
	ToolRegistry.Add(FCoreTags::Tool_Selection_SimpleGeometry, NewObject<UVoxelSelectionShapeTool>(this));
	ToolRegistry.Add(FCoreTags::Tool_Selection_Brush, NewObject<UVoxelSelectionBrushTool>(this));

	// 占位工具（未实现，显示"敬请期待"）
	ToolRegistry.Add(FCoreTags::Tool_Sample, NewObject<UVoxelSampleTool>(this));
	ToolRegistry.Add(FCoreTags::Tool_Selection_Magic, NewObject<UVoxelMagicWandTool>(this));

	// 区域编辑工具
	ToolRegistry.Add(FCoreTags::Tool_AreaEdit, NewObject<UVoxelAreaEditTool>(this));

	// 透明区域框工具
	ToolRegistry.Add(FCoreTags::Tool_Cutout, NewObject<UVoxelCutoutEditTool>(this));
}

UVoxelToolBase* UVoxelToolSubsystem::FindTool(const FGameplayTag& ToolTag) const
{
	if (const TObjectPtr<UVoxelToolBase>* Found = ToolRegistry.Find(ToolTag))
	{
		return *Found;
	}
	return nullptr;
}
