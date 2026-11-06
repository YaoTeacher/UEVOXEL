#include "Core/UI/Build/AreaTree/VoxelAreaTreeEntryWidget.h"
#include "Core/UI/Build/AreaTree/VoxelAreaTreeNode.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/TreeView.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Core/VoxelGameInstance.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"

void UVoxelAreaTreeEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ExpandButton)
	{
		ExpandButton->OnClicked.AddDynamic(this, &UVoxelAreaTreeEntryWidget::OnExpandClicked);
	}
	if (ItemClickButton)
	{
		ItemClickButton->OnClicked.AddDynamic(this, &UVoxelAreaTreeEntryWidget::OnItemClick);
	}
}

void UVoxelAreaTreeEntryWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UVoxelAreaTreeEntryWidget::SetIsSelected(bool bIsSelected)
{
    bIsCurrentSelected = bIsSelected;
    RefreshColor();
}

void UVoxelAreaTreeEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
    TreeNode = Cast<UVoxelAreaTreeNode>(ListItemObject);
    PopulateFromNode();
}

void UVoxelAreaTreeEntryWidget::PopulateFromNode()
{
    if (!TreeNode) return;

    // 设置区域名
    if (AreaNameText)
    {
        if (TreeNode->Area)
        {
            AreaNameText->SetText(FText::FromName(TreeNode->Area->AreaBase.AreaName));
        }
        else
        {
            AreaNameText->SetText(FText::FromString(TEXT("World Root")));
        }
    }

    // 设置区域类型（显示名统一入口，内含 TagName 兜底）
    if (AreaTypeText)
    {
        if (TreeNode->Area)
        {
            FText DisplayText = FText::GetEmpty();
            FGameplayTag TypeTag = TreeNode->Area->AreaBase.Function.AreaType;

            if (auto* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
            {
                if (auto* ConfigSys = GI->GetSubsystem<UConfigDataSubsystem>())
                {
                    DisplayText = ConfigSys->GetAreaTypeDisplayName(TypeTag);
                }
            }

            if (DisplayText.IsEmpty())
            {
                DisplayText = FText::FromName(TypeTag.GetTagName());
            }
            AreaTypeText->SetText(DisplayText);
        }
        else
        {
            AreaTypeText->SetText(FText::FromString(TEXT("Root")));
        }
    }

    RefreshExpandButton();

    // 根据深度设置缩进宽度
    if (IndentSpacer)
    {
        float Width = TreeNode->Depth * IndentWidth;
        IndentSpacer->SetWidthOverride(Width);
    }

    // 通过 PlayerController 获取当前选中区域，判断本项是否选中
    bIsCurrentSelected = false;
    if (auto* PC = GetOwningPlayer<AVoxelPlayerController>())
    {
        if (PC->StateCacheComp && TreeNode->Area == PC->StateCacheComp->GetCurrentArea())
        {
            bIsCurrentSelected = true;
        }
    }

    RefreshColor();
}

void UVoxelAreaTreeEntryWidget::RefreshExpandButton()
{
    if (!ExpandButton) return;

    // 无子节点的叶节点隐藏展开箭头
    bool bHasChildren = TreeNode && TreeNode->Children.Num() > 0;
    ExpandButton->SetVisibility(bHasChildren ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void UVoxelAreaTreeEntryWidget::RefreshColor()
{
    if (!AreaNameText) return;

    // 权限态：只读灰色，选中红色，正常白色
    if (bIsCurrentSelected)
    {
        AreaNameText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
    }
    else if (TreeNode && TreeNode->bIsReadOnly)
    {
        AreaNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
    }
    else
    {
        AreaNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    }
}

void UVoxelAreaTreeEntryWidget::OnExpandClicked()
{
    if (!TreeNode) return;

    if (UListViewBase* ListView = GetOwningListView())
    {
        if (UTreeView* TreeView = Cast<UTreeView>(ListView))
        {
            bool bExpanded = IsListItemExpanded();
            TreeView->SetItemExpansion(TreeNode, !bExpanded);
            // 必须手动刷新，SetItemExpansion 不会自动触发 UI 更新
            TreeView->RequestRefresh();
        }
    }
}

void UVoxelAreaTreeEntryWidget::OnItemClick()
{
    if (auto* PC = GetOwningPlayer<AVoxelPlayerController>())
    {
        if (PC->InteractionComp && TreeNode)
        {
            PC->InteractionComp->SelectArea(TreeNode->Area);
        }
    }
}
