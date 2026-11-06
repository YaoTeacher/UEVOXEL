// Copyright (c) 2025 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelScreenBase.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelGameMode.h"
#include "Core/Enum/GameStateEnum.h"
#include "Core/UI/Tool/ToolContainer.h"
#include "BuildPanelWidget.generated.h"

// UI组件前向声明
class UPanelWidget;

// 新架构类前向声明
class UToolDetailPanel;
class UToolLibraryPanel;

/**
 * 建造模式主面板（L2 Screen 层）
 *
 * 新架构：
 * - UToolContainer: 完整工具管理（主工具+子工具），内部处理层级
 * - UToolDetailPanel: 管理工具详情模块的显隐和排序
 * - BuildPanelWidget: 纯协调者，只负责初始化和响应广播
 *
 * 通过 AVoxelHUD::OpenScreen() 打开，ZOrder=50
 *
 * 注意：模式切换按钮和区域导航已移至 L1 主面板 (VoxelGameMainPanel)
 */
UCLASS()
class VOXEL_API UBuildPanelWidget : public UVoxelScreenBase
{
    GENERATED_BODY()

public:
    // === 核心初始化 ===
    virtual void NativeConstruct() override;
    virtual void NativeOnInitialized() override;

    // === L2 Screen 生命周期 ===
    virtual void OnEnterScreen(UObject* Context) override;
    virtual void OnExitScreen() override;
    virtual bool HandleEscape() override;

    /** 绑定委托：监听 StateComponent 的状态变化 */
    UFUNCTION(BlueprintCallable, Category = "BuildPanel|Action")
    void BindDelegate();

protected:
    // =========================================================
    // [UI 组件绑定]
    // =========================================================

    /** 工具容器（完整管理主工具+子工具） */
    UPROPERTY(meta = (BindWidget))
    UToolContainer* ToolContainer;

    /** 工具容器类（当BindWidget未绑定时，使用此类动态创建） */
    UPROPERTY(EditDefaultsOnly, Category = "BuildPanel|ToolContainer")
    TSubclassOf<UToolContainer> ToolContainerClass;

    /** 工具详情面板 */
    UPROPERTY(meta = (BindWidget))
    UToolDetailPanel* ToolDetailPanel;

    /** 工具详情面板类（当BindWidget未绑定时，使用此类动态创建） */
    UPROPERTY(EditDefaultsOnly, Category = "BuildPanel|ToolDetailPanel")
    TSubclassOf<UToolDetailPanel> ToolDetailPanelClass;

    /** 工具库面板（底部展示 M1/M1b 等库模块） */
    UPROPERTY(meta = (BindWidget))
    UToolLibraryPanel* ToolLibraryPanel;

    /** 编辑器模式专用面板容器 */
    UPROPERTY(meta = (BindWidget))
    UPanelWidget* EditorModePanel;

    // =========================================================
    // [事件处理] - 响应 StateComponent 广播
    // =========================================================

    /** 统一广播：建造模式参数更新 */
    UFUNCTION()
    void HandleBuildModeParamsUpdated();

    /** ControllerMode 变化：控制 EditorModePanel 显隐 */
    UFUNCTION()
    void HandleControllerModeChanged(UVoxelInteractionComponent* Interaction);

    // =========================================================
    // [初始化]
    // =========================================================

    /** 初始化工具容器 */
    void InitializeToolContainer();

    /** 初始化工具库面板 */
    void InitializeToolLibraryPanel();

    /** 初始化工具详情面板 */
    void InitializeToolDetailPanel();

private:
    /** 上次同步库/详情模块可见性的工具 Tag；工具未变化时不刷新整个前端模块树 */
    FGameplayTag LastSyncedPanelToolTag;
};
