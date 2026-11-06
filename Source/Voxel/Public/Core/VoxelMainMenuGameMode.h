#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VoxelMainMenuGameMode.generated.h"

class UVoxelScreenBase;
class UVoxelPrimaryGameLayout;

/**
 * 主菜单 GameMode：不生成 Pawn，用默认 APlayerController 承载 UI。
 * 在 PostLogin 里创建 PrimaryLayout（无 HUD）并 push 主菜单面板。
 * 不触发体素世界生成（无 LoadSaveData）。
 */
UCLASS()
class VOXEL_API AVoxelMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVoxelMainMenuGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	/** 菜单 Layout 类（默认 C++ UVoxelPrimaryGameLayout，无 HUD） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voxel|Menu")
	TSubclassOf<UVoxelPrimaryGameLayout> MenuLayoutClass;

	/** 主菜单面板类（ConstructorHelpers 硬引用 WBP_MainMenu） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voxel|Menu")
	TSubclassOf<UVoxelScreenBase> MainMenuWidgetClass;
};
