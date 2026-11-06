// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Data/Area/VoxelAreaBase.h" // 引用你的Area定义
#include "VoxelAreaItemWidget.generated.h"

class UButton;
class UTextBlock;
class AVoxelAreaBaseInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAreaItemDoubleClicked, AVoxelAreaBaseInstance*, Area);

/**
 * 区域单元控件基类
 * 用于面包屑节点、子区域列表项
 */
UCLASS()
class VOXEL_API UVoxelAreaItemWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 核心数据：持有的区域指针 (nullptr 代表世界根节点)
	UPROPERTY(BlueprintReadOnly, Category = "Area Data")
	AVoxelAreaBaseInstance* TargetArea;

	// 初始化函数 (供父级 BuildPanel 调用)
	UFUNCTION(BlueprintCallable, Category = "Area UI")
	void Setup(AVoxelAreaBaseInstance* InArea, bool bIsCurrentSelection);

	/** 双击当前区域时广播（Navigator 订阅后展开下属列表） */
	UPROPERTY(BlueprintAssignable, Category = "Area UI|Events")
	FOnAreaItemDoubleClicked OnAreaItemDoubleClicked;

protected:
	// === UI 绑定 ===
	UPROPERTY(meta = (BindWidget))
	UButton* ClickButton;

	UPROPERTY(meta = (BindWidgetOptional)) // 面包屑可能不需要显示ID，所以是可选的
	UTextBlock* NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* IdText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TypeText; 
    
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void OnBtnClicked();

	/** 上次单击目标区域指针（双击识别） */
	AVoxelAreaBaseInstance* LastClickedArea = nullptr;

	/** 上次单击时间（双击识别，秒） */
	double LastClickTime = -1.0;
};
