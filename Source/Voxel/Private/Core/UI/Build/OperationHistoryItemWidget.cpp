// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/OperationHistoryItemWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UOperationHistoryItemWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UOperationHistoryItemWidget::SetOperationData(const FVoxelEditHistoryRecord& HistoryRecord, bool bIsRedo)
{
	// 设置时间显示
	if (TimeText)
	{
		// 格式化时间为 HH:mm:ss
		FString TimeStr = HistoryRecord.Timestamp.ToString(TEXT("%H:%M:%S"));
		TimeText->SetText(FText::FromString(TimeStr));
	}

	// 设置建造状态显示
	if (BuildStateText)
	{
		BuildStateText->SetText(GetBuildStateText(HistoryRecord.ToolTag));
	}

	// 设置编辑状态显示（已废弃，新架构不再单独显示编辑状态）
	// EditStateText 已移除

	// 设置操作描述
	if (DescriptionText)
	{
		DescriptionText->SetText(HistoryRecord.Description);
	}

	// 设置操作类型指示器颜色（已废弃，新架构不再单独显示操作类型）
	// OperationTypeIndicator 已移除

	// 设置重做标记
	if (RedoMark)
	{
		RedoMark->SetVisibility(bIsRedo ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

FText UOperationHistoryItemWidget::GetBuildStateText(FGameplayTag ToolTag) const
{
    // 根据工具类型返回显示文本
    if (ToolTag == FCoreTags::Tool_Edit_SingleVoxel ||
        ToolTag == FCoreTags::Tool_Edit_SimpleGeometry)
    {
        return FText::FromString(TEXT("建造"));
    }
    if (ToolTag == FCoreTags::Tool_Edit_Template)
    {
        return FText::FromString(TEXT("模板"));
    }
    if (ToolTag.MatchesTag(FCoreTags::Tool_AreaEdit))
    {
        return FText::FromString(TEXT("区域"));
    }
    return FText::FromString(TEXT("未知"));
}

FLinearColor UOperationHistoryItemWidget::GetOperationTypeColor(const FVoxelEditCommand& EditCommand) const
{
    switch (EditCommand.PlacementMode)
    {
    case EVoxelPlacementMode::Normal:
        return FLinearColor(0.2f, 0.8f, 0.2f);  // 绿色
    case EVoxelPlacementMode::Complement:
        return FLinearColor(0.2f, 0.2f, 0.8f);  // 蓝色
    case EVoxelPlacementMode::Replace:
        return FLinearColor(0.8f, 0.2f, 0.2f);  // 红色
    case EVoxelPlacementMode::ReplaceMask:
        return FLinearColor(0.8f, 0.2f, 0.8f);  // 紫色
    default:
        return FLinearColor(0.5f, 0.5f, 0.5f);  // 灰色
    }
}
