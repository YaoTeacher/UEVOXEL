// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Editor/VoxelPlacementWorldSubsystem.h"
#include "Voxel/Editor/VoxelEditorData.h"  // FVoxelEditModification
#include "Voxel/Editor/VoxelHistoryData.h"  // FVoxelEdit

#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelItemWorldSubsystem.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Core/VoxelGameInstance.h"
#include "Core/Enum/GameStateEnum.h"  // EActorDirection
#include "Data/Item/VoxelItemBase.h"   // FVoxelBlockItemBase / FVoxelNormalItemBase
#include "Data/Item/InteractableData.h"  // FInteractableData
#include "Item/VoxelItemBaseInstance.h"  // AUVoxelItemBaseInstance
#include "Utils/ChunkUtilsFunction.h"
#include "Core/CoreTags.h"  // FCoreTags::Item_Type_Block / Item_Type_BuildItem

// =========================================================
// Lifecycle
// =========================================================

void UVoxelPlacementWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	BlockSys = Collection.InitializeDependency<UVoxelBlockWorldSubsystem>();
	ItemWorld = Collection.InitializeDependency<UVoxelItemWorldSubsystem>();

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			ConfigSys = GI->GetSubsystem<UConfigDataSubsystem>();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Placement] Subsystem initialized. BlockSys=%s, ItemWorld=%s, ConfigSys=%s"),
		BlockSys ? TEXT("OK") : TEXT("NULL"),
		ItemWorld ? TEXT("OK") : TEXT("NULL"),
		ConfigSys ? TEXT("OK") : TEXT("NULL"));
}

void UVoxelPlacementWorldSubsystem::Deinitialize()
{
	BlockSys = nullptr;
	ItemWorld = nullptr;
	ConfigSys = nullptr;
	Super::Deinitialize();
}

// =========================================================
// Place
// =========================================================

FPlacementHandle UVoxelPlacementWorldSubsystem::Place(int32 ItemId, FIntVector Pos, EPlacementDirection Direction)
{
	if (!ConfigSys)
	{
		UE_LOG(LogTemp, Error, TEXT("[Placement] Place failed: ConfigSys is null"));
		return FPlacementHandle::MakeEmpty();
	}

	const EPlacementType Type = GetPlacementType(ItemId);
	switch (Type)
	{
	case EPlacementType::Block:
		return PlaceBlock(ItemId, Pos);
	case EPlacementType::Furniture:
		return PlaceFurniture(ItemId, Pos, Direction);
	default:
		UE_LOG(LogTemp, Warning, TEXT("[Placement] Place failed: Unknown ItemId type %d"), ItemId);
		return FPlacementHandle::MakeEmpty();
	}
}

FPlacementHandle UVoxelPlacementWorldSubsystem::PlaceBlock(int32 ItemId, FIntVector Pos)
{
	if (!BlockSys) return FPlacementHandle::MakeEmpty();

	// 读旧值用于历史记录
	const int32 OldType = BlockSys->GetBlockType(Pos);

	// 用 BatchModifyVoxelData_Modification 写入新方块
	FVoxelEditModification Modification;
	Modification.Add(Pos, ItemId);
	Modification.AddChunkIndex(UChunkUtilsFunction::Get3DChunkIndexByBlockIndex(Pos, BlockSys->WorldDimension));
	BlockSys->BatchModifyVoxelData_Modification(Modification);

	// 记录操作
	LastOperation.OpType = EPlacementOp::Place;
	LastOperation.Handle = FPlacementHandle::MakeBlock(Pos, ItemId);
	LastOperation.BlockEdits.Empty();
	LastOperation.BlockEdits.Add(FVoxelEdit(Pos, OldType, ItemId));
	LastOperation.BeforeSnapshot = FActorSnapshot();
	LastOperation.AfterSnapshot = FActorSnapshot();
	LastOperation.Timestamp = FDateTime::Now();

	return LastOperation.Handle;
}

