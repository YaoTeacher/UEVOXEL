#include "Core/UI/Build/Modules/M11_MagicWandParamsModule.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/CoreTags.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"

bool UM11_MagicWandParamsModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	return ToolTag == FCoreTags::Tool_Selection_Magic;
}

void UM11_MagicWandParamsModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	// 从本地状态变量读取，驱动UI显示
	UpdateSelectionModeCheckBoxes(bIsConnected);

	if (MaxVoxelCountInput)
	{
		MaxVoxelCountInput->SetText(FText::AsNumber(MaxVoxelCount));
	}

	if (MaxDepthInput)
	{
		MaxDepthInput->SetText(FText::AsNumber(MaxDepth));
	}
}

void UM11_MagicWandParamsModule::UpdateParams(bool bInIsConnected, int32 InMaxVoxelCount, int32 InMaxDepth)
{
	bIsConnected = bInIsConnected;
	MaxVoxelCount = InMaxVoxelCount;
	MaxDepth = InMaxDepth;

	UpdateSelectionModeCheckBoxes(bIsConnected);

	if (MaxVoxelCountInput)
	{
		MaxVoxelCountInput->SetText(FText::AsNumber(MaxVoxelCount));
	}

	if (MaxDepthInput)
	{
		MaxDepthInput->SetText(FText::AsNumber(MaxDepth));
	}
}

void UM11_MagicWandParamsModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ConnectedRegionModeCheckBox)
		ConnectedRegionModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM11_MagicWandParamsModule::OnConnectedRegionModeChanged);
	if (GlobalSimilarModeCheckBox)
		GlobalSimilarModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM11_MagicWandParamsModule::OnGlobalSimilarModeChanged);
	if (MaxVoxelCountInput)
		MaxVoxelCountInput->OnTextChanged.AddDynamic(this, &UM11_MagicWandParamsModule::OnMaxVoxelCountTextChanged);
	if (MaxDepthInput)
		MaxDepthInput->OnTextChanged.AddDynamic(this, &UM11_MagicWandParamsModule::OnMaxDepthTextChanged);
}

void UM11_MagicWandParamsModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM11_MagicWandParamsModule::OnConnectedRegionModeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetSelectionMode(true);
	}
}

void UM11_MagicWandParamsModule::OnGlobalSimilarModeChanged(bool bChecked)
{
	if (bChecked)
	{
		SetSelectionMode(false);
	}
}

void UM11_MagicWandParamsModule::OnMaxVoxelCountTextChanged(const FText& Text)
{
	FString TextString = Text.ToString();
	if (TextString.IsEmpty() || !FCString::IsNumeric(*TextString))
	{
		return;
	}

	MaxVoxelCount = FCString::Atoi(*TextString);
	OnMaxVoxelCountChanged.Broadcast(MaxVoxelCount);
}

void UM11_MagicWandParamsModule::OnMaxDepthTextChanged(const FText& Text)
{
	FString TextString = Text.ToString();
	if (TextString.IsEmpty() || !FCString::IsNumeric(*TextString))
	{
		return;
	}

	MaxDepth = FCString::Atoi(*TextString);
	OnMaxDepthChanged.Broadcast(MaxDepth);
}

void UM11_MagicWandParamsModule::SetSelectionMode(bool bInIsConnected)
{
	bIsConnected = bInIsConnected;
	OnSelectionModeChanged.Broadcast(bIsConnected);
}

void UM11_MagicWandParamsModule::UpdateSelectionModeCheckBoxes(bool bInIsConnected)
{
	if (ConnectedRegionModeCheckBox)
	{
		ConnectedRegionModeCheckBox->SetCheckedState(bInIsConnected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	if (GlobalSimilarModeCheckBox)
	{
		GlobalSimilarModeCheckBox->SetCheckedState(!bInIsConnected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}
}
