#include "Core/VoxelMainMenuGameMode.h"
#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "Core/UI/Base/VoxelScreenBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"

AVoxelMainMenuGameMode::AVoxelMainMenuGameMode()
{
	DefaultPawnClass = nullptr;

	// 固定资产硬引用（与 VoxelGameMode 引用 BP_PlayerController 同一模式，不依赖 CDO 持久化）
	static ConstructorHelpers::FClassFinder<UVoxelScreenBase> MainMenuBP(
		TEXT("/Game/Resource/UI/Menu/WBP_MainMenu.WBP_MainMenu_C"));
	if (MainMenuBP.Class)
	{
		MainMenuWidgetClass = MainMenuBP.Class;
	}
}

void AVoxelMainMenuGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer || !MainMenuWidgetClass) return;

	// 防御性单例：清理跨 OpenLevel 残留的旧 Layout（widget 不随 world 销毁），
	// 保证本地图只有一个 PrimaryLayout，避免输入路由错乱/多层堆叠。
	TArray<UUserWidget*> ExistingLayouts;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(NewPlayer, ExistingLayouts, UVoxelPrimaryGameLayout::StaticClass(), true);
	for (UUserWidget* W : ExistingLayouts)
	{
		W->RemoveFromParent();
	}

	// 创建无 HUD 的 PrimaryLayout（C++ 默认 MainPanelClass 为空）
	const TSubclassOf<UVoxelPrimaryGameLayout> LayoutClass = MenuLayoutClass
		? MenuLayoutClass
		: TSubclassOf<UVoxelPrimaryGameLayout>(UVoxelPrimaryGameLayout::StaticClass());

	if (UVoxelPrimaryGameLayout* Layout = CreateWidget<UVoxelPrimaryGameLayout>(NewPlayer, LayoutClass))
	{
		Layout->AddToViewport(0);
		Layout->PushGameMenu(MainMenuWidgetClass);
		NewPlayer->bShowMouseCursor = true;
	}
}
