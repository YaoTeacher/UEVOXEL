#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "VoxelAreaNavigatorWidget.generated.h"

class UButton;
class UPanelWidget;
class UTreeView;
class UVoxelAreaItemWidget;
class UVoxelAreaTreeWidget;
class UVoxelPlayerStateCacheComponent;
class AVoxelAreaBaseInstance;

/**
 * 区域导航器组件
 * 封装面包屑导航 + 区域树视图，通过展开按钮切换两种模式
 *
 * 默认面包屑模式（水平路径），点击展开按钮切换为区域树视图
 * 面包屑交互：单击切换区域；双击当前级展开下属区域列表
 */
UCLASS()
class VOXEL_API UVoxelAreaNavigatorWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    /** 从 StateComponent 刷新导航器（内部缓存 SC 引用） */
    void Refresh(UVoxelPlayerStateCacheComponent* SC);

    /** 设置面包屑样式类（由外部面板传入） */
    void SetBreadcrumbItemClass(TSubclassOf<UVoxelAreaItemWidget> ItemClass);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    // ===== 面包屑模式 =====

    /** 面包屑容器 */
    UPROPERTY(meta = (BindWidget))
    UPanelWidget* BreadcrumbContainer;

    /** 下属区域列表容器（双击展开，WBP 绑定） */
    UPROPERTY(meta = (BindWidget, OptionalWidget))
    UPanelWidget* SubAreaContainer;

    /** 面包屑单项的样式类 */
    UPROPERTY(EditDefaultsOnly, Category = "UI|Class")
    TSubclassOf<UVoxelAreaItemWidget> BreadcrumbItemClass;

    // ===== 树视图模式 =====

    /** 区域树组件 */
    UPROPERTY(meta = (BindWidget))
    UVoxelAreaTreeWidget* AreaTreeWidget;

    // ===== 切换按钮 =====

    /** 展开/收起按钮 */
    UPROPERTY(meta = (BindWidget))
    UButton* ExpandToggleButton;

    // ===== 状态 =====

    UPROPERTY()
    TObjectPtr<UVoxelPlayerStateCacheComponent> CachedStateComponent;

    bool bTreeMode = false;

    // ===== 内部方法 =====

    UFUNCTION()
    void OnExpandToggleClicked();

    /** 刷新面包屑（从 BuildPanelWidget::UpdateBreadcrumbs 迁移） */
    void UpdateBreadcrumbs(UVoxelPlayerStateCacheComponent* SC);

    /** 刷新树视图 */
    void UpdateTree(UVoxelPlayerStateCacheComponent* SC);

    /** 面包屑双击：展开/收起下属区域列表 */
    UFUNCTION()
    void HandleBreadcrumbDoubleClicked(AVoxelAreaBaseInstance* Area);

    /** 生成下属区域列表项并点击切换 */
    void RefreshSubAreaList(AVoxelAreaBaseInstance* Area);
};
