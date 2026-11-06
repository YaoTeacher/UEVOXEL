#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelScreenBase.h"
#include "VoxelPauseMenuWidget.generated.h"

class UButton;

/**
 * 暂停菜单面板：进入时彻底暂停游戏，提供 继续/设置/退出到主菜单。
 * BP 只需放置同名按钮（BindWidget 自动绑定），无需手写事件图表。
 */
UCLASS(Abstract)
class VOXEL_API UVoxelPauseMenuWidget : public UVoxelScreenBase
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void OnEnterScreen(UObject* Context) override;

	/** 继续：解除暂停并关闭面板 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Menu")
	void Resume();

	/** 打开设置面板 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Menu")
	void OpenSettings();

	/** 退出到主菜单：保存当前场景后切换 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Menu")
	void QuitToMainMenu();

	virtual bool HandleEscape() override { Resume(); return true; }

protected:
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* ResumeButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* SettingsButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* QuitButton;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voxel|Menu")
	TSubclassOf<UVoxelScreenBase> SettingsWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voxel|Menu")
	FString MainMenuMapName = TEXT("/Game/Resource/BP/Scene_BP/MainMenu");

	UFUNCTION() void HandleResume();
	UFUNCTION() void HandleSettings();
	UFUNCTION() void HandleQuit();
};
