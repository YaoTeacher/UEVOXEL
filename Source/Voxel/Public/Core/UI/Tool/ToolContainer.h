// Copyright (c) 2025 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "ToolContainer.generated.h"

class UToolIconButtonWidget;
class UPanelWidget;
class UWidget;
class UButton;
class UVoxelInteractionComponent;
class UVoxelControllerHistoryComponent;

/**
 * 完整工具容器组件
 * 
 * 职责：
 * - 管理主工具列表和子工具列表
 * - 内部处理工具层级关系（点击主工具自动显示子工具）
 * - 直接调用业务逻辑（InteractionComp->SwitchTool）
 * - 从 StateComponent 同步状态
 * 
 * 使用方式：
 * 1. 在 UMG 中绑定 MainToolList 和 SubToolList 容器
 * 2. 调用 Initialize(RootTag) 加载工具
 * 3. 调用 SyncFromState() 同步状态
 */
UCLASS(Abstract, Blueprintable)
class VOXEL_API UToolContainer : public UCommonUserWidget
{
    GENERATED_BODY()
    
public:
    UToolContainer(const FObjectInitializer& ObjectInitializer);

    //=========================================================================
    // 初始化
    //=========================================================================
    
    /**
     * 初始化工具容器
     * @param RootToolTag - 根工具标签（如 FCoreTags::Tool）
     * 自动加载所有直接子工具作为主工具
     */
    UFUNCTION(BlueprintCallable, Category = "Tool Container")
    void InitializeToolList(FGameplayTag RootToolTag);
    
    /**
     * 设置工具按钮的类（必须在Initialize之前调用）
     */
    UFUNCTION(BlueprintCallable, Category = "Tool Container")
    void SetToolButtonClass(TSubclassOf<UToolIconButtonWidget> InClass);
    
    /**
     * 设置 InteractionComp（用于业务逻辑调用，可选）
     * 如果不设置，会尝试从OwningPlayer获取
     */
    UFUNCTION(BlueprintCallable, Category = "Tool Container")
    void SetInteractComp(UVoxelInteractionComponent* InInteractComp);

    //=========================================================================
    // 状态同步
    //=========================================================================
    
    /**
     * 从 StateComponent 同步当前工具状态
     * 自动展开对应的子工具栏
     */
    UFUNCTION(BlueprintCallable, Category = "Tool Container")
    void SyncFromState();

    //=========================================================================
    // 查询
    //=========================================================================
    
    /** 获取当前选中的工具（可能是主工具或子工具） */
    UFUNCTION(BlueprintPure, Category = "Tool Container")
    FGameplayTag GetCurrentTool() const { return CurrentTool; }
    
    /** 获取当前展开的主工具（如果有子工具栏显示） */
    UFUNCTION(BlueprintPure, Category = "Tool Container")
    FGameplayTag GetExpandedMainTool() const { return ExpandedMainTool; }
    
    /** 检查是否有子工具栏显示 */
    UFUNCTION(BlueprintPure, Category = "Tool Container")
    bool HasSubToolsVisible() const;

protected:
    //=========================================================================
    // BindWidget 绑定的控件
    //=========================================================================
    
    /** 主工具列表容器（必须绑定） */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UPanelWidget> MainToolList;
    
