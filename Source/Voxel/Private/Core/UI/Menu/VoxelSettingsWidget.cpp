#include "Core/UI/Menu/VoxelSettingsWidget.h"
#include "Core/VoxelGameUserSettings.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"

void UVoxelSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 自动绑定按钮点击（BP 只需放置同名控件）
	if (ApplyButton)
	{
		ApplyButton->OnClicked.AddDynamic(this, &UVoxelSettingsWidget::HandleApplyClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UVoxelSettingsWidget::HandleCancelClicked);
	}

	// 填充画质下拉选项
	if (QualityPresetCombo)
	{
		QualityPresetCombo->ClearOptions();
		QualityPresetCombo->AddOption(TEXT("低"));
		QualityPresetCombo->AddOption(TEXT("中"));
		QualityPresetCombo->AddOption(TEXT("高"));
		QualityPresetCombo->AddOption(TEXT("极致"));
	}

	// 填充当前值
	const int32 LD = GetInitialLoadDistance();
	if (LoadDistanceSlider)
	{
		// 滑条 0~1 映射到 LoadDistance 1~8
		LoadDistanceSlider->SetValue(static_cast<float>(LD - 1) / 7.0f);
	}
	if (QualityPresetCombo)
	{
		QualityPresetCombo->SetSelectedIndex(GetInitialQualityPreset());
	}
}

int32 UVoxelSettingsWidget::GetInitialLoadDistance() const
{
	if (const UVoxelGameUserSettings* S = UVoxelGameUserSettings::Get())
	{
		return S->LoadDistance;
	}
	return 3;
}

int32 UVoxelSettingsWidget::GetInitialQualityPreset() const
{
	if (const UVoxelGameUserSettings* S = UVoxelGameUserSettings::Get())
	{
		return S->ScalabilityQuality.ViewDistanceQuality;
	}
	return 2;
}

void UVoxelSettingsWidget::ApplyAndSave(int32 LoadDistance, int32 QualityPreset)
{
	if (UVoxelGameUserSettings* S = UVoxelGameUserSettings::Get())
	{
		S->LoadDistance = FMath::Clamp(LoadDistance, 1, 8);
		S->SetQualityPreset(FMath::Clamp(QualityPreset, 0, 3));
		S->ApplySettings();
		S->SaveSettings();
		UE_LOG(LogTemp, Log, TEXT("[Settings] Applied: LoadDistance=%d, QualityPreset=%d"), S->LoadDistance, QualityPreset);
	}
	DeactivateWidget();
}

void UVoxelSettingsWidget::Cancel()
{
	DeactivateWidget();
}

void UVoxelSettingsWidget::HandleApplyClicked()
{
	int32 LD = 3;
	int32 QP = 2;
	if (LoadDistanceSlider)
	{
		LD = FMath::RoundToInt(FMath::Lerp(1.0f, 8.0f, LoadDistanceSlider->GetValue()));
	}
	if (QualityPresetCombo)
	{
		QP = QualityPresetCombo->GetSelectedIndex();
	}
	ApplyAndSave(LD, QP);
}

void UVoxelSettingsWidget::HandleCancelClicked()
{
	Cancel();
}
