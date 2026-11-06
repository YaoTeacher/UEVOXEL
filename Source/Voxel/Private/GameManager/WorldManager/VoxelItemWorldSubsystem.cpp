// Fill out your copyright notice in the Description page of Project Settings.


#include "GameManager/WorldManager/VoxelItemWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Core/CoreTags.h"
#include "Item/VoxelItemBaseInstance.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "GameFramework/PlayerController.h"

void UVoxelItemWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 获取依赖的子系统
	BlockSys = Collection.InitializeDependency<UVoxelBlockWorldSubsystem>();
	AreaSys = Collection.InitializeDependency<UVoxelAreaWorldSubsystem>();
	// ConfigSys 通常挂在 GameInstance 下，可能需要用 GetGameInstance 获取
	// 这里假设能在 Initialize 获取，或者在 Spawn 时动态获取
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		ConfigSys = GI->GetSubsystem<UConfigDataSubsystem>();
	}
	
	UE_LOG(LogTemp, Log, TEXT("VoxelItemWorldSubsystem Initialized."));
}

void UVoxelItemWorldSubsystem::Deinitialize()
{
	// 清理所有 Actor (如果世界卸载，Actor 会自动销毁，但 Map 需要清空)
	GridToItemMap.Empty();
	Super::Deinitialize();
}

AUVoxelItemBaseInstance* UVoxelItemWorldSubsystem::SpawnItemAtGrid(const FIntVector& Position, const FInteractableData& Data, bool bLoadFromSave)
{
	if (!GetWorld() || !ConfigSys) return nullptr;

	// 1. 获取静态配置数据
	FVoxelNormalItemBase* StaticData = ConfigSys->GetItemTypeById<FVoxelNormalItemBase>(Data.HandleId);
	if (!StaticData)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnItem Failed: Invalid ItemId %d"), Data.HandleId);
		return nullptr;
	}

	// 1.5 获取 BlockLength（CalculateOccupiedCells 和 CheckIsSpaceAvailable 需要）
	float BlockLen = 100.0f;
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UVoxelGameInstance* VGI = Cast<UVoxelGameInstance>(GI))
			BlockLen = VGI->WorldConfig.BlockLength;
	}

	// 2. 计算占用的格子列表
	TArray<FIntVector> OccupiedCells = CalculateOccupiedCells(Position, Data.Direction, StaticData, BlockLen);

	// 3. 合法性检查 (读档时不检查，相信存档)
	if (!bLoadFromSave)
	{
		if (!CheckIsSpaceAvailable(Position, Data.Direction, StaticData))
		{
			UE_LOG(LogTemp, Warning, TEXT("SpawnItem Failed: Location Occupied or Invalid."));
			return nullptr;
		}
	}

	// 4. 生成 Actor
	FVector WorldPos = FVector(Position) * BlockLen;
	
	// 计算旋转 (将 EActorDirection 转为 FRotator)
	// 这里简单映射，具体取决于你的 UChunkUtilsFunction::GetRotationFromEnum 实现
	FRotator Rot = FRotator::ZeroRotator;
	switch (Data.Direction)
	{
		case EActorDirection::Forward: Rot = FRotator(0, 0, 0); break;
		case EActorDirection::Right:   Rot = FRotator(0, 90, 0); break;
		case EActorDirection::Backward:Rot = FRotator(0, 180, 0); break;
		case EActorDirection::Left:    Rot = FRotator(0, 270, 0); break;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AUVoxelItemBaseInstance* NewItem = GetWorld()->SpawnActor<AUVoxelItemBaseInstance>(
		AUVoxelItemBaseInstance::StaticClass(), WorldPos, Rot, SpawnParams
	);

	if (NewItem)
	{
		// 5. 初始化实例数据
		// 将 InteractableData 转回 InstanceBase (因为它们结构兼容)
		NewItem->ItemInstance.HandleId = Data.HandleId;
		NewItem->ItemInstance.ItemTag = StaticData->ItemTag;
		NewItem->ItemInstance.Quantity = Data.Quantity;
		NewItem->ItemInstance.Direction = Data.Direction;
		
		// 注入静态数据引用
		NewItem->UpdateItemInform(StaticData);
		
		// 加载模型与初始化
		NewItem->InitiatePlace();

		// 6. [核心] 注册空间索引 (占用锁定)
		for (const FIntVector& Cell : OccupiedCells)
		{
			GridToItemMap.Add(Cell, NewItem);
		}
		// 将占用的格子存回 Actor 方便销毁时查询
		NewItem->OccupiedBlock = OccupiedCells;

		// 7. [交互] 注册到 Area 子系统 (逻辑归属)
		if (AreaSys)
		{
			// 查询中心点所在的区域
			int32 AreaID = AreaSys->GetAreaIndexByPosition(Position);
			if (AreaID != -1)
			{
				if (AVoxelAreaBaseInstance* Area = AreaSys->GetAreaActor(AreaID))
				{
					// 区域记录物品
					Area->RegisterFurnitureItem(NewItem);
					// 物品记录区域
					NewItem->ItemInstance.ParentArea = Area;
					UE_LOG(LogTemp, Log, TEXT("Item spawned and registered to Area: %d"), AreaID);
				}
			}
		}

		// 8. [切面] 应用当前切面参数到新家具（spawn hook）
		// 确保新家具立即响应切面虚化（无需等待下次 ApplySliceVisuals）
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (UVoxelPreviewComponent* VisualUI = PC->FindComponentByClass<UVoxelPreviewComponent>())
			{
				VisualUI->ApplySliceToNewActor(NewItem);
			}
		}

		return NewItem;
	}

	return nullptr;
}

