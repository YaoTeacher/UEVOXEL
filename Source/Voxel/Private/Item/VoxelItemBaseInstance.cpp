// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/VoxelItemBaseInstance.h"

#include "Utils/ChunkUtilsFunction.h"
#include "GameManager/WorldManager/VoxelItemWorldSubsystem.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"

struct FStreamableManager;

AUVoxelItemBaseInstance::AUVoxelItemBaseInstance() : bIsCopy(false), bIsPickup(false)
{
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	BoxVisual =CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxVisual"));
	SetRootComponent(StaticMeshComponent);

	// 配置碰撞：响应 BuildItem 通道（ECC_GameTraceChannel2）
	// 编辑模式下通过该通道射线检测家具 Actor，实现点击查看/拖拽移动
	StaticMeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
}

void AUVoxelItemBaseInstance::ResetItemFlags()
{
	this->bIsCopy = false;
	this->bIsPickup = false;
}

AUVoxelItemBaseInstance* AUVoxelItemBaseInstance::CreateItemCopy()
{
	AUVoxelItemBaseInstance* ItemCopy = NewObject<AUVoxelItemBaseInstance>(StaticClass());
	ItemCopy->ItemInstance = this->ItemInstance;
	ItemCopy->ItemBaseInform = this->ItemBaseInform;


	return ItemCopy;
}

void AUVoxelItemBaseInstance::SetQuantity(const int32 NewQuantity)
{
	if (NewQuantity != this->ItemInstance.Quantity)
	{
		ItemInstance.Quantity = FMath::Clamp(NewQuantity, 0,  this->ItemBaseInform->CirculateDataFragment.MaxStackCount);

		//if (this->OwningInventory)
		//{
		//	if (this->Quantity <= 0)
		//	{
		//		this->OwningInventory->RemoveSingleInstanceOfItem(this);
		//	}
		//}
		//else
		//{
		//	UE_LOG(LogTemp, Warning, TEXT("ItemBase OwningInventory was null (item may be a pickup)."));
		//}
	}
}

void AUVoxelItemBaseInstance::UpdateBoxVisual() const
{	FVector v =FVector(ItemBaseInform->Volume);
	FVector Offset= FVector(v.X,-v.Y,v.Z);
	v=UChunkUtilsFunction::GetRotationVolume(ItemInstance.Direction,Offset);
	FVector Center= v * 0.5f;
	FVector Scale = FVector(std::abs(Center.X), std::abs(Center.Y), std::abs(Center.Z));
	Scale=Scale*2*0.01+0.05;
	
	auto transform = FTransform(
	FRotator::ZeroRotator,
	FVector(Center+RootComponent->GetComponentLocation()) ,
	FVector(Scale)
	);
	this->BoxVisual->SetRelativeTransform(transform);


}

void AUVoxelItemBaseInstance::InitiateFragment()
{
	// switch (ItemBaseInform->ItemType)
	// {
	// case(EVoxelItemType::BuildBlock):
	// 	break;
	// case(EVoxelItemType::BuildItems):
	// 	break;
	// case(EVoxelItemType::ConsumableItems):
	// 	break;
	// case(EVoxelItemType::StaticItems):
	// 	break;
	// case(EVoxelItemType::EquipmentItems):
	// 	break;
	// case(EVoxelItemType::Skills):
	// 	break;
	// default:
	// 	break;
	// } 
}
void AUVoxelItemBaseInstance::UpdateItemInform(FVoxelNormalItemBase* InputData)
{
	ItemBaseInform = InputData;
}

void AUVoxelItemBaseInstance::InitiatePickUp()
{
}

void AUVoxelItemBaseInstance::InitiatePlace()
{
	UE_LOG(LogTemp, Warning, TEXT("Id:%d."), ItemBaseInform->HandleId);

	if (ItemBaseInform->AssetFragment.Mesh != nullptr)
	{
		if (!IsValid(StaticMeshComponent))
		{
			UE_LOG(LogTemp, Error, TEXT("StaticMeshComponent not initialized!"));
			StaticMeshComponent = NewObject<UStaticMeshComponent>(this, "StaticMeshComponent");
		}
		UE_LOG(LogTemp, Warning, TEXT("StaticMeshName:%s."), *ItemBaseInform->AssetFragment.Mesh->GetName());
		StaticMeshComponent->SetStaticMesh(ItemBaseInform->AssetFragment.Mesh);
		StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// 应用 PBR 材质：基于 FurnitureMaterial 创建 MID，设置 BaseColorMap/NormalMap/ORMMap
		if (FurnitureMaterial)
		{
			UMaterialInstanceDynamic* FurnitureMID = UMaterialInstanceDynamic::Create(FurnitureMaterial, this);
			if (FurnitureMID)
			{
				const FVoxelTextureSet& TexSet = ItemBaseInform->AssetFragment.TextureSet;
				if (TexSet.BaseColorMap.IsValid())
				{
					if (UTexture2D* Tex = TexSet.BaseColorMap.LoadSynchronous())
					{
						FurnitureMID->SetTextureParameterValue(FName(TEXT("BaseColorMap")), Tex);
					}
				}
				if (TexSet.NormalMap.IsValid())
				{
					if (UTexture2D* Tex = TexSet.NormalMap.LoadSynchronous())
					{
						FurnitureMID->SetTextureParameterValue(FName(TEXT("NormalMap")), Tex);
					}
				}
				if (TexSet.ORMMap.IsValid())
				{
					if (UTexture2D* Tex = TexSet.ORMMap.LoadSynchronous())
					{
						FurnitureMID->SetTextureParameterValue(FName(TEXT("ORMMap")), Tex);
					}
				}
				StaticMeshComponent->SetMaterial(0, FurnitureMID);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Furniture] FurnitureMaterial not configured! Item Handle=%d. Must set M_Furniture in Blueprint."), ItemBaseInform->HandleId);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load mesh for item!"));
	}

	UpdateBoxVisual();
}

void AUVoxelItemBaseInstance::InitiateDrop()
{
}

void AUVoxelItemBaseInstance::DestroyItem()
{
	// 统一删除入口：委托 ItemWorldSubsystem 处理空间索引清理 + Area 引用清理 + Actor 销毁
	// 优先用 OccupiedBlock[0] 作为查询入口（任意占用格子都能反查到本 Actor）
	if (OccupiedBlock.Num() > 0 && GetWorld())
	{
		if (UVoxelItemWorldSubsystem* ItemWorld = GetWorld()->GetSubsystem<UVoxelItemWorldSubsystem>())
		{
			// Subsystem 内部会：通知 Area 移除引用 → 清理 GridToItemMap → Destroy Actor
			ItemWorld->RemoveItemAtGrid(OccupiedBlock[0]);
			return;
		}
	}

	// 兜底：Subsystem 不可用时，手动清理 + Destroy
	UE_LOG(LogTemp, Warning, TEXT("[DestroyItem] Fallback direct destroy (no Subsystem cleanup)"));

	// 清理 Area 引用
	if (ItemInstance.ParentArea)
	{
		ItemInstance.ParentArea->UnregisterFurnitureItem(this);
		ItemInstance.ParentArea = nullptr;
	}

	Destroy();
}

void AUVoxelItemBaseInstance::Use(AVoxelCharacter* Character)
{
}
