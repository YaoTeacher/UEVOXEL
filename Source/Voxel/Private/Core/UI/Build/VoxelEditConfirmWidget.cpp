#include "Core/UI/Build/VoxelEditConfirmWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/VoxelGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

void UVoxelEditConfirmWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &UVoxelEditConfirmWidget::OnConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UVoxelEditConfirmWidget::OnCancelClicked);
	}
}

void UVoxelEditConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 解析并缓存 StateComponent，订阅显隐相关广播（M1 模块自绑模式）
	if (!StateComponent)
	{
		if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
		{
			StateComponent = PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
			InteractionComponent = PC->FindComponentByClass<UVoxelInteractionComponent>();
		}
	}

	if (StateComponent)
	{
		StateComponent->OnGeometryChanged.AddDynamic(this, &UVoxelEditConfirmWidget::RefreshVisibility);
	}
	if (InteractionComponent)
	{
		InteractionComponent->OnBuildPhaseChanged.AddDynamic(this, &UVoxelEditConfirmWidget::RefreshVisibility);
		InteractionComponent->OnToolChanged.AddDynamic(this, &UVoxelEditConfirmWidget::RefreshVisibility);
	}

	RefreshVisibility();
}

void UVoxelEditConfirmWidget::NativeDestruct()
{
	if (StateComponent)
	{
		StateComponent->OnGeometryChanged.RemoveAll(this);
	}
	if (InteractionComponent)
	{
		InteractionComponent->OnBuildPhaseChanged.RemoveAll(this);
		InteractionComponent->OnToolChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UVoxelEditConfirmWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	// 仅 EDITING 态需要跟随：每帧重投影锚点（相机可移动，投影位置会变化）
	// 投影失败（锚点在相机后方）时隐藏，恢复可投影后自动出现
	if (!StateComponent || StateComponent->GetBuildPhase() != EPreviewInteractionState::EDITING)
	{
		return;
	}

	SetVisibility(UpdateAnchorPosition() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UVoxelEditConfirmWidget::RefreshVisibility()
{
	const bool bEditing = StateComponent
		&& StateComponent->GetBuildPhase() == EPreviewInteractionState::EDITING;

	if (!bEditing)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 先定位再显示，避免首帧出现在错误位置
	SetVisibility(UpdateAnchorPosition() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

bool UVoxelEditConfirmWidget::ComputeAnchorWorldLocation(FVector& OutLocation) const
{
	if (!StateComponent)
	{
		return false;
	}

	float BlockLength = 20.0f;
	if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
	{
		BlockLength = GI->GetBlockLength();
	}

	// 选区类工具的作用范围由 SC 的 HandleSelectionChanged 同步进 TmpGeometry.Box，此处无特例
	const FBox Bounds = StateComponent->GetTmpGeometry().GetBound(BlockLength);
	if (!Bounds.IsValid)
	{
		return false;
	}

	// 锚点 = 包围盒顶部中心（与旧 3D 浮标贴盒顶的思路一致）
	const FVector Center = Bounds.GetCenter();
	OutLocation = FVector(Center.X, Center.Y, Bounds.Max.Z);
	return true;
}

bool UVoxelEditConfirmWidget::UpdateAnchorPosition()
{
	FVector AnchorWorld;
	if (!ComputeAnchorWorldLocation(AnchorWorld))
	{
		return false;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return false;
	}

	FVector2D ScreenPos;
	if (!PC->ProjectWorldLocationToScreen(AnchorWorld, ScreenPos, true))
	{
		return false;
	}

	FVector2D ViewportSize(1280.0f, 720.0f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	// 目标位置：锚点投影点附近偏移（钳制策略复刻 VoxelInfoTooltip::UpdatePositionNearWorldLocation）
	FVector2D TargetPos = ScreenPos + TargetOffset;

	FVector2D DesiredSize = GetDesiredSize();
	if (DesiredSize.X <= 0 || DesiredSize.Y <= 0)
	{
		DesiredSize = FVector2D(200.0f, 60.0f);
	}

	// 超出视口右侧，放到锚点左侧
	if (TargetPos.X + DesiredSize.X > ViewportSize.X)
	{
		TargetPos.X = ScreenPos.X - TargetOffset.X - DesiredSize.X;
	}

	// 超出视口顶部，翻到锚点下方
	if (TargetPos.Y < 0.0f)
	{
		TargetPos.Y = ScreenPos.Y + FMath::Abs(TargetOffset.Y) + 20.0f;
	}

	// 超出视口底部，向上收拢
	if (TargetPos.Y + DesiredSize.Y > ViewportSize.Y)
	{
		TargetPos.Y = ViewportSize.Y - DesiredSize.Y - 10.0f;
	}

	TargetPos.X = FMath::Clamp(TargetPos.X, 0.0f, FMath::Max(0.0f, ViewportSize.X - DesiredSize.X));
	TargetPos.Y = FMath::Clamp(TargetPos.Y, 0.0f, FMath::Max(0.0f, ViewportSize.Y - DesiredSize.Y));

	// UE5.8 的 SetPositionInViewport 只作用于直接 AddToViewport 的控件
	// （走 UGameViewportSubsystem 槽位），作为 RootCanvas 子控件时不会移动
	// CanvasPanelSlot。此处直接写 Canvas 槽位：
	// 投影/钳制在含 DPI 缩放的像素空间完成，槽位坐标需除以 ViewportScale
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetPosition(TargetPos / ViewportScale);
	}
	return true;
}

void UVoxelEditConfirmWidget::OnConfirmClicked()
{
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->RequestConfirmVoxelEdit();
		}
	}
}

void UVoxelEditConfirmWidget::OnCancelClicked()
{
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (PC->InteractionComp)
		{
			// 完整取消，与右键链路（HandleMouseCancel → OnMouseCancel → CancelCurrentEdit）同语义
			PC->InteractionComp->CancelCurrentEdit();
		}
	}
}
