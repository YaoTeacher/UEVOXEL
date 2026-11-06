// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/Modules/M3_GeometryShapeModule.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/CoreTags.h"
#include "Components/CheckBox.h"

bool UM3_GeometryShapeModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	// 在简单几何选区工具或简单几何编辑工具下显示
	return ToolTag == FCoreTags::Tool_Selection_SimpleGeometry ||
	       ToolTag == FCoreTags::Tool_Edit_SimpleGeometry;
}

void UM3_GeometryShapeModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC)
	{
		return;
	}

	StateComponent = SC;

	if (!bDelegatesBound)
	{
		SC->BindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM3_GeometryShapeModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = true;
	}

	EBatchShapeType CurrentShape = SC->GetCurrentBatchShape();
	UpdateShapeCheckBoxes(CurrentShape);
}

void UM3_GeometryShapeModule::NativeDestruct()
{
	if (StateComponent && bDelegatesBound)
	{
		StateComponent->UnbindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM3_GeometryShapeModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = false;
	}
	Super::NativeDestruct();
}

void UM3_GeometryShapeModule::HandleBuildModeParamsUpdated()
{
	if (StateComponent)
	{
		UpdateShapeCheckBoxes(StateComponent->GetCurrentBatchShape());
	}
}

void UM3_GeometryShapeModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (BoxShapeCheckBox)
		BoxShapeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM3_GeometryShapeModule::OnBoxShapeChanged);
	if (SphereShapeCheckBox)
		SphereShapeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM3_GeometryShapeModule::OnSphereShapeChanged);
	if (CylinderShapeCheckBox)
		CylinderShapeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM3_GeometryShapeModule::OnCylinderShapeChanged);
	if (PyramidShapeCheckBox)
		PyramidShapeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM3_GeometryShapeModule::OnPyramidShapeChanged);
}

void UM3_GeometryShapeModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM3_GeometryShapeModule::OnBoxShapeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetGeometryShape(EBatchShapeType::Box);
	}
}

void UM3_GeometryShapeModule::OnSphereShapeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetGeometryShape(EBatchShapeType::Sphere);
	}
}

void UM3_GeometryShapeModule::OnCylinderShapeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetGeometryShape(EBatchShapeType::Cylinder);
	}
}

void UM3_GeometryShapeModule::OnPyramidShapeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetGeometryShape(EBatchShapeType::Pyramid);
	}
}

void UM3_GeometryShapeModule::SetGeometryShape(EBatchShapeType NewShape)
{
	// 交互层：通过 InteractionComp 设置几何形状（显示由 UpdateFromToolState 驱动）
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->SetBatchShape(NewShape);
		}
	}
}

void UM3_GeometryShapeModule::UpdateShapeCheckBoxes(EBatchShapeType CurrentShape)
{
	// 更新所有复选框的选中状态
	if (BoxShapeCheckBox)
	{
		BoxShapeCheckBox->SetCheckedState(CurrentShape == EBatchShapeType::Box ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (SphereShapeCheckBox)
	{
		SphereShapeCheckBox->SetCheckedState(CurrentShape == EBatchShapeType::Sphere ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (CylinderShapeCheckBox)
	{
		CylinderShapeCheckBox->SetCheckedState(CurrentShape == EBatchShapeType::Cylinder ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (PyramidShapeCheckBox)
	{
		PyramidShapeCheckBox->SetCheckedState(CurrentShape == EBatchShapeType::Pyramid ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}
}