    /** 子工具列表容器（可选，如果不绑定则不支持子工具） */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, OptionalWidget))
    TObjectPtr<UPanelWidget> SubToolList;

    /**
     * 二级工具栏整条容器（含背景条，可选）。
     * 绑定后展开/折叠切换整条 Band 的显隐；位置/大小完全由蓝图画布槽在设计器中配置，
     * C++ 不做任何布局操作。未绑定时退回仅切换 SubToolList。
     */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, OptionalWidget))
    TObjectPtr<UWidget> SubToolBand;

    /** 撤销按钮（底带历史组，可选） */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, OptionalWidget))
    TObjectPtr<UButton> UndoButton;

    /** 重做按钮（底带历史组，可选） */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, OptionalWidget))
    TObjectPtr<UButton> RedoButton;

    //=========================================================================
    // 配置
    //=========================================================================
    
    /** 工具按钮类（可在蓝图中覆盖） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool Container|Config")
    TSubclassOf<UToolIconButtonWidget> ToolButtonClass;
    
    /** 是否在选择时播放动画 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool Container|Config")
    bool bPlaySelectionAnimation = true;
    
    /** 是否自动折叠其他主工具的子工具栏 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool Container|Config")
    bool bAutoCollapseOtherSubTools = true;

    //=========================================================================
    // 状态
    //=========================================================================
    
    /** 当前选中的工具 */
    UPROPERTY()
    FGameplayTag CurrentTool;
    
    /** 当前展开的主工具（有子工具栏显示的那个） */
    UPROPERTY()
    FGameplayTag ExpandedMainTool;
    
    /** InteractionComp 引用（用于调用 SwitchTool） */
    UPROPERTY()
    TObjectPtr<UVoxelInteractionComponent> InteractionComp;
    
    /** 主工具按钮映射 */
    UPROPERTY()
    TMap<FGameplayTag, TObjectPtr<UToolIconButtonWidget>> MainToolButtons;
    
    /** 子工具按钮映射 */
    UPROPERTY()
    TMap<FGameplayTag, TObjectPtr<UToolIconButtonWidget>> SubToolButtons;

    //=========================================================================
    // 内部方法
    //=========================================================================
    
    /** 获取直接子工具标签 */
    TArray<FGameplayTag> GetDirectChildTags(FGameplayTag ParentTag) const;
    
    /** 获取父工具标签 */
    FGameplayTag GetParentTag(FGameplayTag ChildTag) const;
    
    /** 检查是否是主工具（直接挂在Root下的） */
    bool IsMainTool(FGameplayTag ToolTag) const;
    
    /** 检查是否有子工具 */
    bool HasSubTools(FGameplayTag ToolTag) const;
    
    /** 创建主工具按钮 */
    void CreateMainToolButtons(const TArray<FGameplayTag>& ToolTags);
    
    /** 创建子工具按钮 */
    void CreateSubToolButtons(FGameplayTag ParentTool);
    
    /** 清除子工具按钮 */
    void ClearSubToolButtons();
    
    /** 展开指定主工具的子工具栏 */
    void ExpandSubTools(FGameplayTag MainTool);
    
    /** 折叠子工具栏 */
    void CollapseSubTools();

    //=========================================================================
    // 事件处理
    //=========================================================================
    
    /** 处理主工具按钮点击 */
    UFUNCTION()
    void OnMainToolClicked(UToolIconButtonWidget* ClickedButton);
    
    /** 处理子工具按钮点击 */
    UFUNCTION()
    void OnSubToolClicked(UToolIconButtonWidget* ClickedButton);
    
    /** 更新按钮视觉状态 */
    void UpdateButtonVisualState(FGameplayTag SelectedTool);
    
    /** 执行工具切换 */
    void ExecuteToolSwitch(FGameplayTag ToolTag);
    
    /** 获取 InteractionComp（自动查找） */
    UVoxelInteractionComponent* GetInteractComp();

    /** 获取 HistoryComp（自动查找） */
    UVoxelControllerHistoryComponent* GetHistoryComp();

    //=========================================================================
    // 历史组（撤销/重做）
    //=========================================================================

    UFUNCTION()
    void OnUndoClicked();

    UFUNCTION()
    void OnRedoClicked();

    /** 历史栈变化时刷新按钮可用态 */
    UFUNCTION()
    void HandleHistoryChanged();

    void RefreshHistoryButtons();

    //=========================================================================
    // 动画
    //=========================================================================
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Tool Container")
    void PlaySelectAnimation(UToolIconButtonWidget* Button);
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Tool Container")
    void PlayDeselectAnimation(UToolIconButtonWidget* Button);
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Tool Container")
    void PlayExpandSubToolsAnimation();
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Tool Container")
    void PlayCollapseSubToolsAnimation();

    //=========================================================================
    // 生命周期
    //=========================================================================

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
};