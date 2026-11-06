#include "Voxel/Editor/TemplateEditWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelItemWorldSubsystem.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Core/VoxelGameInstance.h"
#include "Core/CoreTags.h"
#include "Voxel/Editor/UVoxelTemplateUtils.h"
#include "Engine/Engine.h"

void UTemplateEditWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	BlockSys = GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>();
	ItemWorld = GetWorld()->GetSubsystem<UVoxelItemWorldSubsystem>();
	ConfigSys = GetWorld()->GetGameInstance()->GetSubsystem<UConfigDataSubsystem>();

	// 延迟设置工作区（等待地形生成）
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, [this]()
	{
		SetupWorkArea();

		// 检查是否要加载已有模板
		if (UVoxelGameInstance* GI = Cast<UVoxelGameInstance>(GetWorld()->GetGameInstance()))
		{
			if (GI->PendingEditHandleId >= 0)
			{
				ApplyTemplateToWorkArea(GI->PendingEditHandleId);
			}
		}
	}, 2.0f, false);

	UE_LOG(LogTemp, Log, TEXT("[TemplateEdit] Subsystem initialized"));
}

void UTemplateEditWorldSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

bool UTemplateEditWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->GetName().Contains(TEXT("TemplateEdit"));
	}
	return false;
}

void UTemplateEditWorldSubsystem::SetupWorkArea()
{
	WorkAreaCenter = FIntVector(0, 0, 0);
	WorkAreaSize = 32;
	
	FIntVector Half(WorkAreaSize / 2, WorkAreaSize / 2, 0);
	WorkAreaBox = FBox(
		FVector(WorkAreaCenter - Half),
		FVector(WorkAreaCenter + FIntVector(WorkAreaSize / 2, WorkAreaSize / 2, WorkAreaSize))
	);

	UE_LOG(LogTemp, Log, TEXT("[TemplateEdit] Work area setup: center=%s, size=%d"), *WorkAreaCenter.ToString(), WorkAreaSize);
}

void UTemplateEditWorldSubsystem::ApplyTemplateToWorkArea(int32 TemplateId)
{
	if (!ConfigSys || !BlockSys) return;

	// 从模板仓库按句柄取模板
	if (const FVoxelTemplate* Tmpl = ConfigSys->GetTemplateById(TemplateId))
	{
		// 放置模板到工作区中心
		FIntVector Origin = WorkAreaCenter - Tmpl->Anchor;
		UVoxelTemplateUtils::PlaceTemplate(BlockSys, ItemWorld, ConfigSys, *Tmpl, Origin, EActorDirection::Forward, TMap<FString, FString>());

		UE_LOG(LogTemp, Log, TEXT("[TemplateEdit] Template %d applied to work area"), TemplateId);
	}
}

FVoxelTemplate UTemplateEditWorldSubsystem::CaptureFromWorkArea(const FTemplateMeta& Meta)
{
	if (!BlockSys || !ConfigSys) return FVoxelTemplate();

	// 遍历工作区范围，提取所有方块
	TArray<FIntVector> Selection;
	FIntVector Min = FIntVector(FMath::FloorToInt(WorkAreaBox.Min.X), FMath::FloorToInt(WorkAreaBox.Min.Y), FMath::FloorToInt(WorkAreaBox.Min.Z));
	FIntVector Max = FIntVector(FMath::CeilToInt(WorkAreaBox.Max.X), FMath::CeilToInt(WorkAreaBox.Max.Y), FMath::CeilToInt(WorkAreaBox.Max.Z));

	for (int32 z = Min.Z; z < Max.Z; z++)
	{
		for (int32 y = Min.Y; y < Max.Y; y++)
		{
			for (int32 x = Min.X; x < Max.X; x++)
			{
				int32 Id = BlockSys->GetBlockType(FIntVector(x, y, z));
				if (Id != 0) // 非 Air
				{
					Selection.Add(FIntVector(x, y, z));
				}
			}
		}
	}

	return UVoxelTemplateUtils::CaptureFromSelection(BlockSys, ItemWorld, ConfigSys, Selection, Meta);
}

bool UTemplateEditWorldSubsystem::SaveCurrentWorkArea(const FTemplateMeta& Meta)
{
	if (!ConfigSys) return false;

	FVoxelTemplate T = CaptureFromWorkArea(Meta);
	if (T.Entries.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TemplateEdit] No content to save"));
		return false;
	}

	// 检查是否是编辑已有模板
	UVoxelGameInstance* GI = Cast<UVoxelGameInstance>(GetWorld()->GetGameInstance());
	if (GI && GI->PendingEditHandleId >= 0)
	{
		// 保留原 Guid 和 HandleId
		if (const FVoxelTemplate* OldTmpl = ConfigSys->GetTemplateById(GI->PendingEditHandleId))
		{
			T.TemplateGuid = OldTmpl->TemplateGuid;
			T.HandleId = OldTmpl->HandleId;
			T.Version = OldTmpl->Version + 1;
		}
	}

	T.ModifiedTime = FDateTime::Now();

	// 注册
	int32 Id = ConfigSys->RegisterRuntimeTemplate(MoveTemp(T));

	// 保存到磁盘
	FVoxelTemplate& Ref = ConfigSys->RuntimeTemplates[Id];
	FString FilePath = UVoxelTemplateUtils::GetTmplFilePath(Ref.TemplateGuid);
	bool bSaved = UVoxelTemplateUtils::SaveToTmplFile(Ref, FilePath);

	UE_LOG(LogTemp, Log, TEXT("[TemplateEdit] Saved template Id=%d, file=%s"), Id, bSaved ? *FilePath : TEXT("FAILED"));
	return bSaved;
}
