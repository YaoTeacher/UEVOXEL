// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Widgets/VoxelSearchBox.h"
#include "Core/UI/Widgets/VoxelEditableTextBox.h"
#include "Core/UI/Theme/VoxelUITheme.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UVoxelSearchBox::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (InputBox && !bEventsBound)
	{
		InputBox->OnTextChanged.AddDynamic(this, &UVoxelSearchBox::HandleTextChanged);
		InputBox->OnTextCommitted.AddDynamic(this, &UVoxelSearchBox::HandleTextCommitted);
		bEventsBound = true;
	}
}

void UVoxelSearchBox::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (InputBox)
	{
		InputBox->SetHintText(HintText.IsEmpty() ? NSLOCTEXT("VoxelUI", "SearchHint", "搜索...") : HintText);
	}

	if (SearchIcon)
	{
		if (const UVoxelUITheme* Theme = UVoxelUITheme::Get())
		{
			if (UTexture2D* IconTex = Theme->SearchIconTexture.LoadSynchronous())
			{
				SearchIcon->SetBrushFromTexture(IconTex);
				SearchIcon->SetDesiredSizeOverride(FVector2D(Theme->SearchIconSize, Theme->SearchIconSize));
				SearchIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
			else
			{
				// 主题未配置图标贴图时隐藏图标位，只留输入框
				SearchIcon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void UVoxelSearchBox::SetText(const FText& InText)
{
	if (InputBox)
	{
		InputBox->SetText(InText);
	}
}

FText UVoxelSearchBox::GetText() const
{
	return InputBox ? InputBox->GetText() : FText::GetEmpty();
}

void UVoxelSearchBox::HandleTextChanged(const FText& Text)
{
	OnSearchTextChanged.Broadcast(Text);
}

void UVoxelSearchBox::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	OnSearchTextCommitted.Broadcast(Text, CommitMethod);
}
