#include "Core/UI/HUD/VoxelEnvPanelWidget.h"
#include "Core/UI/HUD/LayerSwitchSectionWidget.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/EditableTextBox.h"
#include "Voxel/VoxelPlayerController.h"

UVoxelEnvPanelWidget::UVoxelEnvPanelWidget()
{
}

void UVoxelEnvPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 占位数据（时间系统 / 小地图渲染未接入，先落样式）
	if (ClockText)
	{
		ClockText->SetText(FText::FromString(TEXT("14:47")));
	}
	if (DateText)
	{
		DateText->SetText(FText::FromString(TEXT("第12天 · 春 · 晴")));
	}
	if (MinimapNameText)
	{
		MinimapNameText->SetText(FText::FromString(TEXT("我的酒店")));
	}
	if (MinimapCoordText)
	{
		MinimapCoordText->SetText(FText::FromString(TEXT("X:646 Y:359")));
	}

	CachedSC = GetStateComponent();
	if (!CachedSC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnvPanel] BuildStateComp 未就绪"));
		return;
	}

	CachedSC->OnSliceChanged.AddDynamic(this, &UVoxelEnvPanelWidget::OnSliceChanged);
	CachedSC->OnPawnHeightChanged.AddDynamic(this, &UVoxelEnvPanelWidget::OnPawnHeightChanged);

	// 按钮事件
	if (TimeDrawerButton)
	{
		TimeDrawerButton->OnClicked.AddDynamic(this, &UVoxelEnvPanelWidget::OnTimeDrawerClicked);
	}
	if (HeightDrawerButton)
	{
		HeightDrawerButton->OnClicked.AddDynamic(this, &UVoxelEnvPanelWidget::OnHeightDrawerClicked);
	}
	if (LocateButton)
	{
		LocateButton->OnClicked.AddDynamic(this, &UVoxelEnvPanelWidget::OnLocateClicked);
	}
	if (LayerPopButton)
	{
		LayerPopButton->OnClicked.AddDynamic(this, &UVoxelEnvPanelWidget::OnLayerPopClicked);
	}
	if (CutoutPopButton)
	{
		CutoutPopButton->OnClicked.AddDynamic(this, &UVoxelEnvPanelWidget::OnCutoutPopClicked);
	}

	// 高度控制事件
	if (PawnSlider)
	{
		PawnSlider->OnValueChanged.AddDynamic(this, &UVoxelEnvPanelWidget::OnPawnSliderChanged);
	}
	if (PawnInput)
	{
		PawnInput->OnTextCommitted.AddDynamic(this, &UVoxelEnvPanelWidget::OnPawnInputCommitted);
	}
	if (RenderSlider)
	{
		RenderSlider->OnValueChanged.AddDynamic(this, &UVoxelEnvPanelWidget::OnRenderSliderChanged);
	}
	if (RenderInput)
	{
		RenderInput->OnTextCommitted.AddDynamic(this, &UVoxelEnvPanelWidget::OnRenderInputCommitted);
	}
	if (SliceToggleButton)
	{
		SliceToggleButton->OnClicked.AddDynamic(this, &UVoxelEnvPanelWidget::OnSliceToggleClicked);
	}
	if (FilterToggleButton)
	{
		FilterToggleButton->OnClicked.AddDynamic(this, &UVoxelEnvPanelWidget::OnFilterToggleClicked);
	}

	RefreshPawnRow();
	RefreshRenderRow();
	SyncDrawerVisibility();
}

void UVoxelEnvPanelWidget::NativeDestruct()
{
	if (CachedSC)
	{
		CachedSC->OnSliceChanged.RemoveDynamic(this, &UVoxelEnvPanelWidget::OnSliceChanged);
		CachedSC->OnPawnHeightChanged.RemoveDynamic(this, &UVoxelEnvPanelWidget::OnPawnHeightChanged);
	}
	Super::NativeDestruct();
}