FPlacementHandle UVoxelPlacementWorldSubsystem::PlaceFurniture(int32 ItemId, FIntVector Pos, EPlacementDirection Direction)
{
	if (!ItemWorld || !ConfigSys) return FPlacementHandle::MakeEmpty();

	// 构造 InteractableData
	FInteractableData Data;
	Data.HandleId = ItemId;
	Data.Quantity = 1;
	Data.Direction = ToActorDirection(Direction);

	AUVoxelItemBaseInstance* NewItem = ItemWorld->SpawnItemAtGrid(Pos, Data, false);
	if (!NewItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Placement] PlaceFurniture failed at %s"), *Pos.ToString());
		return FPlacementHandle::MakeEmpty();
	}

	// 记录操作
	LastOperation.OpType = EPlacementOp::Place;
	LastOperation.Handle = FPlacementHandle::MakeFurniture(NewItem, ItemId);
	LastOperation.BlockEdits.Empty();
	LastOperation.BeforeSnapshot = FActorSnapshot();  // Place 前是空
	LastOperation.AfterSnapshot = MakeSnapshot(NewItem);
	LastOperation.Timestamp = FDateTime::Now();

	return LastOperation.Handle;
}

// =========================================================
// Remove
// =========================================================

bool UVoxelPlacementWorldSubsystem::Remove(const FPlacementHandle& Handle)
{
	if (!Handle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Placement] Remove failed: invalid handle"));
		return false;
	}

	switch (Handle.Type)
	{
	case EPlacementType::Block:
		return RemoveBlock(Handle.BlockPos);
	case EPlacementType::Furniture:
		if (AActor* Actor = Handle.Actor.Get())
		{
			return RemoveFurniture(Actor);
		}
		return false;
	default:
		return false;
	}
}

bool UVoxelPlacementWorldSubsystem::RemoveBlock(FIntVector BlockPos)
{
	if (!BlockSys) return false;

	const int32 OldType = BlockSys->GetBlockType(BlockPos);
	if (OldType == EBlockId::Air || OldType == EBlockId::Invalid)
	{
		return false;
	}

	// 改为 Air
	FVoxelEditModification Modification;
	Modification.Add(BlockPos, EBlockId::Air);
	Modification.AddChunkIndex(UChunkUtilsFunction::Get3DChunkIndexByBlockIndex(BlockPos, BlockSys->WorldDimension));
	BlockSys->BatchModifyVoxelData_Modification(Modification);

	// 记录操作
	LastOperation.OpType = EPlacementOp::Remove;
	LastOperation.Handle = FPlacementHandle::MakeBlock(BlockPos, OldType);
	LastOperation.BlockEdits.Empty();
	LastOperation.BlockEdits.Add(FVoxelEdit(BlockPos, OldType, EBlockId::Air));
	LastOperation.BeforeSnapshot = FActorSnapshot();
	LastOperation.AfterSnapshot = FActorSnapshot();
	LastOperation.Timestamp = FDateTime::Now();

	return true;
}

bool UVoxelPlacementWorldSubsystem::RemoveFurniture(AActor* Actor)
{
	if (!ItemWorld || !Actor) return false;

	AUVoxelItemBaseInstance* ItemActor = Cast<AUVoxelItemBaseInstance>(Actor);
	if (!ItemActor) return false;

	// 先记录快照（销毁前）
	const FActorSnapshot BeforeSnap = MakeSnapshot(ItemActor);

	// 委托 ItemWorld 删除（清 GridToItemMap + Area 引用 + Destroy）
	if (!ItemWorld->RemoveItemAtGrid(ItemActor->OccupiedBlock.Num() > 0 ? ItemActor->OccupiedBlock[0] : FIntVector::ZeroValue))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Placement] RemoveFurniture failed: RemoveItemAtGrid returned false"));
		return false;
	}

	// 记录操作
	LastOperation.OpType = EPlacementOp::Remove;
	LastOperation.Handle = FPlacementHandle::MakeFurniture(Actor, BeforeSnap.HandleId);
	LastOperation.BlockEdits.Empty();
	LastOperation.BeforeSnapshot = BeforeSnap;
	LastOperation.AfterSnapshot = FActorSnapshot();  // Remove 后是空
	LastOperation.Timestamp = FDateTime::Now();

	return true;
}

// =========================================================
// Modify
// =========================================================

bool UVoxelPlacementWorldSubsystem::ModifyPlacement(const FPlacementHandle& Handle, const FPlacementModification& Modification)
{
	if (!Handle.IsValid()) return false;

	switch (Handle.Type)
	{
	case EPlacementType::Block:
		return ModifyBlock(Handle.BlockPos, Modification);
	case EPlacementType::Furniture:
		if (AActor* Actor = Handle.Actor.Get())
		{
			return ModifyFurniture(Actor, Modification);
		}
		return false;
	default:
		return false;
	}
}

