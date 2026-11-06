// Copyright (c) 2025 zolnoor. All rights reserved.

#include "Core/UI/Build/BuildPanelWidget.h"
#include "Core/UI/Build/ToolDetailPanel.h"
#include "Core/UI/Build/ToolLibraryPanel.h"

#include "Components/PanelWidget.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Area/VoxelAreaBaseInstance.h"

void UBuildPanelWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    // 一次性初始化（不随面板开关重复执行）
    InitializeToolContainer();
    InitializeToolLibraryPanel();
    InitializeToolDetailPanel();
    BindDelegate();

    HandleBuildModeParamsUpdated();
}

void UBuildPanelWidget::NativeConstruct() {
    Super::NativeConstruct();

    // 每次激活时同步 UI 状态（委托已在 NativeOnInitialized 绑定）
    HandleBuildModeParamsUpdated();
}

void UBuildPanelWidget::OnEnterScreen(UObject* Context)
{
    HandleBuildModeParamsUpdated();
}

void UBuildPanelWidget::OnExitScreen()
{
}

bool UBuildPanelWidget::HandleEscape()
{
    // ESC 退出建造：必须切换游戏模式（Adventure），由模式广播驱动 MainPanel 关闭面板。
    // 只关面板不切模式会造成「面板已关、模式仍为建造」的失同步（按钮文本与实际状态不符）。
    if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
    {
        if (PC->InteractionComp)
        {
            PC->InteractionComp->SetPlayerMode(EVoxelControllerMode::AdventureMode);
            return true;
        }
        UE_LOG(LogTemp, Error, TEXT("[BuildPanel] HandleEscape: InteractionComp is null, cannot exit build mode"));
        return false;
    }
    UE_LOG(LogTemp, Error, TEXT("[BuildPanel] HandleEscape: PC cast failed"));
    return false;
}

void UBuildPanelWidget::InitializeToolContainer() {
    UE_LOG(LogTemp, Log, TEXT("[BuildPanel] InitializeToolContainer：ToolContainer=%s"),
        ToolContainer ? *ToolContainer->GetName() : TEXT("NULL（蓝图未绑定）"));
    if (!ToolContainer) {
        UE_LOG(LogTemp, Error, TEXT("[BuildPanel] ToolContainer 未绑定（BindWidget 失败），工具栏不会生成"));
        return;
    }
    ToolContainer->InitializeToolList(FGameplayTag::RequestGameplayTag(FName("Tool")));
}

void UBuildPanelWidget::InitializeToolLibraryPanel() {
    if (ToolLibraryPanel) {
        ToolLibraryPanel->RefreshDisplay();
    }
}

void UBuildPanelWidget::InitializeToolDetailPanel() {
    if (ToolDetailPanel) {
        ToolDetailPanel->RefreshDisplay();
    }
}

void UBuildPanelWidget::BindDelegate() {
    AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>();
    if (!PC) {
        UE_LOG(LogTemp, Error, TEXT("[ToolChain] BuildPanel BindDelegate: PC cast failed"));
        return;
    }
    UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
    if (!SC) {
        UE_LOG(LogTemp, Error, TEXT("[ToolChain] BuildPanel BindDelegate: BSC not found via FindComponentByClass"));
        return;
    }
    SC->BindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UBuildPanelWidget, HandleBuildModeParamsUpdated));
    if (UVoxelInteractionComponent* IC = PC->FindComponentByClass<UVoxelInteractionComponent>()) {
        IC->OnControllerModeChanged.AddDynamic(this, &UBuildPanelWidget::HandleControllerModeChanged);
    }
    UE_LOG(LogTemp, Log, TEXT("[ToolChain] BuildPanel BindDelegate: bound BindAllBuildEvents + OnControllerModeChanged OK"));
}

void UBuildPanelWidget::HandleBuildModeParamsUpdated() {
    FGameplayTag CurrentToolTag;
    AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>();
    if (!PC) {
        UE_LOG(LogTemp, Error, TEXT("[ToolChain] BuildPanel HandleBuildModeParamsUpdated: PC cast failed"));
        return;
    }
    UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
    if (!SC) {
        UE_LOG(LogTemp, Error, TEXT("[ToolChain] BuildPanel HandleBuildModeParamsUpdated: BSC not found"));
        return;
    }
    CurrentToolTag = SC->GetCurrentToolTag();
    if (ToolContainer) {
        ToolContainer->SyncFromState();
    }

    if (CurrentToolTag != LastSyncedPanelToolTag)
    {
        UE_LOG(LogTemp, Log, TEXT("[ToolChain] BuildPanel ToolChanged: '%s' -> '%s', updating child panels"),
            *LastSyncedPanelToolTag.ToString(), *CurrentToolTag.ToString());
        LastSyncedPanelToolTag = CurrentToolTag;

        if (ToolLibraryPanel) {
            ToolLibraryPanel->UpdateVisibilityForTool(CurrentToolTag);
        }

        if (ToolDetailPanel) {
            ToolDetailPanel->UpdateVisibilityForTool(CurrentToolTag);
        }
    }
    else
    {
        UE_LOG(LogTemp, Verbose, TEXT("[ToolChain] BuildPanel HandleBuildModeParamsUpdated: skipped, tool unchanged '%s'"), *CurrentToolTag.ToString());
    }
}

void UBuildPanelWidget::HandleControllerModeChanged(UVoxelInteractionComponent* Interaction) {
    if (!Interaction) return;

    EVoxelControllerMode NewMode = Interaction->GetControllerMode();
    if (NewMode == EVoxelControllerMode::SceneBuildMode) {
        if (EditorModePanel) EditorModePanel->SetVisibility(ESlateVisibility::Visible);
    } else {
        if (EditorModePanel) EditorModePanel->SetVisibility(ESlateVisibility::Collapsed);
    }
}
