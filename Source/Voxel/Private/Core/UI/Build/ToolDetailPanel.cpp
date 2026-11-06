// Copyright (c) 2025 zolnoor. All rights reserved.

#include "Core/UI/Build/ToolDetailPanel.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/PanelWidget.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/CoreTags.h"
#include "Voxel/VoxelPlayerController.h"
#include "Voxel/Editor/Tools/VoxelToolBase.h"

// 模块头文件
#include "Core/UI/Build/Modules/M2b1_PlacementModeModule.h"
#include "Core/UI/Build/Modules/M2b2_SelectionOpModeModule.h"
#include "Core/UI/Build/Modules/M3_GeometryShapeModule.h"
#include "Core/UI/Build/Modules/M4a_BoundingBoxModule.h"
#include "Core/UI/Build/Modules/M4b_GeometryParamsModule.h"
#include "Core/UI/Build/Modules/M5_SelectionInfoModule.h"
#include "Core/UI/Build/Modules/M7_BrushParamsModule.h"
// #include "Core/UI/Build/Modules/M8_AreaInfoModule.h" // 已合并到 M13a
#include "Core/UI/Build/Modules/M9_AreaWallBridgeModule.h"
#include "Core/UI/Build/Modules/M10_SampleParamsModule.h"
#include "Core/UI/Build/Modules/M11_MagicWandParamsModule.h"
// #include "Core/UI/Build/Modules/M13a_AreaBasicInfoEditModule.h" // 区域工具详情不再创建该模块
#include "Core/UI/Build/Modules/M15_PositionAndPlacementModule.h"
#include "Core/UI/Build/Modules/M16_ActionButtonsModule.h"

void UToolDetailPanel::NativeOnInitialized() {
    Super::NativeOnInitialized();
    AutoCreateAndRegisterModules();
}

void UToolDetailPanel::NativeConstruct() {
    Super::NativeConstruct();
}

void UToolDetailPanel::RegisterModule(UToolDetailModuleBase* Module)
{
    if (!Module)
    {
        return;
    }

    if (!RegisteredModules.Contains(Module))
    {
        RegisteredModules.Add(Module);
        //UE_LOG(LogTemp, Log, TEXT("ToolDetailPanel: Registered module %s (Order=%d)"),
        //    *Module->GetName(), Module->DisplayOrder);
    }
}

void UToolDetailPanel::UnregisterModule(UToolDetailModuleBase* Module) {
    if (!Module) {
        return;
    }

    RegisteredModules.Remove(Module);
    
    // 从UI容器中移除
    if (ModulesContainer) {
        ModulesContainer->RemoveChild(Module);
    }
}

void UToolDetailPanel::ClearAllModules() {
    RegisteredModules.Empty();
    
    if (ModulesContainer) {
        ModulesContainer->ClearChildren();
    }
}

void UToolDetailPanel::UpdateVisibilityForTool(FGameplayTag ToolTag) {
    UE_LOG(LogTemp, Log, TEXT("[ToolChain] ToolDetailPanel UpdateVisibilityForTool ENTER: Class=%s Tool=%s Registered=%d Container=%s Children=%d"),
        *GetClass()->GetName(),
        *ToolTag.ToString(),
        RegisteredModules.Num(),
        ModulesContainer ? TEXT("VALID") : TEXT("NULL"),
        ModulesContainer ? ModulesContainer->GetChildrenCount() : -1);
    
    if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer())) {
        if (UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>()) {
            bool bFoundM15 = false;
            for (auto& Module : RegisteredModules)
            {
                if (Module)
                {
                    const FString ModuleClassName = Module->GetClass()->GetName();

                    bool bShouldShow = Module->ShouldShowForTool(ToolTag);
                    Module->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

                    if (ModuleClassName.Contains(TEXT("M15")))
                    {
                        bFoundM15 = true;
                        UE_LOG(LogTemp, Warning, TEXT("[ToolDetailPanel] M15 visibility update: Tool=%s ShouldShow=%d ModuleVis=%s Container=%s Children=%d"),
                            *ToolTag.ToString(), bShouldShow ? 1 : 0,
                            *UEnum::GetValueAsString(Module->GetVisibility()),
                            ModulesContainer ? TEXT("VALID") : TEXT("NULL"),
                            ModulesContainer ? ModulesContainer->GetChildrenCount() : -1);
                    }

                    if (bShouldShow) {
                        Module->UpdateFromToolState(SC);
                    }
                }
            }

            if (!bFoundM15)
            {
                UE_LOG(LogTemp, Error, TEXT("[ToolDetailPanel] M15 module is NOT registered in this ToolDetailPanel instance. Registered=%d"), RegisteredModules.Num());
            }
        } else {
            UE_LOG(LogTemp, Warning, TEXT("[ToolChain] ToolDetailPanel: BSC not found via FindComponentByClass"));
        }
    } else {
        UE_LOG(LogTemp, Warning, TEXT("[ToolChain] ToolDetailPanel: PC cast failed"));
    }

    // 面板级显隐：由当前工具的声明决定（工具声明方案，单一真相源在工具侧）
    ApplyPanelVisibilityForTool(ToolTag);
}