bool UVoxelPlacementWorldSubsystem::ModifyBlock(FIntVector BlockPos, const FPlacementModification& Mod)
{
	if (!BlockSys) return false;

	// Block 只支持 ReplaceType（改 BlockType）
	if (Mod.ModType != EPlacementModificationType::ReplaceType)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Placement] Block only supports ReplaceType modification"));
		return false;
	}

	const int32 OldType = BlockSys->GetBlockType(BlockPos);
	const int32 NewType = Mod.NewHandleId;

	FVoxelEditModification Modification;
	Modification.Add(BlockPos, NewType);
	Modification.AddChunkIndex(UChunkUtilsFunction::Get3DChunkIndexByBlockIndex(BlockPos, BlockSys->WorldDimension));
	BlockSys->BatchModifyVoxelData_Modification(Modification);

	// 记录操作
	LastOperation.OpType = EPlacementOp::Modify;
	LastOperation.Handle = FPlacementHandle::MakeBlock(BlockPos, OldType);
	LastOperation.BlockEdits.Empty();
	LastOperation.BlockEdits.Add(FVoxelEdit(BlockPos, OldType, NewType));
	LastOperation.BeforeSnapshot = FActorSnapshot();
	LastOperation.AfterSnapshot = FActorSnapshot();
	LastOperation.Timestamp = FDateTime::Now();

	return true;
}

bool UVoxelPlacementWorldSubsystem::ModifyFurniture(AActor* Actor, const FPlacementModification& Mod)
{
	if (!ItemWorld || !Actor) return false;

	AUVoxelItemBaseInstance* ItemActor = Cast<AUVoxelItemBaseInstance>(Actor);
	if (!ItemActor) return false;

	// 记录修改前快照
	const FActorSnapshot BeforeSnap = MakeSnapshot(ItemActor);

	bool bSuccess = false;
	switch (Mod.ModType)
	{
	case EPlacementModificationType::Rotate:
		bSuccess = ItemWorld->RotateFurniture(ItemActor, ToActorDirection(Mod.NewDirection));
		break;

	case EPlacementModificationType::Translate:
		bSuccess = ItemWorld->TranslateFurniture(ItemActor, Mod.Translation);
		break;

	case EPlacementModificationType::ReplaceType:
		{
			AUVoxelItemBaseInstance* NewItem = ItemWorld->ReplaceFurnitureType(ItemActor, Mod.NewHandleId);
			if (NewItem)
			{
				// 替换后旧 Actor 已销毁，Handle 指向新 Actor
				LastOperation.Handle.Actor = NewItem;
				bSuccess = true;
			}
			break;
		}
	}

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Placement] ModifyFurniture failed: ModType=%d"),
			(int32)Mod.ModType);
		return false;
	}

	// 记录操作
	LastOperation.OpType = EPlacementOp::Modify;
	LastOperation.Handle = FPlacementHandle::MakeFurniture(Actor, BeforeSnap.HandleId);
	LastOperation.BlockEdits.Empty();
	LastOperation.BeforeSnapshot = BeforeSnap;
	LastOperation.AfterSnapshot = MakeSnapshot(ItemActor);
	LastOperation.Timestamp = FDateTime::Now();

	return true;
}

// =========================================================
// Batch
// =========================================================

TArray<FPlacementHandle> UVoxelPlacementWorldSubsystem::BatchPlace(const TArray<FPlacementRequest>& Requests)
{
	TArray<FPlacementHandle> Results;
	Results.Reserve(Requests.Num());

	// 简单实现：循环调 Place
	// 优化点：Block 请求合并为一个 BatchModify（减少网格重建）
	// 当前阶段保持简单，后续按需优化
	for (const FPlacementRequest& Req : Requests)
	{
		Results.Add(Place(Req.HandleId, Req.Position, Req.Direction));
	}

	return Results;
}

int32 UVoxelPlacementWorldSubsystem::BatchRemove(const TArray<FPlacementHandle>& Handles)
{
	int32 SuccessCount = 0;
	for (const FPlacementHandle& Handle : Handles)
	{
		if (Remove(Handle))
		{
			++SuccessCount;
		}
	}
	return SuccessCount;
}

