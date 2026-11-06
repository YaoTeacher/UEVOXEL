#include "Core/UI/HUD/LayerSwitchSectionWidget.h"
#include "Core/UI/Tool/ToolIconButtonWidget.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/CoreTags.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "GameplayTagsManager.h"
#include "Voxel/VoxelPlayerController.h"

void ULayerSwitchSectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CachedSC = GetStateComponent();
	if (!CachedSC) return;

	CachedSC->OnLayersChanged.AddDynamic(this, &ULayerSwitchSectionWidget::OnLayersChanged);

	if (CollapseButton)
	{
		CollapseButton->OnClicked.AddDynamic(this, &ULayerSwitchSectionWidget::OnCollapseClicked);
	}
	if (FoldedButton)
	{
		FoldedButton->OnClicked.AddDynamic(this, &ULayerSwitchSectionWidget::OnFoldedClicked);
	}

	// 默认根标签：Voxel.Layer
	if (!RootTag.IsValid())
	{
		RootTag = FCoreTags::Layer;
	}
	InitializeLayers(RootTag);
	RefreshSection();
}

void ULayerSwitchSectionWidget::NativeDestruct()
{
	if (CachedSC)
	{
		CachedSC->OnLayersChanged.RemoveDynamic(this, &ULayerSwitchSectionWidget::OnLayersChanged);
	}
	Super::NativeDestruct();
}

void ULayerSwitchSectionWidget::InitializeLayers(FGameplayTag InRootTag)
{
	RootTag = InRootTag;
	if (!LayerIconRow) return;

	// 配置兜底：蓝图 CDO 未配置时加载默认资产（不依赖蓝图手动设置）。
	// UToolIconButtonWidget 是 BindWidget 型类，C++ 原生类实例无任何可视控件——加载失败必须显式报错，禁止静默回退 C++ 类。
	if (!IconButtonClass)
	{
		IconButtonClass = TSoftClassPtr<UToolIconButtonWidget>(FSoftClassPath(TEXT("/Game/Resource/UI/BuildModeUI/WBP_ToolIcon.WBP_ToolIcon_C"))).LoadSynchronous();
		if (!IconButtonClass)
		{
			UE_LOG(LogTemp, Error, TEXT("[LayerSwitch] IconButtonClass 未配置且 WBP_ToolIcon 加载失败，层级图标按钮创建中止"));
		}
	}
	if (LayerDetailClasses.Num() == 0)
	{
		if (UClass* CutoutPanelClass = TSoftClassPtr<UUserWidget>(FSoftClassPath(TEXT("/Game/Resource/UI/Cutout/WBP_CutoutBoxListPanel.WBP_CutoutBoxListPanel_C"))).LoadSynchronous())
		{
			LayerDetailClasses.Add(FCoreTags::Layer_Cutout, CutoutPanelClass);
		}
	}

	LayerIconRow->ClearChildren();
	LayerButtons.Empty();

	// 遍历直接子标签（同 ToolContainer::GetDirectChildTags 思路）
	FGameplayTagContainer ChildTagContainer = UGameplayTagsManager::Get().RequestGameplayTagChildren(RootTag);
	TArray<FGameplayTag> ChildTags;
	ChildTagContainer.GetGameplayTagArray(ChildTags);
	ChildTags.Sort([&RootTag = RootTag](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.ToString() < B.ToString();
	});

	// 只取直接子级（不含孙级）
	TArray<FGameplayTag> DirectChildren;
	for (const FGameplayTag& Tag : ChildTags)
	{
		if (Tag.RequestDirectParent() == RootTag)
		{
			DirectChildren.Add(Tag);
		}
	}

	for (const FGameplayTag& Tag : DirectChildren)
	{
		if (!IconButtonClass) break;
		if (UToolIconButtonWidget* Btn = CreateWidget<UToolIconButtonWidget>(GetOwningPlayer(), IconButtonClass))
		{
			Btn->SetToolTag(Tag);
			Btn->OnToolButtonClicked.AddDynamic(this, &ULayerSwitchSectionWidget::OnLayerIconClicked);
			LayerIconRow->AddChildToHorizontalBox(Btn);
			LayerButtons.Add(Tag, Btn);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[LayerSwitch] 层级图标 CreateWidget 失败：Class=%s Tag=%s"), *IconButtonClass->GetName(), *Tag.ToString());
		}
	}
}

void ULayerSwitchSectionWidget::OnLayerIconClicked(UToolIconButtonWidget* Button)
{
	if (!Button || !CachedSC) return;
	const FGameplayTag Tag = Button->GetToolTag();
	if (UVoxelInteractionComponent* InteractionComp = GetToolComponent())
	{
		InteractionComp->SetLayerActive(Tag, !CachedSC->IsLayerActive(Tag));
	}
}

void ULayerSwitchSectionWidget::OnCollapseClicked()
{
	bCollapsed = true;
	RefreshSection();
}

void ULayerSwitchSectionWidget::OnFoldedClicked()
{
	bCollapsed = false;
	RefreshSection();
}

void ULayerSwitchSectionWidget::OnLayersChanged()
{
	RefreshSection();
}

UVoxelPlayerStateCacheComponent* ULayerSwitchSectionWidget::GetStateComponent() const
{
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		return PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
	}
	return nullptr;
}

UVoxelInteractionComponent* ULayerSwitchSectionWidget::GetToolComponent() const
{
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		return PC->InteractionComp;
	}
	return nullptr;
}

void ULayerSwitchSectionWidget::RefreshSection()
{
	if (!CachedSC) return;

	// 折叠态：隐藏图标行与详情，显示折叠按钮
	if (LayerIconRow)
	{
		LayerIconRow->SetVisibility(bCollapsed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (DetailContainer)
	{
		DetailContainer->SetVisibility(bCollapsed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (CollapseButton)
	{
		CollapseButton->SetVisibility(bCollapsed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (FoldedButton)
	{
		FoldedButton->SetVisibility(bCollapsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (bCollapsed)
	{
		return;
	}

	// 同步图标激活态
	for (const auto& Pair : LayerButtons)
	{
		if (UToolIconButtonWidget* Btn = Pair.Value)
		{
			Btn->SetSelected(CachedSC->IsLayerActive(Pair.Key));
		}
	}

	RebuildDetails();
}

void ULayerSwitchSectionWidget::RebuildDetails()
{
	if (!DetailContainer) return;

	DetailContainer->ClearChildren();

	const TSet<FGameplayTag>& Active = CachedSC->GetActiveLayers();
	for (const FGameplayTag& Tag : Active)
	{
		if (const TSubclassOf<UUserWidget>* DetailClass = LayerDetailClasses.Find(Tag))
		{
			if (*DetailClass)
			{
				if (UUserWidget* Detail = CreateWidget<UUserWidget>(this, *DetailClass))
				{
					DetailContainer->AddChildToVerticalBox(Detail);
				}
			}
		}
	}
}
