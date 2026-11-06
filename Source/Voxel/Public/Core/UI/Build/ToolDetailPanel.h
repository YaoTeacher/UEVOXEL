// Copyright (c) 2025 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ToolDetailPanel.generated.h"

// 前向声明
class UPanelWidget;
class UToolDetailModuleBase;

// 模块前向声明
class UM2b1_PlacementModeModule;
class UM2b2_SelectionOpModeModule;
class UM3_GeometryShapeModule;
class UM4a_BoundingBoxModule;
class UM4b_GeometryParamsModule;
class UM5_SelectionInfoModule;
class UM7_BrushParamsModule;
class UM8_AreaInfoModule;
class UM9_AreaWallBridgeModule;
class UM10_SampleParamsModule;
    class UM11_MagicWandParamsModule;
    class UM13a_AreaBasicInfoEditModule;
class UM15_PositionAndPlacementModule;
class UM16_ActionButtonsModule;

/**
 * ToolDetailPanel - 工具详情面板
 * 
 * 职责：管理当前选中工具的详情展示
 * - 维护详情模块注册表
 * - 根据当前工具动态显示/隐藏对应的详情模块
 * - 支持详情模块的排序和显隐控制
 * 
 * 初始化：在 NativeConstruct 中自动调用 AutoCreateAndRegisterModules()
 */
UCLASS()
class VOXEL_API UToolDetailPanel : public UCommonUserWidget {
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    // === 容器设置 ===
    UFUNCTION(BlueprintCallable, Category = "ToolDetailPanel")
    void SetModulesContainer(UPanelWidget* Container) { ModulesContainer = Container; }

    UFUNCTION(BlueprintPure, Category = "ToolDetailPanel")
    UPanelWidget* GetModulesContainer() const { return ModulesContainer; }

    // === 模块管理 ===
    UFUNCTION(BlueprintCallable, Category = "ToolDetailPanel")
    void RegisterModule(UToolDetailModuleBase* Module);

    UFUNCTION(BlueprintCallable, Category = "ToolDetailPanel")
    void UnregisterModule(UToolDetailModuleBase* Module);

    UFUNCTION(BlueprintCallable, Category = "ToolDetailPanel")
    void ClearAllModules();

    // === 显示控制 ===
    UFUNCTION(BlueprintCallable, Category = "ToolDetailPanel")
    void UpdateVisibilityForTool(FGameplayTag ToolTag);

    UFUNCTION(BlueprintCallable, Category = "ToolDetailPanel")
    void RefreshDisplay();

    /** 面板级显隐：由当前工具的 bNeedsDetailPanel 声明决定 */
    void ApplyPanelVisibilityForTool(FGameplayTag ToolTag);

    // === 自动初始化 ===
    /** 自动创建并注册所有配置的模块 */
    UFUNCTION(BlueprintCallable, Category = "ToolDetailPanel")
    void AutoCreateAndRegisterModules();

    /** 绑定模块委托到业务逻辑 */
    // ToolDetailPanel 只负责模块创建/排序/显隐，不绑定模块业务事件。

protected:
    UPROPERTY(meta = (BindWidget, Optional))
    UPanelWidget* ModulesContainer;

    // === 模块蓝图类配置 ===
    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM2b1_PlacementModeModule> M2b1_PlacementModeModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM2b2_SelectionOpModeModule> M2b2_SelectionOpModeModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM3_GeometryShapeModule> M3_GeometryShapeModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM4a_BoundingBoxModule> M4a_BoundingBoxModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM4b_GeometryParamsModule> M4b_GeometryParamsModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM5_SelectionInfoModule> M5_SelectionInfoModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM7_BrushParamsModule> M7_BrushParamsModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM8_AreaInfoModule> M8_AreaInfoModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM9_AreaWallBridgeModule> M9_AreaWallBridgeModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM10_SampleParamsModule> M10_SampleParamsModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM11_MagicWandParamsModule> M11_MagicWandParamsModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM13a_AreaBasicInfoEditModule> M13a_AreaBasicInfoEditModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM15_PositionAndPlacementModule> M15_PositionAndPlacementModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolDetailPanel|Module Classes")
    TSubclassOf<UM16_ActionButtonsModule> M16_ActionButtonsModuleClass;

private:
    UPROPERTY()
    TArray<TObjectPtr<UToolDetailModuleBase>> RegisteredModules;
};