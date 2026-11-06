// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/VoxelTemplateSaveDialog.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/CoreTags.h"
#include "Data/Item/VoxelItemBase.h"
#include "Voxel/VoxelPlayerController.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"

void UVoxelTemplateSaveDialog::OnDialogOpened(UObject* Context)
{
	// 从 Context 获取 StateComponent
	StateComponent = Cast<UVoxelPlayerStateCacheComponent>(Context);

	if (!StateComponent || !StateComponent->HasSelection())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TemplateSaveDialog] Opened without valid selection"));
		Close();
		return;
	}

	// 刷新选区统计
	RefreshSelectionStats();
}

void UVoxelTemplateSaveDialog::NativeConstruct()
{
	Super::NativeConstruct();

	// 绑定按钮事件
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &UVoxelTemplateSaveDialog::OnConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UVoxelTemplateSaveDialog::OnCancelClicked);
	}

	// 设置默认值
	if (TemplateNameInput && TemplateNameInput->GetText().IsEmpty())
	{
		TemplateNameInput->SetText(FText::FromString(TEXT("New Template")));
	}
	if (TemplateAuthorInput && TemplateAuthorInput->GetText().IsEmpty())
	{
		TemplateAuthorInput->SetText(FText::FromString(TEXT("Player")));
	}
}

void UVoxelTemplateSaveDialog::RefreshSelectionStats()
{
	if (!SelectionStatsText || !StateComponent) return;

	const TArray<FIntVector>& Selection = StateComponent->GetCurrentSelection();
	if (Selection.Num() == 0)
	{
		SelectionStatsText->SetText(FText::FromString(TEXT("选区为空")));
		return;
	}

	// 计算包围盒（仅展示用几何信息，只依赖选区列表本身）
	FIntVector Min = Selection[0];
	FIntVector Max = Selection[0];
	for (const FIntVector& Pos : Selection)
	{
		Min = FIntVector(FMath::Min(Min.X, Pos.X), FMath::Min(Min.Y, Pos.Y), FMath::Min(Min.Z, Pos.Z));
		Max = FIntVector(FMath::Max(Max.X, Pos.X), FMath::Max(Max.Y, Pos.Y), FMath::Max(Max.Z, Pos.Z));
	}
	const FIntVector Size = Max - Min + FIntVector(1, 1, 1);

	// 素材统计走 BlockWorldSubsystem 统一业务入口（UI 不做统计，只做名称解析与格式化）
	UWorld* World = GetWorld();
	UVoxelBlockWorldSubsystem* BlockSys = World ? World->GetSubsystem<UVoxelBlockWorldSubsystem>() : nullptr;
	UConfigDataSubsystem* ConfigSys = World ? World->GetGameInstance()->GetSubsystem<UConfigDataSubsystem>() : nullptr;

	TMap<int32, int32> MaterialCounts;
	int32 FurnitureCount = 0;
	if (BlockSys)
	{
		BlockSys->CalculateSelectionMaterials(Selection, MaterialCounts, FurnitureCount);
	}

	// 格式化素材清单（查询每个 ItemId 的名称）
	FString MaterialList;
	for (const auto& Pair : MaterialCounts)
	{
		FString ItemName = FString::Printf(TEXT("ItemId_%d"), Pair.Key);
		if (ConfigSys)
		{
			if (const FVoxelItemBase* Item = ConfigSys->GetItemTypeById<FVoxelItemBase>(Pair.Key))
			{
				ItemName = Item->ItemName.ToString();
			}
		}
		MaterialList += FString::Printf(TEXT("\n- %s x%d"), *ItemName, Pair.Value);
	}

	const FString Stats = FString::Printf(
		TEXT("体素数: %d\n尺寸: %dx%dx%d\n家具数: %d\n范围: (%d,%d,%d) ~ (%d,%d,%d)\n\n素材清单:%s"),
		Selection.Num(),
		Size.X, Size.Y, Size.Z,
		FurnitureCount,
		Min.X, Min.Y, Min.Z,
		Max.X, Max.Y, Max.Z,
		*MaterialList);
	SelectionStatsText->SetText(FText::FromString(Stats));
}

void UVoxelTemplateSaveDialog::OnConfirmClicked()
{
	if (!StateComponent || !StateComponent->HasSelection())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TemplateSaveDialog] No selection on confirm"));
		Close();
		return;
	}

	// 从输入框读取元信息（输入框未配置时 fallback 到默认值）
	auto ReadTextOr = [](UEditableTextBox* Box, const FString& Fallback) -> FString
	{
		return (Box && !Box->GetText().IsEmpty())
			? Box->GetText().ToString()
			: Fallback;
	};
	auto ReadDescOr = [](UEditableTextBox* Box) -> FText
	{
		return Box ? Box->GetText() : FText::GetEmpty();
	};

	FTemplateMeta Meta;
	Meta.DisplayName = FText::FromString(ReadTextOr(TemplateNameInput, TEXT("New Template")));
	Meta.Description = ReadDescOr(TemplateDescInput);
	Meta.Author = ReadTextOr(TemplateAuthorInput, TEXT("Player"));
	Meta.CategoryTag = FCoreTags::Item_Type_Template;

	// 后端业务入口：捕获选区 + 落盘 + 注册 + 广播刷新（全部由 InteractionComp 完成，UI 层不直连数据层）
	const AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(StateComponent->GetOwner());
	if (PC && PC->InteractionComp)
	{
		PC->InteractionComp->SaveSelectionAsTemplate(Meta);
	}

	// 关闭弹窗（通过栈管理）
	Close();
}

void UVoxelTemplateSaveDialog::OnCancelClicked()
{
	Close();
}
