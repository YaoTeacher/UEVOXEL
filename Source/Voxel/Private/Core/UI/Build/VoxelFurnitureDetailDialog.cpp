// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/VoxelFurnitureDetailDialog.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Item/VoxelItemBaseInstance.h"
#include "Components/TextBlock.h"
#include "Engine/GameViewportClient.h"

// ==================== 生命周期 ====================

void UVoxelFurnitureDetailDialog::OnTooltipShown(UObject* Context)
{
	// 从 Context 获取家具 Actor
	AUVoxelItemBaseInstance* Furniture = Cast<AUVoxelItemBaseInstance>(Context);
	if (!Furniture)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FurnitureTooltip] Shown without valid furniture actor"));
		Close();
		return;
	}

	FurnitureActor = Furniture;

	// 同步选中状态（经 InteractionComp 业务入口，确保 InteractionComp 的 HandleKeyDown 能操作到此家具）
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AVoxelPlayerController* VoxelPC = Cast<AVoxelPlayerController>(PC))
			{
				if (VoxelPC->InteractionComp)
				{
					VoxelPC->InteractionComp->SelectFurniture(Furniture);
				}
			}
		}
	}

	RefreshFurnitureInfo();

	// 基于家具 Actor 世界坐标定位浮窗（在物体上方显示，不跟随鼠标）
	{
		// 获取视口尺寸
		FVector2D ViewportSize(0, 0);
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(ViewportSize);
		}

		// 使用家具 Actor 位置 + 上方偏移（避免遮挡物体）
		FVector ActorWorldPos = Furniture->GetActorLocation();
		ActorWorldPos.Z += 100.0f; // 偏移到物体上方

		UpdatePositionNearWorldLocation(ActorWorldPos, ViewportSize);
	}
}

void UVoxelFurnitureDetailDialog::OnTooltipHidden()
{
	// 清空选中状态
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AVoxelPlayerController* VoxelPC = Cast<AVoxelPlayerController>(PC))
			{
				if (VoxelPC->InteractionComp)
				{
					VoxelPC->InteractionComp->SelectFurniture(nullptr);
				}
			}
		}
	}
	FurnitureActor = nullptr;
}

// ==================== 私有辅助 ====================

void UVoxelFurnitureDetailDialog::RefreshFurnitureInfo()
{
	AUVoxelItemBaseInstance* Furniture = FurnitureActor.Get();
	if (!Furniture)
	{
		Close();
		return;
	}

	const FVoxelItemInstanceBase& Instance = Furniture->ItemInstance;

	// === 标题：家具名称 ===
	if (TitleText)
	{
		if (Furniture->ItemBaseInform && !Furniture->ItemBaseInform->ItemName.IsEmpty())
		{
			TitleText->SetText(Furniture->ItemBaseInform->ItemName);
		}
		else
		{
			TitleText->SetText(FText::FromString(Furniture->GetName()));
		}
	}

	// === 内容：多行信息 ===
	if (ContentText)
	{
		FString Content;

		// 描述
		if (Furniture->ItemBaseInform)
		{
			Content += FString::Printf(TEXT("描述: %s\n"), *Furniture->ItemBaseInform->ItemDescription.ToString());
		}
		else
		{
			Content += TEXT("描述: 无\n");
		}

		// HandleId
		Content += FString::Printf(TEXT("ID: %d\n"), Instance.HandleId);

		// 朝向
		FString DirStr;
		switch (Instance.Direction)
		{
		case EActorDirection::Forward:  DirStr = TEXT("Forward (北)"); break;
		case EActorDirection::Backward: DirStr = TEXT("Backward (南)"); break;
		case EActorDirection::Left:     DirStr = TEXT("Left (西)"); break;
		case EActorDirection::Right:    DirStr = TEXT("Right (东)"); break;
		case EActorDirection::Up:       DirStr = TEXT("Up"); break;
		case EActorDirection::Down:     DirStr = TEXT("Down"); break;
		default:                        DirStr = TEXT("Unknown"); break;
		}
		Content += FString::Printf(TEXT("朝向: %s\n"), *DirStr);

		// 占用格数
		Content += FString::Printf(TEXT("占用: %d 格\n"), Furniture->OccupiedBlock.Num());

		// 数量
		Content += FString::Printf(TEXT("数量: %d"), Instance.Quantity);

		ContentText->SetText(FText::FromString(Content));
	}

	UE_LOG(LogTemp, Log, TEXT("[FurnitureTooltip] Refreshed: %s, HandleId=%d, Dir=%d, Occupied=%d, Qty=%d"),
		*Furniture->GetName(), Instance.HandleId, (int32)Instance.Direction,
		Furniture->OccupiedBlock.Num(), Instance.Quantity);
}
