// Copyright (c) 2025 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ToolLibraryPanel.generated.h"

// 前向声明
class UPanelWidget;
class UToolDetailModuleBase;

// 模块前向声明
class UM1_BuildLibraryModule;
class UM1b_TemplateLibraryModule;
class UM12b_AreaTypeLibraryModule;

/**
 * ToolLibraryPanel - 工具库面板
 *
 * 职责：管理所有"库"类模块的展示
 * - 与 ToolDetailPanel 分离，避免左侧详情面板内容过多
 * - 根据当前工具动态显示/隐藏对应的库模块
 * - 当前包含：M1 建造库、M1b 模板库、M12b 区域类型库
 *
 * 初始化：在 NativeConstruct 中自动调用 AutoCreateAndRegisterModules()
 */
UCLASS()
class VOXEL_API UToolLibraryPanel : public UCommonUserWidget {
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    // === 容器设置 ===
    UFUNCTION(BlueprintCallable, Category = "ToolLibraryPanel")
    void SetLibraryModulesContainer(UPanelWidget* Container) { LibraryModulesContainer = Container; }

    UFUNCTION(BlueprintPure, Category = "ToolLibraryPanel")
    UPanelWidget* GetLibraryModulesContainer() const { return LibraryModulesContainer; }

    // === 模块管理 ===
    UFUNCTION(BlueprintCallable, Category = "ToolLibraryPanel")
    void RegisterModule(UToolDetailModuleBase* Module);

    UFUNCTION(BlueprintCallable, Category = "ToolLibraryPanel")
    void UnregisterModule(UToolDetailModuleBase* Module);

    UFUNCTION(BlueprintCallable, Category = "ToolLibraryPanel")
    void ClearAllModules();

    // === 显示控制 ===
    UFUNCTION(BlueprintCallable, Category = "ToolLibraryPanel")
    void UpdateVisibilityForTool(FGameplayTag ToolTag);

    UFUNCTION(BlueprintCallable, Category = "ToolLibraryPanel")
    void RefreshDisplay();

    /** 面板级显隐：由当前工具的 bNeedsLibraryPanel 声明决定 */
    void ApplyPanelVisibilityForTool(FGameplayTag ToolTag);

    // === 自动初始化 ===
    /** 自动创建并注册所有配置的库模块 */
    UFUNCTION(BlueprintCallable, Category = "ToolLibraryPanel")
    void AutoCreateAndRegisterModules();

    /** 绑定模块委托到业务逻辑 */
    void BindModuleDelegates();

protected:
    UPROPERTY(meta = (BindWidget, Optional))
    UPanelWidget* LibraryModulesContainer;

    // === 模块蓝图类配置 ===
    UPROPERTY(EditDefaultsOnly, Category = "ToolLibraryPanel|Module Classes")
    TSubclassOf<UM1_BuildLibraryModule> M1_BuildLibraryModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolLibraryPanel|Module Classes")
    TSubclassOf<UM1b_TemplateLibraryModule> M1b_TemplateLibraryModuleClass;

    UPROPERTY(EditDefaultsOnly, Category = "ToolLibraryPanel|Module Classes")
    TSubclassOf<UM12b_AreaTypeLibraryModule> M12b_AreaTypeLibraryModuleClass;

private:
    UPROPERTY()
    TArray<TObjectPtr<UToolDetailModuleBase>> RegisteredLibraryModules;
};
