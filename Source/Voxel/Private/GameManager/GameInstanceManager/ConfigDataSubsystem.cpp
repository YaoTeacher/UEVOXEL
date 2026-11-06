// Fill out your copyright notice in the Description page of Project Settings.


#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "GameManager/GameInstanceManager/VoxelTextureRegistry.h"
#include "DataRegistrySubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/Creature/CreatureEventDefinitions.h"
#include "Engine/AssetManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Voxel/Editor/UVoxelTemplateUtils.h"

void UConfigDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{

	Super::Initialize(Collection);
	UE_LOG(LogTemp, Warning, TEXT("START!!!"));
	ItemMap.Empty();
	AreaMap.Empty();
	RuntimeTemplates.Empty();
	LoadAllItemType();
	LoadAllAreaType();
	LoadTagUIConfig();
	CacheRegistryTags();
	LoadAllLifeEvents();
	ScanAndLoadAllFormulas();
	ScanAndLoadAllTemplates();
}

void UConfigDataSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

bool UConfigDataSubsystem::SaveAllData(const FString& FilePath)
{
	return FPaths::FileExists(FilePath);
}

bool UConfigDataSubsystem::LoadAllData(const FString& FilePath)
{
	return FPaths::FileExists(FilePath);
}

void UConfigDataSubsystem::LoadAllItemType()
{
	// 1. 前置校验
	if (!ItemTypeRegistry)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid registry"));
		return;
	}
	// 清空现有数据
	ItemMap.Empty();
	FullIdToHandle.Empty();
    
	// 获取 DataRegistry 中的所有项名
	TArray<FName> ItemNames;
	ItemTypeRegistry->GetItemNames(ItemNames);
	if (ItemNames.Num() <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No items found in AreaTypeRegistry"));
		return;
	}
    
	// 遍历所有项并加载到 TMap
	for (FName& ItemName : ItemNames)
	{
		// 创建 DataRegistryId
		FDataRegistryId ItemId("DR_Item",ItemName);

		// 获取缓存的项目数据
		// 注意：GetCachedItem返回const指针，但DataRegistry管理的数据在运行时是可变的
		if (const FVoxelItemBase* RegistryItem = ItemTypeRegistry->GetCachedItem<FVoxelItemBase>(ItemId))
		{
			FVoxelItemBase* MutableItem = const_cast<FVoxelItemBase*>(RegistryItem);

			// 身份名称段为空时，过渡 fallback 到 RowName（保证编译期不被全表卡死）
			if (MutableItem->IdentityName.IsEmpty())
			{
				MutableItem->IdentityName = ItemName.ToString();
			}
			if (MutableItem->Source.IsEmpty())
			{
				MutableItem->Source = TEXT("core");
			}

			// 由完整身份 ID 决定性计算运行时句柄
			const FString FullId = MutableItem->GetFullId();
			const int32 Handle = ComputeHandle(FullId);
			MutableItem->HandleId = Handle;

			// hash 输出碰撞检测：不同身份 hash 出同一句柄时强制改名
			if (ItemMap.Contains(Handle))
			{
				UE_LOG(LogTemp, Fatal, TEXT("[Item] Handle hash collision: FullId='%s' Handle=%d，请修改 IdentityName 以避免碰撞"), *FullId, Handle);
				continue;
			}

			ItemMap.Add(Handle, MutableItem);
			FullIdToHandle.Add(FullId, Handle);

			UE_LOG(LogTemp, Log, TEXT("Loaded item: FullId='%s' Handle=%d Name=%s"), *FullId, Handle, *MutableItem->ItemName.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to get item data for: %s"), *ItemName.ToString());
		}
	}
    
	UE_LOG(LogTemp, Log, TEXT("Successfully loaded %d item types to map"), ItemMap.Num());

	// 物品加载完成后，构建 Texture2DArray（扫描所有方块的贴图配置）
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UVoxelTextureRegistry* TexRegistry = GI->GetSubsystem<UVoxelTextureRegistry>())
		{
			TexRegistry->BuildTextureArrays(ItemMap);
		}
	}
}

