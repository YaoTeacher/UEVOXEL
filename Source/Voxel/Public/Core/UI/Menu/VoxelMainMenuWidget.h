#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelScreenBase.h"
#include "VoxelMainMenuWidget.generated.h"

class UButton;

/**
 * 主菜单面板：开始游戏 / 设置 / 退出。
 * BP 只需放置同名按钮（BindWidget 自动绑定），无需手写事件图表。
 */
UCLASS(Abstract)
class VOXEL_API UVoxelMainMenuWidget : public UVoxelScreenBase
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	/** 开始游戏：打开游戏地图 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Menu")
	void StartGame();

	/** 打开设置面板 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Menu")
	void OpenSettings();

	/** 退出游戏 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Menu")
	void QuitGame();

protected:
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* StartGameButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* SettingsButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* QuitButton;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voxel|Menu")
	TSubclassOf<UVoxelScreenBase> SettingsWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voxel|Menu")
	FString GameMapName = TEXT("/Game/Resource/Levels/VoxelLevel_Presistent");

	UFUNCTION() void HandleStartGame();
	UFUNCTION() void HandleSettings();
	UFUNCTION() void HandleQuit();
};
