// Copyright (c) 2025 zolnoor. All rights reserved.

#include "Core/UI/CategoryTabButtonWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UCategoryTabButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (RootButton)
	{
		RootButton->OnClicked.AddDynamic(this, &UCategoryTabButtonWidget::HandleRootClicked);
	}
}

void UCategoryTabButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCategoryTabButtonWidget::SetCategoryData(const FText& InLabel, UTexture2D* InIcon)
{
	if (LabelText)
	{
		LabelText->SetText(InLabel);
	}
	if (IconImage)
	{
		if (InIcon)
		{
			IconImage->SetBrushFromTexture(InIcon);
			IconImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			IconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UCategoryTabButtonWidget::SetCategory_Implementation(const FItemLibraryCategory& InCategory)
{
	Super::SetCategory_Implementation(InCategory);
	CategoryId = InCategory.CategoryId; // 兼容旧字段
	RefreshVisualFromCategory();
}

void UCategoryTabButtonWidget::SetSelected_Implementation(bool bSelected)
{
	Super::SetSelected_Implementation(bSelected);
	OnSelectedStateChanged(bSelected);
}

void UCategoryTabButtonWidget::OnSelectedStateChanged_Implementation(bool /*bNewSelected*/)
{
	// 默认空实现，蓝图覆盖
}

void UCategoryTabButtonWidget::HandleRootClicked()
{
	OnTabClicked.Broadcast(Category.CategoryId);
}

void UCategoryTabButtonWidget::RefreshVisualFromCategory()
{
	if (LabelText)
	{
		LabelText->SetText(Category.DisplayName);
	}
	// 图标：如有需要，子类蓝图可重写 SetCategory 用 TagUIDef 解析；这里不强制
}
