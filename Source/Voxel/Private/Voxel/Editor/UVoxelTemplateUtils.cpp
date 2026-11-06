#include "Voxel/Editor/UVoxelTemplateUtils.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelItemWorldSubsystem.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Core/CoreTags.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Data/Item/InteractableData.h"
#include "Item/VoxelItemBaseInstance.h"
#include "Voxel/Utils/ChunkMeshData.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Item/VoxelBlockItemBase.h"
#include "GameManager/GameInstanceManager/VoxelTextureRegistry.h"

// =============================================================
// PlaceTemplate
// =============================================================
bool UVoxelTemplateUtils::PlaceTemplate(
	UVoxelBlockWorldSubsystem* BlockSys,
	UVoxelItemWorldSubsystem* ItemWorld,
	UConfigDataSubsystem* ConfigSys,
	const FVoxelTemplate& Template,
	FIntVector Origin,
	EActorDirection Direction,
	const TMap<FString, FString>& Remap)
{
	if (!BlockSys || !ConfigSys) return false;

	FVoxelEditModification BlockMod;
	int32 BlockCount = 0;
	int32 FurnitureCount = 0;
	int32 RemappedCount = 0;

	for (const auto& Pair : Template.Entries)
	{
		const FIntVector& RelPos = Pair.Key;
		const FVoxelInstanceEntry& Entry = Pair.Value;

		if (!Entry.IsValid() || Entry.IsAir()) continue;

		FIntVector WorldPos = Origin + RotateOffset(RelPos, Direction);
		FVoxelInstanceEntry Inst = Entry;
		Inst.Direction = ComposeDirection(Entry.Direction, Direction);

		// 应用素材替换表（原 FullId → 新 FullId）
		FString FinalFullId = Inst.FullId;
		if (const FString* NewFullId = Remap.Find(Entry.FullId))
		{
			FinalFullId = *NewFullId;
			++RemappedCount;
		}

		// 由 FullId 解析运行时句柄
		const int32 Handle = ConfigSys->GetHandleByFullId(FinalFullId);
		if (Handle == EBlockId::Invalid)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Template] PlaceTemplate: FullId='%s' not found in ItemMap (RelPos=%s, WorldPos=%s). Entry skipped."),
				*FinalFullId, *RelPos.ToString(), *WorldPos.ToString());
			continue;
		}
		Inst.HandleId = Handle;
		Inst.FullId = FinalFullId;

		const FVoxelItemBase* Item = ConfigSys->GetItemTypeById<FVoxelItemBase>(Handle);
		if (!Item)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Template] PlaceTemplate: FullId='%s' (Handle=%d) not found in ItemMap (RelPos=%s, WorldPos=%s). Entry skipped."),
				*FinalFullId, Handle, *RelPos.ToString(), *WorldPos.ToString());
			continue;
		}

		if (Item->ItemTag.MatchesTag(FCoreTags::Item_Type_Block))
		{
			BlockMod.Add(WorldPos, Handle);
			++BlockCount;
		}
		else if (Item->ItemTag.MatchesTag(FCoreTags::Item_Type_BuildItem))
		{
			if (ItemWorld)
			{
				FInteractableData Data(EInteractableType::Place, Handle, 1);
				Data.Direction = Inst.Direction;
				AUVoxelItemBaseInstance* SpawnedItem = ItemWorld->SpawnItemAtGrid(WorldPos, Data, true);
				if (SpawnedItem)
				{
					++FurnitureCount;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[Template] PlaceTemplate: SpawnItemAtGrid failed at WorldPos=%s, FullId='%s'"),
						*WorldPos.ToString(), *FinalFullId);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Template] PlaceTemplate: FullId='%s' has unsupported ItemTag=%s (RelPos=%s). Entry skipped."),
				*FinalFullId, *Item->ItemTag.ToString(), *RelPos.ToString());
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[TemplatePlace] Apply: Origin=%s Direction=%d Blocks=%d Furniture=%d Remapped=%d RemapEntries=%d"),
		*Origin.ToString(), (int32)Direction, BlockCount, FurnitureCount, RemappedCount, Remap.Num());

	BlockSys->BatchModifyVoxelData_Modification(BlockMod);
	return true;
}

