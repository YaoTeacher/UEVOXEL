// Copyright (c) 2025 zolnoor. All rights reserved.

#include "Core/UI/Build/ToolLibraryPanel.h"
#include "Components/PanelWidget.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"
#include "Core/UI/Build/Modules/M1_BuildLibraryModule.h"
#include "Core/UI/Build/Modules/M1b_TemplateLibraryModule.h"
#include "Core/UI/Build/Modules/M12b_AreaTypeLibraryModule.h"
#include "Voxel/VoxelPlayerController.h"

void UToolLibraryPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    AutoCreateAndRegisterModules();
}

void UToolLibraryPanel::NativeConstruct()
{
    Super::NativeConstruct();
}

void UToolLibraryPanel::RegisterModule(UToolDetailModuleBase* Module)
{
    if (!Module)
    {
        return;
    }

    if (!RegisteredLibraryModules.Contains(Module))
    {
        RegisteredLibraryModules.Add(Module);

        // 将模块添加到UI容器
        if (LibraryModulesContainer)
        {
            LibraryModulesContainer->AddChild(Module);
            //UE_LOG(LogTemp, Log, TEXT("[UIChain] ToolLibraryPanel: Added module %s to container, container children=%d, module visibility=%s"),
            //    *Module->GetName(), LibraryModulesContainer->GetChildrenCount(),
            //    *UEnum::GetValueAsString(Module->GetVisibility()));
        }

        //UE_LOG(LogTemp, Log, TEXT("ToolLibraryPanel: Registered module %s"), *Module->GetName());
    }
}

void UToolLibraryPanel::UnregisterModule(UToolDetailModuleBase* Module)
{
    if (!Module)
    {
        return;
    }

    RegisteredLibraryModules.Remove(Module);

    // 从UI容器中移除
    if (LibraryModulesContainer)
    {
        LibraryModulesContainer->RemoveChild(Module);
    }
}

void UToolLibraryPanel::ClearAllModules()
{
    RegisteredLibraryModules.Empty();

    if (LibraryModulesContainer)
    {
        LibraryModulesContainer->ClearChildren();
    }
}

void UToolLibraryPanel::UpdateVisibilityForTool(FGameplayTag ToolTag)
{
    UE_LOG(LogTemp, Log, TEXT("[ToolChain] ToolLibraryPanel UpdateVisibilityForTool ENTER: Tool=%s Modules=%d"),
        *ToolTag.ToString(), RegisteredLibraryModules.Num());

    if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
    {
        if (UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>())
        {
            for (auto& Module : RegisteredLibraryModules)
            {
                if (Module)
                {
                    bool bShouldShow = Module->ShouldShowForTool(ToolTag);
                    Module->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

                    if (bShouldShow)
                    {
                        Module->UpdateFromToolState(SC);
                    }
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[ToolChain] ToolLibraryPanel: BSC not found via FindComponentByClass"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ToolChain] ToolLibraryPanel: PC cast failed"));
    }

    // 面板级显隐：由当前工具的声明决定（工具声明方案，单一真相源在工具侧）
    ApplyPanelVisibilityForTool(ToolTag);
}

void UToolLibraryPanel::ApplyPanelVisibilityForTool(FGameplayTag ToolTag)
{
    bool bPanelNeeded = false;
    if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
    {
        if (UVoxelInteractionComponent* InteractionComp = PC->FindComponentByClass<UVoxelInteractionComponent>())
        {
            if (UVoxelToolBase* CurrentTool = InteractionComp->GetCurrentTool())
            {
                bPanelNeeded = CurrentTool->bNeedsLibraryPanel;
            }
        }
    }

    const ESlateVisibility NewVisibility = bPanelNeeded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
    if (GetVisibility() != NewVisibility)
    {
        UE_LOG(LogTemp, Log, TEXT("[ToolChain] ToolLibraryPanel: %s（工具=%s，bNeedsLibraryPanel=%d）"),
            bPanelNeeded ? TEXT("显示面板") : TEXT("隐藏面板（工具声明不需要）"),
            *ToolTag.ToString(), bPanelNeeded ? 1 : 0);
        SetVisibility(NewVisibility);
    }
}

void UToolLibraryPanel::RefreshDisplay()
{
    if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
    {
        if (UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>())
        {
            FGameplayTag CurrentTool = SC->GetCurrentToolTag();
            UpdateVisibilityForTool(CurrentTool);
        }
    }
}

void UToolLibraryPanel::AutoCreateAndRegisterModules()
{
    int32 CreatedModuleCount = 0;

    auto CreateRequiredLibraryModule = [this, &CreatedModuleCount](TSubclassOf<UToolDetailModuleBase> ModuleClass, const TCHAR* ConfigName)
    {
        if (!ModuleClass)
        {
            UE_LOG(LogTemp, Error, TEXT("[ToolLibraryPanel] %s is NOT configured. Refuse to create native fallback widget."), ConfigName);
            return;
        }

        UToolDetailModuleBase* Module = CreateWidget<UToolDetailModuleBase>(this, ModuleClass);
        if (!Module)
        {
            UE_LOG(LogTemp, Error, TEXT("[ToolLibraryPanel] CreateWidget failed: %s Class=%s"), ConfigName, *ModuleClass->GetName());
            return;
        }

        UE_LOG(LogTemp, Warning, TEXT("[ToolLibraryPanel] Created module %s with class: %s"), ConfigName, *Module->GetClass()->GetName());
        RegisterModule(Module);
        CreatedModuleCount++;
    };

    // M1: 建造库模块
    CreateRequiredLibraryModule(M1_BuildLibraryModuleClass, TEXT("M1_BuildLibraryModuleClass"));

    // M1b: 模板库模块
    CreateRequiredLibraryModule(M1b_TemplateLibraryModuleClass, TEXT("M1b_TemplateLibraryModuleClass"));

    // M12b: 区域类型库模块
    CreateRequiredLibraryModule(M12b_AreaTypeLibraryModuleClass, TEXT("M12b_AreaTypeLibraryModuleClass"));

    //UE_LOG(LogTemp, Log, TEXT("ToolLibraryPanel: Auto-created and registered %d library modules"), CreatedModuleCount);

    // 绑定模块委托
    BindModuleDelegates();
}

void UToolLibraryPanel::BindModuleDelegates()
{
    // 暂无库模块需要面板级委托绑定；模块内部事件由各模块自行处理。
}
