#include "Core/UI/Menu/VoxelPauseMenuWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "Core/VoxelGameInstance.h"
#include "Components/Button.h"

void UVoxelPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddDynamic(this, &UVoxelPauseMenuWidget::HandleResume);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UVoxelPauseMenuWidget::HandleSettings);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UVoxelPauseMenuWidget::HandleQuit);
	}
}

void UVoxelPauseMenuWidget::OnEnterScreen(UObject* Context)
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetPause(true);
	}
}

void UVoxelPauseMenuWidget::HandleResume() { Resume(); }
void UVoxelPauseMenuWidget::HandleSettings() { OpenSettings(); }
void UVoxelPauseMenuWidget::HandleQuit() { QuitToMainMenu(); }

void UVoxelPauseMenuWidget::Resume()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetPause(false);
	}
	DeactivateWidget();
}

void UVoxelPauseMenuWidget::OpenSettings()
{
	if (UVoxelPrimaryGameLayout* Layout = UVoxelPrimaryGameLayout::GetFromPC(GetOwningPlayer()))
	{
		Layout->PushGameMenu(SettingsWidgetClass);
	}
}

void UVoxelPauseMenuWidget::QuitToMainMenu()
{
	if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
	{
		GI->SaveCurScene();
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetPause(false);
	}
	// 同 StartGame：切图前移除所属 Layout，避免游戏 Layout 残留到主菜单地图。
	if (UVoxelPrimaryGameLayout* Layout = GetTypedOuter<UVoxelPrimaryGameLayout>())
	{
		Layout->RemoveFromParent();
	}
	UGameplayStatics::OpenLevel(this, FName(*MainMenuMapName));
}
