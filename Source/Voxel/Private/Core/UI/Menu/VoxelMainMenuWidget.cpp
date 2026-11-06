#include "Core/UI/Menu/VoxelMainMenuWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "Components/Button.h"

void UVoxelMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (StartGameButton)
	{
		StartGameButton->OnClicked.AddDynamic(this, &UVoxelMainMenuWidget::HandleStartGame);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UVoxelMainMenuWidget::HandleSettings);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UVoxelMainMenuWidget::HandleQuit);
	}
}

void UVoxelMainMenuWidget::HandleStartGame() { StartGame(); }
void UVoxelMainMenuWidget::HandleSettings() { OpenSettings(); }
void UVoxelMainMenuWidget::HandleQuit() { QuitGame(); }

void UVoxelMainMenuWidget::StartGame()
{
	// Widget 不随 OpenLevel 销毁（挂在 GameViewport/LocalPlayer 上），
	// 切图前必须移除所属 Layout，否则残留的激活态菜单会截获输入、并造成多层 Layout 堆叠。
	if (UVoxelPrimaryGameLayout* Layout = GetTypedOuter<UVoxelPrimaryGameLayout>())
	{
		Layout->RemoveFromParent();
	}
	UGameplayStatics::OpenLevel(this, FName(*GameMapName));
}

void UVoxelMainMenuWidget::OpenSettings()
{
	if (UVoxelPrimaryGameLayout* Layout = UVoxelPrimaryGameLayout::GetFromPC(GetOwningPlayer()))
	{
		Layout->PushGameMenu(SettingsWidgetClass);
	}
}

void UVoxelMainMenuWidget::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
