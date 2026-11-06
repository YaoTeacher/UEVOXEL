// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Blueprint/UserWidgetPool.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Voxel/Editor/VoxelHistoryData.h"
#include "OperationHistoryItemWidget.generated.h"

class UImage;
class UTextBlock;
/**
 * 操作历史记录条目 Widget
 * 显示单条编辑操作的详细信息
 */
UCLASS()
class VOXEL_API UOperationHistoryItemWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    // === UWidget 接口 ===
    virtual void NativeConstruct() override;

    // === 数据设置 ===

    /**
     * 设置操作历史数据
     * @param HistoryRecord 历史记录数据
     * @param bIsRedo 是否为重做操作
     */
    UFUNCTION(BlueprintCallable, Category = "UI|History")
    void SetOperationData(const FVoxelEditHistoryRecord& HistoryRecord, bool bIsRedo = false);

protected:
    // === UI 组件绑定 ===

    /** 时间显示文本 */
    UPROPERTY(meta = (BindWidget, OptionalWidget = true))
    UTextBlock* TimeText;

    /** 建造状态显示文本 */
    UPROPERTY(meta = (BindWidget, OptionalWidget = true))
    UTextBlock* BuildStateText;

    /** 编辑状态显示文本 */
    UPROPERTY(meta = (BindWidget, OptionalWidget = true))
    UTextBlock* EditStateText;

    /** 操作描述文本 */
    UPROPERTY(meta = (BindWidget, OptionalWidget = true))
    UTextBlock* DescriptionText;

    /** 操作类型图标/颜色指示器 */
    UPROPERTY(meta = (BindWidget, OptionalWidget = true))
    UImage* OperationTypeIndicator;

    /** 重做标记（重做的操作会有特殊标识） */
    UPROPERTY(meta = (BindWidget, OptionalWidget = true))
    UTextBlock* RedoMark;

private:
    /**
     * 获取工具类型的显示文本
     */
    FText GetBuildStateText(FGameplayTag ToolTag) const;

    /**
     * 获取操作类型指示器颜色
     */
    FLinearColor GetOperationTypeColor(const FVoxelEditCommand& EditCommand) const;
};