bool UVoxelItemWorldSubsystem::RemoveItemAtGrid(FIntVector GridPos)
{
	// 1. 查找是否存在物品
	AUVoxelItemBaseInstance** FoundPtr = GridToItemMap.Find(GridPos);
	if (!FoundPtr) return false;

	AUVoxelItemBaseInstance* TargetItem = *FoundPtr;
	if (!IsValid(TargetItem)) return false;

	// 2. 获取它占用的所有格子 (从 Actor 缓存)
	TArray<FIntVector> CellsToClear = TargetItem->OccupiedBlock;
	if (CellsToClear.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveItemAtGrid: Item has no OccupiedBlock data"));
		return false;
	}

	// 3. 通知 Area 移除引用（在 Actor 销毁前清理逻辑归属）
	if (TargetItem->ItemInstance.ParentArea)
	{
		TargetItem->ItemInstance.ParentArea->UnregisterFurnitureItem(TargetItem);
		TargetItem->ItemInstance.ParentArea = nullptr;
	}

	// 4. 清理空间索引（GridToItemMap）
	for (const FIntVector& Cell : CellsToClear)
	{
		GridToItemMap.Remove(Cell);
	}

	// 5. 销毁 Actor
	TargetItem->Destroy();
	
	UE_LOG(LogTemp, Log, TEXT("Item removed at %s, cleared %d cells"), *GridPos.ToString(), CellsToClear.Num());
	return true;
}

AUVoxelItemBaseInstance* UVoxelItemWorldSubsystem::GetItemAtGrid(FIntVector GridPos)
{
	if (AUVoxelItemBaseInstance* const* Found = GridToItemMap.Find(GridPos))
	{
		if (IsValid(*Found))
		{
			return *Found;
		}
	}
	return nullptr;
}

bool UVoxelItemWorldSubsystem::CheckIsSpaceAvailable(FIntVector OriginPos, EActorDirection Direction, const FVoxelNormalItemBase* StaticData)
{
	if (!BlockSys || !StaticData) return false;

	// 获取 BlockLength（CalculateOccupiedCells 需要）
	float BlockLen = 100.0f;
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UVoxelGameInstance* VGI = Cast<UVoxelGameInstance>(GI))
			BlockLen = VGI->WorldConfig.BlockLength;
	}

	TArray<FIntVector> Cells = CalculateOccupiedCells(OriginPos, Direction, StaticData, BlockLen);

	for (const FIntVector& Cell : Cells)
	{
		// 1. 检查是否超出世界边界
		if (!BlockSys->IsBlockIndexInScene(Cell)) return false;

		// 2. 检查 Block 层是否为空气 (Air)
		// 注意：这里假设 0 或者特定 Tag 是空气。请根据 CoreTags 修改。
		// 如果你想允许家具放在 "草" 上但不能放在 "墙" 里，这里需要更复杂的 Tag 判断
		int32 BlockType = BlockSys->GetBlockType(UChunkUtilsFunction::GetBlockIdByIndex(Cell, BlockSys->WorldDimension));
		if (BlockType != EBlockId::Air)
		{
			return false; // 已经被方块占据
		}

		// 3. 检查 Item 层是否已被其他家具占据
		if (GridToItemMap.Contains(Cell))
		{
			return false; // 已经被其他家具占据
		}
	}

	return true;
}

