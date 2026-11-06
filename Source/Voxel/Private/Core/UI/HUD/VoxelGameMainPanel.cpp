#include "Core/UI/HUD/VoxelGameMainPanel.h"
#include "Core/UI/HUD/VoxelTopLeftBarWidget.h"
#include "Core/UI/HUD/VoxelEnvPanelWidget.h"
#include "Core/UI/HUD/VoxelEntityDetailPanel.h"
#include "Core/UI/Interaction/InteractionWidget.h"
#include "Core/UI/Interaction/BuildInteractionWidget.h"
#include "Core/UI/Build/AreaTree/VoxelAreaNavigatorWidget.h"
#include "Core/UI/VoxelAreaItemWidget.h"
#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "CommonActivatableWidget.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

void UVoxelGameMainPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 按钮绑定（一次性）
	if (ModeToggleButton)
	{
		ModeToggleButton->OnClicked.AddDynamic(this, &UVoxelGameMainPanel::OnModeToggleClicked);
	}

	// 委托绑定（一次性）
	BindDelegate();
}

void UVoxelGameMainPanel::NativeConstruct()
{
	Super::NativeConstruct();
	bConstructed = true;
	RefreshModeDisplay();

	// 环境域面板：右上角常驻（类由蓝图配置，缺失时明确报错——不放回退实现）
	if (!SpawnedEnvPanel)
	{
		if (!EnvPanelClass)
		{
			UE_LOG(LogTemp, Error, TEXT("[MainPanel] EnvPanelClass 未在 WBP_VoxelGameHUD 配置"));
		}
		else
		{
			SpawnedEnvPanel = CreateWidget<UVoxelEnvPanelWidget>(this, EnvPanelClass);
			if (SpawnedEnvPanel && CanvasPanel_0)
			{
				// 右上锚定、右缘对齐、尺寸随内容（抽屉展开时向下生长）
				UCanvasPanelSlot* EnvSlot = CanvasPanel_0->AddChildToCanvas(SpawnedEnvPanel);
				EnvSlot->SetAnchors(FAnchors(1.0f, 0.0f));
				EnvSlot->SetAlignment(FVector2D(1.0f, 0.0f));
				EnvSlot->SetPosition(FVector2D(-10.0f, 8.0f));
				EnvSlot->SetAutoSize(true);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[MainPanel] EnvPanel 创建失败：CanvasPanel_0 未绑定或 CreateWidget 失败"));
			}
		}
	}

	// 实体详情面板：右列环境组下方常驻（类由蓝图配置；空态/区域卡显隐由面板自身订阅选中广播驱动）
	if (!SpawnedEntityDetailPanel)
	{
		if (!EntityDetailPanelClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MainPanel] EntityDetailPanelClass 未在 WBP_VoxelGameHUD 配置，实体详情槽不可用"));
		}
		else
		{
			SpawnedEntityDetailPanel = CreateWidget<UVoxelEntityDetailPanel>(this, EntityDetailPanelClass);
			if (SpawnedEntityDetailPanel && CanvasPanel_0)
			{
				// 右下锚定（避开向下生长的 EnvPanel 与底部操作带），尺寸随内容
				UCanvasPanelSlot* DetailSlot = CanvasPanel_0->AddChildToCanvas(SpawnedEntityDetailPanel);
				DetailSlot->SetAnchors(FAnchors(1.0f, 1.0f));
				DetailSlot->SetAlignment(FVector2D(1.0f, 1.0f));
				DetailSlot->SetPosition(FVector2D(-10.0f, -70.0f));
				DetailSlot->SetAutoSize(true);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[MainPanel] EntityDetailPanel 创建失败：CanvasPanel_0 未绑定或 CreateWidget 失败"));
			}
		}
	}

	// 启动时序兜底：构造期广播可能因 GetOwningPlayer 尚不可用而丢失（面板推入失败），
	// 在 NativeConstruct 重新同步面板与模式，避免「模式=建造但面板未打开」的失同步
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>())
		{
			ApplyModeToScreenPanel(SC->GetControllerMode());
		}
	}
}

void UVoxelGameMainPanel::NativeDestruct()
{
	UnbindDelegate();
	Super::NativeDestruct();
}

void UVoxelGameMainPanel::BindDelegate()
{
	AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolChain] MainPanel BindDelegate: PC cast failed"));
		return;
	}

	UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
	if (!SC)
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolChain] MainPanel BindDelegate: BSC not found via FindComponentByClass"));
		return;
	}

	if (UVoxelInteractionComponent* IC = PC->FindComponentByClass<UVoxelInteractionComponent>())
	{
		IC->OnControllerModeChanged.AddDynamic(this, &UVoxelGameMainPanel::HandleControllerModeChanged);
	}
	// 区域导航刷新只依赖区域相关事件，避免 Brush/ItemInHand 等无关事件触发全量刷新
	SC->OnCurrentAreaChanged.AddDynamic(this, &UVoxelGameMainPanel::HandleCurrentAreaChanged);
	UE_LOG(LogTemp, Log, TEXT("[ToolChain] MainPanel BindDelegate: bound OK"));

	RefreshNavigator(SC);
	if (UVoxelInteractionComponent* IC = PC->FindComponentByClass<UVoxelInteractionComponent>())
	{
		HandleControllerModeChanged(IC);
	}
}

