// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/Modules/M9_AreaWallBridgeModule.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/CoreTags.h"
#include "Components/Button.h"

bool UM9_AreaWallBridgeModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	return ToolTag.MatchesTag(FCoreTags::Tool_AreaEdit);
}

void UM9_AreaWallBridgeModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WallBridgeButton)
		WallBridgeButton->OnClicked.AddDynamic(this, &UM9_AreaWallBridgeModule::OnWallBridgeClicked);
}

void UM9_AreaWallBridgeModule::OnWallBridgeClicked()
{
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (UVoxelInteractionComponent* InteractionComp =
			PC->FindComponentByClass<UVoxelInteractionComponent>())
		{
			// 桥接：区域边界 → 几何体编辑壳体请求 + 切工具（无选中区域时门面内警告）
			InteractionComp->BeginWallShellFromArea();
		}
	}
}
