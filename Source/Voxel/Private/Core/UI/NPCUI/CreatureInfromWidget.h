// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/UI/Base/VoxelScreenBase.h"
#include "CreatureInfromWidget.generated.h"

class UTendencyRowWidget;
class UDynamicRowWidget;
struct FCreatureGenerationResult;
class UDemandRowWidget;
class UCreatureBaseInformWidget;
class UWrapBox;
class UStatRowWidget;
class UEventHistoryRowWidget;
class UTextBlock;
class UPanelWidget;
class ULoadingIndicatorWidget;
/**
 * 
 */
UCLASS()
class UCreatureInfromWidget : public UVoxelScreenBase
{
	GENERATED_BODY()
public:
	// --- 子控件绑定 ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCreatureBaseInformWidget> BaseInformText;
    
	// 用于放属性 (Health, Strength...)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWrapBox> BaseStateContainer;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWrapBox> CombatStateContainer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWrapBox> WorkStateContainer;
	// 用于放需求 (Hunger, Thirst...)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWrapBox> DemandContainer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWrapBox> DynamicContainer;
	//性格模型容器
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWrapBox> PersonalityContainer;
	// 事件历史容器
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWrapBox> EventHistoryContainer;

	// --- 配置项 (指定生成的子控件类) ---
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UDemandRowWidget> DynamicClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UStatRowWidget> StatClass;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UDemandRowWidget> DemandClass;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UEventHistoryRowWidget> EventHistoryRowClass;

	//倾向条控件类配置
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UTendencyRowWidget> TendencyRowClass;
	
	UPROPERTY(EditAnywhere, Category = "Config")
	TSubclassOf<AActor> PreviewActorClass;

	// --- 核心逻辑 ---

	/** * 刷新整个面板 
	 * @param Result - 从生成器传回来的完整结果 (包含 DNA 和 初始状态)
	 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateCreatureInfo(const FCreatureGenerationResult& Result);
	void AddStatToContainer(UWrapBox* TargetContainer, FGameplayTag Tag, float Value);

private:
	/** * 刷新事件历史记录 
	 * @param Result - 从生成器传回来的完整结果 
	 */
	void UpdateEventHistory(const FCreatureGenerationResult& Result);

	/** * 显示加载状态 
	 */
	void ShowLoadingState();

	/** * 显示事件历史内容 
	 */
	void ShowEventHistoryContent();

	/** * 显示错误状态 
	 * @param ErrorMessage - 错误信息 
	 */
	void ShowErrorState(const FString& ErrorMessage);

	// // 缓存最近一次的事件历史数据，避免重复处理
	// UPROPERTY(Transient)
	// FCreatureGenerationResult CachedGenerationResult;

	// 标记是否正在加载
	// UPROPERTY(Transient)
	// bool bIsLoading;
};
