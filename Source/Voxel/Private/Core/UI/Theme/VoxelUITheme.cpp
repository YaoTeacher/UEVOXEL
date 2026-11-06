// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Theme/VoxelUITheme.h"
#include "Core/UI/Theme/VoxelUISettings.h"
#include "Styling/CoreStyle.h"

UVoxelUITheme::UVoxelUITheme()
{
	// 文本角色默认值镜像 DESIGN_UMG.md §3
	PanelTitleStyle.bUseSerifFont = true;
	PanelTitleStyle.Size = 19;
	PanelTitleStyle.Color = TitleGold;

	SectionTitleStyle.bUseSerifFont = true;
	SectionTitleStyle.Size = 11;
	SectionTitleStyle.Color = Gold2;
	SectionTitleStyle.LetterSpacing = 200;

	ItemLabelStyle.Size = 11;
	ItemLabelStyle.Color = Muted;

	BodyStyle.Size = 11;
	BodyStyle.Color = Text;

	HintStyle.Size = 10;
	HintStyle.Color = Faint;

	DataValueStyle.bUseSerifFont = true;
	DataValueStyle.Size = 13;
	DataValueStyle.Color = Gold;

	ButtonTextStyle.Size = 11;
	ButtonTextStyle.Color = Text;

	KbdHintStyle.Size = 9;
	KbdHintStyle.Color = Muted;
}

const UVoxelUITheme* UVoxelUITheme::Get()
{
	const UVoxelUISettings* Settings = GetDefault<UVoxelUISettings>();
	if (!Settings)
	{
		return nullptr;
	}
	// 已加载时 LoadSynchronous 仅做解析，开销可忽略；不缓存以支持热切换主题资产
	return Settings->DefaultTheme.LoadSynchronous();
}

const FVoxelTextRoleStyle& UVoxelUITheme::GetRoleStyle(EVoxelTextRole Role) const
{
	switch (Role)
	{
	case EVoxelTextRole::PanelTitle:   return PanelTitleStyle;
	case EVoxelTextRole::SectionTitle: return SectionTitleStyle;
	case EVoxelTextRole::ItemLabel:    return ItemLabelStyle;
	case EVoxelTextRole::Body:         return BodyStyle;
	case EVoxelTextRole::Hint:         return HintStyle;
	case EVoxelTextRole::DataValue:    return DataValueStyle;
	case EVoxelTextRole::ButtonText:   return ButtonTextStyle;
	case EVoxelTextRole::KbdHint:      return KbdHintStyle;
	}
	return BodyStyle;
}

FSlateFontInfo UVoxelUITheme::MakeFontInfo(EVoxelTextRole Role) const
{
	const FVoxelTextRoleStyle& RoleStyle = GetRoleStyle(Role);

	// 必须从引擎默认字体（含有效 CompositeFont）起步：
	// 空 FSlateFontInfo 无字体可解析，SEditableText 路径会空指针崩溃，STextBlock 退化为 LastResort
	FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle(NAME_None, static_cast<float>(RoleStyle.Size));
	FontInfo.LetterSpacing = RoleStyle.LetterSpacing;
	if (const TSoftObjectPtr<UObject>& FontAsset = RoleStyle.bUseSerifFont ? SerifFont : BodyFont; !FontAsset.IsNull())
	{
		if (UObject* LoadedFont = FontAsset.LoadSynchronous())
		{
			FontInfo.FontObject = LoadedFont;
		}
	}
	return FontInfo;
}