void UToolDetailPanel::ApplyPanelVisibilityForTool(FGameplayTag ToolTag)
{
    bool bPanelNeeded = false;
    if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer())) {
        if (UVoxelInteractionComponent* InteractionComp = PC->FindComponentByClass<UVoxelInteractionComponent>()) {
            if (UVoxelToolBase* CurrentTool = InteractionComp->GetCurrentTool()) {
                bPanelNeeded = CurrentTool->bNeedsDetailPanel;
            }
        }
    }

    const ESlateVisibility NewVisibility = bPanelNeeded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
    if (GetVisibility() != NewVisibility) {
        UE_LOG(LogTemp, Log, TEXT("[ToolChain] ToolDetailPanel: %s（工具=%s，bNeedsDetailPanel=%d）"),
            bPanelNeeded ? TEXT("显示面板") : TEXT("隐藏面板（工具声明不需要）"),
            *ToolTag.ToString(), bPanelNeeded ? 1 : 0);
        SetVisibility(NewVisibility);
    }
}

void UToolDetailPanel::RefreshDisplay() {
    if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer())) {
        if (UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>()) {
            FGameplayTag CurrentTool = SC->GetCurrentToolTag();
            UpdateVisibilityForTool(CurrentTool);
        }
    }
}

void UToolDetailPanel::AutoCreateAndRegisterModules() {
    int32 CreatedModuleCount = 0;

    auto CreateRequiredModule = [this, &CreatedModuleCount](TSubclassOf<UToolDetailModuleBase> ModuleClass, const TCHAR* ConfigName)
    {
        if (!ModuleClass)
        {
            UE_LOG(LogTemp, Error, TEXT("[ToolDetailPanel] %s is NOT configured. Refuse to create native fallback widget."), ConfigName);
            return;
        }

        UToolDetailModuleBase* Module = CreateWidget<UToolDetailModuleBase>(this, ModuleClass);
        if (!Module)
        {
            UE_LOG(LogTemp, Error, TEXT("[ToolDetailPanel] CreateWidget failed: %s Class=%s"), ConfigName, *ModuleClass->GetName());
            return;
        }

        UE_LOG(LogTemp, Warning, TEXT("[ToolDetailPanel] Created module %s with class: %s"), ConfigName, *Module->GetClass()->GetName());
        RegisterModule(Module);
        CreatedModuleCount++;
    };

    // M2b-1: 放置模式模块
    CreateRequiredModule(M2b1_PlacementModeModuleClass, TEXT("M2b1_PlacementModeModuleClass"));

    // M2b-2: 选区运算模式模块
    CreateRequiredModule(M2b2_SelectionOpModeModuleClass, TEXT("M2b2_SelectionOpModeModuleClass"));

    // M3: 几何形状模块
    CreateRequiredModule(M3_GeometryShapeModuleClass, TEXT("M3_GeometryShapeModuleClass"));

    // M4a: 包围盒模块
    CreateRequiredModule(M4a_BoundingBoxModuleClass, TEXT("M4a_BoundingBoxModuleClass"));

    // M4b: 几何参数控制模块
    CreateRequiredModule(M4b_GeometryParamsModuleClass, TEXT("M4b_GeometryParamsModuleClass"));

    // M5: 选区信息模块
    CreateRequiredModule(M5_SelectionInfoModuleClass, TEXT("M5_SelectionInfoModuleClass"));

    // M7: 笔刷参数控制模块
    CreateRequiredModule(M7_BrushParamsModuleClass, TEXT("M7_BrushParamsModuleClass"));

    // M8: 区域信息显示模块（已合并到 M13a，保留配置但不创建）

    // M10: 采样参数控制模块
    CreateRequiredModule(M10_SampleParamsModuleClass, TEXT("M10_SampleParamsModuleClass"));

    // M11: 魔棒参数控制模块
    CreateRequiredModule(M11_MagicWandParamsModuleClass, TEXT("M11_MagicWandParamsModuleClass"));

    // M13a: 区域导航模块（区域工具详情不再展示业务/导航信息，保留配置但不创建）

    // M9: 区域建墙桥接模块（外壳生成统一由 M4b 几何参数模块的厚度+壳体面控制，保留配置但不创建）

    // M15: 位置和放置控制模块
    CreateRequiredModule(M15_PositionAndPlacementModuleClass, TEXT("M15_PositionAndPlacementModuleClass"));

    // M16: 操作按钮模块（v6 原型：确认/取消改为世界内浮标，不再在配置面板展示，保留配置但不创建）

    //UE_LOG(LogTemp, Log, TEXT("ToolDetailPanel: Auto-created and registered %d modules"), CreatedModuleCount);

    // 所有模块创建完成后，统一按 DisplayOrder 排序并一次性添加到 UI 容器
    // 避免每次 RegisterModule 都 ClearChildren+AddChild 导致子模块 NativeConstruct 被重复调用
    if (ModulesContainer)
    {
        ModulesContainer->ClearChildren();
        RegisteredModules.Sort([](const UToolDetailModuleBase& A, const UToolDetailModuleBase& B)
        {
            return A.DisplayOrder < B.DisplayOrder;
        });

        for (auto& SortedModule : RegisteredModules)
        {
            if (SortedModule)
            {
                ModulesContainer->AddChild(SortedModule);
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("[ToolDetailPanel] AutoCreate: ModulesContainer=VALID Registered=%d Children=%d"),
            RegisteredModules.Num(), ModulesContainer->GetChildrenCount());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[ToolDetailPanel] AutoCreate: ModulesContainer=NULL. Modules are registered but NOT visible."));
    }

}

