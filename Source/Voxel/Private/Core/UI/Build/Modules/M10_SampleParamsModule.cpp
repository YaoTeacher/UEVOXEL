#include "Core/UI/Build/Modules/M10_SampleParamsModule.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/CoreTags.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"

bool UM10_SampleParamsModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	return ToolTag == FCoreTags::Tool_Sample;
}

void UM10_SampleParamsModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	// 从本地状态变量读取，驱动UI显示
	UpdateSampleModeCheckBoxes(bIsAreaMode);
	UpdateSamplePreview();
}

void UM10_SampleParamsModule::UpdateParams(bool bInIsAreaMode)
{
	bIsAreaMode = bInIsAreaMode;
	UpdateSampleModeCheckBoxes(bIsAreaMode);
	UpdateSamplePreview();
}

void UM10_SampleParamsModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SingleVoxelModeCheckBox)
		SingleVoxelModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM10_SampleParamsModule::OnSingleVoxelModeChanged);
	if (AreaSampleModeCheckBox)
		AreaSampleModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM10_SampleParamsModule::OnAreaSampleModeChanged);
}

void UM10_SampleParamsModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM10_SampleParamsModule::OnSingleVoxelModeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetSampleMode(false);
	}
}

void UM10_SampleParamsModule::OnAreaSampleModeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetSampleMode(true);
	}
}

void UM10_SampleParamsModule::SetSampleMode(bool bInIsAreaMode)
{
	bIsAreaMode = bInIsAreaMode;
	OnSampleModeChanged.Broadcast(bIsAreaMode);
}

void UM10_SampleParamsModule::UpdateSamplePreview()
{
	if (!PreviewTextBlock)
	{
		return;
	}

	if (bIsAreaMode)
	{
		PreviewTextBlock->SetText(FText::FromString(TEXT("区域采样模式")));
	}
	else
	{
		PreviewTextBlock->SetText(FText::FromString(TEXT("单体素采样模式")));
	}
}

void UM10_SampleParamsModule::UpdateSampleModeCheckBoxes(bool bInIsAreaMode)
{
	if (SingleVoxelModeCheckBox)
	{
		SingleVoxelModeCheckBox->SetCheckedState(!bInIsAreaMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (AreaSampleModeCheckBox)
	{
		AreaSampleModeCheckBox->SetCheckedState(bInIsAreaMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}
}
