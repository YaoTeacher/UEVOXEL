// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Widgets/VoxelTextBlock.h"

void UVoxelTextBlock::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyThemeStyle();
}

void UVoxelTextBlock::SetTextRole(EVoxelTextRole NewRole)
{
	TextRole = NewRole;
	if (GetCachedWidget().IsValid())
	{
		SynchronizeProperties();
	}
}

void UVoxelTextBlock::ApplyThemeStyle()
{
	if (!bUseThemeStyle)
	{
		return;
	}
	const UVoxelUITheme* Theme = UVoxelUITheme::Get();
	if (!Theme)
	{
		return;
	}

	FSlateFontInfo FontInfo = Theme->MakeFontInfo(TextRole);
	if (SizeOverride > 0)
	{
		FontInfo.Size = SizeOverride;
	}
	SetFont(FontInfo);
	SetColorAndOpacity(FSlateColor(Theme->GetRoleStyle(TextRole).Color));
}