// =============================================================
// GetDistinctMaterials - 统计模板内去重的可替换素材（返回 FullId 列表）
// =============================================================
TArray<FString> UVoxelTemplateUtils::GetDistinctMaterials(const FVoxelTemplate& Template, UConfigDataSubsystem* ConfigSys)
{
	TArray<FString> Result;
	if (!ConfigSys) return Result;

	TSet<FString> Seen;
	for (const auto& Pair : Template.Entries)
	{
		const FVoxelInstanceEntry& Entry = Pair.Value;
		if (!Entry.IsValid() || Entry.IsAir()) continue;
		if (Seen.Contains(Entry.FullId)) continue;

		const int32 Handle = ConfigSys->GetHandleByFullId(Entry.FullId);
		if (Handle == EBlockId::Invalid) continue;

		const FVoxelItemBase* Item = ConfigSys->GetItemTypeById<FVoxelItemBase>(Handle);
		if (!Item) continue;

		// 仅允许替换 Block / BuildItem 类型
		if (Item->ItemTag.MatchesTag(FCoreTags::Item_Type_Block) ||
			Item->ItemTag.MatchesTag(FCoreTags::Item_Type_BuildItem))
		{
			Seen.Add(Entry.FullId);
			Result.Add(Entry.FullId);
		}
	}

	// 稳定顺序：按 FullId 字符串排序，避免每次遍历 TMap 顺序不定导致 UI 行乱跳
	Result.Sort();
	return Result;
}

// =============================================================
// GetCenteredPlacementOrigin - 中心点转放置原点
// =============================================================
FIntVector UVoxelTemplateUtils::GetCenteredPlacementOrigin(const FVoxelTemplate& Template, FIntVector CenterIndex)
{
	if (Template.Entries.Num() == 0)
	{
		return CenterIndex;
	}

	FIntVector TmplMin = Template.Size;
	FIntVector TmplMax = FIntVector::ZeroValue;
	for (const auto& Pair : Template.Entries)
	{
		const FIntVector& Pos = Pair.Key;
		TmplMin = FIntVector(FMath::Min(TmplMin.X, Pos.X), FMath::Min(TmplMin.Y, Pos.Y), FMath::Min(TmplMin.Z, Pos.Z));
		TmplMax = FIntVector(FMath::Max(TmplMax.X, Pos.X), FMath::Max(TmplMax.Y, Pos.Y), FMath::Max(TmplMax.Z, Pos.Z));
	}

	const FIntVector TmplSize = TmplMax - TmplMin + FIntVector(1);
	const FIntVector CenterOffset(
		-(TmplSize.X / 2),
		-(TmplSize.Y / 2),
		0);

	return CenterIndex + CenterOffset;
}

// =============================================================
// GetRemapFilterTag - 计算某素材的"同类"过滤 Tag
// 方块 → Item.Type.Block（任意方块互换）
// 家具 → 其在 Item.Type.BuildItem 下的直接子分类（如 Chair 只换 Chair）
// =============================================================
FGameplayTag UVoxelTemplateUtils::GetRemapFilterTag(const FString& FullId, UConfigDataSubsystem* ConfigSys)
{
	if (!ConfigSys) return FGameplayTag::EmptyTag;

	const int32 Handle = ConfigSys->GetHandleByFullId(FullId);
	if (Handle == EBlockId::Invalid) return FGameplayTag::EmptyTag;

	const FVoxelItemBase* Item = ConfigSys->GetItemTypeById<FVoxelItemBase>(Handle);
	if (!Item) return FGameplayTag::EmptyTag;

	const FGameplayTag& Tag = Item->ItemTag;

	// 方块：所有方块互换
	if (Tag.MatchesTag(FCoreTags::Item_Type_Block))
	{
		return FCoreTags::Item_Type_Block;
	}

	// 家具：限制同子分类替换，取 Item.Type.BuildItem 的直接子标签
	if (Tag.MatchesTag(FCoreTags::Item_Type_BuildItem))
	{
		const FGameplayTag BuildItemRoot = FCoreTags::Item_Type_BuildItem;
		// 逐级向上找，直到父级正好是 Item.Type.BuildItem，当前级即为具体子分类
		FGameplayTag Current = Tag;
		while (Current.IsValid())
		{
			FGameplayTag Parent = Current.RequestDirectParent();
			if (Parent == BuildItemRoot)
			{
				return Current; // 具体子分类（如 Item.Type.BuildItem.Chair）
			}
			if (Parent == FCoreTags::Item_Type || !Parent.IsValid())
			{
				break;
			}
			Current = Parent;
		}
		// 无具体子分类（Tag 本身就是 Item.Type.BuildItem），退回按整个 BuildItem 过滤
		return BuildItemRoot;
	}

	return FGameplayTag::EmptyTag;
}