TArray<FIntVector> UVoxelItemWorldSubsystem::CalculateOccupiedCells(const FIntVector Origin, const EActorDirection Dir, const FVoxelNormalItemBase* StaticData, float BlockLength) 
{
	TArray<FIntVector> Result;
	if (!StaticData)
	{
		return Result;
	}

	// 优先使用数据资产配置的 OccupiedBlocks（支持不规则形状，如 L 形家具）
	if (StaticData->OccupiedBlocks.Num() > 0)
	{
		for (const FIntVector& Offset : StaticData->OccupiedBlocks)
		{
			// 旋转偏移：显式转换传入参数，并将结果显式转回 FIntVector
			FIntVector RotatedOffset = FIntVector(UChunkUtilsFunction::GetRotationVolume(Dir, FVector(Offset)));
			Result.Add(Origin + RotatedOffset);
		}
		return Result;
	}

	// OccupiedBlocks 未配置 → 用 Volume 自动生成矩形占用（与包围盒预览一致）
	// GetVoxelVolume 返回 (VoxelX, -VoxelY, VoxelZ)，约定 X+ / Y- / Z+ 方向延伸
	const FIntVector VoxelVol = StaticData->GetVoxelVolume(BlockLength);
	const int32 CountX = FMath::Max(1, VoxelVol.X);
	const int32 CountY = FMath::Max(1, FMath::Abs(VoxelVol.Y));
	const int32 CountZ = FMath::Max(1, VoxelVol.Z);

	// 默认局部偏移（Forward 朝向）：(x, -y, z)
	// 旋转后用 GetRotationVolume 统一处理（与 OccupiedBlocks 分支一致，支持 4 个方向）
	for (int32 x = 0; x < CountX; ++x)
	{
		for (int32 y = 0; y < CountY; ++y)
		{
			for (int32 z = 0; z < CountZ; ++z)
			{
				const FIntVector LocalOffset(x, -y, z);
				const FIntVector RotatedOffset = FIntVector(UChunkUtilsFunction::GetRotationVolume(Dir, FVector(LocalOffset)));
				Result.Add(Origin + RotatedOffset);
			}
		}
	}

	// 兜底：若 Volume 为零，至少占 Origin 一格
	if (Result.Num() == 0)
	{
		Result.Add(Origin);
	}
	return Result;
}

// =========================================================
// Modify API：旋转 / 平移 / 替换类型
// =========================================================

bool UVoxelItemWorldSubsystem::RotateFurniture(AUVoxelItemBaseInstance* ItemActor, EActorDirection NewDirection)
{
	if (!ItemActor || !ConfigSys) return false;

	// 跳过相同朝向
	if (ItemActor->ItemInstance.Direction == NewDirection) return true;

	// 获取静态数据 + BlockLength
	FVoxelNormalItemBase* StaticData = ConfigSys->GetItemTypeById<FVoxelNormalItemBase>(ItemActor->ItemInstance.HandleId);
	if (!StaticData) return false;

	float BlockLen = 100.0f;
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UVoxelGameInstance* VGI = Cast<UVoxelGameInstance>(GI))
			BlockLen = VGI->WorldConfig.BlockLength;
	}

	// 计算旋转后的占用格子
	const FIntVector Origin = ItemActor->OccupiedBlock.Num() > 0 ? ItemActor->OccupiedBlock[0] : FIntVector::ZeroValue;
	TArray<FIntVector> NewCells = CalculateOccupiedCells(Origin, NewDirection, StaticData, BlockLen);

	// 清理旧占用（临时释放，让校验通过）
	TArray<FIntVector> OldCells = ItemActor->OccupiedBlock;
	for (const FIntVector& Cell : OldCells)
	{
		GridToItemMap.Remove(Cell);
	}

	// 校验新占用是否合法
	for (const FIntVector& Cell : NewCells)
	{
		if (!BlockSys->IsBlockIndexInScene(Cell)) goto FailRestore;
		const int32 BlockType = BlockSys->GetBlockType(UChunkUtilsFunction::GetBlockIdByIndex(Cell, BlockSys->WorldDimension));
		if (BlockType != EBlockId::Air) goto FailRestore;
		if (GridToItemMap.Contains(Cell)) goto FailRestore;
	}

	// 校验通过：应用新占用
	for (const FIntVector& Cell : NewCells)
	{
		GridToItemMap.Add(Cell, ItemActor);
	}
	ItemActor->OccupiedBlock = NewCells;
	ItemActor->ItemInstance.Direction = NewDirection;

	// 更新视觉（旋转 Yaw）
	FRotator Rot = FRotator::ZeroRotator;
	switch (NewDirection)
	{
	case EActorDirection::Forward:  Rot = FRotator(0, 0, 0);   break;
	case EActorDirection::Right:    Rot = FRotator(0, 90, 0);  break;
	case EActorDirection::Backward: Rot = FRotator(0, 180, 0); break;
	case EActorDirection::Left:     Rot = FRotator(0, 270, 0); break;
	}
	ItemActor->SetActorRotation(Rot);

	UE_LOG(LogTemp, Log, TEXT("[ItemWorld] RotateFurniture: %s -> dir=%d, cells=%d"),
		*ItemActor->GetName(), (int32)NewDirection, NewCells.Num());
	return true;

