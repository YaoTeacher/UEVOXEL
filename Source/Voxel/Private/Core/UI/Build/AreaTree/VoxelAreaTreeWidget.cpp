#include "Core/UI/Build/AreaTree/VoxelAreaTreeWidget.h"
#include "Core/UI/Build/AreaTree/VoxelAreaTreeNode.h"
#include "Core/UI/Build/AreaTree/VoxelAreaTreeEntryWidget.h"
#include "Components/TreeView.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"

void UVoxelAreaTreeWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (AreaTreeView)
    {
        // 用 C++ 模板方法绑定 OnGetChildren 回调
        AreaTreeView->SetOnGetItemChildren(this, &UVoxelAreaTreeWidget::OnGetChildren);
    }
}

void UVoxelAreaTreeWidget::InitializeTree(AVoxelAreaBaseInstance* RootArea, int32 CharId)
{
    if (!AreaTreeView) return;

    // 构建树数据
    RootNode = UVoxelAreaTreeNode::BuildTree(RootArea, CharId, GetWorld());
    if (!RootNode) return;

    // 清空并重新填充
    AreaTreeView->ClearListItems();
    AreaTreeView->SetListItems(TArray<UObject*>{ RootNode });

    // 展开 RootNode 本身，让它的直接子节点（顶级区域）可见
    AreaTreeView->SetItemExpansion(RootNode, true);

    // 高亮当前选中区域
    SetSelectedArea(CurrentSelected);
}

void UVoxelAreaTreeWidget::SetSelectedArea(AVoxelAreaBaseInstance* SelectedArea)
{
    CurrentSelected = SelectedArea;

    // 刷新所有可见项以更新高亮
    if (AreaTreeView)
    {
        AreaTreeView->RequestRefresh();
    }
}

void UVoxelAreaTreeWidget::OnGetChildren(UObject* Item, TArray<UObject*>& OutChildren)
{
    UVoxelAreaTreeNode* Node = Cast<UVoxelAreaTreeNode>(Item);
    if (Node)
    {
        for (UVoxelAreaTreeNode* Child : Node->Children)
        {
            OutChildren.Add(Child);
        }
    }
}

UVoxelAreaTreeNode* UVoxelAreaTreeWidget::FindNodeForArea(UVoxelAreaTreeNode* Node, AVoxelAreaBaseInstance* TargetArea)
{
    if (!Node) return nullptr;

    if (Node->Area == TargetArea) return Node;

    for (UVoxelAreaTreeNode* Child : Node->Children)
    {
        if (UVoxelAreaTreeNode* Found = FindNodeForArea(Child, TargetArea))
        {
            return Found;
        }
    }
    return nullptr;
}