void UVoxelEnvPanelWidget::SyncDrawerVisibility() const
{
	if (TimeDrawer)
	{
		TimeDrawer->SetVisibility(bTimeDrawerOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (HeightDrawer)
	{
		HeightDrawer->SetVisibility(bHeightDrawerOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (LayerPop)
	{
		LayerPop->SetVisibility(bLayerPopOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (CutoutPop)
	{
		CutoutPop->SetVisibility(bCutoutPopOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

// ==================== 抽屉/弹窗开合 ====================

void UVoxelEnvPanelWidget::OnTimeDrawerClicked()
{
	bTimeDrawerOpen = !bTimeDrawerOpen;
	SyncDrawerVisibility();
}

void UVoxelEnvPanelWidget::OnHeightDrawerClicked()
{
	bHeightDrawerOpen = !bHeightDrawerOpen;
	SyncDrawerVisibility();
}

void UVoxelEnvPanelWidget::OnLocateClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[EnvPanel] 定位按钮点击（镜头回正未接入）"));
}

void UVoxelEnvPanelWidget::OnLayerPopClicked()
{
	bLayerPopOpen = !bLayerPopOpen;
	SyncDrawerVisibility();
}

void UVoxelEnvPanelWidget::OnCutoutPopClicked()
{
	bCutoutPopOpen = !bCutoutPopOpen;
	SyncDrawerVisibility();
}

// ==================== 高度控制（迁移自 TopLeftBar） ====================

void UVoxelEnvPanelWidget::OnPawnSliderChanged(float Value)
{
	if (bIsUpdatingSlider) return;
	if (UVoxelInteractionComponent* InteractionComp = GetToolComponent())
	{
		InteractionComp->SetPawnHeightLayer(FMath::RoundToInt(Value));
		RefreshPawnRow();
	}
}

void UVoxelEnvPanelWidget::OnPawnInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const FString InputStr = Text.ToString();
	if (InputStr.IsNumeric())
	{
		if (UVoxelInteractionComponent* InteractionComp = GetToolComponent())
		{
			InteractionComp->SetPawnHeightLayer(FCString::Atoi(*InputStr));
		}
	}
	RefreshPawnRow();
}

void UVoxelEnvPanelWidget::OnRenderSliderChanged(float Value)
{
	if (bIsUpdatingSlider) return;
	if (UVoxelInteractionComponent* InteractionComp = GetToolComponent())
	{
		InteractionComp->SetEditSliceLayer(FMath::RoundToInt(Value));
		if (CachedSC && !CachedSC->IsSliceEnabled())
		{
			InteractionComp->SetSliceEnabled(true);
		}
		RefreshRenderRow();
	}
}

void UVoxelEnvPanelWidget::OnRenderInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const FString InputStr = Text.ToString();
	if (InputStr.IsNumeric())
	{
		if (UVoxelInteractionComponent* InteractionComp = GetToolComponent())
		{
			InteractionComp->SetEditSliceLayer(FCString::Atoi(*InputStr));
			if (CachedSC && !CachedSC->IsSliceEnabled())
			{
				InteractionComp->SetSliceEnabled(true);
			}
		}
	}
	RefreshRenderRow();
}

void UVoxelEnvPanelWidget::OnSliceToggleClicked()
{
	if (CachedSC && GetToolComponent())
	{
		GetToolComponent()->SetSliceEnabled(!CachedSC->IsSliceEnabled());
		RefreshRenderRow();
	}
}

void UVoxelEnvPanelWidget::OnFilterToggleClicked()
{
	if (!CachedSC) return;

	const ESliceFilterMode Cur = CachedSC->GetSliceFilterMode();
	const ESliceFilterMode NewMode = (Cur == ESliceFilterMode::All)
		? ESliceFilterMode::PlayerManaged
		: ESliceFilterMode::All;

	if (UVoxelInteractionComponent* InteractionComp = GetToolComponent())
	{
		InteractionComp->SetSliceFilterMode(NewMode);
		RefreshRenderRow();
	}
}

// ==================== 状态刷新 ====================

void UVoxelEnvPanelWidget::OnSliceChanged()
{
	RefreshRenderRow();
}

void UVoxelEnvPanelWidget::OnPawnHeightChanged()
{
	RefreshPawnRow();
}

UVoxelPlayerStateCacheComponent* UVoxelEnvPanelWidget::GetStateComponent() const
{
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		return PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
	}
	return nullptr;
}

UVoxelInteractionComponent* UVoxelEnvPanelWidget::GetToolComponent() const
{
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		return PC->InteractionComp;
	}
	return nullptr;
}

void UVoxelEnvPanelWidget::RefreshPawnRow()
{
	if (!CachedSC) return;
	const int32 CurLayer = CachedSC->GetPawnHeightLayer();
	const int32 MinLayer = CachedSC->GetMinSliceLayer();
	const int32 MaxLayer = CachedSC->GetMaxSliceLayer();

	if (PawnValueText)
	{
		PawnValueText->SetText(FText::FromString(FString::Printf(TEXT("%d"), CurLayer)));
	}
	if (PawnInput && !PawnInput->HasKeyboardFocus())
	{
		PawnInput->SetText(FText::FromString(FString::FromInt(CurLayer)));
	}
	if (PawnSlider)
	{
		SyncSlider(PawnSlider, MinLayer, MaxLayer, CurLayer);
	}
}

void UVoxelEnvPanelWidget::RefreshRenderRow()
{
	if (!CachedSC) return;
	const int32 CurLayer = CachedSC->GetEditSliceLayer();
	const int32 MinLayer = CachedSC->GetMinSliceLayer();
	const int32 MaxLayer = CachedSC->GetMaxSliceLayer();

	if (RenderValueText)
	{
		RenderValueText->SetText(FText::FromString(FString::Printf(TEXT("%d"), CurLayer)));
	}
	if (RenderInput && !RenderInput->HasKeyboardFocus())
	{
		RenderInput->SetText(FText::FromString(FString::FromInt(CurLayer)));
	}
	if (RenderSlider)
	{
		SyncSlider(RenderSlider, MinLayer, MaxLayer, CurLayer);
	}
}

void UVoxelEnvPanelWidget::SyncSlider(USlider* Slider, int32 MinLayer, int32 MaxLayer, int32 CurLayer)
{
	if (!Slider) return;
	bIsUpdatingSlider = true;
	Slider->SetMinValue(static_cast<float>(MinLayer));
	Slider->SetMaxValue(static_cast<float>(MaxLayer));
	Slider->SetValue(static_cast<float>(CurLayer));
	bIsUpdatingSlider = false;
}