FailRestore:
	// 失败：恢复旧占用
	for (const FIntVector& Cell : OldCells)
	{
		GridToItemMap.Add(Cell, ItemActor);
	}
	UE_LOG(LogTemp, Warning, TEXT("[ItemWorld] RotateFurniture: space not available"));
	return false;
}

bool UVoxelItemWorldSubsystem::TranslateFurniture(AUVoxelItemBaseInstance* ItemActor, const FIntVector& DeltaPos)
{
	if (!ItemActor || !ConfigSys) return false;
	if (DeltaPos == FIntVector::ZeroValue) return true;

	FVoxelNormalItemBase* StaticData = ConfigSys->GetItemTypeById<FVoxelNormalItemBase>(ItemActor->ItemInstance.HandleId);
	if (!StaticData) return false;

	float BlockLen = 100.0f;
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UVoxelGameInstance* VGI = Cast<UVoxelGameInstance>(GI))
			BlockLen = VGI->WorldConfig.BlockLength;
	}

	const FIntVector OldOrigin = ItemActor->OccupiedBlock.Num() > 0 ? ItemActor->OccupiedBlock[0] : FIntVector::ZeroValue;
	const FIntVector NewOrigin = OldOrigin + DeltaPos;

	// 计算新占用格子（用原朝向 + 新原点）
	TArray<FIntVector> NewCells = CalculateOccupiedCells(NewOrigin, ItemActor->ItemInstance.Direction, StaticData, BlockLen);

	// 清理旧占用
	TArray<FIntVector> OldCells = ItemActor->OccupiedBlock;
	for (const FIntVector& Cell : OldCells)
	{
		GridToItemMap.Remove(Cell);
	}

	// 校验新占用
	for (const FIntVector& Cell : NewCells)
	{
		if (!BlockSys->IsBlockIndexInScene(Cell)) goto FailRestore;
		const int32 BlockType = BlockSys->GetBlockType(UChunkUtilsFunction::GetBlockIdByIndex(Cell, BlockSys->WorldDimension));
		if (BlockType != EBlockId::Air) goto FailRestore;
		if (GridToItemMap.Contains(Cell)) goto FailRestore;
	}

	// 应用
	for (const FIntVector& Cell : NewCells)
	{
		GridToItemMap.Add(Cell, ItemActor);
	}
	ItemActor->OccupiedBlock = NewCells;

	// 更新世界位置
	ItemActor->SetActorLocation(FVector(NewOrigin) * BlockLen);

	UE_LOG(LogTemp, Log, TEXT("[ItemWorld] TranslateFurniture: %s moved by %s"),
		*ItemActor->GetName(), *DeltaPos.ToString());
	return true;

FailRestore:
	for (const FIntVector& Cell : OldCells)
	{
		GridToItemMap.Add(Cell, ItemActor);
	}
	UE_LOG(LogTemp, Warning, TEXT("[ItemWorld] TranslateFurniture: space not available"));
	return false;
}

AUVoxelItemBaseInstance* UVoxelItemWorldSubsystem::ReplaceFurnitureType(AUVoxelItemBaseInstance* ItemActor, int32 NewItemId)
{
	if (!ItemActor || NewItemId == 0) return nullptr;

	// 记录原状态
	const FIntVector Origin = ItemActor->OccupiedBlock.Num() > 0 ? ItemActor->OccupiedBlock[0] : FIntVector::ZeroValue;
	const EActorDirection Dir = ItemActor->ItemInstance.Direction;
	const int32 Quantity = ItemActor->ItemInstance.Quantity;

	// 销毁旧家具
	if (!RemoveItemAtGrid(Origin))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ItemWorld] ReplaceFurnitureType: failed to remove old item"));
		return nullptr;
	}

	// 生成新家具
	FInteractableData Data;
	Data.HandleId = NewItemId;
	Data.Quantity = Quantity;
	Data.Direction = Dir;

	AUVoxelItemBaseInstance* NewItem = SpawnItemAtGrid(Origin, Data, false);
	if (NewItem)
	{
		UE_LOG(LogTemp, Log, TEXT("[ItemWorld] ReplaceFurnitureType: %d -> %d at %s"),
			ItemActor->ItemInstance.HandleId, NewItemId, *Origin.ToString());
	}
	return NewItem;
}