// =============================================================
// CaptureFromSelection
// =============================================================
FVoxelTemplate UVoxelTemplateUtils::CaptureFromSelection(
	UVoxelBlockWorldSubsystem* BlockSys,
	UVoxelItemWorldSubsystem* ItemWorld,
	UConfigDataSubsystem* ConfigSys,
	const TArray<FIntVector>& Selection,
	const FTemplateMeta& Meta)
{
	FVoxelTemplate T;
	T.TemplateGuid = FGuid::NewGuid();
	T.Version = 1;
	T.FormatVersion = TEXT("1.1");
	T.ItemName = Meta.DisplayName;
	T.ItemDescription = Meta.Description;
	T.Author = Meta.Author;
	T.Tags = Meta.Tags;
	T.CreatedTime = FDateTime::Now();
	T.ModifiedTime = T.CreatedTime;
	T.ItemTag = Meta.CategoryTag.IsValid() ? Meta.CategoryTag : FCoreTags::Item_Type_Template;

	if (Selection.Num() == 0 || !BlockSys || !ConfigSys)
	{
		return T;
	}

	// 计算包围盒
	FBox Bounds = ComputeSelectionBounds(Selection);
	FIntVector Min = FIntVector(FMath::FloorToInt(Bounds.Min.X), FMath::FloorToInt(Bounds.Min.Y), FMath::FloorToInt(Bounds.Min.Z));
	FIntVector Max = FIntVector(FMath::CeilToInt(Bounds.Max.X), FMath::CeilToInt(Bounds.Max.Y), FMath::CeilToInt(Bounds.Max.Z));
	T.Size = Max - Min + FIntVector(1, 1, 1);
	T.Anchor = FIntVector::ZeroValue;

	// 遍历选区，提取方块
	int32 BlockEntryCount = 0;
	for (const FIntVector& WorldPos : Selection)
	{
		int32 BlockHandle = BlockSys->GetBlockType(WorldPos);
		if (ShouldSkipFromTemplate(BlockHandle, ConfigSys)) continue;

		// 由 Handle 反查 FullId（持久化权威身份）
		FString FullId = ConfigSys->GetFullIdByHandle(BlockHandle);
		if (FullId.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[TemplateCapture] Block Handle=%d not found in ItemMap, skip"), BlockHandle);
			continue;
		}

		FIntVector RelPos = WorldPos - Min;
		T.Entries.Add(RelPos, FVoxelInstanceEntry::MakeBlock(FullId, BlockHandle));
		++BlockEntryCount;
	}

	// 遍历选区，提取家具（参考 VoxelSelectionEditTool::MoveSelectionContent 的去重模式）
	int32 FurnitureEntryCount = 0;
	int32 FurnitureNullCount = 0;       // GetItemAtGrid 返回 nullptr 的次数
	int32 FurnitureVisitCount = 0;      // 实际访问到的家具数（去重前）
	if (ItemWorld)
	{
		TSet<AUVoxelItemBaseInstance*> Visited;
		for (const FIntVector& WorldPos : Selection)
		{
			AUVoxelItemBaseInstance* Item = ItemWorld->GetItemAtGrid(WorldPos);
			if (!Item) { ++FurnitureNullCount; continue; }
			if (Visited.Contains(Item)) continue;
			++FurnitureVisitCount;
			Visited.Add(Item);

			// 家具 Origin = OccupiedBlock[0]
			const FIntVector Origin = Item->OccupiedBlock.IsValidIndex(0)
				? Item->OccupiedBlock[0]
				: WorldPos;

			// 由家具实例的 Handle 反查 FullId
			FString FullId = ConfigSys->GetFullIdByHandle(Item->ItemInstance.HandleId);
			if (FullId.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("[TemplateCapture] Furniture Handle=%d not found in ItemMap, skip"),
					Item->ItemInstance.HandleId);
				continue;
			}

			const FIntVector RelPos = Origin - Min;
			T.Entries.Add(RelPos, FVoxelInstanceEntry::MakeFurniture(
				FullId, Item->ItemInstance.Direction, Item->ItemInstance.HandleId));
			++FurnitureEntryCount;

			UE_LOG(LogTemp, Log, TEXT("[TemplateCapture] Furniture: FullId='%s', Dir=%d, Origin=%s, RelPos=%s, OccupiedNum=%d"),
				*FullId, (int32)Item->ItemInstance.Direction,
				*Origin.ToString(), *RelPos.ToString(),
				Item->OccupiedBlock.Num());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[TemplateCapture] ItemWorld is null, furniture not captured!"));
	}

	UE_LOG(LogTemp, Warning, TEXT("[Template] Captured: Selection=%d, Blocks=%d, Furniture=%d (visited=%d, nullGrid=%d), TotalEntries=%d, Size=%s"),
		Selection.Num(), BlockEntryCount, FurnitureEntryCount, FurnitureVisitCount, FurnitureNullCount,
		T.Entries.Num(), *T.Size.ToString());

	return T;
}

