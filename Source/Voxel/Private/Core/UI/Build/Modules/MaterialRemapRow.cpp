// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/Modules/MaterialRemapRow.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Data/Item/VoxelItemBase.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UMaterialRemapRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (PickButton)
		PickButton->OnClicked.AddDynamic(this, &UMaterialRemapRow::HandlePickClicked);
}

void UMaterialRemapRow::NativeConstruct()
{
	Super::NativeConstruct();
}

void UMaterialRemapRow::InitRow(const FString& InOriginFullId, const FString& InCurrentFullId, UConfigDataSubsystem* ConfigSys)
{
	OriginFullId = InOriginFullId;

	if (!ConfigSys) return;

	// 原素材视觉
	const int32 OriginHandle = ConfigSys->GetHandleByFullId(InOriginFullId);
	if (const FVoxelItemBase* OriginItem = ConfigSys->GetItemTypeById<FVoxelItemBase>(OriginHandle))
	{
		if (OriginNameText) OriginNameText->SetText(OriginItem->ItemName);
		if (OriginIcon)
		{
			if (UTexture2D* Tex = OriginItem->AssetFragment.Icon)
			{
				OriginIcon->SetBrushFromTexture(Tex);
			}
		}
	}

	// 当前生效素材视觉（未替换时 == 原素材）
	const int32 CurHandle = ConfigSys->GetHandleByFullId(InCurrentFullId);
	if (const FVoxelItemBase* CurItem = ConfigSys->GetItemTypeById<FVoxelItemBase>(CurHandle))
	{
		if (CurrentNameText) CurrentNameText->SetText(CurItem->ItemName);
		if (CurrentIcon)
		{
			if (UTexture2D* Tex = CurItem->AssetFragment.Icon)
			{
				CurrentIcon->SetBrushFromTexture(Tex);
			}
		}
	}
}

void UMaterialRemapRow::HandlePickClicked()
{
	OnPickRequested.Broadcast(OriginFullId);
}
