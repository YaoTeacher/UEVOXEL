// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"

void UToolDetailModuleBase::NativeConstruct()
{
	Super::NativeConstruct();
	//UE_LOG(LogTemp, Log, TEXT("[UIChain] ToolDetailModuleBase::NativeConstruct: %s, class=%s, root visibility=%s, desiredSize=%s"),
	//    *GetName(), *GetClass()->GetName(),
	//    *UEnum::GetValueAsString(GetVisibility()), *GetDesiredSize().ToString());
	// 子类可以在此绑定 UI 控件和事件
}

void UToolDetailModuleBase::NativeDestruct()
{
	Super::NativeDestruct();
	// 子类可以在此清理委托和资源
}

bool UToolDetailModuleBase::ShouldShowForTool(FGameplayTag ToolTag) const
{
	// 默认实现: 不显示任何工具
	// 子类应该重写此方法实现具体的显示逻辑
	//UE_LOG(LogTemp, Log, TEXT("[UIChain] ToolDetailModuleBase::ShouldShowForTool(%s) default false for %s"), *ToolTag.ToString(), *GetName());
	return false;
}

void UToolDetailModuleBase::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	// 默认实现: 空操作
	// 子类应该重写此方法从 StateComponent 读取数据并更新 UI

	// 保存 StateComponent 引用 (由 BuildPanelWidget 设置)
	StateComponent = SC;
}