// =========================================================
// Query
// =========================================================

FPlacementHandle UVoxelPlacementWorldSubsystem::QueryAt(FIntVector Pos) const
{
	// 1. 先查 Actor（家具跨格，优先）
	if (ItemWorld)
	{
		if (AUVoxelItemBaseInstance* Item = ItemWorld->GetItemAtGrid(Pos))
		{
			return FPlacementHandle::MakeFurniture(Item, Item->ItemInstance.HandleId);
		}
	}

	// 2. 再查 Block
	if (BlockSys)
	{
		const int32 BlockType = BlockSys->GetBlockType(Pos);
		if (BlockType != EBlockId::Air && BlockType != EBlockId::Invalid)
		{
			return FPlacementHandle::MakeBlock(Pos, BlockType);
		}
	}

	return FPlacementHandle::MakeEmpty();
}

bool UVoxelPlacementWorldSubsystem::CanPlaceAt(int32 ItemId, FIntVector Pos, EPlacementDirection Direction) const
{
	if (!ConfigSys) return false;

	const EPlacementType Type = GetPlacementType(ItemId);
	switch (Type)
	{
	case EPlacementType::Block:
		// Block 校验：边界（IsBlockIndexInScene 当前总是返回 true，保留扩展点）
		return BlockSys ? BlockSys->IsBlockIndexInScene(Pos) : false;

	case EPlacementType::Furniture:
		{
			// Furniture 校验：调 ItemWorld 的 CheckIsSpaceAvailable
			if (!ItemWorld) return false;
			const FVoxelNormalItemBase* StaticData = ConfigSys->GetItemTypeById<FVoxelNormalItemBase>(ItemId);
			if (!StaticData) return false;
			return ItemWorld->CheckIsSpaceAvailable(Pos, ToActorDirection(Direction), StaticData);
		}

	default:
		return false;
	}
}

// =========================================================
// History
// =========================================================

bool UVoxelPlacementWorldSubsystem::ExecuteUndo(const FPlacementOperation& Op)
{
	if (Op.IsEmpty()) return false;

	switch (Op.OpType)
	{
	case EPlacementOp::Place:
		// 撤销放置 = 删除
		return Remove(Op.Handle);

	case EPlacementOp::Remove:
		// 撤销删除 = 重新放置
		if (Op.Handle.IsBlock() && Op.BlockEdits.Num() > 0)
		{
			const FVoxelEdit& Edit = Op.BlockEdits[0];
			return Place(Edit.OldHandleId, Op.Handle.BlockPos, EPlacementDirection::Forward).IsValid();
		}
		if (Op.Handle.IsFurniture() && !Op.BeforeSnapshot.IsEmpty())
		{
			const FActorSnapshot& Snap = Op.BeforeSnapshot;
			return Place(Snap.HandleId, Snap.Origin, Snap.Direction).IsValid();
		}
		return false;

	case EPlacementOp::Modify:
		// 撤销修改 = 用 Before 数据恢复
		if (Op.Handle.IsBlock() && Op.BlockEdits.Num() > 0)
		{
			const FVoxelEdit& Edit = Op.BlockEdits[0];
			return Place(Edit.OldHandleId, Op.Handle.BlockPos, EPlacementDirection::Forward).IsValid();
		}
		if (Op.Handle.IsFurniture() && !Op.BeforeSnapshot.IsEmpty())
		{
			const FActorSnapshot& Snap = Op.BeforeSnapshot;
			return Place(Snap.HandleId, Snap.Origin, Snap.Direction).IsValid();
		}
		return false;
	}

	return false;
}