// =============================================================
// RotateOffset
// =============================================================
FIntVector UVoxelTemplateUtils::RotateOffset(FIntVector Offset, EActorDirection Direction)
{
	switch (Direction)
	{
	case EActorDirection::Forward:
		return Offset;  // 0°
	case EActorDirection::Right:
		return FIntVector(-Offset.Y, Offset.X, Offset.Z);  // 90° CW
	case EActorDirection::Backward:
		return FIntVector(-Offset.X, -Offset.Y, Offset.Z);  // 180°
	case EActorDirection::Left:
		return FIntVector(Offset.Y, -Offset.X, Offset.Z);  // 270° CW
	default:
		return Offset;
	}
}

// =============================================================
// ComposeDirection
// =============================================================
EActorDirection UVoxelTemplateUtils::ComposeDirection(EActorDirection Base, EActorDirection Apply)
{
	// 简单的 4 方向旋转组合
	int32 BaseIdx = static_cast<int32>(Base);
	int32 ApplyIdx = static_cast<int32>(Apply);
	int32 Result = (BaseIdx + ApplyIdx) % 4;
	return static_cast<EActorDirection>(Result);
}

// =============================================================
// ShouldSkipFromTemplate
// =============================================================
bool UVoxelTemplateUtils::ShouldSkipFromTemplate(int32 ItemId, UConfigDataSubsystem* ConfigSys)
{
	if (ItemId == 0) return true;  // Air
	if (ItemId < 0) return true;    // Invalid

	if (ConfigSys)
	{
		if (const FVoxelItemBase* Item = ConfigSys->GetItemTypeById<FVoxelItemBase>(ItemId))
		{
			return Item->ItemTag.MatchesTag(FCoreTags::Item_Type_Block_Marker);
		}
	}
	return false;
}

// =============================================================
// ComputeSelectionBounds
// =============================================================
FBox UVoxelTemplateUtils::ComputeSelectionBounds(const TArray<FIntVector>& Selection)
{
	if (Selection.Num() == 0) return FBox();

	FIntVector Min = Selection[0];
	FIntVector Max = Selection[0];

	for (const FIntVector& Pos : Selection)
	{
		Min = FIntVector(
			FMath::Min(Min.X, Pos.X),
			FMath::Min(Min.Y, Pos.Y),
			FMath::Min(Min.Z, Pos.Z));
		Max = FIntVector(
			FMath::Max(Max.X, Pos.X),
			FMath::Max(Max.Y, Pos.Y),
			FMath::Max(Max.Z, Pos.Z));
	}

	return FBox(FVector(Min), FVector(Max) + FVector(1.0f));
}

