#include "Core/UI/Build/AreaTree/VoxelAreaTreeNode.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"

UVoxelAreaTreeNode* UVoxelAreaTreeNode::BuildTree(AVoxelAreaBaseInstance* RootArea, int32 CharId, UObject* Outer)
{
    return BuildTreeInternal(RootArea, CharId, Outer, 0);
}

UVoxelAreaTreeNode* UVoxelAreaTreeNode::BuildTreeInternal(AVoxelAreaBaseInstance* RootArea, int32 CharId, UObject* Outer, int32 Depth)
{
    if (!Outer) return nullptr;

    UVoxelAreaTreeNode* Node = NewObject<UVoxelAreaTreeNode>(Outer);
    Node->Area = RootArea;
    Node->Depth = Depth;

    // 设置权限标记
    if (RootArea)
    {
        Node->bIsReadOnly = !RootArea->IsAreaEditable(CharId);
    }

    // 收集子区域
    TArray<AVoxelAreaBaseInstance*> ChildAreas;

    if (RootArea)
    {
        // 有根区域：遍历其 SubAreasIndex
        RootArea->SubAreasIndex.GenerateValueArray(ChildAreas);
    }
    else
    {
        // 无根区域（世界根）：获取所有顶层区域（Subsystem 统一取数入口，禁止直遍 AreaIndex）
        if (UWorld* World = Outer->GetWorld())
        {
            if (UVoxelAreaWorldSubsystem* AreaSys = World->GetSubsystem<UVoxelAreaWorldSubsystem>())
            {
                ChildAreas = AreaSys->GetTopLevelAreas();
            }
        }
    }

    // 按 AreaId 排序
    ChildAreas.Sort([](const AVoxelAreaBaseInstance& A, const AVoxelAreaBaseInstance& B)
    {
        return A.AreaBase.AreaId < B.AreaBase.AreaId;
    });

    // 递归构建子节点
    for (AVoxelAreaBaseInstance* ChildArea : ChildAreas)
    {
        if (!ChildArea || !ChildArea->IsValidLowLevel()) continue;

        if (UVoxelAreaTreeNode* ChildNode = BuildTreeInternal(ChildArea, CharId, Outer, Depth + 1))
        {
            Node->Children.Add(ChildNode);
        }
    }

    return Node;
}
