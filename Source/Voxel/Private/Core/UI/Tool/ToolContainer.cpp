// Copyright (c) 2025 zolnoor. All rights reserved.

#include "Core/UI/Tool/ToolContainer.h"
#include "Core/UI/Tool/ToolIconButtonWidget.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Components/Button.h"
#include "Core/CoreTags.h"
#include "GameplayTagsManager.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"

UToolContainer::UToolContainer(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , bPlaySelectionAnimation(true)
    , bAutoCollapseOtherSubTools(true)
{
}

void UToolContainer::NativeConstruct()
{
    Super::NativeConstruct();

    UE_LOG(LogTemp, Log, TEXT("[ToolContainer] NativeConstruct %s：MainToolList=%s SubToolList=%s SubToolBand=%s Undo=%s Redo=%s ToolButtonClass=%s"),
        *GetName(),
        MainToolList ? *MainToolList->GetName() : TEXT("NULL"),
        SubToolList ? *SubToolList->GetName() : TEXT("NULL"),
        SubToolBand ? *SubToolBand->GetName() : TEXT("NULL"),
        UndoButton ? TEXT("OK") : TEXT("NULL"),
        RedoButton ? TEXT("OK") : TEXT("NULL"),
        ToolButtonClass ? *ToolButtonClass->GetName() : TEXT("NULL"));

    if (!MainToolList)
    {
        UE_LOG(LogTemp, Error, TEXT("[ToolContainer] MainToolList 未绑定（蓝图缺少该控件或名字不符）"));
    }

    // 历史组按钮（可选绑定）
    if (UndoButton)
    {
        UndoButton->OnClicked.AddUniqueDynamic(this, &UToolContainer::OnUndoClicked);
    }
    if (RedoButton)
    {
        RedoButton->OnClicked.AddUniqueDynamic(this, &UToolContainer::OnRedoClicked);
    }
    if (UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComp())
    {
        HistoryComp->OnHistoryChanged.AddUniqueDynamic(this, &UToolContainer::HandleHistoryChanged);
    }
    RefreshHistoryButtons();
}

void UToolContainer::NativeDestruct()
{
    if (UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComp())
    {
        HistoryComp->OnHistoryChanged.RemoveDynamic(this, &UToolContainer::HandleHistoryChanged);
    }
    Super::NativeDestruct();
}

void UToolContainer::InitializeToolList(FGameplayTag RootToolTag)
{
    if (!MainToolList)
    {
        UE_LOG(LogTemp, Error, TEXT("ToolContainer: Cannot initialize - MainToolList is null"));
        return;
    }

    // 清除现有按钮
    MainToolList->ClearChildren();
    MainToolButtons.Empty();
    ClearSubToolButtons();

    // 获取根工具的所有直接子工具（主工具）
    TArray<FGameplayTag> MainTools = GetDirectChildTags(RootToolTag);

    TArray<FString> MainToolNames;
    for (const FGameplayTag& T : MainTools) { MainToolNames.Add(T.ToString()); }

    UE_LOG(LogTemp, Log, TEXT("[ToolContainer] InitializeToolList Root=%s：MainToolList=%s ToolButtonClass=%s 主工具数=%d [%s]"),
        *RootToolTag.ToString(),
        *MainToolList->GetName(),
        ToolButtonClass ? *ToolButtonClass->GetName() : TEXT("NULL"),
        MainTools.Num(),
        *FString::Join(MainToolNames, TEXT(", ")));

    if (MainTools.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[ToolContainer] Root=%s 下没有任何直接子工具标签，工具栏将为空"), *RootToolTag.ToString());
    }

    // 创建主工具按钮
    CreateMainToolButtons(MainTools);
}

void UToolContainer::SetToolButtonClass(TSubclassOf<UToolIconButtonWidget> InClass)
{
    ToolButtonClass = InClass;
}

void UToolContainer::SetInteractComp(UVoxelInteractionComponent* InInteractComp)
{
    InteractionComp = InInteractComp;
}

void UToolContainer::SyncFromState()
{
    if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
    {
        if (UVoxelPlayerStateCacheComponent* SC = PC->StateCacheComp)
        {
            FGameplayTag CurrentToolTag = SC->GetCurrentToolTag();
            
            if (!CurrentToolTag.IsValid())
            {
                return;
            }
            
            CurrentTool = CurrentToolTag;
            
            // 判断当前工具是主工具还是子工具
            if (IsMainTool(CurrentToolTag))
            {
                // 主工具被选中
                if (HasSubTools(CurrentToolTag))
                {
                    // 有子工具，展开子工具栏
                    ExpandSubTools(CurrentToolTag);
                }
                else
                {
                    // 没有子工具，折叠子工具栏
                    CollapseSubTools();
                }
            }
            else
            {
                // 子工具被选中，找到其父工具并展开
                FGameplayTag ParentTag = GetParentTag(CurrentToolTag);
                if (ParentTag.IsValid())
                {
                    ExpandSubTools(ParentTag);
                }
            }
            
            // 更新按钮视觉状态
            UpdateButtonVisualState(CurrentToolTag);
        }
    }
}

TArray<FGameplayTag> UToolContainer::GetDirectChildTags(FGameplayTag ParentTag) const
{
    TArray<FGameplayTag> DirectChildren;
    
    if (!ParentTag.IsValid())
    {
        return DirectChildren;
    }
    
    FGameplayTagContainer AllChildrenContainer = UGameplayTagsManager::Get()
        .RequestGameplayTagChildren(ParentTag);
    TArray<FGameplayTag> AllChildren = AllChildrenContainer.GetGameplayTagArray();
    
    FString ParentString = ParentTag.ToString();
    
    for (const FGameplayTag& ChildTag : AllChildren)
    {
        FString ChildString = ChildTag.GetTagName().ToString();
        
        if (ChildString.StartsWith(ParentString + "."))
        {
            FString Remainder = ChildString.RightChop(ParentString.Len() + 1);
            if (!Remainder.Contains("."))
            {
                DirectChildren.Add(ChildTag);
            }
        }
    }
    
    return DirectChildren;
}

FGameplayTag UToolContainer::GetParentTag(FGameplayTag ChildTag) const
{
    FGameplayTagContainer ParentsContainer = ChildTag.GetGameplayTagParents();
    TArray<FGameplayTag> Parents = ParentsContainer.GetGameplayTagArray();
    
    // 找到匹配的父标签（排除自身）
    for (const FGameplayTag& Parent : Parents)
    {
        if (Parent != ChildTag && Parent.MatchesTag(FCoreTags::Tool))
        {
            // 检查是否直接子标签（中间没有隔层）
            FString ChildStr = ChildTag.ToString();
            FString ParentStr = Parent.ToString();
            FString Remainder = ChildStr.RightChop(ParentStr.Len() + 1);
            if (!Remainder.Contains("."))
            {
                return Parent;
            }
        }
    }
    
    return FGameplayTag::EmptyTag;
}

bool UToolContainer::IsMainTool(FGameplayTag ToolTag) const
{
    // 主工具是直接挂在 Root 下的（只有一个"."）
    FString ToolStr = ToolTag.ToString();
    int32 DotCount = 0;
    for (TCHAR C : ToolStr)
    {
        if (C == '.') DotCount++;
    }
    return DotCount == 1;  // 如 "Tool.Build" 有1个点
}

bool UToolContainer::HasSubTools(FGameplayTag ToolTag) const
{
    TArray<FGameplayTag> SubTools = GetDirectChildTags(ToolTag);
    return SubTools.Num() > 0;
}

void UToolContainer::CreateMainToolButtons(const TArray<FGameplayTag>& ToolTags)
{
    // UToolIconButtonWidget 是 BindWidget 型类：控件树必须由蓝图资产提供，
    // C++ 原生类实例没有任何可视控件（按钮/图标全空）。未配置蓝图类必须显式报错，禁止静默回退 C++ 类。
    if (!ToolButtonClass || ToolButtonClass.Get() == UToolIconButtonWidget::StaticClass())
    {
        UE_LOG(LogTemp, Error, TEXT("[ToolContainer] ToolButtonClass 未配置或配置为 C++ 原生类（应为 WBP_ToolIcon 蓝图类），主工具按钮创建中止"));
        return;
    }

    for (const FGameplayTag& ToolTag : ToolTags)
    {
        if (UToolIconButtonWidget* Button = CreateWidget<UToolIconButtonWidget>(this, ToolButtonClass))
        {
            // 设置按钮数据
            Button->SetToolTag(ToolTag);
            // TODO: 设置图标、名称等

            // 绑定点击事件
            Button->OnToolButtonClicked.AddDynamic(this, &UToolContainer::OnMainToolClicked);

            // 添加到容器
            UPanelSlot* AddedSlot = MainToolList->AddChild(Button);
            MainToolButtons.Add(ToolTag, Button);

            UE_LOG(LogTemp, Log, TEXT("[ToolContainer] 主工具按钮创建成功 %s → Widget=%s Slot=%s 当前容器子项=%d"),
                *ToolTag.ToString(), *Button->GetName(),
                AddedSlot ? TEXT("OK") : TEXT("NULL"),
                MainToolList->GetChildrenCount());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[ToolContainer] 主工具按钮 CreateWidget 失败：Class=%s Tool=%s"), *ToolButtonClass->GetName(), *ToolTag.ToString());
        }
    }
}

void UToolContainer::CreateSubToolButtons(FGameplayTag ParentTool)
{
    if (!SubToolList)
    {
        return;
    }
    
    // 清除现有子工具按钮
    ClearSubToolButtons();
    
    // 获取子工具
    TArray<FGameplayTag> SubTools = GetDirectChildTags(ParentTool);

    // 同 CreateMainToolButtons：未配置蓝图类显式报错，禁止回退 C++ 原生类
    if (!ToolButtonClass || ToolButtonClass.Get() == UToolIconButtonWidget::StaticClass())
    {
        UE_LOG(LogTemp, Error, TEXT("[ToolContainer] ToolButtonClass 未配置或配置为 C++ 原生类（应为 WBP_ToolIcon 蓝图类），子工具按钮创建中止：Parent=%s"), *ParentTool.ToString());
        return;
    }

    for (const FGameplayTag& ToolTag : SubTools)
    {
        if (UToolIconButtonWidget* Button = CreateWidget<UToolIconButtonWidget>(this, ToolButtonClass))
        {
            // 设置按钮数据
            Button->SetToolTag(ToolTag);
            // TODO: 设置图标、名称等

            // 绑定点击事件
            Button->OnToolButtonClicked.AddDynamic(this, &UToolContainer::OnSubToolClicked);

            // 添加到容器
            UPanelSlot* AddedSlot = SubToolList->AddChild(Button);
            SubToolButtons.Add(ToolTag, Button);

            UE_LOG(LogTemp, Log, TEXT("[ToolContainer] 子工具按钮创建成功 %s（Parent=%s）→ Widget=%s Slot=%s 当前容器子项=%d"),
                *ToolTag.ToString(), *ParentTool.ToString(),
                *Button->GetName(),
                AddedSlot ? TEXT("OK") : TEXT("NULL"),
                SubToolList->GetChildrenCount());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[ToolContainer] 子工具按钮 CreateWidget 失败：Class=%s Tool=%s Parent=%s"), *ToolButtonClass->GetName(), *ToolTag.ToString(), *ParentTool.ToString());
        }
    }
}

void UToolContainer::ClearSubToolButtons()
{
    if (SubToolList)
    {
        SubToolList->ClearChildren();
    }
    SubToolButtons.Empty();
}

void UToolContainer::ExpandSubTools(FGameplayTag MainTool)
{
    if (!SubToolList)
    {
        // 没有子工具容器，直接执行工具切换
        ExecuteToolSwitch(MainTool);
        return;
    }
    
    // 如果已经展开了这个主工具，不做任何事
    if (ExpandedMainTool == MainTool)
    {
        return;
    }
    
    // 如果需要自动折叠其他子工具栏
    if (bAutoCollapseOtherSubTools && ExpandedMainTool.IsValid())
    {
        CollapseSubTools();
    }
    
    // 创建子工具按钮
    CreateSubToolButtons(MainTool);

    // 显示二级工具栏：优先切换整条 Band（含背景条，位置由蓝图槽配置）；
    // 未绑定 Band 时退回仅切换 SubToolList
    if (SubToolBand)
    {
        SubToolBand->SetVisibility(ESlateVisibility::Visible);
    }
    SubToolList->SetVisibility(ESlateVisibility::Visible);
    ExpandedMainTool = MainTool;
    
    // 播放展开动画
    if (bPlaySelectionAnimation)
    {
        PlayExpandSubToolsAnimation();
    }
    
    //UE_LOG(LogTemp, Verbose, TEXT("ToolContainer: Expanded sub tools for %s"), *MainTool.ToString());
}

void UToolContainer::CollapseSubTools()
{
    if (!SubToolList)
    {
        return;
    }
    
    // 隐藏二级工具栏：优先折叠整条 Band；未绑定 Band 时退回折叠 SubToolList
    if (SubToolBand)
    {
        SubToolBand->SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        SubToolList->SetVisibility(ESlateVisibility::Collapsed);
    }
    ClearSubToolButtons();
    ExpandedMainTool = FGameplayTag::EmptyTag;
    
    // 播放折叠动画
    if (bPlaySelectionAnimation)
    {
        PlayCollapseSubToolsAnimation();
    }
    
    //UE_LOG(LogTemp, Verbose, TEXT("ToolContainer: Collapsed sub tools"));
}

void UToolContainer::OnMainToolClicked(UToolIconButtonWidget* ClickedButton)
{
    if (!ClickedButton)
    {
        return;
    }
    
    FGameplayTag ToolTag = ClickedButton->GetToolTag();
    
    //UE_LOG(LogTemp, Log, TEXT("ToolContainer: Main tool clicked - %s"), *ToolTag.ToString());
    
    // 检查是否有子工具
    if (HasSubTools(ToolTag))
    {
        // 有子工具，展开子工具栏
        ExpandSubTools(ToolTag);
    }
    else
    {
        // 没有子工具，直接执行工具切换
        ExecuteToolSwitch(ToolTag);
        
        // 折叠子工具栏（如果有）
        if (ExpandedMainTool.IsValid())
        {
            CollapseSubTools();
        }
    }
    
    // 更新视觉状态
    UpdateButtonVisualState(ToolTag);
}

void UToolContainer::OnSubToolClicked(UToolIconButtonWidget* ClickedButton)
{
    if (!ClickedButton)
    {
        return;
    }
    
    FGameplayTag ToolTag = ClickedButton->GetToolTag();
    
    //UE_LOG(LogTemp, Log, TEXT("ToolContainer: Sub tool clicked - %s"), *ToolTag.ToString());
    
    // 执行工具切换
    ExecuteToolSwitch(ToolTag);
    
    // 更新视觉状态
    UpdateButtonVisualState(ToolTag);
}

void UToolContainer::UpdateButtonVisualState(FGameplayTag SelectedTool)
{
    CurrentTool = SelectedTool;
    
    // 更新主工具按钮状态
    for (auto& Pair : MainToolButtons)
    {
        if (UToolIconButtonWidget* Button = Pair.Value)
        {
            bool bIsSelected = (Pair.Key == SelectedTool);
            Button->SetSelected(bIsSelected);
            
            if (bIsSelected && bPlaySelectionAnimation)
            {
                PlaySelectAnimation(Button);
            }
            else if (!bIsSelected && bPlaySelectionAnimation)
            {
                PlayDeselectAnimation(Button);
            }
        }
    }
    
    // 更新子工具按钮状态
    for (auto& Pair : SubToolButtons)
    {
        if (UToolIconButtonWidget* Button = Pair.Value)
        {
            bool bIsSelected = (Pair.Key == SelectedTool);
            Button->SetSelected(bIsSelected);
            
            if (bIsSelected && bPlaySelectionAnimation)
            {
                PlaySelectAnimation(Button);
            }
            else if (!bIsSelected && bPlaySelectionAnimation)
            {
                PlayDeselectAnimation(Button);
            }
        }
    }
}

void UToolContainer::ExecuteToolSwitch(FGameplayTag ToolTag)
{
    if (UVoxelInteractionComponent* IC = GetInteractComp())
    {
        IC->SwitchTool(ToolTag);
        //UE_LOG(LogTemp, Log, TEXT("ToolContainer: Switched to tool %s"), *ToolTag.ToString());
    }
    else
    {
        //UE_LOG(LogTemp, Warning, TEXT("ToolContainer: Cannot switch tool - InteractionComp is null"));
    }
}

UVoxelInteractionComponent* UToolContainer::GetInteractComp()
{
    // 如果已设置，直接返回
    if (InteractionComp)
    {
        return InteractionComp;
    }
    
    // 尝试从 OwningPlayer 获取
    if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
    {
        if (PC->InteractionComp)
        {
            InteractionComp = PC->InteractionComp;
            return InteractionComp;
        }
    }
    
    return nullptr;
}

bool UToolContainer::HasSubToolsVisible() const
{
    if (!ExpandedMainTool.IsValid())
    {
        return false;
    }
    if (SubToolBand)
    {
        return SubToolBand->GetVisibility() == ESlateVisibility::Visible;
    }
    return SubToolList && SubToolList->GetVisibility() == ESlateVisibility::Visible;
}

UVoxelControllerHistoryComponent* UToolContainer::GetHistoryComp()
{
    if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
    {
        return PC->HistoryComp ? PC->HistoryComp.Get() : PC->FindComponentByClass<UVoxelControllerHistoryComponent>();
    }
    return nullptr;
}

void UToolContainer::OnUndoClicked()
{
    if (UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComp())
    {
        HistoryComp->Undo();
    }
}

void UToolContainer::OnRedoClicked()
{
    if (UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComp())
    {
        HistoryComp->Redo();
    }
}

void UToolContainer::HandleHistoryChanged()
{
    RefreshHistoryButtons();
}

void UToolContainer::RefreshHistoryButtons()
{
    if (UVoxelControllerHistoryComponent* HistoryComp = GetHistoryComp())
    {
        if (UndoButton)
        {
            UndoButton->SetIsEnabled(HistoryComp->CanUndo());
        }
        if (RedoButton)
        {
            RedoButton->SetIsEnabled(HistoryComp->CanRedo());
        }
    }
}
