#include "Core/UI/Base/VoxelInfoTooltip.h"
#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Player.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"

void UVoxelInfoTooltip::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 绑定关闭按钮点击事件（一次性）
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UVoxelInfoTooltip::HandleCloseClicked);
	}
}

void UVoxelInfoTooltip::NativeConstruct()
{
	Super::NativeConstruct();
}

void UVoxelInfoTooltip::HandleCloseClicked()
{
	Close();
}

void UVoxelInfoTooltip::Close()
{
	if (UVoxelPrimaryGameLayout* Layout = GetLayout())
	{
		Layout->HideInfoTooltip();
	}
}

void UVoxelInfoTooltip::UpdatePositionNearWorldLocation(const FVector& WorldLocation, const FVector2D& ViewportSize)
{
	// 将世界坐标投影到屏幕
	FVector2D ScreenPos(0, 0);
	bool bProjected = false;

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			bProjected = PC->ProjectWorldLocationToScreen(WorldLocation, ScreenPos, true);
		}
	}

	if (!bProjected)
	{
		// 投影失败（如 Actor 在摄像机后方），默认放在视口中心
		ScreenPos = ViewportSize * 0.5f;
	}

	// 计算目标位置：在投影点附近偏移
	FVector2D TargetPos = ScreenPos + TargetOffset;

	// 获取浮窗期望尺寸
	FVector2D DesiredSize = GetDesiredSize();
	// 如果 DesiredSize 还未计算（首帧可能为 0），用默认尺寸
	if (DesiredSize.X <= 0 || DesiredSize.Y <= 0)
	{
		DesiredSize = FVector2D(200.0f, 150.0f);
	}

	// 边界检测：如果超出视口右侧，放到左侧
	if (TargetPos.X + DesiredSize.X > ViewportSize.X)
	{
		TargetPos.X = ScreenPos.X - TargetOffset.X - DesiredSize.X;
	}

	// 边界检测：如果超出视口顶部，向下偏移
	if (TargetPos.Y < 0.0f)
	{
		TargetPos.Y = ScreenPos.Y + FMath::Abs(TargetOffset.Y) + 20.0f;
	}

	// 边界检测：如果超出视口底部，向上偏移
	if (TargetPos.Y + DesiredSize.Y > ViewportSize.Y)
	{
		TargetPos.Y = ViewportSize.Y - DesiredSize.Y - 10.0f;
	}

	// 确保不超出视口
	TargetPos.X = FMath::Clamp(TargetPos.X, 0.0f, FMath::Max(0.0f, ViewportSize.X - DesiredSize.X));
	TargetPos.Y = FMath::Clamp(TargetPos.Y, 0.0f, FMath::Max(0.0f, ViewportSize.Y - DesiredSize.Y));

	// 设置屏幕位置
	SetPositionInViewport(TargetPos);
}