// =============================================================
// SaveToTmplFile - 二进制序列化模板
// =============================================================
bool UVoxelTemplateUtils::SaveToTmplFile(const FVoxelTemplate& T, const FString& FilePath)
{
	FBufferArchive Ar;

	// Header
	uint32 Magic = 0x4C505456; // "VTPL"
	Ar << Magic;

	FString FormatVer = T.FormatVersion;
	Ar << FormatVer;

	FGuid Guid = T.TemplateGuid;
	Ar << Guid;

	int32 Version = T.Version;
	Ar << Version;

	// Meta
	FString NameStr = T.ItemName.ToString();
	Ar << NameStr;

	FString DescStr = T.ItemDescription.ToString();
	Ar << DescStr;

	FString Author = T.Author;
	Ar << Author;

	FGameplayTag Tag = T.ItemTag;
	FString TagStr = Tag.ToString();
	Ar << TagStr;

	// Geometry
	FIntVector Size = T.Size;
	Ar << Size;

	FIntVector Anchor = T.Anchor;
	Ar << Anchor;

	// Entries（v1.1+：用 FullId 代替 ItemId 持久化）
	int32 EntryCount = T.Entries.Num();
	Ar << EntryCount;

	for (const auto& Pair : T.Entries)
	{
		FIntVector Pos = Pair.Key;
		Ar << Pos;

		FString FullId = Pair.Value.FullId;
		Ar << FullId;

		uint8 Dir = static_cast<uint8>(Pair.Value.Direction);
		Ar << Dir;

		FString Label = Pair.Value.CustomLabel;
		Ar << Label;
	}

	// Tags
	int32 TagCount = T.Tags.Num();
	Ar << TagCount;
	for (const FString& TagItem : T.Tags)
	{
		Ar << const_cast<FString&>(TagItem);
	}

	// Timestamps
	int64 Created = T.CreatedTime.GetTicks();
	Ar << Created;
	int64 Modified = T.ModifiedTime.GetTicks();
	Ar << Modified;

	if (Ar.IsError()) return false;

	bool bSaved = FFileHelper::SaveArrayToFile(Ar, *FilePath);
	return bSaved;
}

// =============================================================
// LoadFromTmplFile - 反序列化模板
// =============================================================
bool UVoxelTemplateUtils::LoadFromTmplFile(const FString& FilePath, FVoxelTemplate& OutT)
{
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath)) return false;

	FMemoryReader Ar(FileData, true);

	uint32 Magic;
	Ar << Magic;
	if (Magic != 0x4C505456) return false;

	Ar << OutT.FormatVersion;
	Ar << OutT.TemplateGuid;
	Ar << OutT.Version;

	FString NameStr;
	Ar << NameStr;
	OutT.ItemName = FText::FromString(NameStr);

	FString DescStr;
	Ar << DescStr;
	OutT.ItemDescription = FText::FromString(DescStr);

	Ar << OutT.Author;

	FString TagStr;
	Ar << TagStr;
	OutT.ItemTag = FGameplayTag::RequestGameplayTag(*TagStr);

	Ar << OutT.Size;
	Ar << OutT.Anchor;

	int32 EntryCount;
	Ar << EntryCount;
	OutT.Entries.Empty(EntryCount);

	for (int32 i = 0; i < EntryCount; i++)
	{
		FIntVector Pos;
		Ar << Pos;

		FVoxelInstanceEntry Entry;
		Ar << Entry.FullId;   // v1.1+：读 FullId（持久化身份）

		uint8 Dir;
		Ar << Dir;
		Entry.Direction = static_cast<EActorDirection>(Dir);

		Ar << Entry.CustomLabel;

		// ItemId 留空（-1），由调用方通过 FullIdToHandle 解析填充
		OutT.Entries.Add(Pos, Entry);
	}

	int32 TagCount;
	Ar << TagCount;
	OutT.Tags.Empty(TagCount);
	for (int32 i = 0; i < TagCount; i++)
	{
		FString TagItem;
		Ar << TagItem;
		OutT.Tags.Add(TagItem);
	}

	int64 Created, Modified;
	Ar << Created;
	Ar << Modified;
	OutT.CreatedTime = FDateTime(Created);
	OutT.ModifiedTime = FDateTime(Modified);

	OutT.SourceFilePath = FilePath;

	return !Ar.IsError();
}

