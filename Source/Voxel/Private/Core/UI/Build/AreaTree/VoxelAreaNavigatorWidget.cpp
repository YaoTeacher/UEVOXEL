#include "Core/UI/Build/AreaTree/VoxelAreaNavigatorWidget.h"
#include "Core/UI/Build/AreaTree/VoxelAreaTreeWidget.h"
#include "Core/UI/VoxelAreaItemWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Core/VoxelGameInstance.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"

void UVoxelAreaNavigatorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ExpandToggleButton)
	{
		ExpandToggleButton->OnClicked.AddDynamic(this, &UVoxelAreaNavigatorWidget::OnExpandToggleClicked);
	}
}

void UVoxelAreaNavigatorWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 默认面包屑模式，隐藏树视图
    if (AreaTreeWidget)
    {
        AreaTreeWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (SubAreaContainer)
    {
        SubAreaContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UVoxelAreaNavigatorWidget::Refresh(UVoxelPlayerStateCacheComponent* SC)
{
    CachedStateComponent = SC;
    if (!SC) return;

    if (bTreeMode)
    {
        UpdateTree(SC);
    }
    else
    {
        UpdateBreadcrumbs(SC);
    }

    // 刷新时收起下属列表，避免残留
    if (SubAreaContainer)
    {
        SubAreaContainer->ClearChildren();
        SubAreaContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UVoxelAreaNavigatorWidget::SetBreadcrumbItemClass(TSubclassOf<UVoxelAreaItemWidget> ItemClass)
{
    BreadcrumbItemClass = ItemClass;
}

void UVoxelAreaNavigatorWidget::OnExpandToggleClicked()
{
    bTreeMode = !bTreeMode;

    if (bTreeMode)
    {
        if (BreadcrumbContainer)
        {
            BreadcrumbContainer->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (SubAreaContainer)
        {
            SubAreaContainer->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (AreaTreeWidget)
        {
            AreaTreeWidget->SetVisibility(ESlateVisibility::Visible);
        }
    }
    else
    {
        if (BreadcrumbContainer)
        {
            BreadcrumbContainer->SetVisibility(ESlateVisibility::Visible);
        }
        if (AreaTreeWidget)
        {
            AreaTreeWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (CachedStateComponent)
    {
        Refresh(CachedStateComponent);
    }
}

void UVoxelAreaNavigatorWidget::UpdateBreadcrumbs(UVoxelPlayerStateCacheComponent* SC)
{
    if (!BreadcrumbContainer || !BreadcrumbItemClass || !SC) return;

    BreadcrumbContainer->ClearChildren();

    TArray<AVoxelAreaBaseInstance*> Path;

    AVoxelAreaBaseInstance* ContextIter = SC->GetCurrentParentArea();
    while (ContextIter)
    {
        Path.Insert(ContextIter, 0);
        ContextIter = ContextIter->ParentArea;
    }

    if (AVoxelAreaBaseInstance* Selection = SC->GetCurrentArea())
    {
        Path.Add(Selection);
    }

    bool bIsRootSelected = (Path.Num() == 0);

    if (UVoxelAreaItemWidget* HomeItem = CreateWidget<UVoxelAreaItemWidget>(GetOwningPlayer(), BreadcrumbItemClass))
    {
        HomeItem->Setup(nullptr, bIsRootSelected);
        HomeItem->OnAreaItemDoubleClicked.AddDynamic(this, &UVoxelAreaNavigatorWidget::HandleBreadcrumbDoubleClicked);
        BreadcrumbContainer->AddChild(HomeItem);
    }

    for (int32 i = 0; i < Path.Num(); i++)
    {
        if (UVoxelAreaItemWidget* Item = CreateWidget<UVoxelAreaItemWidget>(GetOwningPlayer(), BreadcrumbItemClass))
        {
            bool bIsCurrent = (i == Path.Num() - 1);
            Item->Setup(Path[i], bIsCurrent);
            Item->OnAreaItemDoubleClicked.AddDynamic(this, &UVoxelAreaNavigatorWidget::HandleBreadcrumbDoubleClicked);
            BreadcrumbContainer->AddChild(Item);
        }
    }
}

void UVoxelAreaNavigatorWidget::HandleBreadcrumbDoubleClicked(AVoxelAreaBaseInstance* Area)
{
    // 若已展开且是同一区域 → 收起
    if (SubAreaContainer && SubAreaContainer->IsVisible() && SubAreaContainer->GetChildrenCount() > 0)
    {
        // 记录当前展开目标的简易判定：收起后重开
        SubAreaContainer->ClearChildren();
        SubAreaContainer->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    RefreshSubAreaList(Area);
}

void UVoxelAreaNavigatorWidget::RefreshSubAreaList(AVoxelAreaBaseInstance* Area)
{
    if (!SubAreaContainer || !BreadcrumbItemClass) return;

    SubAreaContainer->ClearChildren();

    // 收集直属子区域
    TArray<AVoxelAreaBaseInstance*> Children;
    if (Area)
    {
        Area->SubAreasIndex.GenerateValueArray(Children);
    }
    else
    {
        // 世界根：取 AreaIndex 中 ParentArea == nullptr 的顶级区域
        if (auto* PC = GetOwningPlayer<AVoxelPlayerController>())
        {
            if (UVoxelAreaWorldSubsystem* AreaSys = PC->GetAreaWorldSubsystem())
            {
                for (const auto& Pair : AreaSys->AreaIndex)
                {
                    if (Pair.Value && Pair.Value->ParentArea == nullptr)
                    {
                        Children.Add(Pair.Value);
                    }
                }
            }
        }
    }

    Children.Sort([](const AVoxelAreaBaseInstance& A, const AVoxelAreaBaseInstance& B)
    {
        return A.AreaBase.AreaId < B.AreaBase.AreaId;
    });

    if (Children.Num() == 0)
    {
        SubAreaContainer->SetVisibility(ESlateVisibility::Visible);
        return;
    }

    for (AVoxelAreaBaseInstance* Child : Children)
    {
        if (UVoxelAreaItemWidget* Item = CreateWidget<UVoxelAreaItemWidget>(GetOwningPlayer(), BreadcrumbItemClass))
        {
            Item->Setup(Child, false);
            SubAreaContainer->AddChild(Item);
        }
    }
    SubAreaContainer->SetVisibility(ESlateVisibility::Visible);
}

void UVoxelAreaNavigatorWidget::UpdateTree(UVoxelPlayerStateCacheComponent* SC)
{
    if (!AreaTreeWidget || !SC) return;

    // CharId 取值统一入口（取不到时 -1 = 未指派，整树只读）
    const int32 CharId = UVoxelGameInstance::GetMainCharacterIdSafe(this);

    AreaTreeWidget->InitializeTree(nullptr, CharId);
    AreaTreeWidget->SetSelectedArea(SC->GetCurrentArea());
}
