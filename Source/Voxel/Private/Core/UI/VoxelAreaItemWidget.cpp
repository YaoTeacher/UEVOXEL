// Fill out your copyright notice in the Description page of Project Settings.
#include "VoxelAreaItemWidget.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/VoxelGameInstance.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"

void UVoxelAreaItemWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (ClickButton)
	{
		ClickButton->OnClicked.AddDynamic(this, &UVoxelAreaItemWidget::OnBtnClicked);
	}
}

void UVoxelAreaItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UVoxelAreaItemWidget::Setup(AVoxelAreaBaseInstance* InArea, bool bIsCurrentSelection)
{
	TargetArea = InArea;

	// 1. 设置名称
	if (NameText)
	{
		if (TargetArea)
		{
			NameText->SetText(FText::FromName(TargetArea->AreaBase.AreaName));
		}
		else
		{
			NameText->SetText(FText::FromString(TEXT("World Root"))); // 根节点
		}
	}

	// 2. 设置 ID (如果有绑定的话)
	if (IdText && TargetArea)
	{
		IdText->SetText(FText::AsNumber(TargetArea->AreaBase.AreaId));
	}
	// 2. 设置 ID (如果有绑定的话)
	if (TypeText && TargetArea)
	{
		FText DisplayText = FText::GetEmpty();
		FGameplayTag TypeTag = TargetArea->AreaBase.Function.AreaType;

		// 显示名统一入口（内含 TagName 兜底）
		if (auto* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
		{
			if (auto* ConfigSys = GI->GetSubsystem<UConfigDataSubsystem>())
			{
				DisplayText = ConfigSys->GetAreaTypeDisplayName(TypeTag);
			}
		}

		// [Fallback] Subsystem 不可用时显示 Tag 字符串，避免显示空白
		if (DisplayText.IsEmpty())
		{
			// 显示为 "Area.Type.Shop" 这种原始格式
			DisplayText = FText::FromName(TypeTag.GetTagName());
		}

		TypeText->SetText(DisplayText);
	}
	else if (!TargetArea)
	{
		TypeText->SetText(FText::FromString(TEXT("World Root"))); // 根节点
	}
	// 3. 处理选中状态 (变色或禁用点击)
	if (ClickButton)
	{
		// 当前项不再禁用点击（支持双击展开下属），改为视觉区分由子类/蓝图高亮处理
		ClickButton->SetIsEnabled(true);
	}
}

void UVoxelAreaItemWidget::OnBtnClicked()
{
	if (auto* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		// 双击识别：同一目标 0.3s 内第二次点击 → 广播双击（单击语义不变，先切区域再展开）
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		const bool bIsDoubleClick = (TargetArea == LastClickedArea) && (Now - LastClickTime >= 0.0) && (Now - LastClickTime < 0.3);
		LastClickedArea = TargetArea;
		LastClickTime = Now;

		if (bIsDoubleClick)
		{
			OnAreaItemDoubleClicked.Broadcast(TargetArea);
		}

		// 调用你现有的请求逻辑
		// TargetArea 为 nullptr 时，SelectArea 内部应该处理为"返回根节点"
		PC->InteractionComp->SelectArea(TargetArea);
	}
}

