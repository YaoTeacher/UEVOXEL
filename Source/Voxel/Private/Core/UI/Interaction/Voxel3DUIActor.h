// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Voxel3DUIActor.generated.h"

class UTextRenderComponent;

enum class EActorDirection : uint8;
class AVoxelPlayerController;

// 输入事件委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUIActorDragged, AVoxel3DUIActor*, DraggedActor, FVector, DragDelta);

/**
 * 3D UI Actor（箭头、手柄等）
 * 支持点击和拖动输入事件
 */
UCLASS()
class AVoxel3DUIActor : public AActor
{
	GENERATED_BODY()

	AVoxel3DUIActor();

public:
	// === 组件 ===
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	UStaticMeshComponent* UIMesh;

	/** 文字标签（可选，如 3D 按钮"确认/取消"） */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	UTextRenderComponent* LabelComponent;

	// === 属性 ===
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Components")
	EActorDirection Direction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Components")
	TObjectPtr<UMaterialInterface> DefaultMetarial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Components")
	TObjectPtr<UMaterialInterface> SelectedMetarial;

	// === 输入事件 ===

	/** 拖动事件（按住鼠标移动时触发） */
	UPROPERTY(BlueprintAssignable, Category = "UI|Input")
	FOnUIActorDragged OnDragged;

	/** 所属 PlayerController（用于事件处理） */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Input")
	TObjectPtr<AVoxelPlayerController> OwningController;

	// === 方法 ===
	UFUNCTION(BlueprintCallable, Category = "Components")
	void IsSelected(bool input);

	UFUNCTION(BlueprintCallable, Category = "Components")
	void SetDir(EActorDirection input);

	/** 设置文字标签（空文本隐藏标签） */
	UFUNCTION(BlueprintCallable, Category = "Components")
	void SetLabel(const FText& Text);

	/** 设置标签颜色 */
	UFUNCTION(BlueprintCallable, Category = "Components")
	void SetLabelColor(const FLinearColor& Color);

	/** 设置所属 Controller */
	UFUNCTION(BlueprintCallable, Category = "UI|Input")
	void SetOwningController(AVoxelPlayerController* Controller);

protected:
	// === AActor 接口 ===
	virtual void BeginPlay() override;
	virtual void NotifyActorOnClicked(FKey ButtonPressed) override;
	virtual void NotifyActorOnInputTouchBegin(const ETouchIndex::Type FingerIndex) override;
};
