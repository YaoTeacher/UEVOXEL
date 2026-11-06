// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/Modules/M7_BrushParamsModule.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/CoreTags.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"

bool UM7_BrushParamsModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	// 在笔刷选区工具与单体素编辑工具（复用笔刷参数）下显示
	return ToolTag == FCoreTags::Tool_Selection_Brush
		|| ToolTag == FCoreTags::Tool_Edit_SingleVoxel;
}

void UM7_BrushParamsModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC)
	{
		return;
	}

	// 保存 StateComponent 引用
	StateComponent = SC;

	// 绑定构建模式参数更新委托（首次绑定时，用于笔刷参数变化时实时刷新）
	if (!bDelegatesBound)
	{
		SC->BindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM7_BrushParamsModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = true;
	}

	// 笔刷形状：从 StateComponent 读取并更新复选框
	EBatchShapeType CurrentShape = SC->GetCurrentBatchShape();
	UpdateBrushShapeCheckBoxes(CurrentShape);

	// 笔刷维度：从 StateComponent 读取并更新复选框
	EBrushDimension CurrentDimension = SC->GetBrushDimension();
	UpdateBrushDimensionCheckBoxes(CurrentDimension);

	// 笔刷大小：从 StateComponent 读取（权威源），同步本地缓存并更新滑块显示
	CurrentBrushSize = static_cast<float>(SC->GetBrushSize());
	if (BrushSizeSlider)
	{
		BrushSizeSlider->SetValue(CurrentBrushSize);
	}
	if (BrushSizeText)
	{
		BrushSizeText->SetText(FText::AsNumber(FMath::RoundToInt(CurrentBrushSize)));
	}
}

void UM7_BrushParamsModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CircleBrushCheckBox)
		CircleBrushCheckBox->OnCheckStateChanged.AddDynamic(this, &UM7_BrushParamsModule::OnCircleBrushChanged);
	if (SquareBrushCheckBox)
		SquareBrushCheckBox->OnCheckStateChanged.AddDynamic(this, &UM7_BrushParamsModule::OnSquareBrushChanged);
	if (FlatBrushCheckBox)
		FlatBrushCheckBox->OnCheckStateChanged.AddDynamic(this, &UM7_BrushParamsModule::OnFlatBrushChanged);
	if (VolumetricBrushCheckBox)
		VolumetricBrushCheckBox->OnCheckStateChanged.AddDynamic(this, &UM7_BrushParamsModule::OnVolumetricBrushChanged);
	if (BrushSizeSlider)
		BrushSizeSlider->OnValueChanged.AddDynamic(this, &UM7_BrushParamsModule::OnBrushSizeChanged);
}

void UM7_BrushParamsModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM7_BrushParamsModule::OnCircleBrushChanged(bool bChecked)
{
	if (bChecked)
	{
		SetBrushShape(EBatchShapeType::Sphere); // 使用 Sphere 表示圆形笔刷
	}
}

void UM7_BrushParamsModule::OnSquareBrushChanged(bool bChecked)
{
	if (bChecked)
	{
		SetBrushShape(EBatchShapeType::Box); // 使用 Box 表示方形笔刷
	}
}

void UM7_BrushParamsModule::OnFlatBrushChanged(bool bChecked)
{
	if (bChecked)
	{
		SetBrushDimension(EBrushDimension::Flat); // 平面：单层 Z
	}
}

void UM7_BrushParamsModule::OnVolumetricBrushChanged(bool bChecked)
{
	if (bChecked)
	{
		SetBrushDimension(EBrushDimension::Volumetric); // 立体：3D 体积
	}
}

void UM7_BrushParamsModule::OnBrushSizeChanged(float Value)
{
	// 本地缓存用于即时滑块显示，权威值同步到 StateComponent
	CurrentBrushSize = FMath::RoundToInt(Value);
	// 即时更新数字显示
	if (BrushSizeText)
	{
		BrushSizeText->SetText(FText::AsNumber(FMath::RoundToInt(Value)));
	}
	// 交互层：通过 InteractionComp 设置笔刷大小（显示由 OnBuildModeParamsUpdated 驱动）
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->SetBrushSize(FMath::RoundToInt(Value));
		}
	}
}

void UM7_BrushParamsModule::SetBrushShape(EBatchShapeType NewShape)
{
	// 交互层：通过 InteractionComp 设置笔刷形状（复用 BatchShape）
	// 显示由 UpdateFromToolState 统一驱动，交互层不直接改UI
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->SetBatchShape(NewShape);
		}
	}
}

void UM7_BrushParamsModule::SetBrushDimension(EBrushDimension NewDimension)
{
	// 交互层：通过 InteractionComp 设置笔刷维度
	// 显示由 UpdateFromToolState 统一驱动，交互层不直接改UI
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->SetBrushDimension(NewDimension);
		}
	}
}

void UM7_BrushParamsModule::UpdateBrushShapeCheckBoxes(EBatchShapeType CurrentShape)
{
	// 更新笔刷形状复选框的选中状态
	if (CircleBrushCheckBox)
	{
		CircleBrushCheckBox->SetCheckedState(CurrentShape == EBatchShapeType::Sphere ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (SquareBrushCheckBox)
	{
		SquareBrushCheckBox->SetCheckedState(CurrentShape == EBatchShapeType::Box ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}
}

void UM7_BrushParamsModule::UpdateBrushDimensionCheckBoxes(EBrushDimension CurrentDimension)
{
	// 更新笔刷维度复选框的选中状态
	if (FlatBrushCheckBox)
	{
		FlatBrushCheckBox->SetCheckedState(CurrentDimension == EBrushDimension::Flat ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (VolumetricBrushCheckBox)
	{
		VolumetricBrushCheckBox->SetCheckedState(CurrentDimension == EBrushDimension::Volumetric ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}
}

void UM7_BrushParamsModule::NativeDestruct()
{
	if (StateComponent && bDelegatesBound)
	{
		StateComponent->UnbindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM7_BrushParamsModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = false;
	}
	Super::NativeDestruct();
}

void UM7_BrushParamsModule::HandleBuildModeParamsUpdated()
{
	// 笔刷参数变化时（形状/维度/大小）实时刷新 UI
	if (StateComponent)
	{
		UpdateFromToolState(StateComponent);
	}
}
