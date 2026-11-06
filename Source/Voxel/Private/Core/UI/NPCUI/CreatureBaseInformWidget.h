// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelScreenBase.h"
#include "Core/UI/TagChipWidget.h"
#include "CreatureBaseInformWidget.generated.h"

struct FTrueCreatureData;
class UWrapBox;
class UTextBlock;
/**
 * 生物基础信息页（Screen 层），由 AVoxelHUD.OpenScreen 管理
 */
UCLASS()
class UCreatureBaseInformWidget : public UVoxelScreenBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UTextBlock> TrueNameText;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UTextBlock> TrueAgeText;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UTextBlock> TrueGenderText;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UTextBlock> RaceNameText;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(BindWidget))
	TObjectPtr<UTextBlock> ProfNameText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWrapBox> TraitsContainer;

	// --- 配置项 ---

	// [修改点 2] 指定用来显示"单个特质"的子控件类 (WBP_TagChip)
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UTagChipWidget> TraitChipClass;

	// --- 功能函数 ---
    
	// 供外部调用的刷新函数
	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateCreatureInfo(const FTrueCreatureData& Data);
	
};