// =============================================================
// ExportToVtpl - 导出为可分享格式（JSON manifest + 二进制数据）
// =============================================================
bool UVoxelTemplateUtils::ExportToVtpl(const FVoxelTemplate& T, const FString& VtplPath, UConfigDataSubsystem* ConfigSys)
{
	// 生成 manifest.json
	FString Manifest = FString::Printf(TEXT(
		"{\n"
		"  \"format\": \"VoxelTemplate\",\n"
		"  \"formatVersion\": \"%s\",\n"
		"  \"templateGuid\": \"%s\",\n"
		"  \"version\": %d,\n"
		"  \"name\": \"%s\",\n"
		"  \"description\": \"%s\",\n"
		"  \"author\": \"%s\",\n"
		"  \"size\": [%d, %d, %d],\n"
		"  \"entryCount\": %d\n"
		"}"),
		*T.FormatVersion,
		*T.TemplateGuid.ToString(),
		T.Version,
		*T.ItemName.ToString(),
		*T.ItemDescription.ToString(),
		*T.Author,
		T.Size.X, T.Size.Y, T.Size.Z,
		T.Entries.Num()
	);

	// 生成 dependencies.json（v1.1+：用 FullId 代替 ItemId）
	FString Dependencies = TEXT("{\n  \"items\": [\n");
	TSet<FString> SeenIds;
	bool bFirst = true;
	for (const auto& Pair : T.Entries)
	{
		const FString& FullId = Pair.Value.FullId;
		if (SeenIds.Contains(FullId)) continue;
		SeenIds.Add(FullId);

		FString TagStr = TEXT("Unknown");
		if (ConfigSys)
		{
			const int32 Handle = ConfigSys->GetHandleByFullId(FullId);
			if (Handle >= 0)
			{
				if (const FVoxelItemBase* Item = ConfigSys->GetItemTypeById<FVoxelItemBase>(Handle))
				{
					TagStr = Item->ItemTag.ToString();
				}
			}
		}

		if (!bFirst) Dependencies += TEXT(",\n");
		bFirst = false;
		Dependencies += FString::Printf(TEXT("    {\"fullId\": \"%s\", \"tag\": \"%s\"}"), *FullId, *TagStr);
	}
	Dependencies += TEXT("\n  ]\n}");

	// 生成 data.bin（二进制 entries，v1.1+：存 FullId）
	FBufferArchive DataAr;
	int32 EntryCount = T.Entries.Num();
	DataAr << EntryCount;
	for (const auto& Pair : T.Entries)
	{
		FIntVector Pos = Pair.Key;
		DataAr << Pos;
		FString FullId = Pair.Value.FullId;
		DataAr << FullId;
		uint8 Dir = static_cast<uint8>(Pair.Value.Direction);
		DataAr << Dir;
		FString Label = Pair.Value.CustomLabel;
		DataAr << Label;
	}

	// 写入 .vtpl 文件（简单格式：manifest + deps + data 拼接）
	// 格式：[manifestLen(4B)][manifest][depsLen(4B)][deps][data]
	FBufferArchive VtplAr;

	// Manifest
	uint32 ManifestLen = Manifest.Len();
	VtplAr << ManifestLen;
	TArray<uint8> ManifestBytes;
	ManifestBytes.Append((uint8*)TCHAR_TO_ANSI(*Manifest), ManifestLen);
	VtplAr << ManifestBytes;

	// Dependencies
	uint32 DepsLen = Dependencies.Len();
	VtplAr << DepsLen;
	TArray<uint8> DepsBytes;
	DepsBytes.Append((uint8*)TCHAR_TO_ANSI(*Dependencies), DepsLen);
	VtplAr << DepsBytes;

	// Data
	TArray<uint8> DataBytes(DataAr);
	uint32 DataLen = DataBytes.Num();
	VtplAr << DataLen;
	VtplAr << DataBytes;

	bool bSaved = FFileHelper::SaveArrayToFile(VtplAr, *VtplPath);
	if (bSaved)
	{
		UE_LOG(LogTemp, Log, TEXT("[Template] Exported to %s, entries=%d"), *VtplPath, EntryCount);
	}
	return bSaved;
}

