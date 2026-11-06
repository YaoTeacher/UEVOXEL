// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/Modules/M2b1_PlacementModeModule.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/CoreTags.h"
#include "Components/CheckBox.h"

bool UM2b1_PlacementModeModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	// 普通编辑工具显示放置模式；模板工具有独立的 M15 放置/替换面板，不显示本模块
	return ToolTag.MatchesTag(FCoreTags::Tool_Edit) &&
	       ToolTag != FCoreTags::Tool_Edit_Template;
}

void UM2b1_PlacementModeModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC)
	{
		return;
	}

	StateComponent = SC;

	// 一次性订阅广播（仿 M3 模式）
	if (!bDelegatesBound)
	{
		SC->BindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM2b1_PlacementModeModule, HandlePlacementModeChanged));
		bDelegatesBound = true;
	}

	// 获取当前放置模式并更新UI
	EVoxelPlacementMode CurrentMode = SC->GetPlacementMode();
	UpdateModeCheckBoxes(CurrentMode);
}

void UM2b1_PlacementModeModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (OverwriteModeCheckBox)
		OverwriteModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM2b1_PlacementModeModule::OnOverwriteModeChanged);
	if (FillModeCheckBox)
		FillModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM2b1_PlacementModeModule::OnFillModeChanged);
	if (ReplaceModeCheckBox)
		ReplaceModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM2b1_PlacementModeModule::OnReplaceModeChanged);
	if (SpecifiedReplaceModeCheckBox)
		SpecifiedReplaceModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM2b1_PlacementModeModule::OnSpecifiedReplaceModeChanged);
}

void UM2b1_PlacementModeModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM2b1_PlacementModeModule::NativeDestruct()
{
	if (StateComponent && bDelegatesBound)
	{
		StateComponent->UnbindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM2b1_PlacementModeModule, HandlePlacementModeChanged));
		bDelegatesBound = false;
	}
	Super::NativeDestruct();
}

void UM2b1_PlacementModeModule::HandlePlacementModeChanged()
{
	if (StateComponent)
	{
		UpdateModeCheckBoxes(StateComponent->GetPlacementMode());
	}
}

void UM2b1_PlacementModeModule::OnOverwriteModeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetPlacementMode(EVoxelPlacementMode::Normal);
	}
}

void UM2b1_PlacementModeModule::OnFillModeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetPlacementMode(EVoxelPlacementMode::Complement);
	}
}

void UM2b1_PlacementModeModule::OnReplaceModeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetPlacementMode(EVoxelPlacementMode::Replace);
	}
}

void UM2b1_PlacementModeModule::OnSpecifiedReplaceModeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetPlacementMode(EVoxelPlacementMode::ReplaceMask);
	}
}

void UM2b1_PlacementModeModule::SetPlacementMode(EVoxelPlacementMode NewMode)
{
	// 交互层：通过 InteractionComp 设置放置模式（显示由 UpdateFromToolState 驱动）
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->SetPlacementMode(NewMode);
		}
	}
}

void UM2b1_PlacementModeModule::UpdateModeCheckBoxes(EVoxelPlacementMode CurrentMode)
{
	// 更新所有复选框的选中状态
	if (OverwriteModeCheckBox)
	{
		OverwriteModeCheckBox->SetCheckedState(CurrentMode == EVoxelPlacementMode::Normal ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (FillModeCheckBox)
	{
		FillModeCheckBox->SetCheckedState(CurrentMode == EVoxelPlacementMode::Complement ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (ReplaceModeCheckBox)
	{
		ReplaceModeCheckBox->SetCheckedState(CurrentMode == EVoxelPlacementMode::Replace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (SpecifiedReplaceModeCheckBox)
	{
		SpecifiedReplaceModeCheckBox->SetCheckedState(CurrentMode == EVoxelPlacementMode::ReplaceMask ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}
}
