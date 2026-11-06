// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/VoxelCharacter.h"
#include "Data/Item/VoxelItemBase.h"
#include "VoxelItemBaseInstance.generated.h"

class UMaterialInterface;

/**
 * 
 */
UCLASS()
class AUVoxelItemBaseInstance : public AActor
{
	GENERATED_BODY()

public:

	AUVoxelItemBaseInstance();

	FVoxelNormalItemBase* ItemBaseInform;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Item")
	FVoxelItemInstanceBase ItemInstance;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Item")
	TArray<FIntVector> OccupiedBlock;

	bool bIsCopy;
	bool bIsPickup;

	UFUNCTION(Category = "Instance/Item")
	void ResetItemFlags();

	UFUNCTION(Category = "Instance/Item")
	virtual AUVoxelItemBaseInstance* CreateItemCopy();

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Area/Visual")
	UStaticMeshComponent* BoxVisual;
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Area/Visual")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;

	/** 家具实际材质（PBR，蓝图配置为 M_Furniture）
	 *  运行时基于此创建 MID 并设置 BaseColorMap/NormalMap/ORMMap 参数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area/Visual|Material")
	TObjectPtr<UMaterialInterface> FurnitureMaterial;

	UFUNCTION(Category = "Instance/Item")
	FORCEINLINE float GetItemStackWeight() const {return ItemInstance.Quantity * ItemBaseInform->CirculateDataFragment.Weight; };
	UFUNCTION(Category = "Instance/Item")
	FORCEINLINE float GetItemSingleWeight() const { return ItemBaseInform->CirculateDataFragment.Weight; };
	UFUNCTION(Category = "Instance/Item")
	FORCEINLINE bool IsFullItemStack() const { return ItemInstance.Quantity==ItemBaseInform->CirculateDataFragment.MaxStackCount; };

	/** 获取家具的 HandleId（用于占用检查和历史记录） */
	UFUNCTION(BlueprintPure, Category = "Instance/Item")
	int32 GetFurnitureHandleId() const { return ItemInstance.HandleId; }
	UFUNCTION(Category = "Instance/Item")
	void SetQuantity(const int32 NewQuantity);
	void UpdateBoxVisual() const;

	UFUNCTION(Category = "Instance/Item")
	void InitiateFragment();

	void UpdateItemInform(FVoxelNormalItemBase* InputData);
	UFUNCTION(Category = "Instance/Item")
	void InitiatePickUp();

	UFUNCTION(Category = "Instance/Item")
	void InitiatePlace();

	UFUNCTION(Category = "Instance/Item")
	void InitiateDrop();

	UFUNCTION(Category = "Instance/Item")
	void DestroyItem();
	
	UFUNCTION(Category = "Instance/Item")
	virtual void Use(AVoxelCharacter* Character);
	
protected:
	bool operator==(const FGameplayTag OtherId) const 
	{
		return ItemInstance.ItemTag == OtherId;
	};
	
};
