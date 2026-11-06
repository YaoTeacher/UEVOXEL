// Copyright (c) 2025 zolnoor. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "ToolContainerDelegates.generated.h"

// 工具被选中
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToolSelected, FGameplayTag, SelectedTool);

// 子工具被选中
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSubToolSelected, FGameplayTag, ParentTool, FGameplayTag, SubTool);

// 工具悬停（预览）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnToolHovered, FGameplayTag, ToolTag, bool, bHovered);

// 模块请求更新数据
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModuleRequestUpdate, UObject*, StateProvider);

// 模块显隐变化
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnModuleVisibilityChanged, class UToolDetailModuleBase*, Module, ESlateVisibility, NewVisibility);

// 工具容器事件集合（可选，用于批量绑定）
USTRUCT(BlueprintType)
struct FToolContainerEvents
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintAssignable)
    FOnToolSelected OnToolSelected;
    
    UPROPERTY(BlueprintAssignable)
    FOnToolHovered OnToolHovered;
};