// =============================================================
// ImportFromVtpl - 从 .vtpl 导入
// =============================================================
bool UVoxelTemplateUtils::ImportFromVtpl(const FString& VtplPath, UConfigDataSubsystem* ConfigSys, FVoxelTemplate& OutT)
{
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *VtplPath)) return false;

	FMemoryReader Ar(FileData, true);

	// Manifest
	uint32 ManifestLen;
	Ar << ManifestLen;
	TArray<uint8> ManifestBytes;
	ManifestBytes.SetNum(ManifestLen);
	Ar << ManifestBytes;
	FString Manifest(ANSI_TO_TCHAR(reinterpret_cast<const char*>(ManifestBytes.GetData())));

	// Dependencies
	uint32 DepsLen;
	Ar << DepsLen;
	TArray<uint8> DepsBytes;
	DepsBytes.SetNum(DepsLen);
	Ar << DepsBytes;
	FString Dependencies(ANSI_TO_TCHAR(reinterpret_cast<const char*>(DepsBytes.GetData())));

	// Data
	uint32 DataLen;
	Ar << DataLen;
	TArray<uint8> DataBytes;
	DataBytes.SetNum(DataLen);
	Ar << DataBytes;

	// 解析 manifest 提取 Guid 和基本信息（简单字符串查找）
	// 实际生产建议用 JSON 解析器，这里用简化方式
	int32 GuidStart = Manifest.Find(TEXT("\"templateGuid\": \""));
	if (GuidStart != INDEX_NONE)
	{
		GuidStart += FString(TEXT("\"templateGuid\": \"")).Len();
		int32 GuidEnd = Manifest.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, GuidStart);
		if (GuidEnd != INDEX_NONE)
		{
			FString GuidStr = Manifest.Mid(GuidStart, GuidEnd - GuidStart);
			FGuid::Parse(GuidStr, OutT.TemplateGuid);
		}
	}

	// 解析 data.bin（v1.1+：直接读 FullId，无需依赖映射）
	FMemoryReader DataAr(DataBytes, true);
	int32 EntryCount;
	DataAr << EntryCount;

	OutT.Entries.Empty(EntryCount);

	for (int32 i = 0; i < EntryCount; i++)
	{
		FIntVector Pos;
		DataAr << Pos;

		FString FullId;
		DataAr << FullId;

		uint8 Dir;
		DataAr << Dir;

		FString Label;
		DataAr << Label;

		FVoxelInstanceEntry Entry;
		Entry.FullId = FullId;
		Entry.Direction = static_cast<EActorDirection>(Dir);
		Entry.CustomLabel = Label;
		// ItemId 留空（-1），由调用方通过 FullIdToHandle 解析填充
		OutT.Entries.Add(Pos, Entry);
	}

	OutT.Version = 1;
	OutT.FormatVersion = TEXT("1.1");
	OutT.CreatedTime = FDateTime::Now();
	OutT.ModifiedTime = OutT.CreatedTime;

	UE_LOG(LogTemp, Log, TEXT("[Template] Imported from %s, entries=%d"), *VtplPath, OutT.Entries.Num());
	return !Ar.IsError() && !DataAr.IsError();
}

