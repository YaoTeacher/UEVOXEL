// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/Modules/M5_SelectionInfoModule.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/CoreTags.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "Core/UI/Base/VoxelDialogBase.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "UObject/ConstructorHelpers.h"

UM5_SelectionInfoModule::UM5_SelectionInfoModule()
{
	// C++ 构造函数设置默认 Dialog 类，确保无论 M5 以何种方式实例化都有弹窗可用
	static ConstructorHelpers::FClassFinder<UVoxelDialogBase> TemplateSaveDialogFinder(
		TEXT("/Game/Resource/UI/BuildModeUI/BuildDetailMode/WBP_VoxelTemplateSaveDialog.WBP_VoxelTemplateSaveDialog_C"));
	if (TemplateSaveDialogFinder.Succeeded())
	{
		TemplateSaveDialogClass = TemplateSaveDialogFinder.Class;
	}
}

bool UM5_SelectionInfoModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	// 在所有选区工具下显示
	return ToolTag.MatchesTag(FCoreTags::Tool_Selection);
}

void UM5_SelectionInfoModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC)
	{
		return;
	}

	// 保存 StateComponent 引用
	StateComponent = SC;

	// 绑定选区变更委托（首次绑定时）
	if (SC && !bSelectionDelegateBound)
	{
		SC->OnSelectionChanged.AddDynamic(this, &UM5_SelectionInfoModule::OnSelectionChangedHandler);
		bSelectionDelegateBound = true;
	}

	// 绑定构建模式参数更新委托（首次绑定时，用于拖拽/几何变化时实时刷新选区信息）
	if (!bBuildModeDelegateBound)
	{
		SC->BindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM5_SelectionInfoModule, HandleBuildModeParamsUpdated));
		bBuildModeDelegateBound = true;
	}

	// 获取当前临时几何信息
	FVoxelGeometryDesc Geometry = SC->GetTmpGeometry();
	FAreaCoverageFragment Box = Geometry.Box;

	// 检查包围盒是否有效
	bool bIsValid = Box.isValid();

	// 更新选区状态
	if (SelectionStatusText)
	{
		FString Status = bIsValid ? TEXT("已选择") : TEXT("未选择");
		SelectionStatusText->SetText(FText::FromString(Status));
	}

	if (!bIsValid)
	{
		// 如果无效，清空所有显示
		if (VoxelCountText) VoxelCountText->SetText(FText::FromString(TEXT("-")));
		if (SizeText) SizeText->SetText(FText::FromString(TEXT("-")));
		if (CenterText) CenterText->SetText(FText::FromString(TEXT("-")));
		if (MinCoordText) MinCoordText->SetText(FText::FromString(TEXT("-")));
		if (MaxCoordText) MaxCoordText->SetText(FText::FromString(TEXT("-")));
		return;
	}

	// 计算选区信息
	FIntVector MinCoord = Box.MinIndex;
	FIntVector MaxCoord = Box.MaxIndex;
	FIntVector Size = MaxCoord - MinCoord;
	FIntVector Center = (MinCoord + MaxCoord) / 2;

	// 体素数量：优先使用 CurrentSelection 的实际数量
	int64 VoxelCount;
	if (SC->HasSelection())
	{
		VoxelCount = SC->GetCurrentSelection().Num();
	}
	else
	{
		VoxelCount = static_cast<int64>(Size.X) * Size.Y * Size.Z;
	}

	// 更新显示
	if (VoxelCountText)
	{
		VoxelCountText->SetText(FText::AsNumber(VoxelCount));
	}

	if (SizeText)
	{
		FString SizeStr = FString::Printf(TEXT("%d x %d x %d"), Size.X, Size.Y, Size.Z);
		SizeText->SetText(FText::FromString(SizeStr));
	}

	if (CenterText)
	{
		FString CenterStr = FString::Printf(TEXT("(%d, %d, %d)"), Center.X, Center.Y, Center.Z);
		CenterText->SetText(FText::FromString(CenterStr));
	}

	if (MinCoordText)
	{
		FString MinStr = FString::Printf(TEXT("(%d, %d, %d)"), MinCoord.X, MinCoord.Y, MinCoord.Z);
		MinCoordText->SetText(FText::FromString(MinStr));
	}

	if (MaxCoordText)
	{
		FString MaxStr = FString::Printf(TEXT("(%d, %d, %d)"), MaxCoord.X, MaxCoord.Y, MaxCoord.Z);
		MaxCoordText->SetText(FText::FromString(MaxStr));
	}
}

void UM5_SelectionInfoModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ClearSelectionButton)
		ClearSelectionButton->OnClicked.AddDynamic(this, &UM5_SelectionInfoModule::OnClearSelectionClicked);
	if (SaveAsTemplateButton)
		SaveAsTemplateButton->OnClicked.AddDynamic(this, &UM5_SelectionInfoModule::OnSaveAsTemplateClicked);
}

void UM5_SelectionInfoModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM5_SelectionInfoModule::OnClearSelectionClicked()
{
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (UVoxelInteractionComponent* InteractionComp = PC->FindComponentByClass<UVoxelInteractionComponent>())
		{
			InteractionComp->CancelCurrentEdit();
		}
	}
}

void UM5_SelectionInfoModule::OnSaveAsTemplateClicked()
{
	if (!StateComponent || !StateComponent->HasSelection())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Template] No selection to save"));
		return;
	}

	// 通过 PrimaryGameLayout.PushModal 打开模板保存弹窗
	if (!TemplateSaveDialogClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[M5] TemplateSaveDialogClass not configured"));
		return;
	}

	UVoxelPrimaryGameLayout* Layout = UVoxelPrimaryGameLayout::GetFromPC(GetOwningPlayer());
	if (!Layout)
	{
		UE_LOG(LogTemp, Warning, TEXT("[M5] PrimaryGameLayout not available"));
		return;
	}

	Layout->PushModal(TemplateSaveDialogClass, StateComponent);
}

void UM5_SelectionInfoModule::OnSelectionChangedHandler()
{
	// 选区变更时刷新面板
	if (StateComponent)
	{
		UpdateFromToolState(StateComponent);
	}
}

void UM5_SelectionInfoModule::NativeDestruct()
{
	if (StateComponent)
	{
		if (bSelectionDelegateBound)
		{
			StateComponent->OnSelectionChanged.RemoveDynamic(this, &UM5_SelectionInfoModule::OnSelectionChangedHandler);
			bSelectionDelegateBound = false;
		}
		if (bBuildModeDelegateBound)
		{
			StateComponent->UnbindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM5_SelectionInfoModule, HandleBuildModeParamsUpdated));
			bBuildModeDelegateBound = false;
		}
	}
	Super::NativeDestruct();
}

void UM5_SelectionInfoModule::HandleBuildModeParamsUpdated()
{
	// 构建模式参数变化时（如拖拽改变包围盒）实时刷新选区信息
	if (StateComponent)
	{
		UpdateFromToolState(StateComponent);
	}
}
