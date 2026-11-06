#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "VoxelAreaTreeWidget.generated.h"

class UTreeView;
class UVoxelAreaTreeNode;
class UVoxelAreaTreeEntryWidget;
class AVoxelAreaBaseInstance;

/**
 * 通用区域树容器
 * 封装 UTreeView + 数据填充 + 展开控制
 * 两处复用：全局导航器 + M13a 下属树
 */
UCLASS()
class VOXEL_API UVoxelAreaTreeWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    /**
     * 初始化树：指定根区域，构建并填充 TreeView
     * @param RootArea 根区域，nullptr 表示从世界根开始
     * @param CharId 当前角色 ID，用于权限判定
     * 默认展开根节点的直接子节点（第一级），其他层级折叠
     */
    void InitializeTree(AVoxelAreaBaseInstance* RootArea, int32 CharId);

    /** 高亮指定区域（当前选中区域） */
    void SetSelectedArea(AVoxelAreaBaseInstance* SelectedArea);

protected:
    virtual void NativeConstruct() override;

    /** UTreeView 控件 */
    UPROPERTY(meta = (BindWidget))
    UTreeView* AreaTreeView;

    /** 列表项样式类 */
    UPROPERTY(EditDefaultsOnly, Category = "AreaTree")
    TSubclassOf<UVoxelAreaTreeEntryWidget> EntryWidgetClass;

private:
    UPROPERTY()
    TObjectPtr<UVoxelAreaTreeNode> RootNode;

    UPROPERTY()
    TObjectPtr<AVoxelAreaBaseInstance> CurrentSelected;

    /** UTreeView 获取子节点回调（由 SetOnGetItemChildren 绑定） */
    void OnGetChildren(UObject* Item, TArray<UObject*>& OutChildren);

    /** 在树中查找指定 Area 对应的节点 */
    UVoxelAreaTreeNode* FindNodeForArea(UVoxelAreaTreeNode* Node, AVoxelAreaBaseInstance* TargetArea);
};