// =============================================================
// GenerateTemplatePreviewMesh - 生成模板预览网格
// =============================================================
bool UVoxelTemplateUtils::GenerateTemplatePreviewMesh(
	const FVoxelTemplate& T,
	UConfigDataSubsystem* ConfigSys,
	float BlockLength,
	FChunkMeshData& OutMeshData)
{
	if (!ConfigSys || T.Entries.Num() == 0) return false;

	// 获取 TextureRegistry 用于查询贴图索引
	UVoxelTextureRegistry* TexRegistry = nullptr;
	if (const UGameInstance* GI = ConfigSys->GetGameInstance())
	{
		TexRegistry = GI->GetSubsystem<UVoxelTextureRegistry>();
	}

	int32 VertexCount = 0;

	for (const auto& Pair : T.Entries)
	{
		const FIntVector& RelPos = Pair.Key;
		const FVoxelInstanceEntry& Entry = Pair.Value;

		if (!Entry.IsValid() || Entry.IsAir()) continue;

		// 由 FullId 解析 Handle
		const int32 Handle = ConfigSys->GetHandleByFullId(Entry.FullId);
		if (Handle == EBlockId::Invalid) continue;

		// 只预览方块
		const FVoxelItemBase* Item = ConfigSys->GetItemTypeById<FVoxelItemBase>(Handle);
		if (!Item || !Item->ItemTag.MatchesTag(FCoreTags::Item_Type_Block)) continue;

		const FVoxelBlockItemBase* BlockItem = static_cast<const FVoxelBlockItemBase*>(Item);
		if (!BlockItem) continue;

		FVector BasePos = FVector(RelPos) * BlockLength;

		// 6 个面的顶点
		const FVector Vertices[8] = {
			BasePos,                                  // 0: min
			BasePos + FVector(BlockLength, 0, 0),     // 1
			BasePos + FVector(0, BlockLength, 0),     // 2
			BasePos + FVector(BlockLength, BlockLength, 0), // 3
			BasePos + FVector(0, 0, BlockLength),     // 4
			BasePos + FVector(BlockLength, 0, BlockLength), // 5
			BasePos + FVector(0, BlockLength, BlockLength), // 6
			BasePos + FVector(BlockLength, BlockLength, BlockLength) // 7
		};

		// 通过 TextureRegistry 查询 6 面贴图索引
		const FVoxelBlockTextureSetup& TexSetup = BlockItem->BlockFragment.TextureSetup;
		int32 TexIdxBottom = TexRegistry ? TexRegistry->GetTextureIndex(TexSetup.Bottom) : 0;
		int32 TexIdxTop    = TexRegistry ? TexRegistry->GetTextureIndex(TexSetup.Top) : 0;
		int32 TexIdxSide   = TexRegistry ? TexRegistry->GetTextureIndex(TexSetup.Side) : 0;

		// 6 个面（每个面 2 个三角形），侧面统一用 TexIdxSide
		struct FFaceDef { int32 V[4]; FVector Normal; int32 TexIdx; };
		const FFaceDef Faces[6] = {
			{{0,1,3,2}, FVector(0,0,-1), TexIdxBottom},  // Bottom
			{{4,6,7,5}, FVector(0,0,1),  TexIdxTop},     // Top
			{{0,4,5,1}, FVector(0,-1,0), TexIdxSide},    // Left (Y-)
			{{2,3,7,6}, FVector(0,1,0),  TexIdxSide},    // Right (Y+)
			{{0,2,6,4}, FVector(-1,0,0), TexIdxSide},    // Back (X-)
			{{1,5,7,3}, FVector(1,0,0),  TexIdxSide},    // Forward (X+)
		};

		for (const auto& Face : Faces)
		{
			int32 BaseIdx = VertexCount;

			OutMeshData.Vertices.Append({
				Vertices[Face.V[0]], Vertices[Face.V[1]],
				Vertices[Face.V[2]], Vertices[Face.V[3]]
			});

			OutMeshData.Triangles.Append({
				BaseIdx, BaseIdx + 2, BaseIdx + 1,
				BaseIdx, BaseIdx + 3, BaseIdx + 2
			});

			OutMeshData.Normals.Append({ Face.Normal, Face.Normal, Face.Normal, Face.Normal });

			FColor Color(0, 0, 0, Face.TexIdx);
			OutMeshData.Colors.Append({ Color, Color, Color, Color });

			OutMeshData.UV0.Append({
				FVector2D(0, 0), FVector2D(1, 0),
				FVector2D(1, 1), FVector2D(0, 1)
			});

			VertexCount += 4;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Template] Preview mesh: %d verts, %d tris"),
		OutMeshData.Vertices.Num(), OutMeshData.Triangles.Num());
	return OutMeshData.Vertices.Num() > 0;
}