void UConfigDataSubsystem::LoadAllAreaType()
{
	if (!AreaTypeRegistry)
	{
		UE_LOG(LogTemp, Warning, TEXT("AreaTypeRegistry is not set!"));
		return;
	}
    
	// 清空现有数据
	AreaMap.Empty();
    
	// 获取 DataRegistry 中的所有项名
	TArray<FName> AreaNames;
	AreaTypeRegistry->GetItemNames(AreaNames);
	if (AreaNames.Num() <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No items found in AreaTypeRegistry"));
		return;
	}
    
	// 遍历所有项并加载到 TMap
	for (FName& AreaName : AreaNames)
	{
		// 创建 DataRegistryId
		FDataRegistryId AreaId("DR_Area",AreaName);
        
		// 获取缓存的项目数据
		if (const FAreaTypeBase* RegistryItem = AreaTypeRegistry->GetCachedItem<FAreaTypeBase>(AreaId))
		{
			// 使用 AreaTypeId 作为键
			FGameplayTag key = RegistryItem->AreaTypeTag;
            
			// 检查键是否已存在
			if (AreaMap.Contains(key))
			{
				UE_LOG(LogTemp, Warning, TEXT("Duplicate key found: %s for area type: %s"), *key.ToString(), *AreaName.ToString());
				continue;
			}
            
			// 将指针存入 TMap
			AreaMap.Add(key, RegistryItem);
            
			UE_LOG(LogTemp, Log, TEXT("Loaded area type: %s with key: %s"), *RegistryItem->DisplayName.ToString(), *key.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to get item data for: %s"), *AreaName.ToString());
		}
	}
    
	UE_LOG(LogTemp, Log, TEXT("Successfully loaded %d area types to map"), AreaMap.Num());
}

void UConfigDataSubsystem::LoadAllLifeEvents()
{
    // 1. 获取 Asset Registry 模块
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    // 2. 构建过滤器 (Filter)
    FARFilter Filter;
    
    // [设置]：你的事件资产存放在哪个文件夹？
    // 请确保路径正确，"/Game" 对应 Content 目录
    FString EventSearchPath = TEXT("/Game/Resource/DataRresource/Config/EventConfig"); 
    Filter.PackagePaths.Add(*EventSearchPath);
    Filter.bRecursivePaths = true; // 包含子文件夹
    
    // [设置]：我们要找什么类？(UCreatureLifeEvent)
    #if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        Filter.ClassPaths.Add(UCreatureEvent::StaticClass()->GetClassPathName());
    #else
        Filter.ClassNames.Add(UCreatureLifeEvent::StaticClass()->GetFName());
    #endif
    
    // 3. 执行扫描
    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssets(Filter, AssetList);

    UE_LOG(LogTemp, Log, TEXT("[ConfigData] Scanning Events in [%s]. Found %d assets."), *EventSearchPath, AssetList.Num());

    // 4. 遍历并加载
    if (AssetList.Num() > 0)
    {
        // 建议先清空缓存，防止重复添加
        // CachedLifeEvents.Empty(); 

        for (const FAssetData& AssetData : AssetList)
        {
            // A. 同步加载资产
            // GetAsset() 会检查内存，如果没有则从硬盘同步加载
            UObject* LoadedObj = AssetData.GetAsset();
            
            // B. 类型转换与缓存
            if (UCreatureEvent* Event = Cast<UCreatureEvent>(LoadedObj))
            {
                // 获取 PrimaryAssetId (用于后续存档查找)
                // 确保你的 UCreatureLifeEvent 类里重写了 GetPrimaryAssetId()，如果没有，可以用 AssetData.PackageName 转 ID
                FPrimaryAssetId ID = Event->GetPrimaryAssetId();
                
                // 如果 ID 无效 (比如没重写 GetPrimaryAssetId)，手动构建一个兜底
                if (!ID.IsValid())
                {
                     ID = FPrimaryAssetId("CreatureLifeEvent", Event->GetFName());
                }

                CachedLifeEvents.Add(ID, Event);
                
                // UE_LOG(LogTemp, Log, TEXT("   -> Loaded Event: %s"), *Event->GetName());
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("[ConfigData] Successfully cached %d LifeEvents."), CachedLifeEvents.Num());
}

TMap<FGameplayTag, FText> UConfigDataSubsystem::GetAllAreaType()
{
	TMap<FGameplayTag, FText> AreaTypes;
	for(auto& Area : AreaMap)
	{
		AreaTypes.Add(Area.Key, Area.Value->DisplayName);
	}
	return AreaTypes;
}

void UConfigDataSubsystem::LoadTagUIConfig()
{
	if (!TagConfigTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ConfigData] TagConfigTable is not set!"));
		return;
	}

	TagUIMap.Empty();
	FString ContextString;
	TArray<FTagUIDefinition*> AllRows;
	
	// 读取所有行
	TagConfigTable->GetAllRows<FTagUIDefinition>(ContextString, AllRows);

	for (FTagUIDefinition* Row : AllRows)
	{
		if (Row && Row->Tag.IsValid())
		{
			TagUIMap.Add(Row->Tag, *Row);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[ConfigData] Loaded %d Tag UI Configs"), TagUIMap.Num());
}

FText UConfigDataSubsystem::GetTagDisplayName(FGameplayTag Tag) const
{
	if (const FTagUIDefinition* Config = TagUIMap.Find(Tag))
	{
		return Config->DisplayName;
	}
	// 兜底：返回 Tag 名字
	return FText::FromName(Tag.GetTagName());
}

FText UConfigDataSubsystem::GetAreaTypeDisplayName(const FGameplayTag AreaTypeTag) const
{
	if (const FAreaTypeBase* const* Found = AreaMap.Find(AreaTypeTag))
	{
		const FText& DisplayName = (*Found)->DisplayName;
		if (!DisplayName.IsEmpty())
		{
			return DisplayName;
		}
	}
	// 兜底：显示 Tag 原始名，避免空白
	return FText::FromName(AreaTypeTag.GetTagName());
}

TSoftObjectPtr<UTexture2D> UConfigDataSubsystem::GetTagIcon(FGameplayTag Tag) const
{
	if (const FTagUIDefinition* Config = TagUIMap.Find(Tag))
	{
		return Config->Icon;
	}
	return nullptr;
}

FLinearColor UConfigDataSubsystem::GetTagColor(FGameplayTag Tag) const
{
	if (const FTagUIDefinition* Config = TagUIMap.Find(Tag))
	{
		return Config->UIColor;
	}
	return FLinearColor::White;
}

void UConfigDataSubsystem::ScanAndLoadAllFormulas()
{
	// 1. 获取 Asset Registry 模块
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// 2. 构建过滤器 (Filter)
	FARFilter Filter;
	// 设定要扫描的目录 (Recursive = true 代表包含子文件夹)
	Filter.PackagePaths.Add(*FormulaSearchPath);
	Filter.bRecursivePaths = true;
    
	// 设定要查找的类 (UE5.1+ 建议使用 ClassPaths，旧版本用 ClassNames)
	#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	Filter.ClassPaths.Add(UGlobalAttributeFormulas::StaticClass()->GetClassPathName());
	#else
	Filter.ClassNames.Add(UGlobalAttributeFormulas::StaticClass()->GetFName());
	#endif
    
	// 3. 执行扫描
	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	UE_LOG(LogTemp, Log, TEXT("[ConfigData] Scanning formulas in [%s]. Found %d assets."), *FormulaSearchPath, AssetList.Num());

	// 4. 遍历并加载
	if (AssetList.Num() > 0)
	{
		// 先清空旧缓存 (视需求而定，如果你想保留基础配置则不要清空)
		CachedFormulaMap.Empty();

		for (const FAssetData& AssetData : AssetList)
		{
			// 同步加载资产
			// 注意：GetAsset() 会触发加载，如果文件很大建议改用 StreamableManager 异步加载，
			// 但公式配置通常很小，同步加载没问题。
			UObject* LoadedObj = AssetData.GetAsset();
			if (auto* FormulaAsset = Cast<UGlobalAttributeFormulas>(LoadedObj))
			{
				MergeFormulasToCache(FormulaAsset);
				UE_LOG(LogTemp, Log, TEXT("   -> Merged: %s"), *AssetData.AssetName.ToString());
			}
		}
	}
}

void UConfigDataSubsystem::MergeFormulasToCache(const UGlobalAttributeFormulas* Asset)
{
	if (!Asset) return;

	for (auto Def : Asset->Formulas)
	{
		if (Def.Key.IsValid())
		{
			// [合并策略]：后来居上 (覆盖模式)
			// 如果两个文件都定义了 "Attack" 的公式，后加载的文件会覆盖前面的。
			// 这非常适合 MOD 逻辑：MOD 的文件通常覆盖原生文件。
			if (CachedFormulaMap.Contains(Def.Key))
			{
				UE_LOG(LogTemp, Warning, TEXT("[ConfigData] Overwriting formula for %s"), *Def.Key.ToString());
			}
            
			CachedFormulaMap.Add(Def.Key, Def.Value);
			UE_LOG(LogTemp, Warning, TEXT("[ConfigData] writing formula for %s"), *Def.Key.ToString());
		}
	}
}

// =============================================================
// DataRegistry 实现 (Creature Logic)
// =============================================================
void UConfigDataSubsystem::CacheRegistryTags()
{
    UDataRegistrySubsystem* DR = GEngine ? GEngine->GetEngineSubsystem<UDataRegistrySubsystem>() : nullptr;
    if (!DR) 
    {
        UE_LOG(LogTemp, Error, TEXT("[ConfigData] DataRegistrySubsystem missing!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("========== [CREATURE REGISTRY SCAN START] =========="));
	
    // --- 修正后的 Lambda (更简单的版本) ---
    // 我们定义一个宏或者模板函数来处理“类型”问题，
    // 或者直接写三个循环，虽然啰嗦但最清晰且不容易出错。
    // 鉴于你要“复刻 LoadAllAreaType”，我推荐使用【模板成员函数】或者如下的【模板 Lambda】：

    auto ScanSpecificRegistry = [&]<typename T>(FName RegistryType, TArray<FGameplayTag>& OutArray, FGameplayTag T::* TagPtr)
    {
        const UDataRegistry* Registry = DR->GetRegistryForType(RegistryType);
    	if (!Registry)
    	{
    		UE_LOG(LogTemp, Error, TEXT("Registry NOT FOUND: %s. Please check Project Settings or Asset Name."), *RegistryType.ToString());
    		return;
    	}

        OutArray.Empty();
        TArray<FName> ItemNames;
        Registry->GetItemNames(ItemNames);

        for (const FName& Name : ItemNames)
        {
            FDataRegistryId RegistryId(RegistryType, Name);

            // [关键点]：GetCachedItem<T>
            // 只有 DT 里配了数据，这里才不为空
            if (const T* Item = Registry->GetCachedItem<T>(RegistryId))
            {
                // [关键点]：直接读取结构体里的 Tag
                FGameplayTag Tag = Item->*TagPtr;
                
                if (Tag.IsValid())
                {
                    OutArray.AddUnique(Tag);
                    // 可选：打印日志
                    // UE_LOG(LogTemp, Log, TEXT("Loaded: %s"), *Tag.ToString());
                }
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("Registry [%s] loaded %d tags from DataTable."), *RegistryType.ToString(), OutArray.Num());
    };

    // =========================================================
    // 执行扫描
    // =========================================================

    // 1. 种族 (Race)
    // 假设结构体是 FCreatureRaceRegistryItem，Tag 成员是 RaceTag
    ScanSpecificRegistry(
        FName("Creature.Identity.Race"), 
        CachedRaceTags, 
        &FCreatureRaceRegistryItem::Tag
    );

    // 2. 职业 (Profession)
    // 假设结构体是 FCreatureProffesionRegistryItem，Tag 成员是 ProfessionTag
    ScanSpecificRegistry(
        FName("Creature.Identity.Profession"), 
        CachedProfessionTags, 
        &FCreatureProffesionRegistryItem::Tag
    );

    // 3. 特质 (Trait)
    // 假设结构体是 FCreatureTraitsRegistryItem，Tag 成员是 TraitTag
    ScanSpecificRegistry(
        FName("Creature.Identity.Trait"), 
        CachedTraitTags, 
        &FCreatureTraitsRegistryItem::Tag 
        // 或者是 &FCreatureTraitsRegistryItem::TraitTemplate->Tag? 
        // 通常 RegistryItem 里会存一个 Tag 副本，如果没有，你需要读取 Template 里的 Tag
    );

    UE_LOG(LogTemp, Warning, TEXT("========== [CREATURE REGISTRY SCAN FINISHED] =========="));
}

// --- 核心泛型查找函数 ---
template <typename TRegistryItem, typename TAssetType>
TAssetType* UConfigDataSubsystem::GetAssetFromRegistry(FName RegistryType, FGameplayTag Tag, TSoftObjectPtr<TAssetType> TRegistryItem::* AssetPtrMember)
{
	if (!Tag.IsValid()) return nullptr;

	UDataRegistrySubsystem* DR = GEngine ? GEngine->GetEngineSubsystem<UDataRegistrySubsystem>() : nullptr;
	if (!DR) return nullptr;

	// 构造查询 ID (RegistryType + TagName)
	FDataRegistryId Id(RegistryType, Tag.GetTagName());
	
	// 获取缓存项 (这会自动去查你在 NewDataRegistry 里配置的 DataTable)
	const TRegistryItem* Item = DR->GetCachedItem<TRegistryItem>(Id);
	
	// 检查并加载
	if (Item)
	{
		// 读取成员变量 (SoftPtr)
		TSoftObjectPtr<TAssetType> SoftPtr = Item->*AssetPtrMember;
		if (!SoftPtr.IsNull())
		{
			// 同步加载资产
			return SoftPtr.LoadSynchronous();
		}
	}
	return nullptr;
}

// --- 具体 Getter ---

UCreatureRaceTemplate* UConfigDataSubsystem::GetRaceTemplate(FGameplayTag RaceTag)
{
	// "Creature.Race" 对应你在 Project Settings 里配置的 Registry Type Name
	return GetAssetFromRegistry<FCreatureRaceRegistryItem, UCreatureRaceTemplate>(
		FName("Creature.Identity.Race"), 
		RaceTag, 
		&FCreatureRaceRegistryItem::RaceTemplate
	);
}

UCreatureProfessionTemplate* UConfigDataSubsystem::GetProfessionTemplate(FGameplayTag ProfessionTag)
{
	return GetAssetFromRegistry<FCreatureProffesionRegistryItem, UCreatureProfessionTemplate>(
		FName("Creature.Identity.Profession"), 
		ProfessionTag, 
		&FCreatureProffesionRegistryItem::ProfessionTemplate
	);
}

UCreatureTraitDefinition* UConfigDataSubsystem::GetTraitDefinition(FGameplayTag TraitTag)
{
	return GetAssetFromRegistry<FCreatureTraitsRegistryItem, UCreatureTraitDefinition>(
		FName("Creature.Identity.Trait"), 
		TraitTag, 
		&FCreatureTraitsRegistryItem::TraitTemplate
	);
}

TMap<FGameplayTag,UCreatureTraitDefinition*> UConfigDataSubsystem::GetAllTraitDefinitions()
{
	TMap<FGameplayTag,UCreatureTraitDefinition*> Result;

	// 遍历之前缓存的所有 Trait Tag
	for (const FGameplayTag& Tag : CachedTraitTags)
	{
		// 复用你已经写好的 GetTraitDefinition 函数
		if (UCreatureTraitDefinition* TraitDef = GetTraitDefinition(Tag))
		{
			Result.Add(Tag,TraitDef);
		}
	}

	return Result;
}

UCreatureEvent* UConfigDataSubsystem::GetLifeEventById(const FPrimaryAssetId Id) const
{

	// 1. 尝试直接从缓存查 (基于 PrimaryAssetId)
	// SoftPtr.GetUniqueID() 在资产被 AssetManager 扫描过的情况下会自动转换为 PrimaryAssetId
    
	if (UCreatureEvent* const* Found = CachedLifeEvents.Find(Id))
	{
		return *Found;
	}

	return nullptr;
}

TArray<UCreatureEvent*> UConfigDataSubsystem::GetAllLifeEvents() const
{
	TArray<UCreatureEvent*> Result;
	CachedLifeEvents.GenerateValueArray(Result);
	return Result;
}

FGameplayTag UConfigDataSubsystem::GetRandomRaceTag()
{
	if (CachedRaceTags.Num() == 0) return FGameplayTag();
	return CachedRaceTags[FMath::RandRange(0, CachedRaceTags.Num() - 1)];
}

// =============================================================
// Template System
// =============================================================

void UConfigDataSubsystem::ScanAndLoadAllTemplates()
{
	UE_LOG(LogTemp, Log, TEXT("[Template] Scanning templates..."));

	FString TemplateDir = FPaths::ProjectSavedDir() / TEXT("Templates");
	if (!IFileManager::Get().DirectoryExists(*TemplateDir))
	{
		IFileManager::Get().MakeDirectory(*TemplateDir, true);
		UE_LOG(LogTemp, Log, TEXT("[Template] Directory created, no templates to load."));
		return;
	}

	TArray<FString> TmplFiles;
	IFileManager::Get().FindFilesRecursive(TmplFiles, *TemplateDir, TEXT("*.tmpl"), true, false);

	for (const FString& File : TmplFiles)
	{
		FVoxelTemplate T;
		if (UVoxelTemplateUtils::LoadFromTmplFile(File, T))
		{
			// 句柄由身份(Guid)运行时重算，不信任磁盘里的旧 ItemId
			RegisterRuntimeTemplate(MoveTemp(T));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Template] Failed to load: %s"), *File);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Template] Scan complete. Loaded %d templates."), RuntimeTemplates.Num());
}

int32 UConfigDataSubsystem::RegisterRuntimeTemplate(FVoxelTemplate&& T)
{
	// 模板身份 = TemplateGuid，句柄由身份决定性生成
	if (T.TemplateGuid.IsValid() == false)
	{
		T.TemplateGuid = FGuid::NewGuid();
	}

	// 身份字符串：Source(默认 core) + tmpl.<Guid>
	if (T.Source.IsEmpty())
	{
		T.Source = TEXT("core");
	}
	T.IdentityName = TEXT("tmpl.") + T.TemplateGuid.ToString(EGuidFormats::Digits);

	const FString FullId = T.GetFullId();
	const int32 Handle = ComputeHandle(FullId);

	// [阻断点 #7] Handle 碰撞检测：与 LoadAllItemType 一致
	// 模板 Guid 罕见碰撞，或同 Guid 重复注册（编辑已有模板场景）会触发
	// 重复注册场景：TemplateEditWorldSubsystem.SaveCurrentWorkArea 编辑同一模板时
	//              会用同 Guid 重新注册，这是预期覆盖，使用 Add 覆盖即可
	// 异常碰撞场景：两个不同 Guid 但 hash 撞上，必须 Fatal（数据完整性优先）
	if (RuntimeTemplates.Contains(Handle) && FullIdToHandle.FindRef(FullId) != Handle)
	{
		// 碰撞且非同 FullId：真正的 hash 冲突
		UE_LOG(LogTemp, Fatal,
			TEXT("[Template] Handle hash collision: New FullId='%s' Handle=%d, but Handle already used by another FullId"),
			*FullId, Handle);
	}

	T.HandleId = Handle;

	// 模板只进 RuntimeTemplates，不进 ItemMap（与配置物品隔离）
	// Add 对同 key 是覆盖语义，支持模板编辑场景的同 Guid 重新注册
	FVoxelTemplate& Stored = RuntimeTemplates.Add(Handle, MoveTemp(T));
	FullIdToHandle.Add(FullId, Handle);

	UE_LOG(LogTemp, Log, TEXT("[Template] Registered template FullId='%s' Handle=%d Guid=%s"),
		*FullId, Handle, *Stored.TemplateGuid.ToString());
	return Handle;
}

bool UConfigDataSubsystem::UnregisterTemplate(int32 HandleId)
{
	if (FVoxelTemplate* T = RuntimeTemplates.Find(HandleId))
	{
		FullIdToHandle.Remove(T->GetFullId());
		RuntimeTemplates.Remove(HandleId);
		UE_LOG(LogTemp, Log, TEXT("[Template] Unregistered template Id=%d"), HandleId);
		return true;
	}
	return false;
}

FVoxelTemplate* UConfigDataSubsystem::GetTemplateById(int32 Handle)
{
	return RuntimeTemplates.Find(Handle);
}

TArray<FVoxelTemplate*> UConfigDataSubsystem::GetAllTemplates()
{
	TArray<FVoxelTemplate*> Result;
	Result.Reserve(RuntimeTemplates.Num());
	for (auto& Pair : RuntimeTemplates)
	{
		Result.Add(&Pair.Value);
	}
	return Result;
}

FVoxelItemBase* UConfigDataSubsystem::FindByHandle(int32 Handle)
{
	if (FVoxelItemBase** Found = ItemMap.Find(Handle))
	{
		return *Found;
	}
	return RuntimeTemplates.Find(Handle);
}

int32 UConfigDataSubsystem::GetHandleByFullId(const FString& FullId) const
{
	if (const int32* Found = FullIdToHandle.Find(FullId))
	{
		return *Found;
	}

	// [Diagnostic] FullIdToHandle 查找失败，打印诊断信息
	UE_LOG(LogTemp, Warning, TEXT("[ConfigData] GetHandleByFullId: FullId='%s' not in FullIdToHandle (size=%d). Falling back to ItemMap scan (size=%d)..."),
		*FullId, FullIdToHandle.Num(), ItemMap.Num());

	// [Fallback] FullIdToHandle 与 ItemMap 可能不同步，回退遍历 ItemMap
	for (const auto& Pair : ItemMap)
	{
		if (Pair.Value && Pair.Value->GetFullId() == FullId)
		{
			// 找到了！自修复 FullIdToHandle（const_cast: 修复缓存不一致）
			const_cast<UConfigDataSubsystem*>(this)->FullIdToHandle.Add(FullId, Pair.Key);
			UE_LOG(LogTemp, Warning, TEXT("[ConfigData] GetHandleByFullId: Self-repaired! FullId='%s' -> Handle=%d (added to FullIdToHandle)"),
				*FullId, Pair.Key);
			return Pair.Key;
		}
	}

	// 也检查 RuntimeTemplates
	for (const auto& Pair : RuntimeTemplates)
	{
		if (Pair.Value.GetFullId() == FullId)
		{
			const_cast<UConfigDataSubsystem*>(this)->FullIdToHandle.Add(FullId, Pair.Key);
			UE_LOG(LogTemp, Warning, TEXT("[ConfigData] GetHandleByFullId: Self-repaired from RuntimeTemplates! FullId='%s' -> Handle=%d"),
				*FullId, Pair.Key);
			return Pair.Key;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("[ConfigData] GetHandleByFullId: FullId='%s' not found in any map!"), *FullId);
	return -1;
}

FString UConfigDataSubsystem::GetFullIdByHandle(int32 Handle) const
{
	if (const FVoxelItemBase* const* Found = ItemMap.Find(Handle))
	{
		return (*Found) ? (*Found)->GetFullId() : FString();
	}
	if (const FVoxelTemplate* Tmpl = RuntimeTemplates.Find(Handle))
	{
		return Tmpl->GetFullId();
	}
	return FString();
}

const FVoxelTemplate* UConfigDataSubsystem::FindTemplateByGuid(const FGuid& Guid) const
{
	for (const auto& Pair : RuntimeTemplates)
	{
		if (Pair.Value.TemplateGuid == Guid)
		{
			return &Pair.Value;
		}
	}
	return nullptr;
}

TArray<int32> UConfigDataSubsystem::GetAllTemplateIds() const
{
	TArray<int32> Result;
	RuntimeTemplates.GetKeys(Result);
	return Result;
}
