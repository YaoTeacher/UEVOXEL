#include "Core/UI/Base/VoxelDragProgressWidget.h"
#include "Components/TextBlock.h"

void UVoxelDragProgressWidget::UpdateProgress_Implementation(float Progress)
{
	// C++ 默认实现：更新 ProgressText 百分比文本
	if (ProgressText)
	{
		const int32 Percent = FMath::RoundToInt(Progress * 100.0f);
		ProgressText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Percent)));
	}
}

void UVoxelDragProgressWidget::UpdatePosition(const FVector2D& MousePosition, const FVector2D& ViewportSize)
{
	FVector2D DesiredSize = GetDesiredSize();

	// 进度圈中心对齐鼠标位置
	FVector2D TargetPos = MousePosition + CenterOffset - DesiredSize * 0.5f;

	// 边界检测
	TargetPos.X = FMath::Clamp(TargetPos.X, 0.0f, FMath::Max(0.0f, ViewportSize.X - DesiredSize.X));
	TargetPos.Y = FMath::Clamp(TargetPos.Y, 0.0f, FMath::Max(0.0f, ViewportSize.Y - DesiredSize.Y));

	SetPositionInViewport(TargetPos);
}