void UVoxelGameMainPanel::UnbindDelegate()
{
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (UVoxelInteractionComponent* IC = PC->FindComponentByClass<UVoxelInteractionComponent>())
		{
			IC->OnControllerModeChanged.RemoveDynamic(this, &UVoxelGameMainPanel::HandleControllerModeChanged);
		}
		if (UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>())
		{
			SC->OnCurrentAreaChanged.RemoveDynamic(this, &UVoxelGameMainPanel::HandleCurrentAreaChanged);
		}
	}
}

void UVoxelGameMainPanel::ShowInteraction() const
{
	if (InteractionWidget)
	{
		InteractionWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void UVoxelGameMainPanel::HideInteraction() const
{
	if (InteractionWidget)
	{
		InteractionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UVoxelGameMainPanel::ShowBuildInteraction() const
{
	if (BuildInteractionWidget)
	{
		BuildInteractionWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void UVoxelGameMainPanel::HideBuildInteraction() const
{
	if (BuildInteractionWidget)
	{
		BuildInteractionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UVoxelGameMainPanel::OnModeToggleClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[ModeToggle] OnModeToggleClicked ENTER"));
	AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolChain] OnModeToggleClicked: PC cast failed"));
		return;
	}
	UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
	if (!SC)
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolChain] OnModeToggleClicked: BSC not found"));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[ModeToggle] PC=%s SC=%s InteractionComp=%s CurrentMode=%d"),
		*GetNameSafe(PC), *GetNameSafe(SC), *GetNameSafe(PC->InteractionComp), (int32)SC->GetControllerMode());

	// 读取可直读；写入必须走业务逻辑层（InteractionComp->SetPlayerMode → SC 广播）
	const EVoxelControllerMode NewMode = (SC->GetControllerMode() == EVoxelControllerMode::SceneBuildMode)
		? EVoxelControllerMode::AdventureMode
		: EVoxelControllerMode::SceneBuildMode;

	if (PC->InteractionComp)
	{
		UE_LOG(LogTemp, Log, TEXT("[ModeToggle] Switching mode -> %d via InteractionComp->SetPlayerMode"), (int32)NewMode);
		PC->InteractionComp->SetPlayerMode(NewMode);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ModeToggle] InteractionComp is NULL, mode switch ABORTED"));
	}
}

void UVoxelGameMainPanel::HandleControllerModeChanged(UVoxelInteractionComponent* Interaction)
{
	if (!Interaction)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ToolChain] HandleControllerModeChanged: Interaction is null"));
		return;
	}

	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>())
		{
			RefreshNavigator(SC);
		}
	}
	RefreshModeDisplay();
	ApplyModeToScreenPanel(Interaction->GetControllerMode());
}

void UVoxelGameMainPanel::HandleCurrentAreaChanged(AVoxelAreaBaseInstance* /*OldArea*/, AVoxelAreaBaseInstance* /*NewArea*/)
{
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>())
		{
			RefreshNavigator(SC);
		}
	}
}

void UVoxelGameMainPanel::RefreshModeDisplay()
{
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (UVoxelPlayerStateCacheComponent* SC = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>())
		{
			const bool bIsBuildMode = (SC->GetControllerMode() == EVoxelControllerMode::SceneBuildMode);
			if (ModeToggleText)
			{
				ModeToggleText->SetText(bIsBuildMode
					? FText::FromString(TEXT("退出建造"))
					: FText::FromString(TEXT("进入建造")));
			}
		}
	}
}

void UVoxelGameMainPanel::RefreshNavigator(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC) return;

	// 新架构：TopLeftBar 聚合容器内部持有 AreaNavigator
	if (TopLeftBar)
	{
		TopLeftBar->SetBreadcrumbItemClass(BreadcrumbItemClass);
		TopLeftBar->RefreshNavigator(SC);
		return;
	}

	// 旧架构兜底：MainPanel 直接持有 AreaNavigator
	if (AreaNavigator)
	{
		AreaNavigator->SetBreadcrumbItemClass(BreadcrumbItemClass);
		AreaNavigator->Refresh(SC);
	}
}

void UVoxelGameMainPanel::ApplyModeToScreenPanel(EVoxelControllerMode Mode)
{
	UVoxelPrimaryGameLayout* Layout = UVoxelPrimaryGameLayout::GetFromPC(GetOwningPlayer());
	if (!Layout)
	{
		// 构造期（PreConstruct 阶段 BindDelegate）PrimaryLayout 尚未赋值属预期时序，
		// NativeConstruct 会兜底重试；构造后仍取不到才是真实故障
		if (bConstructed)
		{
			UE_LOG(LogTemp, Error, TEXT("[ToolChain] MainPanel ApplyModeToScreenPanel: GetFromPC returned null (constructed, real failure)"));
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("[ToolChain] MainPanel ApplyModeToScreenPanel: Layout 未就绪（构造期），NativeConstruct 将兜底"));
		}
		return;
	}

	if (Mode == EVoxelControllerMode::SceneBuildMode)
	{
		if (BuildPanelClass)
		{
			Layout->PushGameMenu(BuildPanelClass);
			UE_LOG(LogTemp, Log, TEXT("[ToolChain] MainPanel ApplyModeToScreenPanel: PushGameMenu(BuildPanel) OK"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[ToolChain] MainPanel ApplyModeToScreenPanel: BuildPanelClass is null, cannot push"));
		}
	}
	else
	{
		// 退出建造模式：关闭 GameMenu Stack 上的建造面板（含无激活面板时的强制清空兜底）
		Layout->CloseGameMenu();
	}
}
