// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Widgets/VoxelInputRow.h"
#include "Core/UI/Widgets/VoxelTextBlock.h"
#include "Core/UI/Widgets/VoxelEditableTextBox.h"
#include "Core/UI/Theme/VoxelUITheme.h"

void UVoxelInputRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (InputBox && !bEventsBound)
	{
		InputBox->OnTextCommitted.AddDynamic(this, &UVoxelInputRow::HandleTextCommitted);
		bEventsBound = true;
	}
}

void UVoxelInputRow::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (LabelText)
	{
		LabelText->SetTextRole(EVoxelTextRole::ItemLabel);
		LabelText->SetText(Label);
	}
	if (InputBox && !HintText.IsEmpty())
	{
		InputBox->SetHintText(HintText);
	}

	// 布局令牌：标签列宽跨模块对齐（统一改尺寸只改主题资产）
	if (const UVoxelUITheme* Theme = UVoxelUITheme::Get())
	{
		if (LabelText)
		{
			LabelText->SetMinDesiredWidth(Theme->Layout.LabelWidth);
		}
	}
}

void UVoxelInputRow::SetText(const FText& InText)
{
	if (InputBox)
	{
		InputBox->SetText(InText);
	}
}

FText UVoxelInputRow::GetText() const
{
	return InputBox ? InputBox->GetText() : FText::GetEmpty();
}

void UVoxelInputRow::SetLabelText(const FText& InText)
{
	Label = InText;
	if (LabelText)
	{
		LabelText->SetText(InText);
	}
}

void UVoxelInputRow::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	OnTextCommitted.Broadcast(Text, CommitMethod);
}
