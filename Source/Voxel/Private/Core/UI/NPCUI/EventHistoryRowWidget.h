// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "EventHistoryRowWidget.generated.h"

struct FLifeHistoryEntry;
class UConfigDataSubsystem;
class UTextBlock;
class UBorder;

/**
 * 单条NPC事件历史记录Widget
 */
UCLASS()
class UEventHistoryRowWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    // 事件年龄显示
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> AgeText;

    // 事件内容描述
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> EventDescriptionText;

    // 事件结果显示
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> EventResultText;

    // 分割线
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UBorder> Separator;

    /**
     * 更新事件记录数据
     * @param Entry 事件记录数据
     * @param ConfigSubsystem 配置子系统，用于解析标签
     * @param CreatureName NPC名称，用于生成事件描述
     */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void UpdateEventData(const FLifeHistoryEntry& Entry, UConfigDataSubsystem* ConfigSubsystem, const FString& CreatureName);
};