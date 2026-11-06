// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Widgets/VoxelComboRow.h"
#include "Core/UI/Widgets/VoxelTextBlock.h"
#include "Core/UI/Widgets/VoxelComboBoxString.h"
#include "Core/UI/Theme/VoxelUITheme.h"

void UVoxelComboRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ComboBox && !bEventsBound)
	{
		ComboBox->OnSelectionChanged.AddDynamic(this, &UVoxelComboRow::HandleSelectionChanged);
		bEventsBound = true;
	}
}

void UVoxelComboRow::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (LabelText)
	{
		LabelText->SetTextRole(EVoxelTextRole::ItemLabel);
		LabelText->SetText(Label);
	}
	if (ComboBox && ComboBox->GetOptionCount() == 0 && DefaultOptions.Num() > 0)
	{
		SetOptions(DefaultOptions);
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

void UVoxelComboRow::SetOptions(const TArray<FString>& InOptions)
{
	if (!ComboBox)
	{
		return;
	}
	ComboBox->ClearOptions();
	for (const FString& Option : InOptions)
	{
		ComboBox->AddOption(Option);
	}
	ComboBox->RefreshOptions();
	if (InOptions.Num() > 0)
	{
		ComboBox->SetSelectedIndex(0);
	}
}

void UVoxelComboRow::SetSelectedOption(FString Option)
{
	if (ComboBox)
	{
		ComboBox->SetSelectedOption(Option);
	}
}

FString UVoxelComboRow::GetSelectedOption() const
{
	return ComboBox ? ComboBox->GetSelectedOption() : FString();
}

void UVoxelComboRow::SetLabelText(const FText& InText)
{
	Label = InText;
	if (LabelText)
	{
		LabelText->SetText(InText);
	}
}

void UVoxelComboRow::HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	OnSelectionChanged.Broadcast(SelectedItem, SelectionType);
}
