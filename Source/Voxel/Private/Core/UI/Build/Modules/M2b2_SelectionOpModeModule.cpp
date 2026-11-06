// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/Modules/M2b2_SelectionOpModeModule.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/CoreTags.h"
#include "Components/CheckBox.h"

bool UM2b2_SelectionOpModeModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	// 在所有选区工具和选区编辑工具下显示
	return ToolTag.MatchesTag(FCoreTags::Tool_Selection) ||
	       ToolTag == FCoreTags::Tool_Edit_Selection;
}

void UM2b2_SelectionOpModeModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC)
	{
		return;
	}

	// 保存 StateComponent 引用
	StateComponent = SC;

	// 获取当前选区运算模式并更新UI
	ESelectionOpMode CurrentMode = SC->GetSelectionMode();
	UpdateModeCheckBoxes(CurrentMode);
}

void UM2b2_SelectionOpModeModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (NewModeCheckBox)
	{
		NewModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM2b2_SelectionOpModeModule::OnNewModeChanged);
	}
	if (AddModeCheckBox)
	{
		AddModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM2b2_SelectionOpModeModule::OnAddModeChanged);
	}
	if (SubtractModeCheckBox)
	{
		SubtractModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM2b2_SelectionOpModeModule::OnSubtractModeChanged);
	}
	if (IntersectModeCheckBox)
	{
		IntersectModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM2b2_SelectionOpModeModule::OnIntersectModeChanged);
	}
}

void UM2b2_SelectionOpModeModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM2b2_SelectionOpModeModule::OnNewModeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetSelectionMode(ESelectionOpMode::New);
	}
}

void UM2b2_SelectionOpModeModule::OnAddModeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetSelectionMode(ESelectionOpMode::Add);
	}
}

void UM2b2_SelectionOpModeModule::OnSubtractModeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetSelectionMode(ESelectionOpMode::Subtract);
	}
}

void UM2b2_SelectionOpModeModule::OnIntersectModeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetSelectionMode(ESelectionOpMode::Intersect);
	}
}

void UM2b2_SelectionOpModeModule::SetSelectionMode(ESelectionOpMode NewMode)
{
	// 交互层：通过 InteractionComp 设置选区运算模式（显示由 UpdateFromToolState 驱动）
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->SetSelectionMode(NewMode);
		}
	}
}

void UM2b2_SelectionOpModeModule::UpdateModeCheckBoxes(ESelectionOpMode CurrentMode)
{
	// 更新所有复选框的选中状态
	if (NewModeCheckBox)
	{
		NewModeCheckBox->SetCheckedState(CurrentMode == ESelectionOpMode::New ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (AddModeCheckBox)
	{
		AddModeCheckBox->SetCheckedState(CurrentMode == ESelectionOpMode::Add ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (SubtractModeCheckBox)
	{
		SubtractModeCheckBox->SetCheckedState(CurrentMode == ESelectionOpMode::Subtract ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (IntersectModeCheckBox)
	{
		IntersectModeCheckBox->SetCheckedState(CurrentMode == ESelectionOpMode::Intersect ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}
}
