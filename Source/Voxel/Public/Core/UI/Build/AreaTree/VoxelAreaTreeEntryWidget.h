#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "VoxelAreaTreeEntryWidget.generated.h"

class UButton;
class UTextBlock;
class UTreeView;
class USizeBox;
class UVoxelAreaTreeNode;
class AVoxelAreaBaseInstance;

/**
 * 区域树节点列表项控件
 * 用于 UTreeView 中的每一行，展示展开箭头 + 区域名/类型 + 权限颜色
 * 实现 IUserObjectListEntry 接口，UTreeView 会自动调用 NativeOnListItemObjectSet 传入 Item
 */
UCLASS()
class VOXEL_API UVoxelAreaTreeEntryWidget : public UCommonUserWidget, public IUserObjectListEntry
{
    GENERATED_BODY()

public:
    /** 设置是否为当前选中区域 */
    void SetIsSelected(bool bIsSelected);

protected:
    // === IUserObjectListEntry 接口 ===
    virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

		virtual void NativeOnInitialized() override;
		virtual void NativeConstruct() override;

    // === UI 控件绑定 ===

    /** 展开箭头按钮 */
    UPROPERTY(meta = (BindWidget))
    UButton* ExpandButton;

    /** 区域名 */
    UPROPERTY(meta = (BindWidget))
    UTextBlock* AreaNameText;

    /** 区域类型 */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* AreaTypeText;

    /** 点击选中按钮（覆盖整行） */
    UPROPERTY(meta = (BindWidget))
    UButton* ItemClickButton;

    /** 缩进占位（根据 Depth 动态调整宽度） */
    UPROPERTY(meta = (BindWidgetOptional))
    USizeBox* IndentSpacer;

    /** 每级缩进宽度（像素） */
    static constexpr float IndentWidth = 24.0f;

private:
    UPROPERTY()
    TObjectPtr<UVoxelAreaTreeNode> TreeNode;

    bool bIsCurrentSelected = false;

    UFUNCTION()
    void OnExpandClicked();

    UFUNCTION()
    void OnItemClick();

    /** 刷新展开箭头的显隐和状态 */
    void RefreshExpandButton();

    /** 刷新颜色（权限态 + 选中态） */
    void RefreshColor();

    /** 从 TreeNode 填充 UI 内容 */
    void PopulateFromNode();
};
