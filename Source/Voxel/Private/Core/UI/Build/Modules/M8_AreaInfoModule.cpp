// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/Modules/M8_AreaInfoModule.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/CoreTags.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Core/UI/VoxelAreaItemWidget.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"
#include "Area/VoxelAreaBaseInstance.h"

bool UM8_AreaInfoModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	// 显示条件: 区域编辑工具 (Tool.AreaEdit.*)
	static const FGameplayTag AreaEditTag = FCoreTags::Tool_AreaEdit;
	return ToolTag.MatchesTag(AreaEditTag);
}

void UM8_AreaInfoModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC)
	{
		return;
	}

	// 保存 StateComponent 引用
	StateComponent = SC;

	// 从 StateComponent 获取当前区域信息
	FString AreaName = TEXT("未选择区域");
	FString AreaType = TEXT("-");
	FString AreaOwner = TEXT("-");
	FString AreaStatus = TEXT("-");

	if (AVoxelAreaBaseInstance* CurrentArea = SC->GetCurrentArea())
	{
		AreaName = CurrentArea->AreaBase.AreaName.ToString();
		AreaType = CurrentArea->AreaBase.Function.AreaType.ToString();
		AreaOwner = FString::Printf(TEXT("%d"), CurrentArea->AreaBase.OwnerCreatureId);

		// 状态枚举转文本
		if (const UEnum* EnumPtr = StaticEnum<EAreaStatus>())
		{
			AreaStatus = EnumPtr->GetDisplayNameTextByValue((int64)CurrentArea->AreaBase.Function.Status).ToString();
		}
	}

	// 更新区域基本信息显示
	if (AreaNameText)
	{
		AreaNameText->SetText(FText::FromString(AreaName));
	}

	if (AreaTypeText)
	{
		AreaTypeText->SetText(FText::FromString(AreaType));
	}

	if (AreaOwnerText)
	{
		AreaOwnerText->SetText(FText::FromString(AreaOwner));
	}

	if (AreaStatusText)
	{
		AreaStatusText->SetText(FText::FromString(AreaStatus));
	}

	// 更新子区域列表
	UpdateSubAreaList();
}

void UM8_AreaInfoModule::NativeConstruct()
{
	Super::NativeConstruct();

	// M8 是纯显示模块,不需要绑定事件
}

void UM8_AreaInfoModule::UpdateSubAreaList()
{
	if (!SubAreaListContainer)
	{
		return;
	}

	SubAreaListContainer->ClearChildren();

	if (!SubAreaItemClass)
	{
		// 如果没有设置子列表项类，显示占位符
		if (UTextBlock* Placeholder = NewObject<UTextBlock>(this))
		{
			Placeholder->SetText(FText::FromString(TEXT("未设置 SubAreaItemClass")));
			SubAreaListContainer->AddChild(Placeholder);
		}
		return;
	}

	// ============================================================
	// [从 AreaModifyWidget 迁移的完整逻辑]
	// ============================================================

	if (!StateComponent)
	{
		return;
	}

	AVoxelAreaBaseInstance* Current = nullptr;

	if (StateComponent->GetCurrentArea())
	{
		// 优先级 1: 选中了具体区域
		Current = StateComponent->GetCurrentArea();
	}
	else if (StateComponent->GetCurrentParentArea())
	{
		// 优先级 2: 没选中，但处于某个父级下
		Current = StateComponent->GetCurrentParentArea();
	}
	else
	{
		Current = nullptr;
	}

	TArray<AVoxelAreaBaseInstance*> AreasToShow;

	if (Current)
	{
		// 情况 A: 当前选中了区域，显示该区域下的子区域
		Current->SubAreasIndex.GenerateValueArray(AreasToShow);
	}
	else
	{
		// 情况 B: 当前在世界根节点 (Current == nullptr)，显示所有顶层区域（Subsystem 统一取数入口）
		if (UWorld* World = GetWorld())
		{
			if (UVoxelAreaWorldSubsystem* AreaSys = World->GetSubsystem<UVoxelAreaWorldSubsystem>())
			{
				AreasToShow = AreaSys->GetTopLevelAreas();
			}
		}
	}

	// 排序：按 ID 从小到大
	AreasToShow.Sort([](const AVoxelAreaBaseInstance& A, const AVoxelAreaBaseInstance& B) {
		return A.AreaBase.AreaId < B.AreaBase.AreaId;
	});

	// 生成列表项
	for (AVoxelAreaBaseInstance* ChildArea : AreasToShow)
	{
		if (!ChildArea || !ChildArea->IsValidLowLevel()) continue;

		if (UVoxelAreaItemWidget* ListItem = CreateWidget<UVoxelAreaItemWidget>(GetOwningPlayer(), SubAreaItemClass))
		{
			// Setup(Area, false) - 列表项可点击
			ListItem->Setup(ChildArea, false);
			SubAreaListContainer->AddChild(ListItem);
		}
	}
}
