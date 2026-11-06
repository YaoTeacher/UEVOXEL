// 测试控制台命令示例（可选）
// 此文件提供快速测试体素编辑系统的控制台命令
// 将此文件添加到 Voxel 模块后，可在 UE5 控制台中直接调用测试功能

#include "CoreMinimal.h"
#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Voxel/Editor/Tools/VoxelBatchEditTool.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Voxel/Editor/UVoxelTemplateUtils.h"
#include "Data/Item/InteractableData.h"

// 控制台命令: 显示状态
FAutoConsoleCommand ConsoleCommandShowStatus(
    TEXT("Voxel.ShowStatus"),
    TEXT("显示编辑器状态"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (UWorld* World = GWorld)
        {
            UE_LOG(LogTemp, Log, TEXT("=== 体素编辑系统状态 ==="));

            if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(World->GetFirstPlayerController()))
            {
                if (UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>())
                {
                    UE_LOG(LogTemp, Log, TEXT("  - 控制器模式: %d"), static_cast<int32>(SC->GetControllerMode()));
                }
            }

            UE_LOG(LogTemp, Log, TEXT("========================"));
        }
    })
);

// // 控制台命令: 清空历史上AaBA
// FAutoConsoleCommand ConsoleCommandClearHistory(
//     TEXT("Voxel.ClearHistory"),
//     TEXT("清空所有撤销/重做历史"),入股
//     FConsoleCommandDelegate::CreateLambda([]()
//     {luo
//         // 需要重构：从 PlayerController 获取 HistoryComponent
//         if (UWorld* World = GWorld)
//         {
//             if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(World->GetFirstPlayerController()))
//             {
//                 if (UVoxelControllerHistoryComponent* HistoryComp = PC->FindComponentByClass<UVoxelControllerHistoryComponent>())
//                 {
//                     HistoryComp->ClearOperationLogs();
//                     HistoryComp->ClearRedoStack();
//                     HistoryComp->ClearUndoStack();
//                     UE_LOG(LogTemp, Log, TEXT("控制台: 历史记录已清空"));
//                 }
//             }
//         }
//     })
// );

// =============================================================
// 模板素材替换测试命令
// =============================================================

namespace VoxelTemplateRemapConsole
{
	// 获取当前手持模板 + 相关子系统
	static bool GetContext(UVoxelPlayerStateCacheComponent*& OutSC, UConfigDataSubsystem*& OutCDS, const FVoxelTemplate*& OutTmpl)
	{
		OutSC = nullptr; OutCDS = nullptr; OutTmpl = nullptr;
		UWorld* World = GWorld;
		if (!World) return false;

		AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(World->GetFirstPlayerController());
		if (!PC) return false;

		OutSC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
		if (World->GetGameInstance())
		{
			OutCDS = World->GetGameInstance()->GetSubsystem<UConfigDataSubsystem>();
		}
		if (!OutSC || !OutCDS) return false;

		const FInteractableData HandData = OutSC->GetInteractableDataInHand();
		OutTmpl = OutCDS->GetTemplateById(HandData.HandleId);
		return OutTmpl != nullptr;
	}
}

// 列出当前手持模板的可替换素材，以及每种素材的同类候选
FAutoConsoleCommand ConsoleCommandTemplateListMats(
	TEXT("Voxel.Template.ListMats"),
	TEXT("列出当前手持模板的可替换素材及同类候选（需先在模板工具下手持一个模板）"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UVoxelPlayerStateCacheComponent* SC = nullptr;
		UConfigDataSubsystem* CDS = nullptr;
		const FVoxelTemplate* Tmpl = nullptr;
		if (!VoxelTemplateRemapConsole::GetContext(SC, CDS, Tmpl))
		{
			UE_LOG(LogTemp, Warning, TEXT("[TemplateRemap] 未手持有效模板，请先在模板工具下选中一个模板"));
			return;
		}

		TArray<FString> Mats = UVoxelTemplateUtils::GetDistinctMaterials(*Tmpl, CDS);
		UE_LOG(LogTemp, Log, TEXT("[TemplateRemap] 模板可替换素材数=%d"), Mats.Num());

		const TMap<FString, FString>& Remap = SC->GetTemplateRemap();
		for (const FString& FullId : Mats)
		{
			const int32 Handle = CDS->GetHandleByFullId(FullId);
			const FVoxelItemBase* Item = CDS->GetItemTypeById<FVoxelItemBase>(Handle);
			FGameplayTag FilterTag = UVoxelTemplateUtils::GetRemapFilterTag(FullId, CDS);
			const FString* Cur = Remap.Find(FullId);

			UE_LOG(LogTemp, Log, TEXT("  素材 FullId='%s' [%s] FilterTag=%s%s"),
				*FullId,
				Item ? *Item->ItemName.ToString() : TEXT("?"),
				*FilterTag.ToString(),
				Cur ? *FString::Printf(TEXT("  -> 已替换为 '%s'"), **Cur) : TEXT(""));

			// 列出同类候选
			TArray<FVoxelItemBase*> Candidates = CDS->GetItemTypeArrayByCategory<FVoxelItemBase>(FilterTag);
			FString CandStr;
			for (const FVoxelItemBase* C : Candidates)
			{
				if (C) CandStr += FString::Printf(TEXT("%s(%s) "), *C->GetFullId(), *C->ItemName.ToString());
			}
			UE_LOG(LogTemp, Log, TEXT("    同类候选: %s"), *CandStr);
		}
	})
);

// 设置素材替换：Voxel.Template.Remap <OriginFullId> <NewFullId>（相同则取消替换）
FAutoConsoleCommand ConsoleCommandTemplateRemap(
	TEXT("Voxel.Template.Remap"),
	TEXT("设置模板素材替换: Voxel.Template.Remap <OriginFullId> <NewFullId>（会触发预览刷新）"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TemplateRemap] 用法: Voxel.Template.Remap <OriginFullId> <NewFullId>"));
			return;
		}

		UVoxelPlayerStateCacheComponent* SC = nullptr;
		UConfigDataSubsystem* CDS = nullptr;
		const FVoxelTemplate* Tmpl = nullptr;
		if (!VoxelTemplateRemapConsole::GetContext(SC, CDS, Tmpl))
		{
			UE_LOG(LogTemp, Warning, TEXT("[TemplateRemap] 未手持有效模板"));
			return;
		}

		const FString OriginFullId = Args[0];
		const FString NewFullId = Args[1];

		// 同类校验：新素材必须与原素材同类
		if (OriginFullId != NewFullId)
		{
			FGameplayTag FilterTag = UVoxelTemplateUtils::GetRemapFilterTag(OriginFullId, CDS);
			const int32 NewHandle = CDS->GetHandleByFullId(NewFullId);
			const FVoxelItemBase* NewItem = CDS->GetItemTypeById<FVoxelItemBase>(NewHandle);
			if (!NewItem || !NewItem->ItemTag.MatchesTag(FilterTag))
			{
				UE_LOG(LogTemp, Warning, TEXT("[TemplateRemap] 拒绝替换: NewFullId='%s' 与 OriginFullId='%s' 不同类 (FilterTag=%s)"),
					*NewFullId, *OriginFullId, *FilterTag.ToString());
				return;
			}
		}

		SC->SetTemplateRemapEntry(OriginFullId, NewFullId);
		UE_LOG(LogTemp, Log, TEXT("[TemplateRemap] 已设置替换 '%s' -> '%s'，预览应已刷新"), *OriginFullId, *NewFullId);
	})
);
