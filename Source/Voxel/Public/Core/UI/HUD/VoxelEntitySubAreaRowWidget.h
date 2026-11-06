#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "VoxelEntitySubAreaRowWidget.generated.h"

class UButton;
class UTextBlock;

/** 子区域行点击广播（携带目标区域 Id，面板订阅后导航） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSubAreaRowClicked, int32, AreaId);

/**
 * 实体详情面板·子区域行控件
 *
 * 职责边界：C++ 只做数据填充（名称/状态）与点击转发（携带 AreaId 广播），
 * 视觉样式全部在对应 WBP（WBP_EntitySubAreaRow）中维护，C++ 不写任何样式。
 */
UCLASS()
class VOXEL_API UVoxelEntitySubAreaRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 行点击（切换选中到目标区域），由面板订阅 */
	UPROPERTY(BlueprintAssignable, Category = "Entity Detail")
	FOnSubAreaRowClicked OnRowClicked;

	/** 填充行数据（区域显示名 / 状态文本 / 目标区域 Id） */
	UFUNCTION(BlueprintCallable, Category = "Entity Detail")
	void InitializeRow(const FText& AreaName, const FText& AreaStatus, int32 InAreaId);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RowButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void HandleRowClicked();

	/** 点击导航目标（无参数委托靠它寻址） */
	int32 TargetAreaId = -1;
};
