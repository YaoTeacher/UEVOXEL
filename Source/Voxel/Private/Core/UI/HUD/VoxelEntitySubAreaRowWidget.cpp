#include "Core/UI/HUD/VoxelEntitySubAreaRowWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UVoxelEntitySubAreaRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RowButton)
	{
		RowButton->OnClicked.AddDynamic(this, &UVoxelEntitySubAreaRowWidget::HandleRowClicked);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[EntityDetailRow] RowButton 未绑定（检查 WBP_EntitySubAreaRow 控件名）"));
	}
}

void UVoxelEntitySubAreaRowWidget::InitializeRow(const FText& AreaName, const FText& AreaStatus, int32 InAreaId)
{
	TargetAreaId = InAreaId;
	if (NameText) NameText->SetText(AreaName);
	if (StatusText) StatusText->SetText(AreaStatus);
}

void UVoxelEntitySubAreaRowWidget::HandleRowClicked()
{
	OnRowClicked.Broadcast(TargetAreaId);
}