bool UVoxelPlacementWorldSubsystem::ExecuteRedo(const FPlacementOperation& Op)
{
	if (Op.IsEmpty()) return false;

	switch (Op.OpType)
	{
	case EPlacementOp::Place:
		// 重做放置 = 重新放置
		if (Op.Handle.IsBlock() && Op.BlockEdits.Num() > 0)
		{
			const FVoxelEdit& Edit = Op.BlockEdits[0];
			return Place(Edit.NewHandleId, Op.Handle.BlockPos, EPlacementDirection::Forward).IsValid();
		}
		if (Op.Handle.IsFurniture() && !Op.AfterSnapshot.IsEmpty())
		{
			const FActorSnapshot& Snap = Op.AfterSnapshot;
			return Place(Snap.HandleId, Snap.Origin, Snap.Direction).IsValid();
		}
		return false;

	case EPlacementOp::Remove:
		// 重做删除 = 删除
		return Remove(Op.Handle);

	case EPlacementOp::Modify:
		// 重做修改 = 用 After 数据应用
		if (Op.Handle.IsBlock() && Op.BlockEdits.Num() > 0)
		{
			const FVoxelEdit& Edit = Op.BlockEdits[0];
			return Place(Edit.NewHandleId, Op.Handle.BlockPos, EPlacementDirection::Forward).IsValid();
		}
		if (Op.Handle.IsFurniture() && !Op.AfterSnapshot.IsEmpty())
		{
			const FActorSnapshot& Snap = Op.AfterSnapshot;
			return Place(Snap.HandleId, Snap.Origin, Snap.Direction).IsValid();
		}
		return false;
	}

	return false;
}

// =========================================================
// Internal
// =========================================================

EPlacementType UVoxelPlacementWorldSubsystem::GetPlacementType(int32 ItemId) const
{
	if (!ConfigSys) return EPlacementType::Empty;

	// 用 ItemTag 识别类型（不能用 GetItemTypeById<T> + static_cast，因为 static_cast 不做运行时检查）
	FVoxelItemBase* Item = ConfigSys->GetItemTypeById<FVoxelItemBase>(ItemId);
	if (!Item) return EPlacementType::Empty;

	// 家具优先匹配（BuildItem 是 Block 的子标签？不，它们是平级的）
	if (Item->ItemTag.MatchesTag(FCoreTags::Item_Type_BuildItem))
	{
		return EPlacementType::Furniture;
	}
	if (Item->ItemTag.MatchesTag(FCoreTags::Item_Type_Block))
	{
		return EPlacementType::Block;
	}
	return EPlacementType::Empty;
}

EActorDirection UVoxelPlacementWorldSubsystem::ToActorDirection(EPlacementDirection Dir)
{
	switch (Dir)
	{
	case EPlacementDirection::Forward:   return EActorDirection::Forward;
	case EPlacementDirection::Right:     return EActorDirection::Right;
	case EPlacementDirection::Backward:  return EActorDirection::Backward;
	case EPlacementDirection::Left:      return EActorDirection::Left;
	}
	return EActorDirection::Forward;
}

EPlacementDirection UVoxelPlacementWorldSubsystem::FromActorDirection(EActorDirection Dir)
{
	switch (Dir)
	{
	case EActorDirection::Forward:   return EPlacementDirection::Forward;
	case EActorDirection::Right:     return EPlacementDirection::Right;
	case EActorDirection::Backward:  return EPlacementDirection::Backward;
	case EActorDirection::Left:      return EPlacementDirection::Left;
	}
	return EPlacementDirection::Forward;
}

FActorSnapshot UVoxelPlacementWorldSubsystem::MakeSnapshot(AActor* Actor) const
{
	FActorSnapshot Snap;
	if (!Actor) return Snap;

	AUVoxelItemBaseInstance* ItemActor = Cast<AUVoxelItemBaseInstance>(Actor);
	if (!ItemActor) return Snap;

	Snap.HandleId = ItemActor->ItemInstance.HandleId;
	Snap.Direction = FromActorDirection(ItemActor->ItemInstance.Direction);
	Snap.OccupiedCells = ItemActor->OccupiedBlock;
	Snap.Quantity = ItemActor->ItemInstance.Quantity;

	// Origin 从 OccupiedCells 取第一个（或从 Actor 位置反算）
	if (ItemActor->OccupiedBlock.Num() > 0)
	{
		Snap.Origin = ItemActor->OccupiedBlock[0];
	}
	else
	{
		// 兜底：从 Actor 世界位置反算
		if (UWorld* World = GetWorld())
		{
			if (UVoxelGameInstance* GI = World->GetGameInstance<UVoxelGameInstance>())
			{
				const float BlockLen = GI->WorldConfig.BlockLength;
				Snap.Origin = UChunkUtilsFunction::GetBlockIndexByPosition(ItemActor->GetActorLocation(), BlockLen);
			}
		}
	}

	return Snap;
}
