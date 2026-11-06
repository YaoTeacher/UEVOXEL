#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VoxelAreaTreeNode.generated.h"

class AVoxelAreaBaseInstance;

/**
 * 区域树节点数据（UObject）
 * 持有 Area 指针 + 子节点数组 + 权限标记，供 UTreeView 使用
 */
UCLASS(BlueprintType)
class UVoxelAreaTreeNode : public UObject
{
    GENERATED_BODY()

public:
    /** 持有的区域指针（nullptr = 世界根节点） */
    UPROPERTY(BlueprintReadOnly, Category = "AreaTree")
    AVoxelAreaBaseInstance* Area = nullptr;

    /** 子节点 */
    UPROPERTY(BlueprintReadOnly, Category = "AreaTree")
    TArray<UVoxelAreaTreeNode*> Children;

    /** 无权限标记（灰色只读） */
    UPROPERTY(BlueprintReadOnly, Category = "AreaTree")
    bool bIsReadOnly = false;

    /** 树深度（0 = 根节点） */
    UPROPERTY(BlueprintReadOnly, Category = "AreaTree")
    int32 Depth = 0;

    /**
     * 从指定父区域递归构建子树
     * @param RootArea 根区域，nullptr 表示从世界根开始（全局树）
     * @param CharId 当前角色 ID，用于权限判定（IsAreaEditable）
     * @param Outer UObject 外部拥有者
     * @return 构建好的根节点
     */
    UFUNCTION(BlueprintCallable, Category = "AreaTree")
    static UVoxelAreaTreeNode* BuildTree(AVoxelAreaBaseInstance* RootArea, int32 CharId, UObject* Outer);

private:
    /** 内部递归构建，传入当前深度 */
    static UVoxelAreaTreeNode* BuildTreeInternal(AVoxelAreaBaseInstance* RootArea, int32 CharId, UObject* Outer, int32 Depth);
};
